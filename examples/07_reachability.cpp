#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("07_reachability\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id=dcd::CacheId(1); c.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; c.node=s.node; c.worker=s.worker; c.worker_boot=s.boot; c.reachability=dcd::Reachability::REACHABLE; c.health=dcd::Health::HEALTHY; c.freshness=dcd::Freshness::CURRENT; c.authority.epoch=dir.epoch(); c.authority.boot=s.boot; c.authority.fence=1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id=dcd::LocationId(1); l.node=s.node; l.worker=s.worker; l.worker_boot=s.boot; l.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key="fs:/a"; l.authority=c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id=dcd::ReplicaId(1); r.location=dcd::LocationId(1); r.cache=dcd::CacheId(1); r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
  dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=dcd::StateGeneration(1); rec.cache=dcd::CacheId(1); rec.entry=dcd::CacheEntryId(1); rec.replica=dcd::ReplicaId(1); rec.location=dcd::LocationId(1); rec.node=s.node; rec.worker=s.worker; rec.worker_boot=s.boot; rec.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec);
  dcd::DirectoryRecordId id=dir.records().back().record_id;
  dir.update_reachability(id, dcd::Reachability::UNREACHABLE, {dir.epoch(), s.boot, dcd::DirectoryGeneration(0), 2});
  dcd::DirectoryQuery q; q.exact_state=true; q.state=dcd::StateId(1); q.requester_process_tag="w";
  auto res=dir.query(q); std::printf("outcome=%s candidates=%u\n", dcd::to_string(res.value.outcome), static_cast<unsigned>(res.value.candidates.size()));
  auto ex = dir.explain_reachability(dcd::Reachability::REACHABLE); for (auto& e: ex) std::printf("  %s\n", e.text.c_str());
  return 0; }
