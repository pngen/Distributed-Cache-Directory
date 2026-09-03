#include <distributedcachedirectory/directory.hpp>
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main() {
  std::printf("downstream consumer: DistributedCacheDirectory::distributedcachedirectory\n");
  dcd::Directory dir;
  // worker A (local) and B (remote-ish)
  dcd::WorkerSession a; a.boot=dcd::WorkerBootId(1); a.worker=dcd::WorkerId(1); a.node=dcd::NodeId(1); a.tag="wa"; dir.register_worker_session(a);
  dcd::WorkerSession b; b.boot=dcd::WorkerBootId(2); b.worker=dcd::WorkerId(2); b.node=dcd::NodeId(2); b.tag="wb"; dir.register_worker_session(b);
  auto reg=[&](dcd::CacheId cid, dcd::LocationId lid, dcd::ReplicaId rid, dcd::WorkerBootId boot, dcd::WorkerId wid, dcd::NodeId nid, dcd::MemoryDomain dom, const char* loc){
    dcd::CacheDescriptor c; c.cache_id=cid; c.domain=dom; c.node=nid; c.worker=wid; c.worker_boot=boot; c.reachability=dcd::Reachability::REACHABLE; c.health=dcd::Health::HEALTHY; c.freshness=dcd::Freshness::CURRENT; c.authority.epoch=dir.epoch(); c.authority.boot=boot; c.authority.fence=1; dir.register_cache(c);
    dcd::LocationDescriptor l; l.location_id=lid; l.node=nid; l.worker=wid; l.worker_boot=boot; l.domain=dom; l.locator.key=loc; l.authority=c.authority; dir.register_location(l);
    dcd::ReplicaDescriptor r; r.replica_id=rid; r.location=lid; r.cache=cid; r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
    dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(100); rec.state_generation=dcd::StateGeneration(1); rec.cache=cid; rec.entry=dcd::CacheEntryId(1); rec.replica=rid; rec.location=lid; rec.node=nid; rec.worker=wid; rec.worker_boot=boot; rec.domain=dom; rec.logical_bytes=4096; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec);
  };
  reg(dcd::CacheId(1),dcd::LocationId(1),dcd::ReplicaId(1),a.boot,a.worker,a.node,dcd::MemoryDomain::HOST_MEMORY,"host:/a");
  reg(dcd::CacheId(2),dcd::LocationId(2),dcd::ReplicaId(2),b.boot,b.worker,b.node,dcd::MemoryDomain::LOCAL_FILESYSTEM,"fs:/b");
  std::printf("registered StateId 100 with %zu replicas\n", dir.records().size());
  // query current candidates (wa)
  dcd::DirectoryQuery q; q.exact_state=true; q.state=dcd::StateId(100); q.requester_process_tag="wa"; q.requester_node=dcd::NodeId(1);
  auto r1=dir.query(q);
  std::printf("query outcome=%s candidates=%u\n", dcd::to_string(r1.value.outcome), static_cast<unsigned>(r1.value.candidates.size()));
  // degrade replica 1 -> ranking changes
  dir.update_health(r1.value.candidates[0].record.record_id, dcd::Health::DEGRADED, {dir.epoch(), a.boot, dcd::DirectoryGeneration(0), 2});
  auto r2=dir.query(q);
  std::printf("after degrade candidates=%u best-replica=%llu\n", static_cast<unsigned>(r2.value.candidates.size()), (unsigned long long)r2.value.candidates[0].record.replica.value());
  // invalidate stale location
  dir.invalidate_location(dcd::LocationId(2), {dir.epoch(), b.boot, dcd::DirectoryGeneration(0), 2}, "stale");
  // supersede generation 1 -> 2
  dcd::DirectoryRecord rec2 = dir.records()[1];  // replica 2 record
  (void)rec2;
  dcd::DirectoryRecord g2; g2.state_id=dcd::StateId(100); g2.state_generation=dcd::StateGeneration(2); g2.kind=dcd::StateKind::TENSOR_STATE; g2.cache=dcd::CacheId(1); g2.entry=dcd::CacheEntryId(1); g2.replica=dcd::ReplicaId(1); g2.location=dcd::LocationId(1); g2.node=a.node; g2.worker=a.worker; g2.worker_boot=a.boot; g2.domain=dcd::MemoryDomain::HOST_MEMORY; g2.logical_bytes=4096; g2.reachability=dcd::Reachability::REACHABLE; g2.health=dcd::Health::HEALTHY; g2.freshness=dcd::Freshness::CURRENT; g2.integrity=dcd::Integrity::VERIFIED; g2.authority.epoch=dir.epoch(); g2.authority.boot=a.boot; g2.authority.fence=3;
  auto sup=dir.register_entry(g2);
  std::printf("supersede to gen2: %s current-gen=%llu\n", sup.ok?"ok":sup.error_text.c_str(), (unsigned long long)dir.records().back().state_generation.value());
  // tombstone old generation 1
  dcd::TombstoneRecord t; t.tombstone_id=dcd::TombstoneId(7); t.target.kind=dcd::TombstoneKind::STATE; t.target.state=dcd::StateId(100); t.target.generation_floor=1; t.epoch=dir.epoch(); t.worker_boot=a.boot; t.authority_generation=dcd::DirectoryGeneration(4); t.reason="poisoned"; t.timestamp_ns=dcd::wall_clock_ns(); dir.tombstone(t);
  // historical query
  dcd::DirectoryQuery qh; qh.exact_state=true; qh.state=dcd::StateId(100); qh.current_only=false;
  auto rh=dir.query(qh);
  std::printf("historical query candidates=%u\n", static_cast<unsigned>(rh.value.candidates.size()));
  auto expl = dir.explain_query(qh, rh.value);
  for (auto& e : expl) std::printf("  explain: %s\n", e.text.c_str());
  // invariant + accounting checks
  auto v = dir.validate_indexes();
  std::string prob;
  bool acc = dir.accounting().validate(prob);
  std::printf("indexes valid=%d accounting valid=%d\n", v.ok?1:0, acc?1:0);
  std::printf("consumer finished clean\n");
  return (v.ok && acc) ? 0 : 1;
}
