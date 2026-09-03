#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main() {
  std::printf("01_cache_identity\n");
  // Caches are explicit, versioned descriptors; they are not implicit maps.
  dcd::CacheId id(7);
  dcd::CacheDescriptor c;
  c.cache_id = id;
  c.kind = dcd::CacheKind::PROCESS_LOCAL;
  c.domain = dcd::MemoryDomain::HOST_MEMORY;
  c.capacity = 1u << 20; c.capacity_known = true;
  c.reachability = dcd::Reachability::REACHABLE;
  c.health = dcd::Health::HEALTHY;
  c.freshness = dcd::Freshness::CURRENT;
  std::printf("cache %llu kind=%s domain=%s capacity=%u reachability=%s\n",
    c.cache_id.value(), dcd::to_string(c.kind), dcd::to_string(c.domain),
    static_cast<unsigned>(c.capacity), dcd::to_string(c.reachability));
  dcd::Directory dir;
  dcd::WorkerSession s; s.boot = dcd::WorkerBootId(1); s.worker = dcd::WorkerId(1); s.node = dcd::NodeId(1); s.tag = "w";
  dir.register_worker_session(s);
  c.worker_boot = s.boot; c.node = s.node; c.worker = s.worker; c.authority.epoch = dir.epoch(); c.authority.boot = s.boot; c.authority.fence = 1;
  auto r = dir.register_cache(c);
  std::printf("registered cache: %s\n", r.ok ? "ok" : r.error_text.c_str());
  std::printf("cache count=%zu\n", dir.caches().size());
  return 0;
}
