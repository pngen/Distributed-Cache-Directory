#include "distributedcachedirectory/directory.hpp"
#include "distributedcachedirectory/digest.hpp"
#include "distributedcachedirectory/persistence.hpp"
#include "distributedcachedirectory/serde.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace distributedcachedirectory {

namespace {

using namespace distributedcachedirectory::persistence;

// ---- write helpers ----

void w_auth(serde::ByteWriter& w, const AuthorityEnvelope& a) {
  w.u64(a.epoch.value()); w.u64(a.boot.value()); w.u64(a.directory_generation.value()); w.u64(a.fence);
}
void w_est(serde::ByteWriter& w, const AccessEstimate& e) {
  w.f64(e.estimated_latency_ns); w.f64(e.estimated_bandwidth_bytes_per_s);
  w.u64(e.transfer_bytes); w.u32(e.staging_steps); w.u8(e.restore_required ? 1u : 0u);
  w.u32(e.remote_hops); w.u8(static_cast<std::uint8_t>(e.locality));
  w.u8(static_cast<std::uint8_t>(e.cost_class)); w.u8(static_cast<std::uint8_t>(e.provenance));
}
void w_lease(serde::ByteWriter& w, const LeaseDescriptor& l) {
  w.u64(l.lease_id.value()); w.u64(l.generation.value()); w.u64(l.holder.value());
  w.u64(l.start_ns); w.u64(l.expiry_ns); w.u8(l.expires ? 1u : 0u);
  w.u8(static_cast<std::uint8_t>(l.state)); w.u64(l.provenance.value()); w_auth(w, l.authority);
}
void w_target(serde::ByteWriter& w, const TombstoneTarget& t) {
  w.u8(static_cast<std::uint8_t>(t.kind));
  w.u64(t.state.value()); w.u64(t.cache.value()); w.u64(t.entry.value()); w.u64(t.replica.value());
  w.u64(t.location.value()); w.u64(t.worker_boot.value()); w.u64(t.node.value()); w.u64(t.device.value());
  w.u64(t.backend.value()); w.u64(t.compatibility.value()); w.u64(t.policy_generation.value());
  w.bytes(t.content); w.u64(t.generation_floor);
}
void w_tomb(serde::ByteWriter& w, const TombstoneRecord& t) {
  w.u64(t.tombstone_id.value()); w_target(w, t.target); w.u64(t.epoch.value());
  w.u64(t.worker_boot.value()); w.u64(t.authority_generation.value()); w.string(t.reason);
  w.u64(t.timestamp_ns); w.u64(t.provenance.value());
}
void w_obs(serde::ByteWriter& w, const ObservationRecord& o) {
  w.u64(o.observation_id.value()); w.u64(o.generation.value()); w.u64(o.worker_boot.value());
  w.string(o.subject); w.string(o.subject_key);
  w.u8(static_cast<std::uint8_t>(o.reachability)); w.u8(static_cast<std::uint8_t>(o.health));
  w.u8(static_cast<std::uint8_t>(o.integrity)); w.u8(static_cast<std::uint8_t>(o.freshness));
  w.f64(o.measured_latency_ns); w.u8(static_cast<std::uint8_t>(o.provenance));
  w_auth(w, o.authority); w.u64(o.observed_ns);
}
void w_cache(serde::ByteWriter& w, const CacheDescriptor& c) {
  w.u64(c.cache_id.value()); w.u8(static_cast<std::uint8_t>(c.kind)); w.u64(c.node.value());
  w.u64(c.worker.value()); w.u64(c.worker_boot.value()); w.u8(static_cast<std::uint8_t>(c.domain));
  w.u64(c.capacity); w.u8(c.capacity_known ? 1u : 0u); w.u64(c.free_capacity); w.u8(c.free_capacity_known ? 1u : 0u);
  w.u8(static_cast<std::uint8_t>(c.reachability)); w.u8(static_cast<std::uint8_t>(c.health));
  w.u8(static_cast<std::uint8_t>(c.freshness)); w.u64(c.provenance.value()); w.u64(c.generation.value());
  w_auth(w, c.authority); w.u64(c.observed_ns);
}
void w_loc(serde::ByteWriter& w, const LocationDescriptor& l) {
  w.u64(l.location_id.value()); w.u64(l.node.value()); w.u64(l.worker.value()); w.u64(l.worker_boot.value());
  w.u64(l.device.value()); w.u8(static_cast<std::uint8_t>(l.domain)); w.u64(l.backend.value()); w.u64(l.tier.value());
  w.string(l.process_tag); w.string(l.locator.key);
  w.u64(l.logical_bytes); w.u64(l.physical_bytes); w.u8(l.physical_bytes_known ? 1u : 0u);
  w.u8(l.content_digest_known ? 1u : 0u); if (l.content_digest_known) w.bytes(l.content_digest);
  w.u8(static_cast<std::uint8_t>(l.locality)); w.u8(static_cast<std::uint8_t>(l.reachability));
  w.u8(static_cast<std::uint8_t>(l.health)); w.u8(static_cast<std::uint8_t>(l.freshness));
  w.u8(static_cast<std::uint8_t>(l.integrity)); w_est(w, l.estimate); w.u64(l.generation.value());
  w.u64(l.provenance.value()); w_auth(w, l.authority); w.u64(l.created_ns); w.u64(l.observed_ns);
}
void w_rep(serde::ByteWriter& w, const ReplicaDescriptor& r) {
  w.u64(r.replica_id.value()); w.u64(r.location.value()); w.u64(r.generation.value());
  w.u64(r.cache.value()); w.u64(r.entry.value()); w.u8(static_cast<std::uint8_t>(r.reachability));
  w.u8(static_cast<std::uint8_t>(r.health)); w.u8(static_cast<std::uint8_t>(r.integrity));
  w.u8(static_cast<std::uint8_t>(r.freshness)); w.u64(r.observed_ns); w_auth(w, r.authority);
}
void w_rec(serde::ByteWriter& w, const DirectoryRecord& r);
void w_rec(serde::ByteWriter& w, const DirectoryRecord& r) {
  w.u64(r.record_id.value()); w.u64(r.state_id.value()); w.u64(r.state_generation.value());
  w.u8(static_cast<std::uint8_t>(r.kind)); w.string(r.name_space);
  w.u64(r.cache.value()); w.u64(r.entry.value()); w.u64(r.replica.value()); w.u64(r.location.value());
  w.u64(r.node.value()); w.u64(r.worker.value()); w.u64(r.worker_boot.value()); w.u64(r.device.value());
  w.u8(static_cast<std::uint8_t>(r.domain)); w.u64(r.logical_bytes); w.u64(r.physical_bytes);
  w.u8(r.physical_bytes_known ? 1u : 0u); w.u8(r.content_digest_known ? 1u : 0u);
  if (r.content_digest_known) w.bytes(r.content_digest);
  w.u64(r.compatibility.value()); w.u64(r.provenance.value());
  w.u64(r.location_generation.value()); w.u64(r.replica_generation.value()); w.u64(r.entry_generation.value());
  w.u8(static_cast<std::uint8_t>(r.health)); w.u8(static_cast<std::uint8_t>(r.freshness));
  w.u8(static_cast<std::uint8_t>(r.integrity)); w.u8(static_cast<std::uint8_t>(r.reachability));
  w.u8(r.has_lease ? 1u : 0u); if (r.has_lease) w_lease(w, r.lease);
  w_est(w, r.estimate); w.u64(r.policy_generation.value()); w_auth(w, r.authority);
  w.u64(r.record_generation.value()); w.u8(static_cast<std::uint8_t>(r.lifecycle)); w.u8(r.current ? 1u : 0u);
  w.u64(r.created_ns); w.u64(r.updated_ns); w.u64(r.observed_ns); w.bytes(r.semantic_digest);
}

// ---- read helpers ----

bool r_auth(serde::ByteReader& rd, AuthorityEnvelope& a) {
  std::uint64_t e = 0, b = 0, d = 0, f = 0;
  if (!rd.u64(e)) return false; if (!rd.u64(b)) return false; if (!rd.u64(d)) return false; if (!rd.u64(f)) return false;
  a.epoch = CoordinatorEpoch(e); a.boot = WorkerBootId(b); a.directory_generation = DirectoryGeneration(d); a.fence = f; return true;
}
bool r_est(serde::ByteReader& rd, AccessEstimate& e) {
  if (!rd.f64(e.estimated_latency_ns)) return false; if (!rd.f64(e.estimated_bandwidth_bytes_per_s)) return false;
  if (!rd.u64(e.transfer_bytes)) return false; if (!rd.u32(e.staging_steps)) return false;
  std::uint8_t r8 = 0, loc = 0, cost = 0, prov = 0; if (!rd.u8(r8)) return false; if (!rd.u32(e.remote_hops)) return false;
  if (!rd.u8(loc)) return false; if (!rd.u8(cost)) return false; if (!rd.u8(prov)) return false;
  e.restore_required = r8 != 0; e.locality = static_cast<LocalityClass>(loc); e.cost_class = static_cast<CostClass>(cost); e.provenance = static_cast<DataProvenance>(prov); return true;
}
bool r_lease(serde::ByteReader& rd, LeaseDescriptor& l) {
  std::uint64_t id = 0, g = 0, h = 0, pv = 0; std::uint8_t exp = 0, st = 0;
  if (!rd.u64(id)) return false; if (!rd.u64(g)) return false; if (!rd.u64(h)) return false;
  if (!rd.u64(l.start_ns)) return false; if (!rd.u64(l.expiry_ns)) return false; if (!rd.u8(exp)) return false;
  if (!rd.u8(st)) return false; if (!rd.u64(pv)) return false; if (!r_auth(rd, l.authority)) return false;
  l.lease_id = LeaseId(id); l.generation = LeaseGeneration(g); l.holder = WorkerBootId(h);
  l.expires = exp != 0; l.state = static_cast<LeaseState>(st); l.provenance = ProvenanceId(pv); return true;
}
bool r_target(serde::ByteReader& rd, TombstoneTarget& t) {
  std::uint8_t k = 0; if (!rd.u8(k)) return false; t.kind = static_cast<TombstoneKind>(k);
  std::uint64_t a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,l=0;
  if (!rd.u64(a)) return false; if (!rd.u64(b)) return false; if (!rd.u64(c)) return false; if (!rd.u64(d)) return false;
  if (!rd.u64(e)) return false; if (!rd.u64(f)) return false; if (!rd.u64(g)) return false; if (!rd.u64(h)) return false;
  if (!rd.u64(i)) return false; if (!rd.u64(j)) return false; if (!rd.u64(l)) return false;
  std::span<const std::uint8_t> c1; if (!rd.bytes(c1, 32)) return false; std::memcpy(t.content.data(), c1.data(), 32);
  if (!rd.u64(t.generation_floor)) return false;
  t.state=StateId(a); t.cache=CacheId(b); t.entry=CacheEntryId(c); t.replica=ReplicaId(d); t.location=LocationId(e);
  t.worker_boot=WorkerBootId(f); t.node=NodeId(g); t.device=DeviceId(h); t.backend=StorageBackendId(i);
  t.compatibility=CompatibilityId(j); t.policy_generation=PolicyGeneration(l); return true;
}
bool r_tomb(serde::ByteReader& rd, TombstoneRecord& t) {
  std::uint64_t id = 0; if (!rd.u64(id)) return false; t.tombstone_id = TombstoneId(id);
  if (!r_target(rd, t.target)) return false;
  std::uint64_t e=0,b=0,a=0,p=0; if (!rd.u64(e)) return false; if (!rd.u64(b)) return false; if (!rd.u64(a)) return false;
  if (!rd.string(t.reason, 1u << 20)) return false; if (!rd.u64(t.timestamp_ns)) return false; if (!rd.u64(p)) return false;
  t.epoch=CoordinatorEpoch(e); t.worker_boot=WorkerBootId(b); t.authority_generation=DirectoryGeneration(a); t.provenance=ProvenanceId(p); return true;
}
bool r_obs(serde::ByteReader& rd, ObservationRecord& o) {
  std::uint64_t id=0,g=0,b=0; std::uint8_t rch=0,hl=0,it=0,fr=0,pv=0;
  if (!rd.u64(id)) return false; if (!rd.u64(g)) return false; if (!rd.u64(b)) return false;
  if (!rd.string(o.subject, 1u << 20)) return false; if (!rd.string(o.subject_key, 1u << 20)) return false;
  if (!rd.u8(rch)) return false; if (!rd.u8(hl)) return false; if (!rd.u8(it)) return false; if (!rd.u8(fr)) return false;
  if (!rd.f64(o.measured_latency_ns)) return false; if (!rd.u8(pv)) return false;
  if (!r_auth(rd, o.authority)) return false; if (!rd.u64(o.observed_ns)) return false;
  o.observation_id=ObservationId(id); o.generation=ObservationGeneration(g); o.worker_boot=WorkerBootId(b);
  o.reachability=static_cast<Reachability>(rch); o.health=static_cast<Health>(hl); o.integrity=static_cast<Integrity>(it);
  o.freshness=static_cast<Freshness>(fr); o.provenance=static_cast<DataProvenance>(pv); return true;
}
bool r_cache(serde::ByteReader& rd, CacheDescriptor& c) {
  std::uint64_t id=0,n=0,w=0,b=0,pv=0,g=0,obs=0; std::uint8_t k=0,d=0,rch=0,hl=0,fr=0,ck=0,fk=0;
  if (!rd.u64(id)) return false; if (!rd.u8(k)) return false; if (!rd.u64(n)) return false; if (!rd.u64(w)) return false;
  if (!rd.u64(b)) return false; if (!rd.u8(d)) return false; if (!rd.u64(c.capacity)) return false; if (!rd.u8(ck)) return false;
  if (!rd.u64(c.free_capacity)) return false; if (!rd.u8(fk)) return false; if (!rd.u8(rch)) return false; if (!rd.u8(hl)) return false;
  if (!rd.u8(fr)) return false; if (!rd.u64(pv)) return false; if (!rd.u64(g)) return false;
  if (!r_auth(rd, c.authority)) return false; if (!rd.u64(obs)) return false;
  c.cache_id=CacheId(id); c.kind=static_cast<CacheKind>(k); c.node=NodeId(n); c.worker=WorkerId(w); c.worker_boot=WorkerBootId(b);
  c.domain=static_cast<MemoryDomain>(d); c.capacity_known=ck!=0; c.free_capacity_known=fk!=0;
  c.reachability=static_cast<Reachability>(rch); c.health=static_cast<Health>(hl); c.freshness=static_cast<Freshness>(fr);
  c.provenance=ProvenanceId(pv); c.generation=CacheGeneration(g); c.observed_ns=obs; return true;
}
bool r_loc(serde::ByteReader& rd, LocationDescriptor& l) {
  std::uint64_t id=0,n=0,w=0,b=0,dv=0,bk=0,ti=0,obs=0,cr=0,pv=0,g=0; std::uint8_t dm=0,dk=0,pk=0,loc=0,rch=0,hl=0,fr=0,it=0;
  if (!rd.u64(id)) return false; if (!rd.u64(n)) return false; if (!rd.u64(w)) return false; if (!rd.u64(b)) return false;
  if (!rd.u64(dv)) return false; if (!rd.u8(dm)) return false; if (!rd.u64(bk)) return false; if (!rd.u64(ti)) return false;
  if (!rd.string(l.process_tag, 1u << 20)) return false; if (!rd.string(l.locator.key, 1u << 24)) return false;
  if (!rd.u64(l.logical_bytes)) return false; if (!rd.u64(l.physical_bytes)) return false; if (!rd.u8(pk)) return false;
  if (!rd.u8(dk)) return false; if (dk) { std::span<const std::uint8_t> c1; if (!rd.bytes(c1,32)) return false; std::memcpy(l.content_digest.data(), c1.data(), 32); }
  if (!rd.u8(loc)) return false; if (!rd.u8(rch)) return false; if (!rd.u8(hl)) return false; if (!rd.u8(fr)) return false;
  if (!rd.u8(it)) return false; if (!r_est(rd, l.estimate)) return false; if (!rd.u64(g)) return false;
  if (!rd.u64(pv)) return false; if (!r_auth(rd, l.authority)) return false; if (!rd.u64(cr)) return false; if (!rd.u64(obs)) return false;
  l.location_id=LocationId(id); l.node=NodeId(n); l.worker=WorkerId(w); l.worker_boot=WorkerBootId(b); l.device=DeviceId(dv);
  l.domain=static_cast<MemoryDomain>(dm); l.backend=StorageBackendId(bk); l.tier=StorageTierId(ti);
  l.physical_bytes_known=pk!=0; l.content_digest_known=dk!=0; l.locality=static_cast<LocalityClass>(loc);
  l.reachability=static_cast<Reachability>(rch); l.health=static_cast<Health>(hl); l.freshness=static_cast<Freshness>(fr);
  l.integrity=static_cast<Integrity>(it); l.generation=LocationGeneration(g); l.provenance=ProvenanceId(pv);
  l.created_ns=cr; l.observed_ns=obs; return true;
}
bool r_rep(serde::ByteReader& rd, ReplicaDescriptor& r) {
  std::uint64_t id=0,loc=0,g=0,c=0,e=0,obs=0; std::uint8_t rch=0,hl=0,it=0,fr=0;
  if (!rd.u64(id)) return false; if (!rd.u64(loc)) return false; if (!rd.u64(g)) return false; if (!rd.u64(c)) return false;
  if (!rd.u64(e)) return false; if (!rd.u8(rch)) return false; if (!rd.u8(hl)) return false; if (!rd.u8(it)) return false;
  if (!rd.u8(fr)) return false; if (!rd.u64(obs)) return false; if (!r_auth(rd, r.authority)) return false;
  r.replica_id=ReplicaId(id); r.location=LocationId(loc); r.generation=ReplicaGeneration(g); r.cache=CacheId(c); r.entry=CacheEntryId(e);
  r.reachability=static_cast<Reachability>(rch); r.health=static_cast<Health>(hl); r.integrity=static_cast<Integrity>(it);
  r.freshness=static_cast<Freshness>(fr); r.observed_ns=obs; return true;
}
bool r_rec(serde::ByteReader& rd, DirectoryRecord& r) {
  std::uint64_t rid=0,sid=0,sg=0,cid=0,eid=0,rid2=0,lid=0,nid=0,wid=0,wb=0,dv=0,com=0,pv=0,lg=0,rg=0,eg=0,pg=0,rg2=0,cr=0,up=0,ob=0; std::uint8_t k=0,dm=0,pk=0,dk=0,hl=0,fr=0,it=0,rch=0,le=0,lc=0,cv=0;
  if (!rd.u64(rid)) return false; if (!rd.u64(sid)) return false; if (!rd.u64(sg)) return false; if (!rd.u8(k)) return false;
  if (!rd.string(r.name_space, 1u << 20)) return false; if (!rd.u64(cid)) return false; if (!rd.u64(eid)) return false;
  if (!rd.u64(rid2)) return false; if (!rd.u64(lid)) return false; if (!rd.u64(nid)) return false; if (!rd.u64(wid)) return false;
  if (!rd.u64(wb)) return false; if (!rd.u64(dv)) return false; if (!rd.u8(dm)) return false;
  if (!rd.u64(r.logical_bytes)) return false; if (!rd.u64(r.physical_bytes)) return false; if (!rd.u8(pk)) return false;
  if (!rd.u8(dk)) return false; if (dk) { std::span<const std::uint8_t> c1; if (!rd.bytes(c1,32)) return false; std::memcpy(r.content_digest.data(), c1.data(), 32); }
  if (!rd.u64(com)) return false; if (!rd.u64(pv)) return false; if (!rd.u64(lg)) return false; if (!rd.u64(rg)) return false;
  if (!rd.u64(eg)) return false; if (!rd.u8(hl)) return false; if (!rd.u8(fr)) return false; if (!rd.u8(it)) return false;
  if (!rd.u8(rch)) return false; if (!rd.u8(le)) return false; if (le) { if (!r_lease(rd, r.lease)) return false; r.has_lease = true; }
  if (!r_est(rd, r.estimate)) return false; if (!rd.u64(pg)) return false; if (!r_auth(rd, r.authority)) return false;
  if (!rd.u64(rg2)) return false; if (!rd.u8(lc)) return false; if (!rd.u8(cv)) return false;
  if (!rd.u64(cr)) return false; if (!rd.u64(up)) return false; if (!rd.u64(ob)) return false;
  std::span<const std::uint8_t> s1; if (!rd.bytes(s1,32)) return false; std::memcpy(r.semantic_digest.data(), s1.data(), 32);
  r.record_id=DirectoryRecordId(rid); r.state_id=StateId(sid); r.state_generation=StateGeneration(sg); r.kind=static_cast<StateKind>(k);
  r.cache=CacheId(cid); r.entry=CacheEntryId(eid); r.replica=ReplicaId(rid2); r.location=LocationId(lid); r.node=NodeId(nid);
  r.worker=WorkerId(wid); r.worker_boot=WorkerBootId(wb); r.device=DeviceId(dv); r.domain=static_cast<MemoryDomain>(dm);
  r.physical_bytes_known=pk!=0; r.content_digest_known=dk!=0; r.compatibility=CompatibilityId(com); r.provenance=ProvenanceId(pv);
  r.location_generation=LocationGeneration(lg); r.replica_generation=ReplicaGeneration(rg); r.entry_generation=EntryGeneration(eg);
  r.health=static_cast<Health>(hl); r.freshness=static_cast<Freshness>(fr); r.integrity=static_cast<Integrity>(it);
  r.reachability=static_cast<Reachability>(rch); r.policy_generation=PolicyGeneration(pg); r.record_generation=RecordGeneration(rg2);
  r.lifecycle=static_cast<LifecycleState>(lc); r.current=cv!=0; r.created_ns=cr; r.updated_ns=up; r.observed_ns=ob; return true;
}

// Recompute the deterministic semantic digest for a record (used after recovery
// mutates freshness/reachability). Field order matches semantic_digest_of in
// directory.cpp.
void recompute_record_digest(DirectoryRecord& r) {
  serde::ByteWriter w;
  w.u64(r.state_id.value()); w.u64(r.state_generation.value()); w.u8(static_cast<std::uint8_t>(r.kind)); w.string(r.name_space);
  w.u64(r.cache.value()); w.u64(r.entry.value()); w.u64(r.replica.value()); w.u64(r.location.value());
  w.u64(r.node.value()); w.u64(r.worker.value()); w.u64(r.worker_boot.value()); w.u64(r.device.value());
  w.u8(static_cast<std::uint8_t>(r.domain)); w.u64(r.logical_bytes); w.u64(r.physical_bytes);
  w.u8(r.physical_bytes_known ? 1u : 0u); w.u8(r.content_digest_known ? 1u : 0u);
  if (r.content_digest_known) w.bytes(r.content_digest);
  w.u64(r.compatibility.value()); w.u64(r.provenance.value()); w.u64(r.location_generation.value());
  w.u64(r.replica_generation.value()); w.u64(r.entry_generation.value()); w.u8(static_cast<std::uint8_t>(r.health));
  w.u8(static_cast<std::uint8_t>(r.freshness)); w.u8(static_cast<std::uint8_t>(r.integrity));
  w.u8(static_cast<std::uint8_t>(r.reachability)); w.u8(r.has_lease ? 1u : 0u);
  w.u64(r.policy_generation.value()); w.u64(r.authority.epoch.value()); w.u64(r.authority.boot.value());
  w.u64(r.authority.directory_generation.value()); w.u64(r.authority.fence);
  r.semantic_digest = digest::sha256(w.bytes());
}

bool is_process_local(MemoryDomain d) {
  return d == MemoryDomain::HOST_MEMORY || d == MemoryDomain::HOST_PINNED || d == MemoryDomain::CUDA_DEVICE ||
         d == MemoryDomain::LOCAL_FILESYSTEM || d == MemoryDomain::LOCAL_NVME_CLASS;
}

std::string why_prefix(const std::string& m) { return m; }

}  // namespace

