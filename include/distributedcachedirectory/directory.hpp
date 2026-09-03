#pragma once
#include "accounting.hpp"
#include "authority.hpp"
#include "errors.hpp"
#include "model.hpp"
#include "query.hpp"
#include "strong_type.hpp"
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace distributedcachedirectory {

// A worker "session" tracked by the directory. Authority for the records a
// worker asserts is scoped to the worker's WorkerBootId (one incarnation).
// Live-ness is tracked separately from identity; a stale incarnation is never a
// live source even if its identity appears again after a restart.
struct WorkerSession {
  WorkerBootId boot;
  WorkerId worker{0};
  NodeId node;
  std::string tag;
  bool live{false};
  std::uint64_t source_authority{0};  // per-boot authority generation
};

// Structural indexes over canonical records. Indexes are rebuilt
// deterministically from the canonical record vector; invariant validation
// checks that each index agrees with the canonical records.
struct DirectoryIndexes {
  std::unordered_multimap<StateId, std::size_t> by_state;
  std::unordered_multimap<StateGeneration, std::size_t> by_state_generation;
  std::unordered_multimap<CacheId, std::size_t> by_cache;
  std::unordered_multimap<CacheEntryId, std::size_t> by_entry;
  std::unordered_multimap<ReplicaId, std::size_t> by_replica;
  std::unordered_multimap<LocationId, std::size_t> by_location;
  std::unordered_multimap<NodeId, std::size_t> by_node;
  std::unordered_multimap<WorkerId, std::size_t> by_worker;
  std::unordered_multimap<WorkerBootId, std::size_t> by_boot;
  std::unordered_multimap<DeviceId, std::size_t> by_device;
  std::unordered_multimap<std::uint8_t, std::size_t> by_domain;        // MemoryDomain
  std::unordered_multimap<std::string, std::size_t> by_digest;         // hex content digest
  std::unordered_multimap<std::uint8_t, std::size_t> by_freshness;
  std::unordered_multimap<std::uint8_t, std::size_t> by_health;
  std::unordered_multimap<std::uint8_t, std::size_t> by_integrity;
  std::unordered_multimap<std::uint8_t, std::size_t> by_reachability;
  std::unordered_multimap<std::uint8_t, std::size_t> by_lifecycle;
  std::unordered_multimap<CompatibilityId, std::size_t> by_compatibility;
};

// Access-policy override for deterministic ranking. All factors have a fixed
// default weight; a caller may adjust weights to express a local preference.
struct RankingPolicy {
  double exact_generation_w{4.0};
  double same_process_w{3.0};
  double same_device_w{2.0};
  double same_node_w{1.0};
  double same_numa_w{0.5};
  double domain_preference_w{1.0};
  double reachability_w{3.0};
  double health_w{3.0};
  double freshness_w{4.0};
  double integrity_w{4.0};
  double latency_w{1.0};
  double bandwidth_w{1.0};
  double transfer_bytes_w{0.5};
  double staging_w{0.5};
  double restore_w{0.5};
  double replica_diversity_w{0.5};
  double policy_preference_w{0.5};
};

class Directory {
 public:
  Directory();

  // ---- epoch / generation / workers ----
  CoordinatorEpoch epoch() const { return epoch_; }
  DirectoryGeneration generation() const { return directory_generation_; }
  const Accounting& accounting() const { return accounting_; }

  Result<WorkerSession> register_worker_session(const WorkerSession& session);
  Result<AckResult> on_worker_loss(WorkerBootId boot, std::string reason);
  bool worker_live(WorkerBootId boot) const;

  // ---- registration ----
  Result<CacheDescriptor> register_cache(const CacheDescriptor& cache);
  Result<LocationDescriptor> register_location(const LocationDescriptor& loc);
  Result<ReplicaDescriptor> register_replica(const ReplicaDescriptor& replica);
  Result<DirectoryRecord> register_entry(const DirectoryRecord& record);

  // ---- controlled updates (all generation-fenced) ----
  Result<DirectoryRecord> update_health(DirectoryRecordId id, Health h, const AuthorityEnvelope& env);
  Result<DirectoryRecord> update_freshness(DirectoryRecordId id, Freshness f, const AuthorityEnvelope& env);
  Result<DirectoryRecord> update_reachability(DirectoryRecordId id, Reachability r, const AuthorityEnvelope& env);
  Result<DirectoryRecord> update_integrity(DirectoryRecordId id, Integrity i, const AuthorityEnvelope& env);
  Result<DirectoryRecord> update_lifecycle(DirectoryRecordId id, LifecycleState s, const AuthorityEnvelope& env);
  Result<CacheDescriptor> update_cache_health(CacheId id, Health h, Reachability r,
                                              const AuthorityEnvelope& env);
  Result<CacheDescriptor> update_cache_capacity(CacheId id, std::uint64_t capacity,
                                               std::uint64_t free_capacity,
                                               const AuthorityEnvelope& env);

  // ---- leases ----
  Result<LeaseDescriptor> register_lease(const LeaseDescriptor& lease);
  Result<LeaseDescriptor> renew_lease(LeaseId id, const AuthorityEnvelope& env);
  Result<AckResult> expire_lease(LeaseId id, const AuthorityEnvelope& env, const std::string& reason);
  Result<AckResult> revoke_lease(LeaseId id, const AuthorityEnvelope& env, const std::string& reason);

  // ---- invalidation / supersession / tombstone ----
  Result<InvalidateResult> invalidate_state(StateId s, StateGeneration g, const AuthorityEnvelope& env,
                                            const std::string& reason);
  Result<InvalidateResult> invalidate_location(LocationId loc, const AuthorityEnvelope& env,
                                               const std::string& reason);
  Result<InvalidateResult> invalidate_replica(ReplicaId rep, const AuthorityEnvelope& env,
                                              const std::string& reason);
  Result<InvalidateResult> invalidate_cache(CacheId c, const AuthorityEnvelope& env,
                                            const std::string& reason);
  Result<InvalidateResult> invalidate_worker_boot(WorkerBootId b, const AuthorityEnvelope& env,
                                                  const std::string& reason);
  Result<AckResult> tombstone(const TombstoneRecord& tombstone);
  bool covered_by_tombstone(const TombstoneTarget& target) const;

  // ---- query ----
  Result<QueryResult> query(const DirectoryQuery& q);        // increments query accounting
  QueryResult query_scan(const DirectoryQuery& q);              // canonical scan reference
  void set_ranking_policy(const RankingPolicy& p) { policy_ = p; }
  const RankingPolicy& ranking_policy() const { return policy_; }

  // ---- persistence ----
  Result<AckResult> save(const std::string& path) const;
  Result<AckResult> recover(const std::string& path);
  Result<AckResult> save_to_buffer(std::vector<std::uint8_t>& out) const;
  Result<AckResult> recover_from_buffer(const std::vector<std::uint8_t>& buf);

  // ---- introspection ----
  const std::vector<DirectoryRecord>& records() const { return records_; }
  const std::unordered_map<CacheId, CacheDescriptor>& caches() const { return caches_; }
  const std::unordered_map<LocationId, LocationDescriptor>& locations() const { return locations_; }
  const std::unordered_map<ReplicaId, ReplicaDescriptor>& replicas() const { return replicas_; }
  const std::unordered_map<LeaseId, LeaseDescriptor>& leases() const { return leases_; }
  const std::vector<TombstoneRecord>& tombstones() const { return tombstones_; }
  const std::vector<ObservationRecord>& observations() const { return observations_; }
  const std::unordered_map<WorkerBootId, WorkerSession>& workers() const { return workers_; }

  std::optional<DirectoryRecord> record(DirectoryRecordId id) const;
  Result<AckResult> validate_indexes() const;
  const DirectoryIndexes& indexes() const { ensure_indexes(); return indexes_; }

  // ---- explanation ----
  std::vector<ExplanationEntry> explain_query(const DirectoryQuery& q, const QueryResult& r) const;
  std::vector<ExplanationEntry> explain_candidate(const Candidate& c) const;
  std::vector<ExplanationEntry> explain_rejection(const Rejection& rej) const;
  std::vector<ExplanationEntry> explain_location(const LocationDescriptor& l) const;
  std::vector<ExplanationEntry> explain_replica(const ReplicaDescriptor& r) const;
  std::vector<ExplanationEntry> explain_reachability(const Reachability r) const;
  std::vector<ExplanationEntry> explain_freshness(const Freshness f) const;
  std::vector<ExplanationEntry> explain_lease(const LeaseDescriptor& l) const;
  std::vector<ExplanationEntry> explain_invalidation(const InvalidateResult& inv) const;
  std::vector<ExplanationEntry> explain_tombstone(const TombstoneRecord& t) const;
  std::vector<ExplanationEntry> explain_recovery() const;

  // ---- ordering / id helpers ----
  std::uint64_t next_object_id() { return next_obj_++; }

 private:
  // index maintenance
  void ensure_indexes() const;
  void build_indexes(DirectoryIndexes& out) const;
  std::size_t find_record_index(DirectoryRecordId id) const;
  QueryResult query_impl(const DirectoryQuery& q, std::vector<std::size_t> idx) const;
  std::vector<RankingFactor> score_factors(const DirectoryRecord& r, const DirectoryQuery& q, double& total) const;

  // authority / fencing
  bool validate_epoch(const AuthorityEnvelope& env) const { return env.epoch == epoch_; }
  Result<AckResult> check_tombstone(const DirectoryRecord& record) const;
  DirectoryError check_registration(const DirectoryRecord& record, std::string& why) const;
  DirectoryError check_update_authority(const DirectoryRecord& rec,
                                        const AuthorityEnvelope& env, std::string& why) const;

  // commit plumbing
  Result<DirectoryRecord> commit_record(DirectoryRecord record, bool supersede_older);
  void mark_state_superseded(StateId state, StateGeneration below);
  void recompute_accounting();

  CoordinatorEpoch epoch_{1};
  DirectoryGeneration directory_generation_{0};
  std::vector<DirectoryRecord> records_;
  std::unordered_map<CacheId, CacheDescriptor> caches_;
  std::unordered_map<LocationId, LocationDescriptor> locations_;
  std::unordered_map<ReplicaId, ReplicaDescriptor> replicas_;
  std::unordered_map<LeaseId, LeaseDescriptor> leases_;
  std::vector<TombstoneRecord> tombstones_;
  std::vector<ObservationRecord> observations_;
  std::unordered_map<StateId, StateGeneration> current_state_gen_;
  std::unordered_map<WorkerBootId, WorkerSession> workers_;
  Accounting accounting_;
  RankingPolicy policy_;
  mutable DirectoryIndexes indexes_;
  mutable bool indexes_dirty_{true};
  std::uint64_t next_obj_{1};
};

}  // namespace distributedcachedirectory
