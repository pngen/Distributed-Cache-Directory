#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("05_local_vs_remote\n"); dcd::Directory dir;
  dcd::WorkerSession a; a.boot=dcd::WorkerBootId(1); a.worker=dcd::WorkerId(1); a.node=dcd::NodeId(1); a.tag="wa"; dir.register_worker_session(a);
  dcd::WorkerSession b; b.boot=dcd::WorkerBootId(2); b.worker=dcd::WorkerId(2); b.node=dcd::NodeId(2); b.tag="wb"; dir.register_worker_session(b);
  auto mk=[&](dcd::CacheId cid, dcd::LocationId lid, dcd::ReplicaId rid, dcd::WorkerBootId boot, dcd::WorkerId wid, dcd::NodeId nid, dcd::MemoryDomain dom, const char* loc){
    dcd::CacheDescriptor c; c.cache_id=cid; c.kind=dcd::CacheKind::NODE_LOCAL; c.domain=dom; c.node=nid; c.worker=wid; c.worker_boot=boot; c.reachability=dcd::Reachability::REACHABLE; c.health=dcd::Health::HEALTHY; c.freshness=dcd::Freshness::CURRENT; c.authority.epoch=dir.epoch(); c.authority.boot=boot; c.authority.fence=1; dir.register_cache(c);
    dcd::LocationDescriptor l; l.location_id=lid; l.node=nid; l.worker=wid; l.worker_boot=boot; l.domain=dom; l.locator.key=loc; l.authority=c.authority; dir.register_location(l);
    dcd::ReplicaDescriptor r; r.replica_id=rid; r.location=lid; r.cache=cid; r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
    dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=dcd::StateGeneration(1); rec.cache=cid; rec.entry=dcd::CacheEntryId(1); rec.replica=rid; rec.location=lid; rec.node=nid; rec.worker=wid; rec.worker_boot=boot; rec.domain=dom; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec); };
  mk(dcd::CacheId(1),dcd::LocationId(1),dcd::ReplicaId(1),a.boot,a.worker,a.node,dcd::MemoryDomain::HOST_MEMORY,"host:/wa");
  mk(dcd::CacheId(2),dcd::LocationId(2),dcd::ReplicaId(2),b.boot,b.worker,b.node,dcd::MemoryDomain::SYNTHETIC_REMOTE,"synth:/node2");
  dcd::DirectoryQuery q; q.exact_state=true; q.state=dcd::StateId(1); q.requester_process_tag="wa"; q.requester_node=dcd::NodeId(1);
  auto res=dir.query(q);
  std::printf("outcome=%s selected=%s candidates=%u\n", dcd::to_string(res.value.outcome), dcd::to_string(res.value.candidates[0].record.domain), static_cast<unsigned>(res.value.candidates.size()));
  return 0; }
