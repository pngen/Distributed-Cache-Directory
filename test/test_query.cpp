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
  // ---- regression: missing evidence and an unmet replica count are never FOUND ----
  {
    auto u = tst::register_one(dcd::StateId(50), dcd::StateGeneration(1), "wa");
    dcd::DirectoryRecord ur = tst::make_record(u); ur.state_id = dcd::StateId(51);
    ur.freshness = dcd::Freshness::UNKNOWN; ur.integrity = dcd::Integrity::UNKNOWN; ur.reachability = dcd::Reachability::UNKNOWN;
    DCD_CHECK(u.dir.register_entry(ur).ok);
    dcd::DirectoryQuery uq; uq.exact_state = true; uq.state = dcd::StateId(51);
    auto ures = u.dir.query(uq);
    DCD_CHECK_EQ(ures.value.candidates.size(), 0u);
    DCD_CHECK(ures.value.outcome == dcd::QueryOutcome::INSUFFICIENT_EVIDENCE);
    // One eligible replica does not satisfy a query that requires two.
    dcd::DirectoryQuery mq; mq.exact_state = true; mq.state = dcd::StateId(50); mq.min_replica_count = 2;
    auto mres = u.dir.query(mq);
    DCD_CHECK_EQ(mres.value.candidates.size(), 0u);
    DCD_CHECK(mres.value.outcome == dcd::QueryOutcome::INSUFFICIENT_EVIDENCE);
    DCD_CHECK(mres.value.rejections.size() == 1u && mres.value.rejections[0].reason == dcd::RejectionReason::NO_REQUIRED_REPLICA_COUNT);
    mq.min_replica_count = 1;
    DCD_CHECK_EQ(u.dir.query(mq).value.candidates.size(), 1u);
  }
  // The two-replica state satisfies a two-replica requirement.
  {
    dcd::DirectoryQuery two; two.exact_state = true; two.state = dcd::StateId(1); two.min_replica_count = 2;
    DCD_CHECK_EQ(t.dir.query(two).value.candidates.size(), 2u);
  }
  return dcdtest::summary("test_query");
}