Result<AckResult> Directory::save_to_buffer(std::vector<std::uint8_t>& out) const {
  serde::ByteWriter body;
  body.u64(records_.size()); body.u64(caches_.size()); body.u64(locations_.size()); body.u64(replicas_.size());
  body.u64(leases_.size()); body.u64(tombstones_.size()); body.u64(observations_.size());
  body.u64(epoch_.value()); body.u64(directory_generation_.value());
  for (const auto& r : records_) w_rec(body, r);
  for (const auto& kv : caches_) w_cache(body, kv.second);
  for (const auto& kv : locations_) w_loc(body, kv.second);
  for (const auto& kv : replicas_) w_rep(body, kv.second);
  for (const auto& kv : leases_) w_lease(body, kv.second);
  for (const auto& t : tombstones_) w_tomb(body, t);
  for (const auto& o : observations_) w_obs(body, o);

  serde::ByteWriter file;
  file.u32(DCD_PERSIST_MAGIC); file.u32(DCD_PERSIST_VERSION);
  file.u32(digest::crc32(body.bytes()));
  file.bytes(digest::sha256(body.bytes()));
  file.bytes(std::span<const std::uint8_t>(body.bytes()));
  out = file.bytes();
  AckResult a; a.ok = true; a.message = "serialized " + std::to_string(records_.size()) + " records";
  return Result<AckResult>::success(a);
}

