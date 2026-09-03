#include "setup.hpp"
#include "test_framework.hpp"
#include "distributedcachedirectory/digest.hpp"
#include <cstdint>
namespace dcd = distributedcachedirectory;
static std::uint64_t seed = 0xC0FFEEull;
static std::uint64_t rnd() { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; }
int main() {
  std::printf("property seed=0x%llx\n", static_cast<unsigned long long>(seed));
  // Build a deterministic synthetic catalog of states with varied quality.
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  for (int i = 0; i < 20; ++i) {
    int gen = 1 + (i % 3);
    dcd::DirectoryRecord rec = tst::make_record(t);
    rec.state_id = dcd::StateId(100 + i); rec.state_generation = dcd::StateGeneration(gen);
    rec.logical_bytes = 512 + rnd() % 4096; rec.content_digest_known = false;
    auto r = t.dir.register_entry(rec);
    DCD_CHECK(r.ok);
  }
  // UNKNOWN freshness/integrity/reachability never become CURRENT/VERIFIED/REACHABLE by themselves.
  dcd::DirectoryRecord ur = tst::make_record(t);
  ur.state_id = dcd::StateId(999); ur.freshness = dcd::Freshness::UNKNOWN; ur.integrity = dcd::Integrity::UNKNOWN; ur.reachability = dcd::Reachability::UNKNOWN; ur.health = dcd::Health::UNKNOWN;
  auto uur = t.dir.register_entry(ur);
  DCD_CHECK(uur.ok);
  DCD_CHECK(uur.value.freshness == dcd::Freshness::UNKNOWN);
  DCD_CHECK(uur.value.integrity == dcd::Integrity::UNKNOWN);
  // Exact lookup deterministic: querying twice yields identical result.
  for (int i = 0; i < 5; ++i) {
    dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1); q.requester_process_tag = "wa";
    auto a = t.dir.query(q); auto b = t.dir.query(q);
    DCD_CHECK_EQ(a.value.candidates.size(), b.value.candidates.size());
    DCD_CHECK(a.value.outcome == b.value.outcome);
  }
  // Current-query result equals canonical scan result.
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1); q.current_only = true;
  auto qq = t.dir.query(q); auto ss = t.dir.query_scan(q);
  DCD_CHECK_EQ(qq.value.candidates.size(), ss.candidates.size());
  for (std::size_t i = 0; i < qq.value.candidates.size(); ++i) DCD_CHECK(qq.value.candidates[i].record.record_id == ss.candidates[i].record.record_id);
  // Indexes agree with canonical records.
  DCD_CHECK(t.dir.validate_indexes().ok);
  // Accounting never negative.
  std::string prob;
  DCD_CHECK(t.dir.accounting().validate(prob));
  // UNKNOWN freshness never becomes CURRENT automatically.
  DCD_CHECK(uur.value.freshness != dcd::Freshness::CURRENT);
  // Tombstone prevents resurrection.
  dcd::TombstoneRecord tb; tb.tombstone_id = dcd::TombstoneId(88); tb.target.kind = dcd::TombstoneKind::STATE; tb.target.state = dcd::StateId(100); tb.target.generation_floor = 1; tb.epoch = t.dir.epoch(); tb.worker_boot = t.boot; tb.authority_generation = dcd::DirectoryGeneration(1); tb.reason = "prop"; tb.timestamp_ns = dcd::wall_clock_ns();
  t.dir.tombstone(tb);
  for (auto& r : t.dir.records()) if (r.state_id == dcd::StateId(100) && r.state_generation.value() <= 1) DCD_CHECK(!r.current);
  return dcdtest::summary("test_property");
}
