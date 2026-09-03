#include "setup.hpp"
#include "test_framework.hpp"

namespace dcd = distributedcachedirectory;

int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(3));
  DCD_CHECK_EQ(t.dir.records().size(), 1u);
  const auto& r = t.dir.records().front();
  DCD_CHECK(r.state_id == dcd::StateId(1));
  DCD_CHECK(r.state_generation == dcd::StateGeneration(3));
  DCD_CHECK(r.replica == dcd::ReplicaId(1));
  DCD_CHECK(r.location == dcd::LocationId(1));
  DCD_CHECK(r.current);
  DCD_CHECK(r.lifecycle == dcd::LifecycleState::AVAILABLE);
  DCD_CHECK_EQ(std::string(dcd::to_string(r.domain)), std::string("HOST_MEMORY"));
  // Identity separation: StateId and CacheId are non-interchangeable types.
  dcd::StateId s(7); dcd::CacheId c(7);
  DCD_CHECK_EQ(s.value(), c.value());  // same numeric value, distinct types
  // Exact duplicate registration rejected.
  dcd::DirectoryRecord dup = tst::make_record(t);
  auto r2 = t.dir.register_entry(dup);
  DCD_CHECK(!r2.ok);
  DCD_CHECK(r2.error == dcd::DirectoryError::DUPLICATE_IDENTITY || r2.error == dcd::DirectoryError::CONFLICTING_DUPLICATE);
  // Generation regression rejected.
  dcd::DirectoryRecord old = tst::make_record(t);
  old.state_generation = dcd::StateGeneration(1);
  auto r3 = t.dir.register_entry(old);
  DCD_CHECK(!r3.ok);
  DCD_CHECK(r3.error == dcd::DirectoryError::STALE_GENERATION);
  return dcdtest::summary("test_record_model");
}
