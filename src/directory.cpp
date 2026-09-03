#include "distributedcachedirectory/directory.hpp"
#include "distributedcachedirectory/digest.hpp"
#include "distributedcachedirectory/serde.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace distributedcachedirectory {

namespace {

int ri(Integrity i) {
  switch (i) { case Integrity::VERIFIED: return 4; case Integrity::UNVERIFIED: return 3;
               case Integrity::UNKNOWN: return 2; case Integrity::CORRUPT: return 1;
               case Integrity::MISSING: return 0; } return 2;
}
int rf(Freshness f) {
  switch (f) { case Freshness::CURRENT: return 3; case Freshness::REVALIDATION_REQUIRED: return 2;
               case Freshness::STALE: return 1; case Freshness::UNKNOWN: return 0; } return 0;
}
int rh(Health h) {
  switch (h) { case Health::HEALTHY: return 4; case Health::DEGRADED: return 3;
               case Health::UNHEALTHY: return 2; case Health::UNAVAILABLE: return 1;
               case Health::UNKNOWN: return 0; } return 0;
}
int rr(Reachability r) {
  switch (r) { case Reachability::REACHABLE: return 4; case Reachability::DEGRADED: return 3;
               case Reachability::REVALIDATION_REQUIRED: return 2; case Reachability::UNKNOWN: return 1;
               case Reachability::UNREACHABLE: return 0; } return 0;
}
double ni(Integrity i) { return ri(i) / 4.0; }
double nf(Freshness f) { return rf(f) / 3.0; }
double nh(Health h) { return rh(h) / 4.0; }
double nr(Reachability r) { return rr(r) / 4.0; }

ContentDigest semantic_digest_of(const DirectoryRecord& r) {
  serde::ByteWriter w;
  w.u64(r.state_id.value());
  w.u64(r.state_generation.value());
  w.u8(static_cast<std::uint8_t>(r.kind));
  w.string(r.name_space);
  w.u64(r.cache.value());
  w.u64(r.entry.value());
  w.u64(r.replica.value());
  w.u64(r.location.value());
  w.u64(r.node.value());
  w.u64(r.worker.value());
  w.u64(r.worker_boot.value());
  w.u64(r.device.value());
  w.u8(static_cast<std::uint8_t>(r.domain));
  w.u64(r.logical_bytes);
  w.u64(r.physical_bytes);
  w.u8(r.physical_bytes_known ? 1u : 0u);
  w.u8(r.content_digest_known ? 1u : 0u);
  if (r.content_digest_known) w.bytes(r.content_digest);
  w.u64(r.compatibility.value());
  w.u64(r.provenance.value());
  w.u64(r.location_generation.value());
  w.u64(r.replica_generation.value());
  w.u64(r.entry_generation.value());
  w.u8(static_cast<std::uint8_t>(r.health));
  w.u8(static_cast<std::uint8_t>(r.freshness));
  w.u8(static_cast<std::uint8_t>(r.integrity));
  w.u8(static_cast<std::uint8_t>(r.reachability));
  w.u8(r.has_lease ? 1u : 0u);
  w.u64(r.policy_generation.value());
  w.u64(r.authority.epoch.value());
  w.u64(r.authority.boot.value());
  w.u64(r.authority.directory_generation.value());
  w.u64(r.authority.fence);
  return digest::sha256(w.bytes());
}

bool identify_matches(const DirectoryRecord& r, const DirectoryQuery& q) {
  if (q.exact_state && r.state_id != q.state) return false;
  if (q.exact_state_generation && r.state_generation != q.state_generation) return false;
  if (q.has_kind && r.kind != q.kind) return false;
  if (!q.name_space.empty() && r.name_space != q.name_space) return false;
  if (q.require_content_digest &&
      !(r.content_digest_known && digest::digest_equal(r.content_digest, q.content_digest)))
    return false;
  if (q.require_compatibility && r.compatibility != q.compatibility) return false;
  return true;
}

// Returns the first hard-filter reason that eliminates the record, or false if
// the record is eligible.
bool hard_reject(const DirectoryRecord& r, const DirectoryQuery& q, RejectionReason& reason) {
  if (q.current_only && !r.current) { reason = RejectionReason::NOT_CURRENT; return true; }
  if (r.lifecycle == LifecycleState::INVALIDATED) { reason = RejectionReason::INVALIDATED; return true; }
  if (r.lifecycle == LifecycleState::TOMBSTONED) { reason = RejectionReason::TOMBSTONED; return true; }
  if (r.lifecycle == LifecycleState::EXPIRED) { reason = RejectionReason::EXPIRED_LEASE; return true; }
  if (r.integrity == Integrity::CORRUPT) { reason = RejectionReason::CORRUPT; return true; }
  if (r.integrity == Integrity::MISSING) { reason = RejectionReason::MISSING_BYTES; return true; }
  if (q.has_integrity && ri(r.integrity) < ri(q.integrity)) { reason = RejectionReason::INTEGRITY_BELOW_REQUIREMENT; return true; }
  if (q.has_freshness && rf(r.freshness) < rf(q.freshness)) { reason = RejectionReason::FRESHNESS_BELOW_REQUIREMENT; return true; }
  if (q.has_health && rh(r.health) < rh(q.health)) { reason = RejectionReason::HEALTH_BELOW_REQUIREMENT; return true; }
  if (q.has_reachability && rr(r.reachability) < rr(q.reachability)) { reason = RejectionReason::REACHABILITY_BELOW_REQUIREMENT; return true; }
  if (!q.allowed_domains.empty()) {
    bool ok = std::find(q.allowed_domains.begin(), q.allowed_domains.end(), r.domain) != q.allowed_domains.end();
    if (!ok) { reason = RejectionReason::DOMAIN_UNAVAILABLE; return true; }
  }
  if (r.reachability == Reachability::UNREACHABLE) { reason = RejectionReason::UNREACHABLE; return true; }
  if (r.freshness == Freshness::STALE) { reason = RejectionReason::STALE_RECORD; return true; }
  if (r.has_lease && r.lease.state == LeaseState::EXPIRED) { reason = RejectionReason::EXPIRED_LEASE; return true; }
  return false;
}

bool candidate_less(const Candidate& a, const Candidate& b) {
  if (a.score != b.score) return a.score > b.score;
  if (a.record.record_id != b.record.record_id) return a.record.record_id.value() < b.record.record_id.value();
  if (a.record.location != b.record.location) return a.record.location.value() < b.record.location.value();
  if (a.record.replica != b.record.replica) return a.record.replica.value() < b.record.replica.value();
  if (a.record.domain != b.record.domain) return static_cast<int>(a.record.domain) < static_cast<int>(b.record.domain);
  return a.record.cache.value() < b.record.cache.value();
}

}  // namespace

Directory::Directory() = default;

// ---- workers ----

Result<WorkerSession> Directory::register_worker_session(const WorkerSession& session) {
  if (session.boot.is_null()) return Result<WorkerSession>::failure(DirectoryError::INVALID_IDENTITY, "empty WorkerBootId");
  if (session.worker.is_null()) return Result<WorkerSession>::failure(DirectoryError::INVALID_IDENTITY, "empty WorkerId");
  auto it = workers_.find(session.boot);
  if (it != workers_.end() && it->second.live)
    return Result<WorkerSession>::failure(DirectoryError::INVALID_IDENTITY, "worker boot already live");
  bool restart = false;
  for (const auto& kv : workers_) if (kv.second.worker == session.worker && kv.second.boot != session.boot) restart = true;
  if (restart) ++accounting_.worker_restarts;
  WorkerSession s = session;
  s.live = true;
  workers_[session.boot] = s;
  return Result<WorkerSession>::success(s);
}

