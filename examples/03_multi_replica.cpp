#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main() {
  std::printf("03_multi_replica\n");
  dcd::Directory dir;
  // worker A
  dcd::WorkerSession a; a.boot = dcd::WorkerBootId(1); a.worker = dcd::WorkerId(1); a.node = dcd::NodeId(1); a.tag = "wa"; dir.register_worker_session(a);
  dcd::CacheDescriptor c1; c1.cache_id = dcd::CacheId(1); c1.kind = dcd::CacheKind::PROCESS_LOCAL; c1.domain = dcd::MemoryDomain::HOST_MEMORY; c1.reachability = dcd::Reachability::REACHABLE; c1.health = dcd::Health::HEALTHY; c1.freshness = dcd::Freshness::CURRENT; c1.authority.epoch = dir.epoch(); c1.authority.boot = a.boot; c1.authority.fence = 1; dir.register_cache(c1);
  dcd::LocationDescriptor l1; l1.location_id = dcd::LocationId(1); l1.domain = dcd::MemoryDomain::HOST_MEMORY; l1.locator.key = "host:/a"; l1.authority = c1.authority; dir.register_location(l1);
  dcd::ReplicaDescriptor r1; r1.replica_id = dcd::ReplicaId(1); r1.location = l1.location_id; r1.cache = c1.cache_id; r1.entry = dcd::CacheEntryId(1); r1.authority = c1.authority; dir.register_replica(r1);
  // worker B
  dcd::WorkerSession b; b.boot = dcd::WorkerBootId(2); b.worker = dcd::WorkerId(2); b.node = dcd::NodeId(2); b.tag = "wb"; dir.register_worker_session(b);
  dcd::CacheDescriptor c2; c2.cache_id = dcd::CacheId(2); c2.kind = dcd::CacheKind::LOCAL_FILESYSTEM_CACHE; c2.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; c2.reachability = dcd::Reachability::REACHABLE; c2.health = dcd::Health::HEALTHY; c2.freshness = dcd::Freshness::CURRENT; c2.authority.epoch = dir.epoch(); c2.authority.boot = b.boot; c2.authority.fence = 1; dir.register_cache(c2);
  dcd::LocationDescriptor l2; l2.location_id = dcd::LocationId(2); l2.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; l2.locator.key = "fs:/b"; l2.authority = c2.authority; dir.register_location(l2);
  dcd::ReplicaDescriptor r2; r2.replica_id = dcd::ReplicaId(2); r2.location = l2.location_id; r2.cache = c2.cache_id; r2.entry = dcd::CacheEntryId(1); r2.authority = c2.authority; dir.register_replica(r2);
  // register same state at both replicas
  auto reg = [&](dcd::ReplicaId rep, dcd::CacheId cache, dcd::LocationId loc, dcd::WorkerBootId boot, dcd::MemoryDomain dom) {
    dcd::DirectoryRecord rec; rec.state_id = dcd::StateId(100); rec.state_generation = dcd::StateGeneration(1); rec.kind = dcd::StateKind::TENSOR_STATE; rec.cache = cache; rec.entry = dcd::CacheEntryId(1); rec.replica = rep; rec.location = loc; rec.domain = dom; rec.logical_bytes = 4096; rec.reachability = dcd::Reachability::REACHABLE; rec.health = dcd::Health::HEALTHY; rec.freshness = dcd::Freshness::CURRENT; rec.integrity = dcd::Integrity::VERIFIED; rec.authority.epoch = dir.epoch(); rec.authority.boot = boot; rec.authority.fence = 1; dir.register_entry(rec);
  };
  reg(r1.replica_id, c1.cache_id, l1.location_id, a.boot, c1.domain);
  reg(r2.replica_id, c2.cache_id, l2.location_id, b.boot, c2.domain);
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(100); q.requester_process_tag = "wa"; q.requester_node = dcd::NodeId(1);
  auto res = dir.query(q);
  std::printf("outcome=%s candidates=%u\n", dcd::to_string(res.value.outcome), static_cast<unsigned>(res.value.candidates.size()));
  for (auto& c : res.value.candidates) std::printf("  replica=%llu domain=%s\n", c.record.replica.value(), dcd::to_string(c.record.domain));
  return 0;
}