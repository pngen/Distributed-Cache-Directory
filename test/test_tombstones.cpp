#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  // A worker cannot republish a record covered by a current tombstone.
  dcd::TombstoneRecord tb; tb.tombstone_id = dcd::TombstoneId(1); tb.target.kind = dcd::TombstoneKind::STATE; tb.target.state = dcd::StateId(1);
  tb.target.generation_floor = 1; tb.epoch = t.dir.epoch(); tb.worker_boot = t.boot; tb.authority_generation = dcd::DirectoryGeneration(5); tb.reason = "poisoned"; tb.timestamp_ns = dcd::wall_clock_ns();
  auto tr = t.dir.tombstone(tb);
  DCD_CHECK(tr.ok);
  DCD_CHECK(t.dir.covered_by_tombstone(tb.target));
  // Any record at generation <= 1 is now tombstoned / non-current.
  for (auto& r : t.dir.records()) if (r.state_id == dcd::StateId(1) && r.state_generation.value() <= 1) DCD_CHECK(!r.current);
  // Stale registration of the tombstoned generation is rejected.
  dcd::DirectoryRecord rec = tst::make_record(t);
  auto st = t.dir.register_entry(rec);
  DCD_CHECK(!st.ok);
  DCD_CHECK(st.error == dcd::DirectoryError::TOMBSTONED);
  // A fresh generation can still be registered after a tombstone of an older gen.
  dcd::DirectoryRecord rec2 = tst::make_record(t); rec2.state_generation = dcd::StateGeneration(2); auto okf = t.dir.register_entry(rec2);
  DCD_CHECK(okf.ok);
  DCD_CHECK(okf.value.state_generation == dcd::StateGeneration(2));
  DCD_CHECK(!t.dir.covered_by_tombstone(dcd::TombstoneTarget{tb.target.kind, dcd::StateId(1), {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, 2}));
  // ---- regression: a rejected update leaves the record unchanged ----
  {
    auto tid = t.dir.records().front().record_id;  // the tombstoned generation-1 record.
    const auto before = t.dir.record(tid).value();
    auto up = t.dir.update_health(tid, dcd::Health::UNAVAILABLE, tst::env(t.dir, t.boot, 5));
    DCD_CHECK(!up.ok);
    DCD_CHECK(up.error == dcd::DirectoryError::INVALID_STATE_TRANSITION);
    DCD_CHECK(t.dir.record(tid).value().health == before.health);
    DCD_CHECK(t.dir.record(tid).value().lifecycle == dcd::LifecycleState::TOMBSTONED);
  }
  // ---- regression: tombstones of other kinds are enforced, not only recorded ----
  {
    auto t2 = tst::register_one(dcd::StateId(5), dcd::StateGeneration(1), "wa");
    dcd::TombstoneRecord rt; rt.tombstone_id = dcd::TombstoneId(2); rt.target.kind = dcd::TombstoneKind::REPLICA; rt.target.replica = t2.replica;
    rt.target.generation_floor = 0; rt.epoch = t2.dir.epoch(); rt.worker_boot = t2.boot; rt.reason = "replica retired"; rt.timestamp_ns = dcd::wall_clock_ns();
    DCD_CHECK(t2.dir.tombstone(rt).ok);
    DCD_CHECK(t2.dir.covered_by_tombstone(rt.target));
    // The replica's current record is tombstoned...
    DCD_CHECK(!t2.dir.records().front().current);
    DCD_CHECK(t2.dir.records().front().lifecycle == dcd::LifecycleState::TOMBSTONED);
    // ...and that replica generation cannot be republished, even for a newer state generation.
    dcd::DirectoryRecord again = tst::make_record(t2); again.state_generation = dcd::StateGeneration(2);
    auto ra = t2.dir.register_entry(again);
    DCD_CHECK(!ra.ok);
    DCD_CHECK(ra.error == dcd::DirectoryError::TOMBSTONED);
    // A newer replica generation is above the tombstone's floor.
    again.replica_generation = dcd::ReplicaGeneration(1);
    DCD_CHECK(t2.dir.register_entry(again).ok);
  }
  {
    // A worker-boot tombstone covers every record of that incarnation.
    auto t3 = tst::register_one(dcd::StateId(6), dcd::StateGeneration(1), "wa");
    dcd::TombstoneRecord wt; wt.tombstone_id = dcd::TombstoneId(3); wt.target.kind = dcd::TombstoneKind::WORKER_BOOT; wt.target.worker_boot = t3.boot;
    wt.epoch = t3.dir.epoch(); wt.worker_boot = t3.boot; wt.reason = "incarnation retired"; wt.timestamp_ns = dcd::wall_clock_ns();
    DCD_CHECK(t3.dir.tombstone(wt).ok);
    DCD_CHECK(!t3.dir.records().front().current);
    dcd::DirectoryRecord more = tst::make_record(t3); more.state_id = dcd::StateId(7);
    auto rm = t3.dir.register_entry(more);
    DCD_CHECK(!rm.ok);
    DCD_CHECK(rm.error == dcd::DirectoryError::TOMBSTONED);
  }
  return dcdtest::summary("test_tombstones");
}