bool Directory::worker_live(WorkerBootId boot) const {
  auto it = workers_.find(boot);
  return it != workers_.end() && it->second.live;
}

Result<AckResult> Directory::on_worker_loss(WorkerBootId boot, std::string reason) {
  (void)reason;
  auto it = workers_.find(boot);
  AckResult out;
  if (it == workers_.end() || !it->second.live) { out.ok = true; out.message = "worker already lost"; return Result<AckResult>::success(out); }
  it->second.live = false;
  ++it->second.source_authority;
  std::uint32_t affected = 0;
  for (auto& r : records_) {
    if (r.worker_boot == boot) {
      bool pl = r.domain == MemoryDomain::HOST_MEMORY || r.domain == MemoryDomain::HOST_PINNED ||
                r.domain == MemoryDomain::CUDA_DEVICE || r.domain == MemoryDomain::LOCAL_FILESYSTEM ||
                r.domain == MemoryDomain::LOCAL_NVME_CLASS;
      if (pl) { r.freshness = Freshness::REVALIDATION_REQUIRED; r.reachability = Reachability::REVALIDATION_REQUIRED; r.lifecycle = LifecycleState::STALE; }
      else { r.freshness = Freshness::STALE; r.reachability = Reachability::UNREACHABLE; r.lifecycle = LifecycleState::UNREACHABLE; }
      ++affected;
    }
  }
  for (auto& kv : locations_) if (kv.second.worker_boot == boot) { kv.second.freshness = Freshness::REVALIDATION_REQUIRED; kv.second.reachability = Reachability::REVALIDATION_REQUIRED; kv.second.health = Health::UNAVAILABLE; }
  for (auto& kv : caches_) if (kv.second.worker_boot == boot) { kv.second.reachability = Reachability::UNREACHABLE; kv.second.health = Health::UNAVAILABLE; kv.second.freshness = Freshness::STALE; }
  for (auto& kv : leases_) if (kv.second.holder == boot) kv.second.state = LeaseState::REVALIDATION_REQUIRED;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true;
  out.message = "worker " + boot.str() + " lost; " + std::to_string(affected) + " records marked";
  return Result<AckResult>::success(out);
}

// ---- registration ----

Result<CacheDescriptor> Directory::register_cache(const CacheDescriptor& cache) {
  if (cache.cache_id.is_null()) return Result<CacheDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty CacheId");
  if (cache.worker_boot.is_null()) return Result<CacheDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty WorkerBootId");
  if (cache.authority.epoch != epoch_) return Result<CacheDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (!worker_live(cache.worker_boot)) return Result<CacheDescriptor>::failure(DirectoryError::NO_AUTHORITY, "worker session not live");
  auto it = caches_.find(cache.cache_id);
  if (it != caches_.end()) {
    if (it->second.worker_boot != cache.worker_boot) return Result<CacheDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "cache owned by another incarnation");
    if (cache.authority.fence < it->second.authority.fence) return Result<CacheDescriptor>::failure(DirectoryError::STALE_AUTHORITY, "stale cache authority");
  }
  caches_[cache.cache_id] = cache;
  ++accounting_.registrations;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<CacheDescriptor>::success(cache);
}

Result<LocationDescriptor> Directory::register_location(const LocationDescriptor& loc) {
  if (loc.location_id.is_null()) return Result<LocationDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty LocationId");
  if (loc.worker_boot.is_null()) return Result<LocationDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty WorkerBootId");
  if (loc.locator.key.empty()) return Result<LocationDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty locator");
  if (loc.authority.epoch != epoch_) return Result<LocationDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (!worker_live(loc.worker_boot)) return Result<LocationDescriptor>::failure(DirectoryError::NO_AUTHORITY, "worker session not live");
  auto it = locations_.find(loc.location_id);
  if (it != locations_.end()) {
    if (it->second.worker_boot != loc.worker_boot) return Result<LocationDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "location owned by another incarnation");
    if (loc.authority.fence < it->second.authority.fence) return Result<LocationDescriptor>::failure(DirectoryError::STALE_AUTHORITY, "stale location authority");
    if (loc.generation.value() < it->second.generation.value()) return Result<LocationDescriptor>::failure(DirectoryError::STALE_GENERATION, "stale location generation");
  }
  locations_[loc.location_id] = loc;
  ++accounting_.registrations;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<LocationDescriptor>::success(loc);
}

Result<ReplicaDescriptor> Directory::register_replica(const ReplicaDescriptor& replica) {
  if (replica.replica_id.is_null()) return Result<ReplicaDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty ReplicaId");
  if (!locations_.count(replica.location)) return Result<ReplicaDescriptor>::failure(DirectoryError::MISSING_DEPENDENCY, "location not registered");
  if (replica.authority.epoch != epoch_) return Result<ReplicaDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (!worker_live(replica.authority.boot)) return Result<ReplicaDescriptor>::failure(DirectoryError::NO_AUTHORITY, "worker session not live");
  auto it = replicas_.find(replica.replica_id);
  if (it != replicas_.end() && it->second.authority.boot != replica.authority.boot)
    return Result<ReplicaDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "replica owned by another incarnation");
  replicas_[replica.replica_id] = replica;
  ++accounting_.registrations;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<ReplicaDescriptor>::success(replica);
}

// ---- registration transaction ----

