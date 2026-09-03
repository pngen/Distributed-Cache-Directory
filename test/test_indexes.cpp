#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  auto v = t.dir.validate_indexes();
  DCD_CHECK(v.ok);
  DCD_CHECK_MSG(v.ok, v.value.message);
  DCD_CHECK_EQ(t.dir.indexes().by_state.size(), 2u);
  DCD_CHECK_EQ(t.dir.indexes().by_state.count(dcd::StateId(1)), 2u);
  DCD_CHECK_EQ(t.dir.indexes().by_cache.count(dcd::CacheId(1)), 1u);
  DCD_CHECK_EQ(t.dir.indexes().by_cache.count(dcd::CacheId(2)), 1u);
  return dcdtest::summary("test_indexes");
}
