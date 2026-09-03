#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1);
  auto r0 = t.dir.query(q);
  DCD_CHECK_EQ(r0.value.candidates.size(), 2u);
  // Mark replica 2 unreachable.
  auto up = t.dir.update_reachability(r0.value.candidates[1].record.record_id, dcd::Reachability::UNREACHABLE, tst::env(t.dir, dcd::WorkerBootId(200), 2));
  DCD_CHECK(up.ok);
  auto r1 = t.dir.query(q);
  DCD_CHECK_EQ(r1.value.candidates.size(), 1u);
  DCD_CHECK(r1.value.candidates[0].record.replica == dcd::ReplicaId(1));
  // Query requiring REACHABLE behaves the same (hard filter).
  dcd::DirectoryQuery qr; qr.exact_state = true; qr.state = dcd::StateId(1); qr.has_reachability = true; qr.reachability = dcd::Reachability::REACHABLE;
  auto r2 = t.dir.query(qr);
  DCD_CHECK_EQ(r2.value.candidates.size(), 1u);
  return dcdtest::summary("test_reachability");
}