DirectoryError Directory::check_registration(const DirectoryRecord& record, std::string& why) const {
  if (record.state_id.is_null()) { why = "empty StateId"; return DirectoryError::INVALID_IDENTITY; }
  if (record.cache.is_null()) { why = "empty CacheId"; return DirectoryError::INVALID_IDENTITY; }
  if (record.entry.is_null()) { why = "empty CacheEntryId"; return DirectoryError::INVALID_IDENTITY; }
  if (record.replica.is_null()) { why = "empty ReplicaId"; return DirectoryError::INVALID_IDENTITY; }
  if (record.location.is_null()) { why = "empty LocationId"; return DirectoryError::INVALID_IDENTITY; }
  if (record.authority.epoch != epoch_) { why = "stale epoch"; return DirectoryError::STALE_EPOCH; }
  if (!worker_live(record.authority.boot)) { why = "worker session not live"; return DirectoryError::NO_AUTHORITY; }

  auto cit = caches_.find(record.cache);
  if (cit == caches_.end()) { why = "cache not registered"; return DirectoryError::MISSING_DEPENDENCY; }
  auto lit = locations_.find(record.location);
  if (lit == locations_.end()) { why = "location not registered"; return DirectoryError::MISSING_DEPENDENCY; }
  auto rit = replicas_.find(record.replica);
  if (rit == replicas_.end()) { why = "replica not registered"; return DirectoryError::MISSING_DEPENDENCY; }

  const auto& loc = lit->second;
  const auto& rep = rit->second;
  if (rep.location != record.location) { why = "replica/location mismatch"; return DirectoryError::MISSING_DEPENDENCY; }
  if (rep.cache != record.cache || rep.entry != record.entry) { why = "replica/cache mismatch"; return DirectoryError::MISSING_DEPENDENCY; }
  if (loc.node != record.node || loc.worker != record.worker || loc.worker_boot != record.worker_boot) { why = "location owner mismatch"; return DirectoryError::MISSING_DEPENDENCY; }
  if (loc.domain != record.domain) { why = "domain mismatch"; return DirectoryError::DIMENSION_MISMATCH; }
  if (lit->second.reachability == Reachability::UNREACHABLE) { why = "location invalidated"; return DirectoryError::STALE_GENERATION; }

  TombstoneTarget t; t.kind = TombstoneKind::STATE; t.state = record.state_id; t.generation_floor = record.state_generation.value();
  if (covered_by_tombstone(t)) { why = "covered by state tombstone"; return DirectoryError::TOMBSTONED; }

  auto cg = current_state_gen_.find(record.state_id);
  if (cg != current_state_gen_.end()) {
    if (record.state_generation.value() < cg->second.value()) { why = "state generation below current"; return DirectoryError::STALE_GENERATION; }
  }

  for (const auto& r : records_) {
    if (!r.current && r.lifecycle != LifecycleState::SUPERSEDED) continue;
    if (r.state_id != record.state_id) continue;
    if (r.state_generation != record.state_generation) continue;
    if (r.replica == record.replica && r.location == record.location && r.cache == record.cache) {
      bool identical = r.logical_bytes == record.logical_bytes &&
                       r.content_digest_known == record.content_digest_known &&
                       (!record.content_digest_known || digest::digest_equal(r.content_digest, record.content_digest)) &&
                       r.replica_generation == record.replica_generation &&
                       r.location_generation == record.location_generation;
      if (identical) { why = "exact duplicate"; return DirectoryError::DUPLICATE_IDENTITY; }
      why = "conflicting duplicate for same copy"; return DirectoryError::CONFLICTING_DUPLICATE;
    }
  }
  return DirectoryError::NONE;
}

Result<DirectoryRecord> Directory::register_entry(const DirectoryRecord& record) {
  std::string why;
  DirectoryError err = check_registration(record, why);
  if (err != DirectoryError::NONE) return Result<DirectoryRecord>::failure(err, why);
  DirectoryRecord r = record;
  bool supersede = false;
  auto cg = current_state_gen_.find(r.state_id);
  if (cg != current_state_gen_.end() && r.state_generation.value() > cg->second.value()) supersede = true;
  return commit_record(std::move(r), supersede);
}

Result<DirectoryRecord> Directory::commit_record(DirectoryRecord record, bool supersede_older) {
  if (record.record_id.is_null()) record.record_id = DirectoryRecordId(next_object_id());
  if (record.created_ns == 0) record.created_ns = wall_clock_ns();
  record.updated_ns = wall_clock_ns();
  record.record_generation = RecordGeneration(directory_generation_.value() + 1);
  record.current = true;
  record.semantic_digest = semantic_digest_of(record);
  record.lifecycle = (record.integrity == Integrity::CORRUPT) ? LifecycleState::FAILED
                   : (record.reachability == Reachability::UNREACHABLE) ? LifecycleState::UNREACHABLE
                   : LifecycleState::AVAILABLE;

  if (supersede_older) {
    for (auto& r : records_) {
      if (r.state_id == record.state_id && r.state_generation.value() < record.state_generation.value() && r.current) {
        r.current = false; r.lifecycle = LifecycleState::SUPERSEDED;
      }
    }
  }
  for (auto& r : records_) {
    if (r.current && r.state_id == record.state_id && r.replica == record.replica &&
        r.location == record.location && r.record_id != record.record_id) {
      if (record.replica_generation.value() > r.replica_generation.value() ||
          record.location_generation.value() > r.location_generation.value()) {
        r.current = false; r.lifecycle = LifecycleState::SUPERSEDED;
      }
    }
  }

  records_.push_back(std::move(record));
  const auto& rec = records_.back();
  current_state_gen_[rec.state_id] = rec.state_generation;
  directory_generation_ = DirectoryGeneration(directory_generation_.value() + 1);
  ++accounting_.registrations;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_.back());
}
// ---- lifecycle / authority helpers ----

namespace {
bool can_lifecycle(LifecycleState from, LifecycleState to) {
  if (from == LifecycleState::TOMBSTONED) return to == LifecycleState::TOMBSTONED;
  if (from == LifecycleState::SUPERSEDED) return to == LifecycleState::TOMBSTONED;
  return true;
}

LifecycleState derive_lifecycle(const DirectoryRecord& r) {
  if (!r.current && r.lifecycle == LifecycleState::SUPERSEDED) return LifecycleState::SUPERSEDED;
  if (r.integrity == Integrity::CORRUPT) return LifecycleState::FAILED;
  if (r.integrity == Integrity::MISSING) return LifecycleState::MISSING;
  if (r.reachability == Reachability::UNREACHABLE) return LifecycleState::UNREACHABLE;
  if (r.health == Health::UNAVAILABLE) return LifecycleState::UNREACHABLE;
  if (r.has_lease && r.lease.state == LeaseState::EXPIRED) return LifecycleState::EXPIRED;
  if (r.freshness == Freshness::STALE) return LifecycleState::STALE;
  if (r.health == Health::DEGRADED || r.reachability == Reachability::DEGRADED) return LifecycleState::DEGRADED;
  if (r.lifecycle == LifecycleState::INVALIDATED) return LifecycleState::INVALIDATED;
  return LifecycleState::AVAILABLE;
}
}  // namespace

std::size_t Directory::find_record_index(DirectoryRecordId id) const {
  for (std::size_t i = 0; i < records_.size(); ++i) if (records_[i].record_id == id) return i;
  return static_cast<std::size_t>(-1);
}

std::optional<DirectoryRecord> Directory::record(DirectoryRecordId id) const {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return std::nullopt;
  return records_[i];
}

DirectoryError Directory::check_update_authority(const DirectoryRecord& rec, const AuthorityEnvelope& env, std::string& why) const {
  if (env.epoch != epoch_) { why = "stale epoch"; return DirectoryError::STALE_EPOCH; }
  if (rec.authority.boot != env.boot) { why = "not the owning incarnation"; return DirectoryError::STALE_WORKER_BOOT; }
  if (env.fence < rec.authority.fence) { why = "stale authority fence"; return DirectoryError::STALE_AUTHORITY; }
  return DirectoryError::NONE;
}

// ---- controlled updates ----

Result<DirectoryRecord> Directory::update_health(DirectoryRecordId id, Health h, const AuthorityEnvelope& env) {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_IDENTITY, "no such record");
  std::string why;
  DirectoryError e = check_update_authority(records_[i], env, why);
  if (e != DirectoryError::NONE) return Result<DirectoryRecord>::failure(e, why);
  auto& r = records_[i];
  auto from = r.lifecycle;
  r.health = h;
  auto to = derive_lifecycle(r);
  if (!can_lifecycle(from, to)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_STATE_TRANSITION, "illegal lifecycle transition");
  r.lifecycle = to;
  r.authority.fence = env.fence;
  r.updated_ns = wall_clock_ns();
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_[i]);
}

