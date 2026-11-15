#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  // Stale-epoch invalidation rejected.
  auto envOld = tst::env(t.dir, t.boot, 1); envOld.epoch = dcd::CoordinatorEpoch(0);
  auto bad = t.dir.invalidate_state(dcd::StateId(1), dcd::StateGeneration(1), envOld, "stale");
  DCD_CHECK(!bad.ok);
  DCD_CHECK(bad.error == dcd::DirectoryError::STALE_EPOCH);
  // Valid invalidation.
  auto inv = t.dir.invalidate_state(dcd::StateId(1), dcd::StateGeneration(1), tst::env(t.dir, t.boot, 2), "stale by admin");
  DCD_CHECK(inv.ok);
  DCD_CHECK_EQ(inv.value.affected_records, 1u);
  DCD_CHECK(!t.dir.record(t.dir.records().front().record_id).value().current);
  DCD_CHECK(t.dir.record(t.dir.records().front().record_id).value().lifecycle == dcd::LifecycleState::INVALIDATED);
  // Invalidate location.
  auto t2 = tst::register_one(dcd::StateId(2), dcd::StateGeneration(1), "wa");
  auto il = t2.dir.invalidate_location(dcd::LocationId(1), tst::env(t2.dir, t2.boot, 2), "loc gone");
  DCD_CHECK(il.ok);
  // ---- regression: an invalidated record stays INVALIDATED through later updates ----
  {
    auto rid = t.dir.records().front().record_id;
    auto ud = t.dir.update_health(rid, dcd::Health::DEGRADED, tst::env(t.dir, t.boot, 3));
    DCD_CHECK(ud.ok);
    DCD_CHECK(t.dir.record(rid).value().lifecycle == dcd::LifecycleState::INVALIDATED);
    DCD_CHECK(!t.dir.record(rid).value().current);
  }
  return dcdtest::summary("test_invalidation");
}
