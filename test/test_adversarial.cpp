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
  return dcdtest::summary("test_adversarial");
}