Result<DirectoryRecord> Directory::update_freshness(DirectoryRecordId id, Freshness f, const AuthorityEnvelope& env) {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_IDENTITY, "no such record");
  std::string why;
  DirectoryError e = check_update_authority(records_[i], env, why);
  if (e != DirectoryError::NONE) return Result<DirectoryRecord>::failure(e, why);
  auto& r = records_[i];
  auto from = r.lifecycle;
  r.freshness = f;
  auto to = derive_lifecycle(r);
  if (!can_lifecycle(from, to)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_STATE_TRANSITION, "illegal lifecycle transition");
  r.lifecycle = to;
  r.authority.fence = env.fence;
  r.updated_ns = wall_clock_ns();
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_[i]);
}

Result<DirectoryRecord> Directory::update_reachability(DirectoryRecordId id, Reachability reach, const AuthorityEnvelope& env) {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_IDENTITY, "no such record");
  std::string why;
  DirectoryError e = check_update_authority(records_[i], env, why);
  if (e != DirectoryError::NONE) return Result<DirectoryRecord>::failure(e, why);
  auto& r = records_[i];
  auto from = r.lifecycle;
  r.reachability = reach;
  auto to = derive_lifecycle(r);
  if (!can_lifecycle(from, to)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_STATE_TRANSITION, "illegal lifecycle transition");
  r.lifecycle = to;
  r.authority.fence = env.fence;
  r.updated_ns = wall_clock_ns();
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_[i]);
}

Result<DirectoryRecord> Directory::update_integrity(DirectoryRecordId id, Integrity integ, const AuthorityEnvelope& env) {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_IDENTITY, "no such record");
  std::string why;
  DirectoryError e = check_update_authority(records_[i], env, why);
  if (e != DirectoryError::NONE) return Result<DirectoryRecord>::failure(e, why);
  auto& r = records_[i];
  auto from = r.lifecycle;
  r.integrity = integ;
  auto to = derive_lifecycle(r);
  if (!can_lifecycle(from, to)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_STATE_TRANSITION, "illegal lifecycle transition");
  r.lifecycle = to;
  r.authority.fence = env.fence;
  r.updated_ns = wall_clock_ns();
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_[i]);
}

Result<DirectoryRecord> Directory::update_lifecycle(DirectoryRecordId id, LifecycleState s, const AuthorityEnvelope& env) {
  std::size_t i = find_record_index(id);
  if (i == static_cast<std::size_t>(-1)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_IDENTITY, "no such record");
  std::string why;
  DirectoryError e = check_update_authority(records_[i], env, why);
  if (e != DirectoryError::NONE) return Result<DirectoryRecord>::failure(e, why);
  auto& r = records_[i];
  if (!can_lifecycle(r.lifecycle, s)) return Result<DirectoryRecord>::failure(DirectoryError::INVALID_STATE_TRANSITION, "illegal lifecycle transition");
  r.lifecycle = s;
  r.authority.fence = env.fence;
  r.updated_ns = wall_clock_ns();
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<DirectoryRecord>::success(records_[i]);
}

Result<CacheDescriptor> Directory::update_cache_health(CacheId id, Health h, Reachability reach, const AuthorityEnvelope& env) {
  auto it = caches_.find(id);
  if (it == caches_.end()) return Result<CacheDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "no such cache");
  if (env.epoch != epoch_) return Result<CacheDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (it->second.worker_boot != env.boot) return Result<CacheDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "cache owned by another incarnation");
  if (env.fence < it->second.authority.fence) return Result<CacheDescriptor>::failure(DirectoryError::STALE_AUTHORITY, "stale authority");
  it->second.health = h;
  it->second.reachability = reach;
  it->second.authority.fence = env.fence;
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<CacheDescriptor>::success(it->second);
}

Result<CacheDescriptor> Directory::update_cache_capacity(CacheId id, std::uint64_t capacity, std::uint64_t free_capacity, const AuthorityEnvelope& env) {
  auto it = caches_.find(id);
  if (it == caches_.end()) return Result<CacheDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "no such cache");
  if (env.epoch != epoch_) return Result<CacheDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (it->second.worker_boot != env.boot) return Result<CacheDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "cache owned by another incarnation");
  if (env.fence < it->second.authority.fence) return Result<CacheDescriptor>::failure(DirectoryError::STALE_AUTHORITY, "stale authority");
  it->second.capacity = capacity; it->second.capacity_known = true;
  it->second.free_capacity = free_capacity; it->second.free_capacity_known = true;
  it->second.authority.fence = env.fence;
  ++accounting_.updates;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<CacheDescriptor>::success(it->second);
}

// ---- leases ----

Result<LeaseDescriptor> Directory::register_lease(const LeaseDescriptor& lease) {
  if (lease.lease_id.is_null()) return Result<LeaseDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty LeaseId");
  if (lease.holder.is_null()) return Result<LeaseDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "empty holder");
  if (lease.authority.epoch != epoch_) return Result<LeaseDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (!worker_live(lease.holder)) return Result<LeaseDescriptor>::failure(DirectoryError::NO_AUTHORITY, "holder not live");
  auto it = leases_.find(lease.lease_id);
  if (it != leases_.end() && it->second.holder != lease.holder)
    return Result<LeaseDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "lease held by another incarnation");
  leases_[lease.lease_id] = lease;
  ++accounting_.registrations;
  indexes_dirty_ = true;
  recompute_accounting();
  return Result<LeaseDescriptor>::success(lease);
}

Result<LeaseDescriptor> Directory::renew_lease(LeaseId id, const AuthorityEnvelope& env) {
  auto it = leases_.find(id);
  if (it == leases_.end()) return Result<LeaseDescriptor>::failure(DirectoryError::INVALID_IDENTITY, "no such lease");
  if (env.epoch != epoch_) return Result<LeaseDescriptor>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (it->second.holder != env.boot) return Result<LeaseDescriptor>::failure(DirectoryError::STALE_WORKER_BOOT, "stale lease renewal from non-holder");
  if (env.fence <= it->second.authority.fence) return Result<LeaseDescriptor>::failure(DirectoryError::STALE_AUTHORITY, "stale lease authority");
  if (it->second.state == LeaseState::REVOKED) return Result<LeaseDescriptor>::failure(DirectoryError::FAILED_LEASE_VALIDATION, "lease revoked");
  auto& l = it->second;
  l.start_ns = wall_clock_ns();
  if (l.expires) l.expiry_ns = l.start_ns + (l.expiry_ns - l.start_ns);
  l.state = LeaseState::ACTIVE;
  l.generation = LeaseGeneration(l.generation.value() + 1);
  l.authority.fence = env.fence;
  l.authority.boot = env.boot;
  ++accounting_.updates;
  recompute_accounting();
  return Result<LeaseDescriptor>::success(l);
}

Result<AckResult> Directory::expire_lease(LeaseId id, const AuthorityEnvelope& env, const std::string& reason) {
  auto it = leases_.find(id);
  if (it == leases_.end()) return Result<AckResult>::failure(DirectoryError::INVALID_IDENTITY, "no such lease");
  if (it->second.holder != env.boot) return Result<AckResult>::failure(DirectoryError::STALE_WORKER_BOOT, "stale lease expiry");
  it->second.state = LeaseState::EXPIRED;
  it->second.authority.fence = env.fence;
  ++accounting_.updates;
  recompute_accounting();
  AckResult a; a.ok = true; a.message = "lease expired: " + reason;
  return Result<AckResult>::success(a);
}

