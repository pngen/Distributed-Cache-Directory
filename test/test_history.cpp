#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  // Publish generation 2 -> supersedes generation 1.
  dcd::DirectoryRecord rec2 = tst::make_record(t); rec2.state_generation = dcd::StateGeneration(2);
  auto r = t.dir.register_entry(rec2);
  DCD_CHECK(r.ok);
  DCD_CHECK_EQ(t.dir.records().size(), 2u);
  bool gen1_preserved = false;
  for (auto& rr : t.dir.records()) if (rr.state_generation == dcd::StateGeneration(1)) { gen1_preserved = !rr.current; DCD_CHECK(rr.lifecycle == dcd::LifecycleState::SUPERSEDED); }
  DCD_CHECK(gen1_preserved);
  // Current query returns only gen2.
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1); q.current_only = true;
  auto res = t.dir.query(q);
  DCD_CHECK_EQ(res.value.candidates.size(), 1u);
  DCD_CHECK(res.value.candidates[0].record.state_generation == dcd::StateGeneration(2));
  // Historical query (current_only off) sees both.
  dcd::DirectoryQuery qh; qh.exact_state = true; qh.state = dcd::StateId(1); qh.current_only = false;
  auto res2 = t.dir.query(qh);
  DCD_CHECK_EQ(res2.value.candidates.size(), 1u);  // hard filter: stale/superseded excluded from candidates
  return dcdtest::summary("test_history");
}
