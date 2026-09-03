#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main() {
  std::printf("02_state_registration\n");
  dcd::Directory dir;
  dcd::WorkerSession s; s.boot = dcd::WorkerBootId(1); s.worker = dcd::WorkerId(1); s.node = dcd::NodeId(1); s.tag = "w";
  dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(1); c.kind = dcd::CacheKind::PROCESS_LOCAL; c.domain = dcd::MemoryDomain::HOST_MEMORY; c.node = s.node; c.worker = s.worker; c.worker_boot = s.boot; c.reachability = dcd::Reachability::REACHABLE; c.health = dcd::Health::HEALTHY; c.freshness = dcd::Freshness::CURRENT; c.authority.epoch = dir.epoch(); c.authority.boot = s.boot; c.authority.fence = 1; dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id = dcd::LocationId(1); l.node = s.node; l.worker = s.worker; l.worker_boot = s.boot; l.domain = dcd::MemoryDomain::HOST_MEMORY; l.locator.key = "host:/tmp/state"; l.authority = c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor rep; rep.replica_id = dcd::ReplicaId(1); rep.location = l.location_id; rep.cache = c.cache_id; rep.entry = dcd::CacheEntryId(1); rep.authority = c.authority; dir.register_replica(rep);
  dcd::DirectoryRecord rec; rec.state_id = dcd::StateId(100); rec.state_generation = dcd::StateGeneration(1); rec.kind = dcd::StateKind::TENSOR_STATE; rec.cache = c.cache_id; rec.entry = rep.entry; rec.replica = rep.replica_id; rec.location = l.location_id; rec.node = s.node; rec.worker = s.worker; rec.worker_boot = s.boot; rec.domain = dcd::MemoryDomain::HOST_MEMORY; rec.logical_bytes = 4096; rec.reachability = dcd::Reachability::REACHABLE; rec.health = dcd::Health::HEALTHY; rec.freshness = dcd::Freshness::CURRENT; rec.integrity = dcd::Integrity::VERIFIED; rec.authority = c.authority;
  auto r = dir.register_entry(rec);
  std::printf("register entry: %s\n", r.ok ? "ok" : r.error_text.c_str());
  std::printf("records=%zu current-state-gen=%llu\n", dir.records().size(), (unsigned long long)dir.records().front().state_generation.value());
  return 0;
}