Result<AckResult> Directory::revoke_lease(LeaseId id, const AuthorityEnvelope& env, const std::string& reason) {
  (void)env;
  auto it = leases_.find(id);
  if (it == leases_.end()) return Result<AckResult>::failure(DirectoryError::INVALID_IDENTITY, "no such lease");
  it->second.state = LeaseState::REVOKED;
  ++accounting_.updates;
  recompute_accounting();
  AckResult a; a.ok = true; a.message = "lease revoked: " + reason;
  return Result<AckResult>::success(a);
}

// ---- invalidation / supersession / tombstone ----

Result<InvalidateResult> Directory::invalidate_state(StateId s, StateGeneration g, const AuthorityEnvelope& env, const std::string& reason) {
  if (env.epoch != epoch_) return Result<InvalidateResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  InvalidateResult out;
  for (auto& r : records_) {
    if (r.state_id == s && r.state_generation == g && r.current) {
      if (r.authority.boot != env.boot && !worker_live(env.boot)) continue;
      r.current = false;
      r.lifecycle = LifecycleState::INVALIDATED;
      r.updated_ns = wall_clock_ns();
      ++out.affected_records;
    }
  }
  ++accounting_.invalidations;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true;
  out.message = "invalidated " + std::to_string(out.affected_records) + " record(s): " + reason;
  return Result<InvalidateResult>::success(out);
}
Result<InvalidateResult> Directory::invalidate_location(LocationId loc, const AuthorityEnvelope& env, const std::string& reason) {
  if (env.epoch != epoch_) return Result<InvalidateResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  InvalidateResult out;
  for (auto& r : records_) {
    if (r.location == loc && r.current) { r.current = false; r.lifecycle = LifecycleState::INVALIDATED; r.reachability = Reachability::UNREACHABLE; r.updated_ns = wall_clock_ns(); ++out.affected_records; }
  }
  auto lit = locations_.find(loc);
  if (lit != locations_.end()) { lit->second.reachability = Reachability::UNREACHABLE; lit->second.freshness = Freshness::STALE; }
  ++accounting_.invalidations;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true; out.message = "invalidated location: " + reason;
  return Result<InvalidateResult>::success(out);
}

Result<InvalidateResult> Directory::invalidate_replica(ReplicaId rep, const AuthorityEnvelope& env, const std::string& reason) {
  if (env.epoch != epoch_) return Result<InvalidateResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  InvalidateResult out;
  for (auto& r : records_) {
    if (r.replica == rep && r.current) { r.current = false; r.lifecycle = LifecycleState::INVALIDATED; r.updated_ns = wall_clock_ns(); ++out.affected_records; }
  }
  ++accounting_.invalidations;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true; out.message = "invalidated replica: " + reason;
  return Result<InvalidateResult>::success(out);
}

Result<InvalidateResult> Directory::invalidate_cache(CacheId c, const AuthorityEnvelope& env, const std::string& reason) {
  if (env.epoch != epoch_) return Result<InvalidateResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  InvalidateResult out;
  for (auto& r : records_) {
    if (r.cache == c && r.current) { r.current = false; r.lifecycle = LifecycleState::INVALIDATED; r.updated_ns = wall_clock_ns(); ++out.affected_records; }
  }
  auto cit = caches_.find(c);
  if (cit != caches_.end()) { cit->second.reachability = Reachability::UNREACHABLE; cit->second.health = Health::UNAVAILABLE; }
  ++accounting_.invalidations;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true; out.message = "invalidated cache: " + reason;
  return Result<InvalidateResult>::success(out);
}

Result<InvalidateResult> Directory::invalidate_worker_boot(WorkerBootId b, const AuthorityEnvelope& env, const std::string& reason) {
  if (env.epoch != epoch_) return Result<InvalidateResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  InvalidateResult out;
  for (auto& r : records_) {
    if (r.worker_boot == b && r.current) { r.current = false; r.lifecycle = LifecycleState::INVALIDATED; r.reachability = Reachability::UNREACHABLE; r.updated_ns = wall_clock_ns(); ++out.affected_records; }
  }
  ++accounting_.invalidations;
  indexes_dirty_ = true;
  recompute_accounting();
  out.ok = true; out.message = "invalidated worker boot: " + reason;
  return Result<InvalidateResult>::success(out);
}

// ---- tombstones ----

