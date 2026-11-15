#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  // duplicate cache id
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(1); c.kind = dcd::CacheKind::PROCESS_LOCAL; c.node = t.node; c.worker = t.worker; c.worker_boot = t.boot; c.domain = dcd::MemoryDomain::HOST_MEMORY; c.authority = tst::env(t.dir, t.boot, 1);
    auto r = t.dir.register_cache(c);
    DCD_CHECK(r.ok);  // idempotent re-register of cache under same authority
  }
  // stale epoch on registration
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    dcd::DirectoryRecord rec = tst::make_record(t); rec.state_id = dcd::StateId(2); rec.authority.epoch = dcd::CoordinatorEpoch(0);
    auto r = t.dir.register_entry(rec);
    DCD_CHECK(!r.ok); DCD_CHECK(r.error == dcd::DirectoryError::STALE_EPOCH);
  }
  // stale boot update
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    auto rid = t.dir.records().front().record_id;
    auto e = tst::env(t.dir, dcd::WorkerBootId(999), 2);  // wrong boot
    auto r = t.dir.update_health(rid, dcd::Health::DEGRADED, e);
    DCD_CHECK(!r.ok); DCD_CHECK(r.error == dcd::DirectoryError::STALE_WORKER_BOOT);
  }
  // stale fence update
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    auto rid = t.dir.records().front().record_id;
    auto e = tst::env(t.dir, t.boot, 0);  // fence below stored
    auto r = t.dir.update_reachability(rid, dcd::Reachability::UNREACHABLE, e);
    DCD_CHECK(!r.ok); DCD_CHECK(r.error == dcd::DirectoryError::STALE_AUTHORITY);
  }
  // current metadata with missing bytes is excluded from current query
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    auto rid = t.dir.records().front().record_id;
    auto r = t.dir.update_integrity(rid, dcd::Integrity::MISSING, tst::env(t.dir, t.boot, 2));
    DCD_CHECK(r.ok);
    dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1);
    auto res = t.dir.query(q);
    DCD_CHECK_EQ(res.value.candidates.size(), 0u);
  }
  // stale worker boot never fences a fresh incarnation: a big fence from old boot rejected.
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    auto rid = t.dir.records().front().record_id;
    auto e = tst::env(t.dir, dcd::WorkerBootId(7), 1000000u);  // old boot, huge fence
    auto r = t.dir.update_freshness(rid, dcd::Freshness::CURRENT, e);
    DCD_CHECK(!r.ok);
    DCD_CHECK(r.error == dcd::DirectoryError::STALE_WORKER_BOOT);
  }
  // ---- regression: a lost incarnation holds no authority, and does not fence its successor ----
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    auto rid = t.dir.records().front().record_id;
    DCD_CHECK(t.dir.on_worker_loss(t.boot, "killed").ok);
    // The same WorkerBootId never becomes a live session again.
    dcd::WorkerSession again; again.boot = t.boot; again.worker = t.worker; again.node = t.node; again.tag = "wa";
    auto rs = t.dir.register_worker_session(again);
    DCD_CHECK(!rs.ok);
    DCD_CHECK(rs.error == dcd::DirectoryError::STALE_WORKER_BOOT);
    DCD_CHECK(!t.dir.worker_live(t.boot));
    // Its envelope no longer updates or invalidates anything.
    const auto e = tst::env(t.dir, t.boot, 9);
    auto uf = t.dir.update_freshness(rid, dcd::Freshness::CURRENT, e);
    DCD_CHECK(!uf.ok);
    DCD_CHECK(uf.error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.record(rid).value().freshness == dcd::Freshness::REVALIDATION_REQUIRED);
    DCD_CHECK(!t.dir.update_cache_health(t.cache, dcd::Health::HEALTHY, dcd::Reachability::REACHABLE, e).ok);
    DCD_CHECK(!t.dir.update_cache_capacity(t.cache, 1, 1, e).ok);
    DCD_CHECK(t.dir.invalidate_state(t.state, t.gen, e, "stale").error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.invalidate_location(t.location, e, "stale").error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.invalidate_replica(t.replica, e, "stale").error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.invalidate_cache(t.cache, e, "stale").error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.invalidate_worker_boot(t.boot, e, "stale").error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(t.dir.record(rid).value().lifecycle == dcd::LifecycleState::STALE);
    // The lost worker's copy is not offered until it is revalidated.
    dcd::DirectoryQuery q; q.exact_state = true; q.state = t.state;
    auto res = t.dir.query(q);
    DCD_CHECK_EQ(res.value.candidates.size(), 0u);
    DCD_CHECK(res.value.outcome == dcd::QueryOutcome::STALE_ONLY);
    // A fresh incarnation of the worker re-registers the same cache, location
    // and replica and publishes a newer generation.
    const dcd::WorkerBootId fresh(101);
    DCD_CHECK(tst::register_holder(t.dir, fresh, t.worker, t.node, t.cache, t.entry, t.replica, t.location, "wa"));
    auto rr = t.dir.register_entry(tst::holder_record(t.dir, t.state, dcd::StateGeneration(2), fresh, t.worker, t.node, t.cache, t.entry, t.replica, t.location));
    DCD_CHECK(rr.ok);
    auto res2 = t.dir.query(q);
    DCD_CHECK_EQ(res2.value.candidates.size(), 1u);
    if (res2.value.candidates.size() == 1u) {
      DCD_CHECK(res2.value.candidates[0].record.state_generation == dcd::StateGeneration(2));
      DCD_CHECK(res2.value.candidates[0].record.worker_boot == fresh);
    }
  }
  // While an incarnation is live, another one cannot take over what it holds.
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    DCD_CHECK(!tst::register_holder(t.dir, dcd::WorkerBootId(300), dcd::WorkerId(3), dcd::NodeId(3), t.cache, t.entry, t.replica, t.location, "wc"));
    DCD_CHECK(t.dir.caches().at(t.cache).worker_boot == t.boot);
    DCD_CHECK(t.dir.locations().at(t.location).worker_boot == t.boot);
    DCD_CHECK(t.dir.replicas().at(t.replica).authority.boot == t.boot);
  }
  // ---- regression: a record id is never used twice ----
  {
    auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
    dcd::DirectoryRecord dupid = tst::make_record(t); dupid.state_id = dcd::StateId(2);
    dupid.record_id = t.dir.records().front().record_id;
    auto r = t.dir.register_entry(dupid);
    DCD_CHECK(!r.ok);
    DCD_CHECK(r.error == dcd::DirectoryError::DUPLICATE_IDENTITY);
    DCD_CHECK_EQ(t.dir.records().size(), 1u);
  }
  return dcdtest::summary("test_adversarial");
}