Result<AckResult> Directory::save(const std::string& path) const {
  std::vector<std::uint8_t> buf;
  auto sr = save_to_buffer(buf);
  if (!sr) return sr;
  std::string tmp = path + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "cannot open temp file");
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!ofs) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "write failed");
    ofs.flush();
    ofs.close();
    if (!ofs) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "flush/close failed");
  }
#ifdef _WIN32
  if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "atomic rename failed");
#else
  std::filesystem::rename(tmp, path);
#endif
  AckResult a; a.ok = true; a.message = "saved to " + path;
  return Result<AckResult>::success(a);
}

Result<AckResult> Directory::recover_from_buffer(const std::vector<std::uint8_t>& buf) {
  if (buf.size() < DCD_PERSIST_HEADER) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: header incomplete");
  serde::ByteReader rd(buf);
  std::uint32_t magic = 0, version = 0, crc = 0;
  if (!rd.u32(magic)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: magic");
  if (magic != DCD_PERSIST_MAGIC) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "bad magic");
  if (!rd.u32(version)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: version");
  if (version != DCD_PERSIST_VERSION) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "unsupported version");
  if (!rd.u32(crc)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: crc");
  std::span<const std::uint8_t> sha1;
  if (!rd.bytes(sha1, 32)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: sha256");
  std::size_t body_off = rd.tell();
  std::span<const std::uint8_t> body(buf.data() + body_off, buf.size() - body_off);
  std::uint32_t c = digest::crc32(body);
  if (c != crc) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "checksum mismatch");
  auto calc = digest::sha256(body);
  if (!digest::digest_equal(calc, std::array<std::uint8_t,32>{sha1[0],sha1[1],sha1[2],sha1[3],sha1[4],sha1[5],sha1[6],sha1[7],sha1[8],sha1[9],sha1[10],sha1[11],sha1[12],sha1[13],sha1[14],sha1[15],sha1[16],sha1[17],sha1[18],sha1[19],sha1[20],sha1[21],sha1[22],sha1[23],sha1[24],sha1[25],sha1[26],sha1[27],sha1[28],sha1[29],sha1[30],sha1[31]}))
    return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "semantic digest mismatch");

  serde::ByteReader r(body);
  std::uint64_t nrecs=0,ncaches=0,nlocs=0,nreps=0,nleases=0,ntombs=0,nobs=0,epoch=0,dirgen=0;
  if (!r.u64(nrecs)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(ncaches)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(nlocs)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(nreps)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(nleases)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(ntombs)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(nobs)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: counts");
  if (!r.u64(epoch)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: epoch");
  if (!r.u64(dirgen)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: directory generation");
  auto maxc = DCD_PERSIST_MAX_COUNT;
  if (nrecs > maxc || ncaches > maxc || nlocs > maxc || nreps > maxc || nleases > maxc || ntombs > maxc || nobs > maxc)
    return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "impossible count");

  std::vector<DirectoryRecord> recs; recs.reserve(nrecs);
  std::vector<CacheDescriptor> caches; caches.reserve(ncaches);
  std::vector<LocationDescriptor> locs; locs.reserve(nlocs);
  std::vector<ReplicaDescriptor> reps; reps.reserve(nreps);
  std::vector<LeaseDescriptor> leases; leases.reserve(nleases);
  std::vector<TombstoneRecord> tombs; tombs.reserve(ntombs);
  std::vector<ObservationRecord> obs; obs.reserve(nobs);

  for (std::uint64_t i = 0; i < nrecs; ++i) { DirectoryRecord x; if (!r_rec(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed record"); recs.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < ncaches; ++i) { CacheDescriptor x; if (!r_cache(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed cache"); caches.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < nlocs; ++i) { LocationDescriptor x; if (!r_loc(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed location"); locs.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < nreps; ++i) { ReplicaDescriptor x; if (!r_rep(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed replica"); reps.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < nleases; ++i) { LeaseDescriptor x; if (!r_lease(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed lease"); leases.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < ntombs; ++i) { TombstoneRecord x; if (!r_tomb(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed tombstone"); tombs.push_back(std::move(x)); }
  for (std::uint64_t i = 0; i < nobs; ++i) { ObservationRecord x; if (!r_obs(r, x)) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed observation"); obs.push_back(std::move(x)); }
  if (!r.at_end()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "trailing garbage");

  // duplicate IDs checked within each entity type (cross-type sharing of the
  // same numeric value is legal, e.g. CacheId 1 and RecordId 1).
  auto dup_check = [](const auto& vec, const char* what, auto&& id) -> std::string {
    std::unordered_set<std::uint64_t> s;
    for (const auto& x : vec) if (!s.insert(id(x)).second) return std::string("duplicate ") + what + " id";
    return {};
  };
  { auto d = dup_check(recs, "record", [](const DirectoryRecord& r){ return r.record_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(caches, "cache", [](const CacheDescriptor& c){ return c.cache_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(locs, "location", [](const LocationDescriptor& l){ return l.location_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(reps, "replica", [](const ReplicaDescriptor& r){ return r.replica_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(leases, "lease", [](const LeaseDescriptor& l){ return l.lease_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(tombs, "tombstone", [](const TombstoneRecord& t){ return t.tombstone_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }
  { auto d = dup_check(obs, "observation", [](const ObservationRecord& o){ return o.observation_id.value(); }); if (!d.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, d); }

  // malformed identity validation
  for (const auto& x : recs) {
    if (x.record_id.is_null() || x.state_id.is_null() || x.cache.is_null() || x.replica.is_null() || x.location.is_null())
      return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed record identity");
  }
  for (const auto& x : tombs) {
    if (x.tombstone_id.is_null()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed tombstone");
    if (static_cast<int>(x.target.kind) < 0 || static_cast<int>(x.target.kind) > 12)
      return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed tombstone kind");
  }
  for (const auto& x : leases) {
    if (x.lease_id.is_null() || x.holder.is_null()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "malformed lease");
  }

  // generation regression: current per-state generation must be the max current record generation
  std::unordered_map<StateId, StateGeneration> cur;
  for (const auto& x : recs) {
    if (x.current) { auto it = cur.find(x.state_id); if (it == cur.end() || it->second.value() < x.state_generation.value()) cur[x.state_id] = x.state_generation; }
  }
  for (const auto& x : recs) {
    if (x.current) {
      auto it = cur.find(x.state_id);
      if (it != cur.end() && x.state_generation.value() < it->second.value())
        return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "generation regression");
    }
  }

  // recover into this directory
  records_ = std::move(recs);
  caches_.clear(); for (auto& cval : caches) caches_[cval.cache_id] = std::move(cval);
  locations_.clear(); for (auto& lval : locs) locations_[lval.location_id] = std::move(lval);
  replicas_.clear(); for (auto& rval : reps) replicas_[rval.replica_id] = std::move(rval);
  leases_.clear(); for (auto& lval : leases) leases_[lval.lease_id] = std::move(lval);
  tombstones_ = std::move(tombs);
  observations_ = std::move(obs);
  workers_.clear();

  // recovery semantics: live authority cleared; process-local/CUDA -> REVALIDATION_REQUIRED;
  // remote reachability observations stale; leases revalidation-required.
  std::uint32_t reval = 0;
  for (auto& recv : records_) {
    if (is_process_local(recv.domain)) { recv.freshness = Freshness::REVALIDATION_REQUIRED; recv.reachability = Reachability::REVALIDATION_REQUIRED; }
    else { recv.freshness = Freshness::STALE; recv.reachability = Reachability::REVALIDATION_REQUIRED; }
    recv.lifecycle = (recv.integrity == Integrity::CORRUPT) ? LifecycleState::FAILED
                 : (recv.reachability == Reachability::UNREACHABLE) ? LifecycleState::UNREACHABLE
                 : LifecycleState::STALE;
    recompute_record_digest(recv);
    ++reval;
  }
  for (auto& kv : locations_) {
    if (is_process_local(kv.second.domain)) { kv.second.freshness = Freshness::REVALIDATION_REQUIRED; kv.second.reachability = Reachability::REVALIDATION_REQUIRED; }
    else { kv.second.freshness = Freshness::STALE; kv.second.reachability = Reachability::REVALIDATION_REQUIRED; }
  }
  for (auto& kv : leases_) kv.second.state = LeaseState::REVALIDATION_REQUIRED;

  // rebuild current per-state generation and advance the epoch (fresh authority)
  current_state_gen_.clear();
  for (const auto& recv : records_) if (recv.current) { auto it = current_state_gen_.find(recv.state_id); if (it == current_state_gen_.end() || it->second.value() < recv.state_generation.value()) current_state_gen_[recv.state_id] = recv.state_generation; }
  epoch_ = CoordinatorEpoch(epoch + 1);
  directory_generation_ = DirectoryGeneration(dirgen);
  indexes_dirty_ = true;
  recompute_accounting();
  AckResult a; a.ok = true; a.message = "recovered " + std::to_string(records_.size()) + " records; " + std::to_string(reval) + " locations flagged revalidation";
  return Result<AckResult>::success(a);
}

Result<AckResult> Directory::recover(const std::string& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "cannot open file");
  std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  if (buf.empty()) return Result<AckResult>::failure(DirectoryError::PERSISTENCE_CORRUPT, "truncation: empty file");
  return recover_from_buffer(buf);
}

}  // namespace distributedcachedirectory