bool Directory::covered_by_tombstone(const TombstoneTarget& target) const {
  for (const auto& t : tombstones_) {
    if (t.target.kind != target.kind) continue;
    const auto& tk = t.target;
    switch (tk.kind) {
      case TombstoneKind::STATE:
        if (tk.state == target.state && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::CACHE:
        if (tk.cache == target.cache && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::ENTRY:
        if (tk.entry == target.entry && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::REPLICA:
        if (tk.replica == target.replica && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::LOCATION:
        if (tk.location == target.location && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::WORKER_BOOT:
        if (tk.worker_boot == target.worker_boot) return true; break;
      case TombstoneKind::NODE:
        if (tk.node == target.node) return true; break;
      case TombstoneKind::DEVICE:
        if (tk.device == target.device) return true; break;
      case TombstoneKind::BACKEND:
        if (tk.backend == target.backend) return true; break;
      case TombstoneKind::COMPATIBILITY:
        if (tk.compatibility == target.compatibility && target.generation_floor <= tk.generation_floor) return true; break;
      case TombstoneKind::POLICY:
        if (tk.policy_generation == target.policy_generation) return true; break;
      case TombstoneKind::CONTENT:
        if (digest::digest_equal(tk.content, target.content)) return true; break;
      case TombstoneKind::GENERIC:
        return true;
    }
  }
  return false;
}

Result<AckResult> Directory::tombstone(const TombstoneRecord& tombstone) {
  if (tombstone.tombstone_id.is_null()) return Result<AckResult>::failure(DirectoryError::INVALID_IDENTITY, "empty TombstoneId");
  if (tombstone.epoch != epoch_) return Result<AckResult>::failure(DirectoryError::STALE_EPOCH, "stale epoch");
  if (!worker_live(tombstone.worker_boot)) return Result<AckResult>::failure(DirectoryError::NO_AUTHORITY, "tombstone author not live");
  // A stale tombstone cannot invalidate a fresh generation: if a newer state
  // generation already exists, a tombstone targeting an older floor is allowed
  // only for that old generation and must not fence a newer current one.
  if (tombstone.target.kind == TombstoneKind::STATE) {
    auto cg = current_state_gen_.find(tombstone.target.state);
    if (cg != current_state_gen_.end() && cg->second.value() > tombstone.target.generation_floor) {
      // Tombstoning an already-superseded generation is idempotent and harmless.
    }
  }
  tombstones_.push_back(tombstone);
  // Mark covered current records as TOMBSTONED and non-current if still current.
  for (auto& r : records_) {
    if (tombstone.target.kind == TombstoneKind::STATE && r.state_id == tombstone.target.state &&
        r.state_generation.value() <= tombstone.target.generation_floor) {
      r.lifecycle = LifecycleState::TOMBSTONED;
      r.current = false;
    }
  }
  ++accounting_.tombstones;
  indexes_dirty_ = true;
  recompute_accounting();
  AckResult a; a.ok = true; a.message = "tombstone " + tombstone.tombstone_id.str() + " recorded";
  return Result<AckResult>::success(a);
}

// ---- ranking ----

std::vector<RankingFactor> Directory::score_factors(const DirectoryRecord& r, const DirectoryQuery& q, double& total) const {
  std::vector<RankingFactor> fs;
  const RankingPolicy& p = policy_;
  total = 0.0;
  auto add = [&](RankingFactorKind k, double w, double v, std::string note) {
    fs.push_back(RankingFactor{k, w, v, w * v, std::move(note)});
    total += w * v;
  };
  double eg = 1.0;
  if (q.exact_state_generation) eg = (r.state_generation == q.state_generation) ? 1.0 : 0.0;
  else {
    auto cgit = current_state_gen_.find(r.state_id);
    if (cgit != current_state_gen_.end()) eg = (r.state_generation == cgit->second) ? 1.0 : 0.0;
  }
  add(RankingFactorKind::EXACT_GENERATION, p.exact_generation_w, eg, "state generation matches current");

  std::string process_tag;
  auto lit = locations_.find(r.location);
  if (lit != locations_.end()) process_tag = lit->second.process_tag;
  double sp = 0.0;
  if (!q.requester_process_tag.empty() && process_tag == q.requester_process_tag) sp = 1.0;
  add(RankingFactorKind::SAME_PROCESS, p.same_process_w, sp, "process-local");

  double sd = 0.0;
  if (q.has_preferred_device && r.device == q.preferred_device && !r.device.is_null()) sd = 1.0;
  add(RankingFactorKind::SAME_DEVICE, p.same_device_w, sd, "same device");

  double sn = 0.0;
  if (!q.requester_node.is_null() && r.node == q.requester_node) sn = 1.0;
  add(RankingFactorKind::SAME_NODE, p.same_node_w, sn, "same node");

  add(RankingFactorKind::SAME_NUMA, p.same_numa_w, (r.estimate.locality == LocalityClass::SAME_NUMA) ? 1.0 : 0.0, "numa locality");
  add(RankingFactorKind::DOMAIN_PREFERENCE, p.domain_preference_w,
      (std::find(q.preferred_domains.begin(), q.preferred_domains.end(), r.domain) != q.preferred_domains.end()) ? 1.0 : 0.0, "preferred domain");
  add(RankingFactorKind::REACHABILITY, p.reachability_w, nr(r.reachability), "reachability");
  add(RankingFactorKind::HEALTH, p.health_w, nh(r.health), "health");
  add(RankingFactorKind::FRESHNESS, p.freshness_w, nf(r.freshness), "freshness");
  add(RankingFactorKind::INTEGRITY, p.integrity_w, ni(r.integrity), "integrity");

  double lat = r.estimate.estimated_latency_ns;
  double lv = lat <= 0.0 ? 0.0 : std::max(0.0, 1.0 - std::min(lat / 1e7, 1.0));
  add(RankingFactorKind::LATENCY, p.latency_w, lv, "estimated latency");
  double bw = r.estimate.estimated_bandwidth_bytes_per_s;
  add(RankingFactorKind::BANDWIDTH, p.bandwidth_w, bw <= 0.0 ? 0.0 : std::min(bw / 1e10, 1.0), "bandwidth");
  double tb = static_cast<double>(r.estimate.transfer_bytes);
  add(RankingFactorKind::TRANSFER_BYTES, p.transfer_bytes_w, tb == 0.0 ? 1.0 : std::max(0.0, 1.0 - std::min(tb / 1e7, 1.0)), "transfer bytes");
  add(RankingFactorKind::STAGING, p.staging_w, std::max(0.0, 1.0 - std::min(r.estimate.staging_steps / 5.0, 1.0)), "staging steps");
  add(RankingFactorKind::RESTORE, p.restore_w, r.estimate.restore_required ? 0.0 : 1.0, "restore requirement");

  std::uint32_t rc = 0;
  for (const auto& rec : records_) if (rec.current && rec.state_id == r.state_id && rec.state_generation == r.state_generation) ++rc;
  add(RankingFactorKind::REPLICA_DIVERSITY, p.replica_diversity_w, rc > 1 ? 1.0 : 0.5, "replica diversity");
  add(RankingFactorKind::POLICY_PREFERENCE, p.policy_preference_w, 0.5, "policy preference");
  return fs;
}

QueryResult Directory::query_impl(const DirectoryQuery& q, std::vector<std::size_t> idx) const {
  QueryResult res;
  res.query_id = q.query_id;
  std::sort(idx.begin(), idx.end());
  idx.erase(std::unique(idx.begin(), idx.end()), idx.end());

  int base_match = 0;
  std::vector<std::size_t> cand_idx;
  for (std::size_t i : idx) {
    const auto& rec = records_[i];
    if (!identify_matches(rec, q)) continue;
    if (q.current_only && !rec.current) continue;
    if (!rec.current) continue;
    ++base_match;
    RejectionReason reason;
    if (hard_reject(rec, q, reason)) {
      res.rejections.push_back(Rejection{rec.record_id, reason, rec.state_id, rec.state_generation, rec.replica, rec.location});
      continue;
    }
    cand_idx.push_back(i);
  }
  for (std::size_t i : cand_idx) {
    Candidate c;
    c.record = records_[i];
    double total = 0.0;
    c.factors = score_factors(c.record, q, total);
    c.score = total;
    res.candidates.push_back(std::move(c));
  }
  std::stable_sort(res.candidates.begin(), res.candidates.end(), candidate_less);
  if (!res.candidates.empty()) { res.candidates[0].selected = true; res.selected_index = 0; }
  res.matched_scan_count = static_cast<std::uint32_t>(res.candidates.size());

  if (!res.candidates.empty()) {
    if (res.candidates.size() > 1) res.outcome = QueryOutcome::FOUND_MULTIPLE;
    else {
      const auto& c = res.candidates[0];
      std::string process_tag;
      auto lit = locations_.find(c.record.location);
      if (lit != locations_.end()) process_tag = lit->second.process_tag;
      bool local = (c.record.estimate.locality == LocalityClass::SAME_PROCESS) ||
                   (!q.requester_process_tag.empty() && process_tag == q.requester_process_tag) ||
                   (q.has_preferred_device && !c.record.device.is_null() && c.record.device == q.preferred_device);
      res.outcome = local ? QueryOutcome::FOUND_LOCAL : QueryOutcome::FOUND_REMOTE;
    }
  } else {
    bool corrupt = false, unreachable = false, stale = false, incompatible = false, evid = false;
    for (const auto& rj : res.rejections) {
      switch (rj.reason) {
        case RejectionReason::CORRUPT:
        case RejectionReason::MISSING_BYTES: corrupt = true; break;
        case RejectionReason::UNREACHABLE:
        case RejectionReason::REACHABILITY_BELOW_REQUIREMENT: unreachable = true; break;
        case RejectionReason::STALE_RECORD:
        case RejectionReason::FRESHNESS_BELOW_REQUIREMENT: stale = true; break;
        case RejectionReason::INCOMPATIBLE: incompatible = true; break;
        case RejectionReason::MISSING_REQUIRED_EVIDENCE: evid = true; break;
        default: break;
      }
    }
    if (corrupt) res.outcome = QueryOutcome::CORRUPT_ONLY;
    else if (unreachable) res.outcome = QueryOutcome::UNREACHABLE_ONLY;
    else if (stale) res.outcome = QueryOutcome::STALE_ONLY;
    else if (incompatible) res.outcome = QueryOutcome::INCOMPATIBLE_ONLY;
    else if (evid || base_match > 0) res.outcome = QueryOutcome::INSUFFICIENT_EVIDENCE;
    else res.outcome = QueryOutcome::NOT_FOUND;
  }
  return res;
}

Result<QueryResult> Directory::query(const DirectoryQuery& q) {
  ensure_indexes();
  std::vector<std::size_t> idx;
  if (q.exact_state) {
    auto range = indexes_.by_state.equal_range(q.state);
    for (auto it = range.first; it != range.second; ++it) idx.push_back(it->second);
  } else {
    idx.resize(records_.size());
    for (std::size_t i = 0; i < records_.size(); ++i) idx[i] = i;
  }
  QueryResult res = query_impl(q, idx);
  ++accounting_.queries;
  if (!res.candidates.empty()) {
    const auto& c = res.candidates[0];
    std::string process_tag;
    auto lit = locations_.find(c.record.location);
    if (lit != locations_.end()) process_tag = lit->second.process_tag;
    bool local = (c.record.estimate.locality == LocalityClass::SAME_PROCESS) ||
                 (!q.requester_process_tag.empty() && process_tag == q.requester_process_tag) ||
                 (q.has_preferred_device && !c.record.device.is_null() && c.record.device == q.preferred_device);
    if (local) ++accounting_.local_hits; else ++accounting_.remote_hits;
  } else if (res.outcome == QueryOutcome::NOT_FOUND || res.outcome == QueryOutcome::INSUFFICIENT_EVIDENCE) {
    ++accounting_.misses;
  } else if (res.outcome == QueryOutcome::STALE_ONLY) ++accounting_.stale_only_outcomes;
  else if (res.outcome == QueryOutcome::UNREACHABLE_ONLY) ++accounting_.unreachable_only_outcomes;
  res.explanation = explain_query(q, res);
  return Result<QueryResult>::success(res);
}

QueryResult Directory::query_scan(const DirectoryQuery& q) {
  std::vector<std::size_t> idx;
  idx.resize(records_.size());
  for (std::size_t i = 0; i < records_.size(); ++i) idx[i] = i;
  QueryResult res = query_impl(q, idx);
  res.explanation = explain_query(q, res);
  return res;
}

// ---- indexes ----

void Directory::build_indexes(DirectoryIndexes& out) const {
  for (std::size_t i = 0; i < records_.size(); ++i) {
    const auto& r = records_[i];
    out.by_state.emplace(r.state_id, i);
    out.by_state_generation.emplace(r.state_generation, i);
    out.by_cache.emplace(r.cache, i);
    out.by_entry.emplace(r.entry, i);
    out.by_replica.emplace(r.replica, i);
    out.by_location.emplace(r.location, i);
    out.by_node.emplace(r.node, i);
    out.by_worker.emplace(r.worker, i);
    out.by_boot.emplace(r.worker_boot, i);
    out.by_device.emplace(r.device, i);
    out.by_domain.emplace(static_cast<std::uint8_t>(r.domain), i);
    if (r.content_digest_known) out.by_digest.emplace(digest::hex(r.content_digest), i);
    out.by_freshness.emplace(static_cast<std::uint8_t>(r.freshness), i);
    out.by_health.emplace(static_cast<std::uint8_t>(r.health), i);
    out.by_integrity.emplace(static_cast<std::uint8_t>(r.integrity), i);
    out.by_reachability.emplace(static_cast<std::uint8_t>(r.reachability), i);
    out.by_lifecycle.emplace(static_cast<std::uint8_t>(r.lifecycle), i);
    out.by_compatibility.emplace(r.compatibility, i);
  }
}

void Directory::ensure_indexes() const {
  if (!indexes_dirty_) return;
  DirectoryIndexes fresh;
  build_indexes(fresh);
  indexes_ = std::move(fresh);
  indexes_dirty_ = false;
}

Result<AckResult> Directory::validate_indexes() const {
  ensure_indexes();
  DirectoryIndexes fresh;
  build_indexes(fresh);
  AckResult a;
  auto chk = [&](bool cond, const char* which) -> bool {
    if (!cond) { a.ok = false; a.message = std::string("index disagrees: ") + which; return false; } return true;
  };
  if (!chk(indexes_.by_state.size() == fresh.by_state.size(), "by_state")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_state_generation.size() == fresh.by_state_generation.size(), "by_state_generation")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_cache.size() == fresh.by_cache.size(), "by_cache")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_entry.size() == fresh.by_entry.size(), "by_entry")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_replica.size() == fresh.by_replica.size(), "by_replica")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_location.size() == fresh.by_location.size(), "by_location")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_node.size() == fresh.by_node.size(), "by_node")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_worker.size() == fresh.by_worker.size(), "by_worker")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_boot.size() == fresh.by_boot.size(), "by_boot")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_device.size() == fresh.by_device.size(), "by_device")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_domain.size() == fresh.by_domain.size(), "by_domain")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_digest.size() == fresh.by_digest.size(), "by_digest")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_freshness.size() == fresh.by_freshness.size(), "by_freshness")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_health.size() == fresh.by_health.size(), "by_health")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_integrity.size() == fresh.by_integrity.size(), "by_integrity")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_reachability.size() == fresh.by_reachability.size(), "by_reachability")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_lifecycle.size() == fresh.by_lifecycle.size(), "by_lifecycle")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  if (!chk(indexes_.by_compatibility.size() == fresh.by_compatibility.size(), "by_compatibility")) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, a.message);
  // Verify every canonical record is reachable from its by_state index.
  for (std::size_t i = 0; i < records_.size(); ++i) {
    bool found = false;
    auto range = indexes_.by_state.equal_range(records_[i].state_id);
    for (auto it = range.first; it != range.second; ++it) if (it->second == i) { found = true; break; }
    if (!found) return Result<AckResult>::failure(DirectoryError::INDEX_INCONSISTENT, "record not present in by_state index");
  }
  std::string acc_problem;
  if (!accounting_.validate(acc_problem)) return Result<AckResult>::failure(DirectoryError::ACCOUNTING_NEGATIVE, acc_problem);
  a.ok = true; a.message = "all indexes agree with canonical records; accounting valid";
  return Result<AckResult>::success(a);
}

