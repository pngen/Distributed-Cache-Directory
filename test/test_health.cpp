#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  // Degrade replica 1.
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1);
  auto r0 = t.dir.query(q);
  DCD_CHECK_EQ(r0.value.candidates.size(), 2u);
  auto rec1 = r0.value.candidates[0].record;
  auto up = t.dir.update_health(rec1.record_id, dcd::Health::DEGRADED, tst::env(t.dir, t.boot, 2));
  DCD_CHECK(up.ok);
  DCD_CHECK(t.dir.record(rec1.record_id).value().lifecycle == dcd::LifecycleState::DEGRADED);
  // A query requiring HEALTHY excludes the degraded replica.
  dcd::DirectoryQuery qh; qh.exact_state = true; qh.state = dcd::StateId(1); qh.has_health = true; qh.health = dcd::Health::HEALTHY;
  auto rh = t.dir.query(qh);
  DCD_CHECK_EQ(rh.value.candidates.size(), 1u);
  // One unhealthy replica does not invalidate all replicas.
  auto ra = t.dir.query(q);
  DCD_CHECK(ra.value.candidates.size() >= 1u);
  bool saw_replica2 = false;
  for (auto& c : ra.value.candidates) if (c.record.replica == dcd::ReplicaId(2)) saw_replica2 = true;
  DCD_CHECK(saw_replica2);
  return dcdtest::summary("test_health");
}
