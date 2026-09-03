#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("12_historical_query\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id=dcd::CacheId(1); c.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; c.authority.epoch=dir.epoch(); c.authority.boot=s.boot; c.authority.fence=1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id=dcd::LocationId(1); l.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key="fs:/a"; l.authority=c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id=dcd::ReplicaId(1); r.location=dcd::LocationId(1); r.cache=dcd::CacheId(1); r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
  auto reg=[&](dcd::StateGeneration g){ dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=g; rec.cache=dcd::CacheId(1); rec.entry=dcd::CacheEntryId(1); rec.replica=dcd::ReplicaId(1); rec.location=dcd::LocationId(1); rec.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec); };
  reg(dcd::StateGeneration(1)); reg(dcd::StateGeneration(2));
  dcd::DirectoryQuery q; q.exact_state=true; q.state=dcd::StateId(1);
  auto res=dir.query(q); std::printf("current query outcome=%s candidates=%u\n", dcd::to_string(res.value.outcome), static_cast<unsigned>(res.value.candidates.size()));
  // Historical records remain inspectable.
  for (auto& rr : dir.records()) if(!rr.current) std::printf("  historical gen=%llu lifecycle=%s\n", (unsigned long long)rr.state_generation.value(), dcd::to_string(rr.lifecycle));
  return 0; }
