#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("06_health_ranking\n"); dcd::Directory dir;
  dcd::WorkerSession a; a.boot=dcd::WorkerBootId(1); a.worker=dcd::WorkerId(1); a.node=dcd::NodeId(1); a.tag="wa"; dir.register_worker_session(a);
  dcd::WorkerSession b; b.boot=dcd::WorkerBootId(2); b.worker=dcd::WorkerId(2); b.node=dcd::NodeId(2); b.tag="wb"; dir.register_worker_session(b);
  auto mk=[&](dcd::CacheId cid, dcd::LocationId lid, dcd::ReplicaId rid, dcd::WorkerBootId boot, dcd::WorkerId wid, dcd::NodeId nid, dcd::Health health){
    dcd::CacheDescriptor c; c.cache_id=cid; c.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; c.node=nid; c.worker=wid; c.worker_boot=boot; c.reachability=dcd::Reachability::REACHABLE; c.health=dcd::Health::HEALTHY; c.freshness=dcd::Freshness::CURRENT; c.authority.epoch=dir.epoch(); c.authority.boot=boot; c.authority.fence=1; dir.register_cache(c);
    dcd::LocationDescriptor l; l.location_id=lid; l.node=nid; l.worker=wid; l.worker_boot=boot; l.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key="fs:/"+std::to_string(lid.value()); l.authority=c.authority; dir.register_location(l);
    dcd::ReplicaDescriptor r; r.replica_id=rid; r.location=lid; r.cache=cid; r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
    dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=dcd::StateGeneration(1); rec.cache=cid; rec.entry=dcd::CacheEntryId(1); rec.replica=rid; rec.location=lid; rec.node=nid; rec.worker=wid; rec.worker_boot=boot; rec.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=health; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec); };
  mk(dcd::CacheId(1),dcd::LocationId(1),dcd::ReplicaId(1),a.boot,a.worker,a.node,dcd::Health::DEGRADED);
  mk(dcd::CacheId(2),dcd::LocationId(2),dcd::ReplicaId(2),b.boot,b.worker,b.node,dcd::Health::HEALTHY);
  dcd::DirectoryQuery q; q.exact_state=true; q.state=dcd::StateId(1); q.requester_node=dcd::NodeId(1); q.requester_process_tag="wa";
  auto res=dir.query(q);
  std::printf("outcome=%s best=%s candidates=%u\n", dcd::to_string(res.value.outcome), dcd::to_string(res.value.candidates[0].record.health), static_cast<unsigned>(res.value.candidates.size()));
  dcd::DirectoryQuery qh; qh.exact_state=true; qh.state=dcd::StateId(1); qh.has_health=true; qh.health=dcd::Health::HEALTHY; qh.requester_node=dcd::NodeId(1);
  auto res2=dir.query(qh); std::printf("healthy-only candidates=%u\n", static_cast<unsigned>(res2.value.candidates.size()));
  return 0; }