// ---- accounting ----

void Directory::recompute_accounting() {
  Accounting a;
  a.caches = static_cast<std::int64_t>(caches_.size());
  a.locations = static_cast<std::int64_t>(locations_.size());
  a.replicas = static_cast<std::int64_t>(replicas_.size());
  for (const auto& kv : locations_) {
    if (kv.second.domain == MemoryDomain::CUDA_DEVICE) ++a.cuda_locations;
    if (kv.second.domain == MemoryDomain::HOST_MEMORY || kv.second.domain == MemoryDomain::HOST_PINNED) ++a.host_locations;
    if (kv.second.domain == MemoryDomain::LOCAL_FILESYSTEM || kv.second.domain == MemoryDomain::LOCAL_NVME_CLASS ||
        kv.second.domain == MemoryDomain::SHARED_FILESYSTEM_CLASS || kv.second.domain == MemoryDomain::OBJECT_STORAGE_CLASS)
      ++a.storage_locations;
  }
  std::int64_t cur = 0, hist = 0, reach = 0, stale = 0, degraded = 0, corrupt = 0;
  std::unordered_set<StateId> states;
  for (const auto& r : records_) {
    if (r.current) ++cur; else ++hist;
    states.insert(r.state_id);
    if (r.reachability == Reachability::REACHABLE || r.reachability == Reachability::DEGRADED) ++reach;
    if (r.freshness == Freshness::STALE) ++stale;
    if (r.health == Health::DEGRADED) ++degraded;
    if (r.integrity == Integrity::CORRUPT) ++corrupt;
  }
  a.current_records = cur;
  a.historical_records = hist;
  a.logical_states = static_cast<std::int64_t>(states.size());
  a.reachable_records = reach;
  a.stale_records = stale;
  a.degraded_records = degraded;
  a.corrupt_records = corrupt;
  std::int64_t active = 0, expi = 0;
  for (const auto& kv : leases_) { if (kv.second.state == LeaseState::ACTIVE) ++active; if (kv.second.state == LeaseState::EXPIRED) ++expi; }
  a.active_leases = active;
  a.expired_leases = expi;
  a.tombstones = static_cast<std::int64_t>(tombstones_.size());
  // Preserve monotonic event counters.
  a.registrations = accounting_.registrations;
  a.updates = accounting_.updates;
  a.invalidations = accounting_.invalidations;
  a.queries = accounting_.queries;
  a.local_hits = accounting_.local_hits;
  a.remote_hits = accounting_.remote_hits;
  a.misses = accounting_.misses;
  a.stale_only_outcomes = accounting_.stale_only_outcomes;
  a.unreachable_only_outcomes = accounting_.unreachable_only_outcomes;
  a.integrity_failures = accounting_.integrity_failures;
  a.stale_mutation_rejections = accounting_.stale_mutation_rejections;
  a.duplicate_conflict_rejections = accounting_.duplicate_conflict_rejections;
  a.worker_restarts = accounting_.worker_restarts;
  accounting_ = a;
}

