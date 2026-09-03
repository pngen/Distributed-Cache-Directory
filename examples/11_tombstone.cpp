#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("11_tombstone\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id=dcd::CacheId(1); c.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; c.authority.epoch=dir.epoch(); c.authority.boot=s.boot; c.authority.fence=1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id=dcd::LocationId(1); l.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key="fs:/a"; l.authority=c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id=dcd::ReplicaId(1); r.location=dcd::LocationId(1); r.cache=dcd::CacheId(1); r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
  dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=dcd::StateGeneration(1); rec.cache=dcd::CacheId(1); rec.entry=dcd::CacheEntryId(1); rec.replica=dcd::ReplicaId(1); rec.location=dcd::LocationId(1); rec.domain=dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec);
  dcd::TombstoneRecord t; t.tombstone_id=dcd::TombstoneId(1); t.target.kind=dcd::TombstoneKind::STATE; t.target.state=dcd::StateId(1); t.target.generation_floor=1; t.epoch=dir.epoch(); t.worker_boot=s.boot; t.authority_generation=dcd::DirectoryGeneration(2); t.reason="poisoned"; t.timestamp_ns=dcd::wall_clock_ns(); dir.tombstone(t);
  std::printf("covered=%d\n", dir.covered_by_tombstone(t.target)?1:0);
  dcd::DirectoryRecord replay = rec; auto r2=dir.register_entry(replay);
  std::printf("resurrection blocked: %s (%s)\n", r2.ok?"NO":"yes", r2.error_text.c_str());
  return 0; }
