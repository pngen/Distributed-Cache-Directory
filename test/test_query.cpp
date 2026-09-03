#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1);
  auto res = t.dir.query(q);
  DCD_CHECK(res.ok);
  DCD_CHECK(res.value.outcome == dcd::QueryOutcome::FOUND_MULTIPLE);
  DCD_CHECK_EQ(res.value.candidates.size(), 2u);
  // Exact digest mismatch -> none.
  dcd::DirectoryQuery bad; bad.exact_state = true; bad.state = dcd::StateId(1); bad.require_content_digest = true; bad.content_digest = dcd::digest::Sha256{};
  auto r2 = t.dir.query(bad);
  DCD_CHECK_EQ(r2.value.candidates.size(), 0u);
  DCD_CHECK(r2.value.outcome == dcd::QueryOutcome::NOT_FOUND || r2.value.outcome == dcd::QueryOutcome::INSUFFICIENT_EVIDENCE);
  // Nonexistent state -> NOT_FOUND.
  dcd::DirectoryQuery nf; nf.exact_state = true; nf.state = dcd::StateId(999);
  auto r3 = t.dir.query(nf);
  DCD_CHECK(r3.value.outcome == dcd::QueryOutcome::NOT_FOUND);
  return dcdtest::summary("test_query");
}