// ---- explanation ----

std::vector<ExplanationEntry> Directory::explain_query(const DirectoryQuery& q, const QueryResult& r) const {
  std::vector<ExplanationEntry> e;
  if (r.candidates.empty()) {
    std::ostringstream o;
    o << "Query " << q.query_id.str() << " returned " << to_string(r.outcome) << ": no eligible current candidate.";
    e.push_back({"query", o.str()});
  } else {
    std::ostringstream o;
    o << "Query " << q.query_id.str() << " returned " << to_string(r.outcome) << " with " << r.candidates.size() << " candidate(s).";
    e.push_back({"query", o.str()});
    for (std::size_t i = 0; i < r.candidates.size(); ++i) {
      o.str(""); o.clear();
      o << "Candidate " << i << ": ReplicaId " << r.candidates[i].record.replica.str()
        << " StateGeneration " << r.candidates[i].record.state_generation.str()
        << " domain " << to_string(r.candidates[i].record.domain)
        << " integrity " << to_string(r.candidates[i].record.integrity)
        << " freshness " << to_string(r.candidates[i].record.freshness)
        << " reachability " << to_string(r.candidates[i].record.reachability)
        << " score " << r.candidates[i].score;
      e.push_back({"candidate", o.str()});
    }
  }
  for (const auto& rej : r.rejections) {
    auto ex = explain_rejection(rej);
    for (auto& x : ex) e.push_back(std::move(x));
  }
  return e;
}

std::vector<ExplanationEntry> Directory::explain_candidate(const Candidate& c) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "ReplicaId " << c.record.replica.str() << " on NodeId " << c.record.node.str()
    << " selected with score " << c.score << " because it is "
    << to_string(c.record.integrity) << ", " << to_string(c.record.freshness) << ", "
    << to_string(c.record.reachability) << " at domain " << to_string(c.record.domain) << ".";
  e.push_back({"candidate", o.str()});
  for (const auto& f : c.factors) {
    o.str(""); o.clear();
    o << to_string(f.kind) << " weight " << f.weight << " value " << f.value << " contribution " << f.contribution << " (" << f.note << ")";
    e.push_back({"factor", o.str()});
  }
  return e;
}

std::vector<ExplanationEntry> Directory::explain_rejection(const Rejection& rej) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "ReplicaId " << rej.replica.str() << " rejected because " << to_string(rej.reason) << ".";
  e.push_back({"rejection", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_location(const LocationDescriptor& l) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "Location " << l.location_id.str() << " domain " << to_string(l.domain) << " reachability "
    << to_string(l.reachability) << " freshness " << to_string(l.freshness) << " integrity "
    << to_string(l.integrity) << " (provenance " << to_string(l.estimate.provenance) << ").";
  e.push_back({"location", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_replica(const ReplicaDescriptor& r) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "Replica " << r.replica_id.str() << " at location " << r.location.str() << " health "
    << to_string(r.health) << " integrity " << to_string(r.integrity) << " reachability "
    << to_string(r.reachability) << " freshness " << to_string(r.freshness) << ".";
  e.push_back({"replica", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_reachability(const Reachability r) const {
  std::vector<ExplanationEntry> e;
  e.push_back({"reachability", std::string("Reachability ") + to_string(r) + " means the location evidence " +
      (r == Reachability::REACHABLE ? "supports reachability" :
       r == Reachability::UNREACHABLE ? "does not support reachability" :
       r == Reachability::REVALIDATION_REQUIRED ? "requires revalidation before use" : "is unknown") + "."});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_freshness(const Freshness f) const {
  std::vector<ExplanationEntry> e;
  e.push_back({"freshness", std::string("Freshness ") + to_string(f) + " means the copy is " +
      (f == Freshness::CURRENT ? "current" : f == Freshness::STALE ? "stale" : f == Freshness::REVALIDATION_REQUIRED ? "pending revalidation" : "of unknown freshness") + "."});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_lease(const LeaseDescriptor& l) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "Lease " << l.lease_id.str() << " generation " << l.generation.str() << " state "
    << to_string(l.state) << " held by WorkerBootId " << l.holder.str() << ".";
  e.push_back({"lease", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_invalidation(const InvalidateResult& inv) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "Invalidation affected " << inv.affected_records << " record(s): " << inv.message;
  e.push_back({"invalidation", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_tombstone(const TombstoneRecord& t) const {
  std::vector<ExplanationEntry> e;
  std::ostringstream o;
  o << "Tombstone " << t.tombstone_id.str() << " covers " << to_string(t.target.kind)
    << " at generation floor " << t.target.generation_floor << " issued under epoch "
    << t.epoch.str() << " by WorkerBootId " << t.worker_boot.str() << ": " << t.reason;
  e.push_back({"tombstone", o.str()});
  return e;
}

std::vector<ExplanationEntry> Directory::explain_recovery() const {
  std::vector<ExplanationEntry> e;
  e.push_back({"recovery", "On coordinator recovery, all live WorkerBootId authority is cleared, process-local and CUDA_DEVICE locations become REVALIDATION_REQUIRED, active leases become revalidation-required/expired, remote reachability observations become stale where required, and current tombstones remain authoritative."});
  return e;
}

}  // namespace distributedcachedirectory


