#include "distributedcachedirectory/directory.hpp"
#include <algorithm>
#include <vector>
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("13_persistence_recovery\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id=dcd::CacheId(1); c.domain=dcd::MemoryDomain::CUDA_DEVICE; c.authority.epoch=dir.epoch(); c.authority.boot=s.boot; c.authority.fence=1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id=dcd::LocationId(1); l.domain=dcd::MemoryDomain::CUDA_DEVICE; l.locator.key="cuda:0/fresh"; l.authority=c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id=dcd::ReplicaId(1); r.location=dcd::LocationId(1); r.cache=dcd::CacheId(1); r.entry=dcd::CacheEntryId(1); r.authority=c.authority; dir.register_replica(r);
  dcd::DirectoryRecord rec; rec.state_id=dcd::StateId(1); rec.state_generation=dcd::StateGeneration(1); rec.cache=dcd::CacheId(1); rec.entry=dcd::CacheEntryId(1); rec.replica=dcd::ReplicaId(1); rec.location=dcd::LocationId(1); rec.domain=dcd::MemoryDomain::CUDA_DEVICE; rec.logical_bytes=1024; rec.reachability=dcd::Reachability::REACHABLE; rec.health=dcd::Health::HEALTHY; rec.freshness=dcd::Freshness::CURRENT; rec.integrity=dcd::Integrity::VERIFIED; rec.authority=c.authority; dir.register_entry(rec);
  std::vector<std::uint8_t> buf; dir.save_to_buffer(buf);
  dcd::Directory d2; auto rr=d2.recover_from_buffer(buf);
  std::printf("recover: %s\n", rr.ok?"ok":rr.error_text.c_str());
  std::printf("recovered records=%zu caches=%zu cuda-locations=%zu\n", d2.records().size(), d2.caches().size(), (size_t)std::count_if(d2.locations().begin(),d2.locations().end(),[](auto&kv){return kv.second.domain==dcd::MemoryDomain::CUDA_DEVICE;}));
  for (auto& x : d2.records()) std::printf("  gen=%llu freshness=%s (recovery sets CUDA to revalidation)\n", (unsigned long long)x.state_generation.value(), dcd::to_string(x.freshness));
  return 0; }
