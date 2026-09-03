#include "distributedcachedirectory/protocol.hpp"
#include "distributedcachedirectory/digest.hpp"
#include <cstring>
#include <string>
#include <vector>

namespace distributedcachedirectory {
namespace protocol {

namespace {

void wr_auth(serde::ByteWriter& w, const AuthorityEnvelope& a) {
  w.u64(a.epoch.value()); w.u64(a.boot.value()); w.u64(a.directory_generation.value()); w.u64(a.fence);
}
bool rd_auth(serde::ByteReader& r, AuthorityEnvelope& a) {
  std::uint64_t e=0,b=0,d=0,f=0;
  if (!r.u64(e)) return false; if (!r.u64(b)) return false; if (!r.u64(d)) return false; if (!r.u64(f)) return false;
  a.epoch=CoordinatorEpoch(e); a.boot=WorkerBootId(b); a.directory_generation=DirectoryGeneration(d); a.fence=f; return true;
}

void wr_rec(serde::ByteWriter& w, const DirectoryRecord& r) {
  w.u64(r.record_id.value()); w.u64(r.state_id.value()); w.u64(r.state_generation.value()); w.u8(static_cast<std::uint8_t>(r.kind));
  w.string(r.name_space); w.u64(r.cache.value()); w.u64(r.entry.value()); w.u64(r.replica.value()); w.u64(r.location.value());
  w.u64(r.node.value()); w.u64(r.worker.value()); w.u64(r.worker_boot.value()); w.u64(r.device.value());
  w.u8(static_cast<std::uint8_t>(r.domain)); w.u64(r.logical_bytes); w.u64(r.physical_bytes);
  w.u8(r.physical_bytes_known?1u:0u); w.u8(r.content_digest_known?1u:0u); if (r.content_digest_known) w.bytes(r.content_digest);
  w.u64(r.compatibility.value()); w.u64(r.provenance.value()); w.u64(r.location_generation.value()); w.u64(r.replica_generation.value());
  w.u64(r.entry_generation.value()); w.u8(static_cast<std::uint8_t>(r.health)); w.u8(static_cast<std::uint8_t>(r.freshness));
  w.u8(static_cast<std::uint8_t>(r.integrity)); w.u8(static_cast<std::uint8_t>(r.reachability)); w.u8(r.has_lease?1u:0u);
  if (r.has_lease) { wr_auth(w, r.lease.authority); w.u64(r.lease.lease_id.value()); w.u64(r.lease.generation.value()); w.u64(r.lease.holder.value()); w.u64(r.lease.start_ns); w.u64(r.lease.expiry_ns); w.u8(r.lease.expires?1u:0u); w.u8(static_cast<std::uint8_t>(r.lease.state)); w.u64(r.lease.provenance.value()); }
  w.f64(r.estimate.estimated_latency_ns); w.f64(r.estimate.estimated_bandwidth_bytes_per_s); w.u64(r.estimate.transfer_bytes);
  w.u32(r.estimate.staging_steps); w.u8(r.estimate.restore_required?1u:0u); w.u32(r.estimate.remote_hops);
  w.u8(static_cast<std::uint8_t>(r.estimate.locality)); w.u8(static_cast<std::uint8_t>(r.estimate.cost_class)); w.u8(static_cast<std::uint8_t>(r.estimate.provenance));
  w.u64(r.policy_generation.value()); wr_auth(w, r.authority); w.u64(r.record_generation.value());
  w.u8(static_cast<std::uint8_t>(r.lifecycle)); w.u8(r.current?1u:0u); w.u64(r.created_ns); w.u64(r.updated_ns); w.u64(r.observed_ns); w.bytes(r.semantic_digest);
}
bool rd_rec(serde::ByteReader& r, DirectoryRecord& x) {
  std::uint64_t a=0,b=0,c=0,e=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,o=0,p=0,q=0,s=0,t=0,u=0,v=0,cr=0,up=0,ob=0;
  std::uint8_t kind=0,dom=0,pk=0,dk=0,hl=0,fr=0,it=0,rch=0,lle=0,lc=0,cv=0;
  if (!r.u64(a)) return false; if (!r.u64(b)) return false; if (!r.u64(c)) return false; if (!r.u8(kind)) return false;
  if (!r.string(x.name_space, 1u<<20)) return false; if (!r.u64(e)) return false; if (!r.u64(g)) return false; if (!r.u64(h)) return false;
  if (!r.u64(i)) return false; if (!r.u64(j)) return false; if (!r.u64(k)) return false; if (!r.u64(l)) return false; if (!r.u64(m)) return false;
  if (!r.u8(dom)) return false; if (!r.u64(x.logical_bytes)) return false; if (!r.u64(x.physical_bytes)) return false;
  if (!r.u8(pk)) return false; if (!r.u8(dk)) return false; if (dk) { std::span<const std::uint8_t> c1; if (!r.bytes(c1,32)) return false; std::memcpy(x.content_digest.data(), c1.data(), 32); }
  if (!r.u64(o)) return false; if (!r.u64(p)) return false; if (!r.u64(s)) return false; if (!r.u64(t)) return false; if (!r.u64(u)) return false;
  if (!r.u8(hl)) return false; if (!r.u8(fr)) return false; if (!r.u8(it)) return false; if (!r.u8(rch)) return false; if (!r.u8(lle)) return false;
  if (lle) { if (!rd_auth(r, x.lease.authority)) return false; std::uint64_t lid=0,lg=0,lh=0,st=0,ex=0,pv=0; std::uint8_t le=0,ls=0; if (!r.u64(lid)) return false; if (!r.u64(lg)) return false; if (!r.u64(lh)) return false; if (!r.u64(st)) return false; if (!r.u64(ex)) return false; if (!r.u8(le)) return false; if (!r.u8(ls)) return false; if (!r.u64(pv)) return false; x.lease.lease_id=LeaseId(lid); x.lease.generation=LeaseGeneration(lg); x.lease.holder=WorkerBootId(lh); x.lease.start_ns=st; x.lease.expiry_ns=ex; x.lease.expires=le!=0; x.lease.state=static_cast<LeaseState>(ls); x.lease.provenance=ProvenanceId(pv); x.has_lease=true; }
  if (!r.f64(x.estimate.estimated_latency_ns)) return false; if (!r.f64(x.estimate.estimated_bandwidth_bytes_per_s)) return false; if (!r.u64(x.estimate.transfer_bytes)) return false;
  if (!r.u32(x.estimate.staging_steps)) return false; std::uint8_t r8=0,loc=0,cost=0,pr=0; if (!r.u8(r8)) return false; if (!r.u32(x.estimate.remote_hops)) return false; if (!r.u8(loc)) return false; if (!r.u8(cost)) return false; if (!r.u8(pr)) return false;
  if (!r.u64(v)) return false; if (!rd_auth(r, x.authority)) return false; if (!r.u64(q)) return false; if (!r.u8(lc)) return false; if (!r.u8(cv)) return false;
  if (!r.u64(cr)) return false; if (!r.u64(up)) return false; if (!r.u64(ob)) return false; std::span<const std::uint8_t> s1; if (!r.bytes(s1,32)) return false; std::memcpy(x.semantic_digest.data(), s1.data(), 32);
  x.record_id=DirectoryRecordId(a); x.state_id=StateId(b); x.state_generation=StateGeneration(c); x.kind=static_cast<StateKind>(kind);
  x.cache=CacheId(e); x.entry=CacheEntryId(g); x.replica=ReplicaId(h); x.location=LocationId(i); x.node=NodeId(j); x.worker=WorkerId(k);
  x.worker_boot=WorkerBootId(l); x.device=DeviceId(m); x.domain=static_cast<MemoryDomain>(dom); x.physical_bytes_known=pk!=0; x.content_digest_known=dk!=0;
  x.compatibility=CompatibilityId(o); x.provenance=ProvenanceId(p); x.location_generation=LocationGeneration(s); x.replica_generation=ReplicaGeneration(t);
  x.entry_generation=EntryGeneration(u); x.health=static_cast<Health>(hl); x.freshness=static_cast<Freshness>(fr); x.integrity=static_cast<Integrity>(it);
  x.reachability=static_cast<Reachability>(rch); x.policy_generation=PolicyGeneration(v); x.record_generation=RecordGeneration(q); x.lifecycle=static_cast<LifecycleState>(lc);
  x.current=cv!=0; x.created_ns=cr; x.updated_ns=up; x.observed_ns=ob; return true;
}

void wr_cache(serde::ByteWriter& w, const CacheDescriptor& c) {
  w.u64(c.cache_id.value()); w.u8(static_cast<std::uint8_t>(c.kind)); w.u64(c.node.value()); w.u64(c.worker.value()); w.u64(c.worker_boot.value());
  w.u8(static_cast<std::uint8_t>(c.domain)); w.u64(c.capacity); w.u8(c.capacity_known?1u:0u); w.u64(c.free_capacity); w.u8(c.free_capacity_known?1u:0u);
  w.u8(static_cast<std::uint8_t>(c.reachability)); w.u8(static_cast<std::uint8_t>(c.health)); w.u8(static_cast<std::uint8_t>(c.freshness));
  w.u64(c.provenance.value()); w.u64(c.generation.value()); wr_auth(w, c.authority); w.u64(c.observed_ns);
}
bool rd_cache(serde::ByteReader& r, CacheDescriptor& c) {
  std::uint64_t id=0,n=0,w=0,b=0,pv=0,g=0,obs=0; std::uint8_t k=0,d=0,rch=0,hl=0,fr=0,ck=0,fk=0;
  if (!r.u64(id)) return false; if (!r.u8(k)) return false; if (!r.u64(n)) return false; if (!r.u64(w)) return false;
  if (!r.u64(b)) return false; if (!r.u8(d)) return false; if (!r.u64(c.capacity)) return false; if (!r.u8(ck)) return false;
  if (!r.u64(c.free_capacity)) return false; if (!r.u8(fk)) return false; if (!r.u8(rch)) return false; if (!r.u8(hl)) return false;
  if (!r.u8(fr)) return false; if (!r.u64(pv)) return false; if (!r.u64(g)) return false; if (!rd_auth(r, c.authority)) return false; if (!r.u64(obs)) return false;
  c.cache_id=CacheId(id); c.kind=static_cast<CacheKind>(k); c.node=NodeId(n); c.worker=WorkerId(w); c.worker_boot=WorkerBootId(b); c.domain=static_cast<MemoryDomain>(d);
  c.capacity_known=ck!=0; c.free_capacity_known=fk!=0; c.reachability=static_cast<Reachability>(rch); c.health=static_cast<Health>(hl); c.freshness=static_cast<Freshness>(fr);
  c.provenance=ProvenanceId(pv); c.generation=CacheGeneration(g); c.observed_ns=obs; return true;
}

void wr_est(serde::ByteWriter& w, const AccessEstimate& e) { w.f64(e.estimated_latency_ns); w.f64(e.estimated_bandwidth_bytes_per_s); w.u64(e.transfer_bytes); w.u32(e.staging_steps); w.u8(e.restore_required?1u:0u); w.u32(e.remote_hops); w.u8(static_cast<std::uint8_t>(e.locality)); w.u8(static_cast<std::uint8_t>(e.cost_class)); w.u8(static_cast<std::uint8_t>(e.provenance)); }
bool rd_est(serde::ByteReader& r, AccessEstimate& e) { std::uint8_t r8=0,loc=0,cost=0,pr=0; if (!r.f64(e.estimated_latency_ns)) return false; if (!r.f64(e.estimated_bandwidth_bytes_per_s)) return false; if (!r.u64(e.transfer_bytes)) return false; if (!r.u32(e.staging_steps)) return false; if (!r.u8(r8)) return false; if (!r.u32(e.remote_hops)) return false; if (!r.u8(loc)) return false; if (!r.u8(cost)) return false; if (!r.u8(pr)) return false; e.restore_required=r8!=0; e.locality=static_cast<LocalityClass>(loc); e.cost_class=static_cast<CostClass>(cost); e.provenance=static_cast<DataProvenance>(pr); return true; }

void wr_loc(serde::ByteWriter& w, const LocationDescriptor& l) {
  w.u64(l.location_id.value()); w.u64(l.node.value()); w.u64(l.worker.value()); w.u64(l.worker_boot.value()); w.u64(l.device.value());
  w.u8(static_cast<std::uint8_t>(l.domain)); w.u64(l.backend.value()); w.u64(l.tier.value()); w.string(l.process_tag); w.string(l.locator.key);
  w.u64(l.logical_bytes); w.u64(l.physical_bytes); w.u8(l.physical_bytes_known?1u:0u); w.u8(l.content_digest_known?1u:0u); if (l.content_digest_known) w.bytes(l.content_digest);
  w.u8(static_cast<std::uint8_t>(l.locality)); w.u8(static_cast<std::uint8_t>(l.reachability)); w.u8(static_cast<std::uint8_t>(l.health)); w.u8(static_cast<std::uint8_t>(l.freshness));
  w.u8(static_cast<std::uint8_t>(l.integrity)); wr_est(w, l.estimate); w.u64(l.generation.value()); w.u64(l.provenance.value()); wr_auth(w, l.authority); w.u64(l.created_ns); w.u64(l.observed_ns);
}
bool rd_loc(serde::ByteReader& r, LocationDescriptor& l) {
  std::uint64_t id=0,n=0,w=0,b=0,dv=0,bk=0,ti=0,obs=0,cr=0,pv=0,g=0; std::uint8_t dm=0,dk=0,pk=0,loc=0,rch=0,hl=0,fr=0,it=0;
  if (!r.u64(id)) return false; if (!r.u64(n)) return false; if (!r.u64(w)) return false; if (!r.u64(b)) return false;
  if (!r.u64(dv)) return false; if (!r.u8(dm)) return false; if (!r.u64(bk)) return false; if (!r.u64(ti)) return false;
  if (!r.string(l.process_tag, 1u<<20)) return false; if (!r.string(l.locator.key, 1u<<24)) return false;
  if (!r.u64(l.logical_bytes)) return false; if (!r.u64(l.physical_bytes)) return false; if (!r.u8(pk)) return false;
  if (!r.u8(dk)) return false; if (dk) { std::span<const std::uint8_t> c1; if (!r.bytes(c1,32)) return false; std::memcpy(l.content_digest.data(), c1.data(), 32); }
  if (!r.u8(loc)) return false; if (!r.u8(rch)) return false; if (!r.u8(hl)) return false; if (!r.u8(fr)) return false;
  if (!r.u8(it)) return false; if (!rd_est(r, l.estimate)) return false; if (!r.u64(g)) return false;
  if (!r.u64(pv)) return false; if (!rd_auth(r, l.authority)) return false; if (!r.u64(cr)) return false; if (!r.u64(obs)) return false;
  l.location_id=LocationId(id); l.node=NodeId(n); l.worker=WorkerId(w); l.worker_boot=WorkerBootId(b); l.device=DeviceId(dv); l.domain=static_cast<MemoryDomain>(dm);
  l.backend=StorageBackendId(bk); l.tier=StorageTierId(ti); l.physical_bytes_known=pk!=0; l.content_digest_known=dk!=0; l.locality=static_cast<LocalityClass>(loc);
  l.reachability=static_cast<Reachability>(rch); l.health=static_cast<Health>(hl); l.freshness=static_cast<Freshness>(fr); l.integrity=static_cast<Integrity>(it);
  l.generation=LocationGeneration(g); l.provenance=ProvenanceId(pv); l.created_ns=cr; l.observed_ns=obs; return true;
}
void wr_rep(serde::ByteWriter& w, const ReplicaDescriptor& r) {
  w.u64(r.replica_id.value()); w.u64(r.location.value()); w.u64(r.generation.value()); w.u64(r.cache.value()); w.u64(r.entry.value());
  w.u8(static_cast<std::uint8_t>(r.reachability)); w.u8(static_cast<std::uint8_t>(r.health)); w.u8(static_cast<std::uint8_t>(r.integrity)); w.u8(static_cast<std::uint8_t>(r.freshness));
  w.u64(r.observed_ns); wr_auth(w, r.authority);
}
bool rd_rep(serde::ByteReader& r, ReplicaDescriptor& x) {
  std::uint64_t id=0,loc=0,g=0,c=0,e=0,obs=0; std::uint8_t rch=0,hl=0,it=0,fr=0;
  if (!r.u64(id)) return false; if (!r.u64(loc)) return false; if (!r.u64(g)) return false; if (!r.u64(c)) return false;
  if (!r.u64(e)) return false; if (!r.u8(rch)) return false; if (!r.u8(hl)) return false; if (!r.u8(it)) return false;
  if (!r.u8(fr)) return false; if (!r.u64(obs)) return false; if (!rd_auth(r, x.authority)) return false;
  x.replica_id=ReplicaId(id); x.location=LocationId(loc); x.generation=ReplicaGeneration(g); x.cache=CacheId(c); x.entry=CacheEntryId(e);
  x.reachability=static_cast<Reachability>(rch); x.health=static_cast<Health>(hl); x.integrity=static_cast<Integrity>(it); x.freshness=static_cast<Freshness>(fr); x.observed_ns=obs; return true;
}

}  // namespace

std::vector<std::uint8_t> encode_frame(MessageKind kind, std::span<const std::uint8_t> payload) {
  serde::ByteWriter w;
  w.u32(FRAME_MAGIC); w.u32(FRAME_VERSION); w.u8(static_cast<std::uint8_t>(kind));
  w.u32(static_cast<std::uint32_t>(payload.size())); w.u32(digest::crc32(payload)); w.bytes(payload);
  return w.bytes();
}

bool decode_frame(std::span<const std::uint8_t> bytes, Frame& out, std::string& err, std::size_t& consumed) {
  if (bytes.size() < 17) { err = "truncation: frame header incomplete"; return false; }
  serde::ByteReader rd(bytes);
  std::uint32_t magic=0, ver=0, len=0, crc=0; std::uint8_t kind=0;
  if (!rd.u32(magic)) { err = "truncation: magic"; return false; }
  if (magic != FRAME_MAGIC) { err = "bad magic"; return false; }
  if (!rd.u32(ver)) { err = "truncation: version"; return false; }
  if (ver != FRAME_VERSION) { err = "unsupported version"; return false; }
  if (!rd.u8(kind)) { err = "truncation: kind"; return false; }
  if (kind > static_cast<std::uint8_t>(MessageKind::ERROR)) { err = "invalid enum"; return false; }
  if (!rd.u32(len)) { err = "truncation: length"; return false; }
  if (len > MAX_PAYLOAD) { err = "oversized payload"; return false; }
  if (!rd.u32(crc)) { err = "truncation: crc"; return false; }
  std::size_t total = 17ull + static_cast<std::size_t>(len);
  if (bytes.size() < total) { err = "truncation: payload"; return false; }
  std::span<const std::uint8_t> p = bytes.subspan(17, len);
  if (digest::crc32(p) != crc) { err = "checksum mismatch"; return false; }
  out.kind = static_cast<MessageKind>(kind);
  out.payload.assign(p.begin(), p.end());
  consumed = total;
  return true;
}

std::vector<std::uint8_t> serialize_ack(const AckResult& a) {
  serde::ByteWriter w; w.u8(a.ok?1u:0u); w.string(a.message); return w.bytes();
}
bool deserialize_ack(std::span<const std::uint8_t> p, AckResult& a) {
  serde::ByteReader r(p); std::uint8_t ok=0; if (!r.u8(ok)) return false; if (!r.string(a.message, 1u<<20)) return false; a.ok = ok!=0; return r.at_end();
}
std::vector<std::uint8_t> serialize_error(DirectoryError e, const std::string& msg) {
  serde::ByteWriter w; w.u8(static_cast<std::uint8_t>(e)); w.string(msg); return w.bytes();
}
bool deserialize_error(std::span<const std::uint8_t> p, DirectoryError& e, std::string& msg) {
  serde::ByteReader r(p); std::uint8_t q=0; if (!r.u8(q)) return false; if (!r.string(msg, 1u<<20)) return false; e=static_cast<DirectoryError>(q); return r.at_end();
}
std::vector<std::uint8_t> serialize_hello(const WorkerSession& s) {
  serde::ByteWriter w; w.u64(s.boot.value()); w.u64(s.worker.value()); w.u64(s.node.value()); w.string(s.tag); w.u8(s.live?1u:0u); w.u64(s.source_authority); return w.bytes();
}
bool deserialize_hello(std::span<const std::uint8_t> p, WorkerSession& s) {
  serde::ByteReader r(p); std::uint64_t b=0,w=0,n=0,sa=0; std::uint8_t lv=0; if (!r.u64(b)) return false; if (!r.u64(w)) return false; if (!r.u64(n)) return false; if (!r.string(s.tag, 1u<<20)) return false; if (!r.u8(lv)) return false; if (!r.u64(sa)) return false; s.boot=WorkerBootId(b); s.worker=WorkerId(w); s.node=NodeId(n); s.live=lv!=0; s.source_authority=sa; return r.at_end();
}
std::vector<std::uint8_t> serialize_epoch_reply(CoordinatorEpoch epoch) { serde::ByteWriter w; w.u64(epoch.value()); return w.bytes(); }
bool deserialize_epoch_reply(std::span<const std::uint8_t> p, CoordinatorEpoch& epoch) { serde::ByteReader r(p); std::uint64_t e=0; if (!r.u64(e)) return false; epoch=CoordinatorEpoch(e); return r.at_end(); }
std::vector<std::uint8_t> serialize_cache(const CacheDescriptor& c) { serde::ByteWriter w; wr_cache(w, c); return w.bytes(); }
bool deserialize_cache(std::span<const std::uint8_t> p, CacheDescriptor& c) { serde::ByteReader r(p); return rd_cache(r, c) && r.at_end(); }
std::vector<std::uint8_t> serialize_location(const LocationDescriptor& l) { serde::ByteWriter w; wr_loc(w, l); return w.bytes(); }
bool deserialize_location(std::span<const std::uint8_t> p, LocationDescriptor& l) { serde::ByteReader r(p); return rd_loc(r, l) && r.at_end(); }
std::vector<std::uint8_t> serialize_replica(const ReplicaDescriptor& r) { serde::ByteWriter w; wr_rep(w, r); return w.bytes(); }
bool deserialize_replica(std::span<const std::uint8_t> p, ReplicaDescriptor& r) { serde::ByteReader rd(p); return rd_rep(rd, r) && rd.at_end(); }
std::vector<std::uint8_t> serialize_record(const DirectoryRecord& r) { serde::ByteWriter w; wr_rec(w, r); return w.bytes(); }
bool deserialize_record(std::span<const std::uint8_t> p, DirectoryRecord& r) { serde::ByteReader rd(p); return rd_rec(rd, r) && rd.at_end(); }
std::vector<std::uint8_t> serialize_path(const std::string& path) { serde::ByteWriter w; w.string(path); return w.bytes(); }
bool deserialize_path(std::span<const std::uint8_t> p, std::string& path) { serde::ByteReader r(p); return r.string(path, 1u<<20) && r.at_end(); }

// ---- query ----
std::vector<std::uint8_t> serialize_query(const DirectoryQuery& q) {
  serde::ByteWriter w;
  w.u64(q.query_id.value()); w.u8(q.exact_state?1u:0u); w.u64(q.state.value()); w.u8(q.exact_state_generation?1u:0u); w.u64(q.state_generation.value());
  w.u8(q.has_kind?1u:0u); w.u8(static_cast<std::uint8_t>(q.kind)); w.string(q.name_space);
  w.u8(q.require_content_digest?1u:0u); if (q.require_content_digest) w.bytes(q.content_digest);
  w.u8(q.require_compatibility?1u:0u); w.u64(q.compatibility.value());
  w.u8(q.has_integrity?1u:0u); w.u8(static_cast<std::uint8_t>(q.integrity));
  w.u8(q.has_freshness?1u:0u); w.u8(static_cast<std::uint8_t>(q.freshness));
  w.u8(q.has_health?1u:0u); w.u8(static_cast<std::uint8_t>(q.health));
  w.u8(q.has_reachability?1u:0u); w.u8(static_cast<std::uint8_t>(q.reachability));
  w.u32(static_cast<std::uint32_t>(q.allowed_domains.size())); for (auto d : q.allowed_domains) w.u8(static_cast<std::uint8_t>(d));
  w.u32(static_cast<std::uint32_t>(q.preferred_domains.size())); for (auto d : q.preferred_domains) w.u8(static_cast<std::uint8_t>(d));
  w.u8(q.has_max_latency?1u:0u); w.f64(q.max_latency_ns); w.u8(q.has_preferred_node?1u:0u); w.u64(q.preferred_node.value());
  w.u8(q.has_preferred_device?1u:0u); w.u64(q.preferred_device.value()); w.string(q.requester_process_tag); w.u64(q.requester_node.value());
  w.u32(q.min_replica_count); w.u8(q.current_only?1u:0u);
  return w.bytes();
}
bool deserialize_query(std::span<const std::uint8_t> p, DirectoryQuery& q) {
  serde::ByteReader r(p); std::uint64_t qid=0,st=0,sg=0,com=0,pn=0,pd=0,rn=0; std::uint8_t es=0,esg=0,hk=0,k=0,dc=0,co=0,hi=0,it=0,hf=0,fr=0,hh=0,hl=0,hr=0,rc=0,ml=0,hp=0,hpd=0,co2=0; std::uint32_t nad=0,npd=0,mr=0;
  if (!r.u64(qid)) return false; if (!r.u8(es)) return false; if (!r.u64(st)) return false; if (!r.u8(esg)) return false; if (!r.u64(sg)) return false;
  if (!r.u8(hk)) return false; if (!r.u8(k)) return false; if (!r.string(q.name_space, 1u<<20)) return false; if (!r.u8(dc)) return false; if (dc) { std::span<const std::uint8_t> c1; if (!r.bytes(c1,32)) return false; std::memcpy(q.content_digest.data(), c1.data(), 32); }
  if (!r.u8(co)) return false; if (!r.u64(com)) return false; if (!r.u8(hi)) return false; if (!r.u8(it)) return false; if (!r.u8(hf)) return false; if (!r.u8(fr)) return false; if (!r.u8(hh)) return false; if (!r.u8(hl)) return false; if (!r.u8(hr)) return false; if (!r.u8(rc)) return false;
  if (!r.u32(nad)) return false; if (nad > 1024) return false; for (std::uint32_t i=0;i<nad;++i){ std::uint8_t d=0; if(!r.u8(d)) return false; q.allowed_domains.push_back(static_cast<MemoryDomain>(d)); }
  if (!r.u32(npd)) return false; if (npd > 1024) return false; for (std::uint32_t i=0;i<npd;++i){ std::uint8_t d=0; if(!r.u8(d)) return false; q.preferred_domains.push_back(static_cast<MemoryDomain>(d)); }
  if (!r.u8(ml)) return false; if (!r.f64(q.max_latency_ns)) return false; if (!r.u8(hp)) return false; if (!r.u64(pn)) return false; if (!r.u8(hpd)) return false; if (!r.u64(pd)) return false;
  if (!r.string(q.requester_process_tag, 1u<<20)) return false; if (!r.u64(rn)) return false; if (!r.u32(mr)) return false; if (!r.u8(co2)) return false;
  q.query_id=QueryId(qid); q.exact_state=es!=0; q.state=StateId(st); q.exact_state_generation=esg!=0; q.state_generation=StateGeneration(sg); q.has_kind=hk!=0; q.kind=static_cast<StateKind>(k);
  q.require_content_digest=dc!=0; q.require_compatibility=co!=0; q.compatibility=CompatibilityId(com); q.has_integrity=hi!=0; q.integrity=static_cast<Integrity>(it);
  q.has_freshness=hf!=0; q.freshness=static_cast<Freshness>(fr); q.has_health=hh!=0; q.health=static_cast<Health>(hl); q.has_reachability=hr!=0; q.reachability=static_cast<Reachability>(rc);
  q.has_max_latency=ml!=0; q.has_preferred_node=hp!=0; q.preferred_node=NodeId(pn); q.has_preferred_device=hpd!=0; q.preferred_device=DeviceId(pd); q.requester_node=NodeId(rn);
  q.min_replica_count=mr; q.current_only=co2!=0; return r.at_end();
}

std::vector<std::uint8_t> serialize_query_result(const QueryResult& qr) {
  serde::ByteWriter w; w.u64(qr.query_id.value()); w.u8(static_cast<std::uint8_t>(qr.outcome)); w.u32(qr.selected_index); w.u32(qr.matched_scan_count);
  w.u32(static_cast<std::uint32_t>(qr.candidates.size())); for (const auto& c : qr.candidates) { wr_rec(w, c.record); w.f64(c.score); w.u8(c.selected?1u:0u);
    w.u32(static_cast<std::uint32_t>(c.factors.size())); for (const auto& f : c.factors) { w.u8(static_cast<std::uint8_t>(f.kind)); w.f64(f.weight); w.f64(f.value); w.f64(f.contribution); w.string(f.note); } }
  w.u32(static_cast<std::uint32_t>(qr.rejections.size())); for (const auto& rj : qr.rejections) { w.u64(rj.record_id.value()); w.u8(static_cast<std::uint8_t>(rj.reason)); w.u64(rj.state.value()); w.u64(rj.state_generation.value()); w.u64(rj.replica.value()); w.u64(rj.location.value()); }
  return w.bytes();
}
bool deserialize_query_result(std::span<const std::uint8_t> p, QueryResult& qr) {
  serde::ByteReader r(p); std::uint64_t qid=0; std::uint8_t oc=0; std::uint32_t sel=0,msc=0,ncand=0,nrej=0;
  if (!r.u64(qid)) return false; if (!r.u8(oc)) return false; if (!r.u32(sel)) return false; if (!r.u32(msc)) return false; if (!r.u32(ncand)) return false; if (ncand>1000000) return false;
  qr.query_id=QueryId(qid); qr.outcome=static_cast<QueryOutcome>(oc); qr.selected_index=sel; qr.matched_scan_count=msc;
  for (std::uint32_t i=0;i<ncand;++i){ Candidate c; if(!rd_rec(r, c.record)) return false; if(!r.f64(c.score)) return false; std::uint8_t selc=0; if(!r.u8(selc)) return false; c.selected=selc!=0;
    std::uint32_t nf=0; if(!r.u32(nf)) return false; if(nf>1024) return false; for(std::uint32_t j=0;j<nf;++j){ std::uint8_t kk=0; double w=0,v=0,co=0; std::string note; if(!r.u8(kk)) return false; if(!r.f64(w)) return false; if(!r.f64(v)) return false; if(!r.f64(co)) return false; if(!r.string(note,1u<<20)) return false; c.factors.push_back(RankingFactor{static_cast<RankingFactorKind>(kk),w,v,co,std::move(note)}); }
    qr.candidates.push_back(std::move(c)); }
  if(!r.u32(nrej)) return false; if(nrej>1000000) return false; for(std::uint32_t i=0;i<nrej;++i){ Rejection rj; std::uint64_t rid=0,s=0,sg=0,rep=0,loc=0; std::uint8_t rs=0; if(!r.u64(rid)) return false; if(!r.u8(rs)) return false; if(!r.u64(s)) return false; if(!r.u64(sg)) return false; if(!r.u64(rep)) return false; if(!r.u64(loc)) return false; rj.record_id=DirectoryRecordId(rid); rj.reason=static_cast<RejectionReason>(rs); rj.state=StateId(s); rj.state_generation=StateGeneration(sg); rj.replica=ReplicaId(rep); rj.location=LocationId(loc); qr.rejections.push_back(rj); }
  return r.at_end();
}

// ---- update/lease/invalidate/tombstone ----
std::vector<std::uint8_t> serialize_update_health(const UpdateHealthMsg& m) { serde::ByteWriter w; w.u64(m.id.value()); w.u8(static_cast<std::uint8_t>(m.health)); wr_auth(w, m.env); return w.bytes(); }
bool deserialize_update_health(std::span<const std::uint8_t> p, UpdateHealthMsg& m) { serde::ByteReader r(p); std::uint64_t id=0; std::uint8_t h=0; if(!r.u64(id)) return false; if(!r.u8(h)) return false; if(!rd_auth(r,m.env)) return false; m.id=DirectoryRecordId(id); m.health=static_cast<Health>(h); return r.at_end(); }
std::vector<std::uint8_t> serialize_update_freshness(const UpdateRefreshMsg& m) { serde::ByteWriter w; w.u64(m.id.value()); w.u8(static_cast<std::uint8_t>(m.freshness)); wr_auth(w, m.env); return w.bytes(); }
bool deserialize_update_freshness(std::span<const std::uint8_t> p, UpdateRefreshMsg& m) { serde::ByteReader r(p); std::uint64_t id=0; std::uint8_t f=0; if(!r.u64(id)) return false; if(!r.u8(f)) return false; if(!rd_auth(r,m.env)) return false; m.id=DirectoryRecordId(id); m.freshness=static_cast<Freshness>(f); return r.at_end(); }
std::vector<std::uint8_t> serialize_update_reachability(const UpdateReachMsg& m) { serde::ByteWriter w; w.u64(m.id.value()); w.u8(static_cast<std::uint8_t>(m.reachability)); wr_auth(w, m.env); return w.bytes(); }
bool deserialize_update_reachability(std::span<const std::uint8_t> p, UpdateReachMsg& m) { serde::ByteReader r(p); std::uint64_t id=0; std::uint8_t f=0; if(!r.u64(id)) return false; if(!r.u8(f)) return false; if(!rd_auth(r,m.env)) return false; m.id=DirectoryRecordId(id); m.reachability=static_cast<Reachability>(f); return r.at_end(); }
std::vector<std::uint8_t> serialize_update_integrity(const UpdateIntegrityMsg& m) { serde::ByteWriter w; w.u64(m.id.value()); w.u8(static_cast<std::uint8_t>(m.integrity)); wr_auth(w, m.env); return w.bytes(); }
bool deserialize_update_integrity(std::span<const std::uint8_t> p, UpdateIntegrityMsg& m) { serde::ByteReader r(p); std::uint64_t id=0; std::uint8_t f=0; if(!r.u64(id)) return false; if(!r.u8(f)) return false; if(!rd_auth(r,m.env)) return false; m.id=DirectoryRecordId(id); m.integrity=static_cast<Integrity>(f); return r.at_end(); }
std::vector<std::uint8_t> serialize_renew_lease(const RenewLeaseMsg& m) { serde::ByteWriter w; w.u64(m.lease_id.value()); wr_auth(w, m.env); return w.bytes(); }
bool deserialize_renew_lease(std::span<const std::uint8_t> p, RenewLeaseMsg& m) { serde::ByteReader r(p); std::uint64_t id=0; if(!r.u64(id)) return false; if(!rd_auth(r,m.env)) return false; m.lease_id=LeaseId(id); return r.at_end(); }
std::vector<std::uint8_t> serialize_invalidate(const InvalidateMsg& m) { serde::ByteWriter w; w.u8(m.kind); w.u64(m.target); w.u64(m.generation); wr_auth(w, m.env); w.string(m.reason); return w.bytes(); }
bool deserialize_invalidate(std::span<const std::uint8_t> p, InvalidateMsg& m) { serde::ByteReader r(p); if(!r.u8(m.kind)) return false; if(!r.u64(m.target)) return false; if(!r.u64(m.generation)) return false; if(!rd_auth(r,m.env)) return false; if(!r.string(m.reason,1u<<20)) return false; return r.at_end(); }
std::vector<std::uint8_t> serialize_tombstone(const TombstoneRecord& t) {
  serde::ByteWriter w; w.u64(t.tombstone_id.value()); w.u8(static_cast<std::uint8_t>(t.target.kind));
  w.u64(t.target.state.value()); w.u64(t.target.cache.value()); w.u64(t.target.entry.value()); w.u64(t.target.replica.value());
  w.u64(t.target.location.value()); w.u64(t.target.worker_boot.value()); w.u64(t.target.node.value()); w.u64(t.target.device.value());
  w.u64(t.target.backend.value()); w.u64(t.target.compatibility.value()); w.u64(t.target.policy_generation.value());
  w.bytes(t.target.content); w.u64(t.target.generation_floor);
  w.u64(t.epoch.value()); w.u64(t.worker_boot.value()); w.u64(t.authority_generation.value()); w.string(t.reason); w.u64(t.timestamp_ns); w.u64(t.provenance.value());
  return w.bytes();
}
bool deserialize_tombstone(std::span<const std::uint8_t> p, TombstoneRecord& t) {
  serde::ByteReader r(p); std::uint64_t tid=0,e=0,wb=0,ag=0,tn=0,pv=0; std::uint8_t k=0;
  if(!r.u64(tid)) return false; if(!r.u8(k)) return false; std::uint64_t a=0,b=0,c=0,d=0,e2=0,f=0,g=0,h=0,i=0,j=0,l=0;
  if(!r.u64(a)) return false; if(!r.u64(b)) return false; if(!r.u64(c)) return false; if(!r.u64(d)) return false; if(!r.u64(e2)) return false; if(!r.u64(f)) return false;
  if(!r.u64(g)) return false; if(!r.u64(h)) return false; if(!r.u64(i)) return false; if(!r.u64(j)) return false; if(!r.u64(l)) return false;
  std::span<const std::uint8_t> c1; if(!r.bytes(c1,32)) return false; std::memcpy(t.target.content.data(), c1.data(), 32);
  if(!r.u64(t.target.generation_floor)) return false; if(!r.u64(e)) return false; if(!r.u64(wb)) return false; if(!r.u64(ag)) return false; if(!r.string(t.reason,1u<<20)) return false; if(!r.u64(tn)) return false; if(!r.u64(pv)) return false;
  t.tombstone_id=TombstoneId(tid); t.target.kind=static_cast<TombstoneKind>(k); t.target.state=StateId(a); t.target.cache=CacheId(b); t.target.entry=CacheEntryId(c);
  t.target.replica=ReplicaId(d); t.target.location=LocationId(e2); t.target.worker_boot=WorkerBootId(f); t.target.node=NodeId(g); t.target.device=DeviceId(h);
  t.target.backend=StorageBackendId(i); t.target.compatibility=CompatibilityId(j); t.target.policy_generation=PolicyGeneration(l);
  t.epoch=CoordinatorEpoch(e); t.worker_boot=WorkerBootId(wb); t.authority_generation=DirectoryGeneration(ag); t.timestamp_ns=tn; t.provenance=ProvenanceId(pv);
  return r.at_end();
}

}  // namespace protocol
}  // namespace distributedcachedirectory
