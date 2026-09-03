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
  return dcdtest::summary("test_tombstones");
}
