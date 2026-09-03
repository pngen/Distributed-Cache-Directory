#include "distributedcachedirectory/directory.hpp"
#include <algorithm>
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("10_supersession\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id=dcd::CacheId(1); c.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; c.authority.epoch=dir.epoch(); c.authority.boot=s.boot; c.authority.fence=1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id=dcd::LocationId(1); l.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key="fs:/a"; l.authority=c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id=dcd::ReplicaId(1); r.location=dcd::LocationId(1); r.cache=dcd::CacheId(1); r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
  auto reg=[&](dcd::StateGeneration g){ dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=g; rec.cache=dcd::CacheId(1); rec.entry=dcd::CacheEntryId(1); rec.replica=dcd::ReplicaId(1); rec.location=dcd::LocationId(1); rec.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec); };
  reg(dcd::StateGeneration(1)); reg(dcd::StateGeneration(2));
  std::printf("records=%zu current=%zu historical=%zu\n", dir.records().size(), (size_t)std::count_if(dir.records().begin(),dir.records().end(),[](auto&r){return r.current;}), (size_t)std::count_if(dir.records().begin(),dir.records().end(),[](auto&r){return !r.current;}));
  for (auto& rr : dir.records()) std::printf("  gen=%llu current=%d lifecycle=%s\n", (unsigned long long)rr.state_generation.value(), rr.current?1:0, dcd::to_string(rr.lifecycle));
  return 0; }
