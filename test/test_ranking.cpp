#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  // A remote replica (filesystem on another node).
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1); q.requester_process_tag = "wa"; q.requester_node = dcd::NodeId(1);
  auto res = t.dir.query(q);
  DCD_CHECK(res.ok);
  DCD_CHECK_EQ(res.value.candidates.size(), 2u);
  // The process-local (wa) replica ranks above the remote replica due to SAME_PROCESS factor.
  DCD_CHECK(res.value.candidates[0].selected);
  DCD_CHECK_EQ(res.value.candidates[0].record.replica, dcd::ReplicaId(1));  // same process
  bool found_process = false;
  for (auto& f : res.value.candidates[0].factors) if (f.kind == dcd::RankingFactorKind::SAME_PROCESS && f.value >= 1.0) found_process = true;
  DCD_CHECK(found_process);
  // Degrade the local replica; ranking should now prefer the healthy remote.
  dcd::DirectoryRecordId local = res.value.candidates[0].record.record_id;
  auto up = t.dir.update_integrity(local, dcd::Integrity::CORRUPT, tst::env(t.dir, t.boot, 2));
  DCD_CHECK(up.ok);
  auto res2 = t.dir.query(q);
  DCD_CHECK_EQ(res2.value.candidates.size(), 1u);
  DCD_CHECK(res2.value.outcome == dcd::QueryOutcome::FOUND_REMOTE || res2.value.candidates[0].record.replica == dcd::ReplicaId(2));
  return dcdtest::summary("test_ranking");
}
