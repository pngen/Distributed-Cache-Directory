#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1));
  DCD_CHECK_EQ(t.dir.caches().size(), 1u);
  const auto& c = t.dir.caches().at(dcd::CacheId(1));
  DCD_CHECK(c.kind == dcd::CacheKind::PROCESS_LOCAL);
  DCD_CHECK(c.capacity_known && c.capacity == 1024);
  DCD_CHECK(c.health == dcd::Health::HEALTHY);
  DCD_CHECK(c.reachability == dcd::Reachability::REACHABLE);
  // Controlled cache health/capacity update.
  auto up = t.dir.update_cache_health(dcd::CacheId(1), dcd::Health::DEGRADED, dcd::Reachability::DEGRADED, tst::env(t.dir, t.boot, 2));
  DCD_CHECK(up.ok);
  DCD_CHECK(t.dir.caches().at(dcd::CacheId(1)).health == dcd::Health::DEGRADED);
  auto up2 = t.dir.update_cache_capacity(dcd::CacheId(1), 2048, 1024, tst::env(t.dir, t.boot, 3));
  DCD_CHECK(up2.ok);
  DCD_CHECK(t.dir.caches().at(dcd::CacheId(1)).free_capacity == 1024);
  return dcdtest::summary("test_cache_model");
}
