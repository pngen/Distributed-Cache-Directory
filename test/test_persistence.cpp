#include "setup.hpp"
#include "test_framework.hpp"
#include "distributedcachedirectory/digest.hpp"
#include "distributedcachedirectory/persistence.hpp"
#include <span>
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  tst::register_extra(t, dcd::WorkerBootId(200), dcd::WorkerId(2), dcd::NodeId(2), dcd::CacheId(2), dcd::CacheEntryId(2), dcd::ReplicaId(2), dcd::LocationId(2), "wb");
  std::vector<std::uint8_t> buf;
  auto save = t.dir.save_to_buffer(buf);
  DCD_CHECK(save.ok);
  DCD_CHECK(buf.size() > 44);
  // Deterministic round-trip: serializing again yields the same bytes.
  std::vector<std::uint8_t> buf2; t.dir.save_to_buffer(buf2);
  DCD_CHECK(buf == buf2);
  // Recover into a fresh directory.
  dcd::Directory d2;
  auto rec = d2.recover_from_buffer(buf);
  DCD_CHECK_MSG(rec.ok, rec.ok ? rec.value.message : rec.error_text);
  DCD_CHECK_EQ(d2.records().size(), 2u);
  DCD_CHECK_EQ(d2.caches().size(), 2u);
  DCD_CHECK_EQ(d2.locations().size(), 2u);
  DCD_CHECK(d2.validate_indexes().ok);
  // Recovery clears live worker authority and flags process-local/CUDA revalidation.
  DCD_CHECK_EQ(d2.workers().size(), 0u);
  for (auto& r : d2.records()) DCD_CHECK(r.freshness == dcd::Freshness::REVALIDATION_REQUIRED);
  // Corruption: bad magic.
  { auto b = buf; b[0] ^= 0xFF; auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK_MSG(r.error_text.find("magic") != std::string::npos, r.error_text); }
  // Corruption: unsupported version.
  { auto b = buf; b[4] = 99; auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("version") != std::string::npos); }
  // Corruption: truncation (short buffer).
  { std::vector<std::uint8_t> b(buf.begin(), buf.begin()+20); auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("truncation") != std::string::npos); }
  // Corruption: checksum mismatch (flip a payload byte).
  { auto b = buf; b[50] ^= 0x01; auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("checksum") != std::string::npos); }
  // Corruption: semantic digest mismatch (flip a sha header byte).
  { auto b = buf; b[12] ^= 0x01; auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("digest") != std::string::npos || r.error_text.find("checksum") != std::string::npos); }
  // Corruption: impossible count (recompute the header so CRC/sha agree).
  { auto b = buf; for (int i=44;i<52;++i) b[i]=0xFF;
    { auto body = std::span<const std::uint8_t>(b.data()+44, b.size()-44); std::uint32_t c=dcd::digest::crc32(body); auto s=dcd::digest::sha256(body); b[8]=c&0xFF;b[9]=(c>>8)&0xFF;b[10]=(c>>16)&0xFF;b[11]=(c>>24)&0xFF; for(int i=0;i<32;++i) b[12+i]=s[i]; }
    auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("count") != std::string::npos); }
  // Corruption: trailing garbage (recompute header).
  { auto b = buf; b.push_back(0xDE); b.push_back(0xAD);
    { auto body = std::span<const std::uint8_t>(b.data()+44, b.size()-44); std::uint32_t c=dcd::digest::crc32(body); auto s=dcd::digest::sha256(body); b[8]=c&0xFF;b[9]=(c>>8)&0xFF;b[10]=(c>>16)&0xFF;b[11]=(c>>24)&0xFF; for(int i=0;i<32;++i) b[12+i]=s[i]; }
    auto r = d2.recover_from_buffer(b); DCD_CHECK(!r.ok); DCD_CHECK(r.error_text.find("trailing") != std::string::npos); }
  // Save to file and recover from file.
  std::string path = "persist_test.bin";
  t.dir.save(path);
  dcd::Directory d3;
  auto r3 = d3.recover(path);
  DCD_CHECK(r3.ok);
  DCD_CHECK_EQ(d3.records().size(), 2u);
  std::remove(path.c_str());
  // ---- regression: recovered copies are not available until revalidated ----
  {
    dcd::DirectoryQuery rq; rq.exact_state = true; rq.state = dcd::StateId(1);
    auto rres = d3.query(rq);
    DCD_CHECK_EQ(rres.value.candidates.size(), 0u);
    DCD_CHECK(rres.value.outcome == dcd::QueryOutcome::STALE_ONLY);
  }
  // ---- regression: record ids named by recovered state are not assigned again ----
  {
    DCD_CHECK(tst::register_holder(d3, dcd::WorkerBootId(300), dcd::WorkerId(1), dcd::NodeId(1), dcd::CacheId(1), dcd::CacheEntryId(1), dcd::ReplicaId(1), dcd::LocationId(1), "wa"));
    auto nr = d3.register_entry(tst::holder_record(d3, dcd::StateId(77), dcd::StateGeneration(1), dcd::WorkerBootId(300), dcd::WorkerId(1), dcd::NodeId(1), dcd::CacheId(1), dcd::CacheEntryId(1), dcd::ReplicaId(1), dcd::LocationId(1)));
    DCD_CHECK_MSG(nr.ok, nr.error_text);
    for (const auto& r : t.dir.records()) DCD_CHECK(nr.value.record_id != r.record_id);
    // The directory can be saved and recovered again.
    std::vector<std::uint8_t> again; d3.save_to_buffer(again);
    dcd::Directory d4;
    auto r4 = d4.recover_from_buffer(again);
    DCD_CHECK_MSG(r4.ok, r4.error_text);
    DCD_CHECK_EQ(d4.records().size(), 3u);
  }
  // ---- regression: recovery keeps the lifecycle of historical records ----
  {
    auto h = tst::register_one(dcd::StateId(9), dcd::StateGeneration(1), "wa");
    dcd::DirectoryRecord g2 = tst::make_record(h); g2.state_generation = dcd::StateGeneration(2);
    DCD_CHECK(h.dir.register_entry(g2).ok);  // generation 1 is now SUPERSEDED.
    dcd::TombstoneRecord tb; tb.tombstone_id = dcd::TombstoneId(1); tb.target.kind = dcd::TombstoneKind::STATE; tb.target.state = dcd::StateId(9);
    tb.target.generation_floor = 2; tb.epoch = h.dir.epoch(); tb.worker_boot = h.boot; tb.reason = "poisoned"; tb.timestamp_ns = dcd::wall_clock_ns();
    DCD_CHECK(h.dir.tombstone(tb).ok);        // both generations are now TOMBSTONED.
    dcd::DirectoryRecord g3 = tst::make_record(h); g3.state_generation = dcd::StateGeneration(3);
    DCD_CHECK(h.dir.register_entry(g3).ok);  // generation 3 is current.
    std::vector<std::uint8_t> hb; h.dir.save_to_buffer(hb);
    dcd::Directory hr;
    DCD_CHECK(hr.recover_from_buffer(hb).ok);
    DCD_CHECK_EQ(hr.records().size(), 3u);
    for (const auto& r : hr.records()) {
      if (r.state_generation.value() <= 2) { DCD_CHECK(!r.current); DCD_CHECK(r.lifecycle == dcd::LifecycleState::TOMBSTONED); }
      else { DCD_CHECK(r.current); DCD_CHECK(r.lifecycle == dcd::LifecycleState::STALE); }
    }
  }
  // ---- regression: the encoding does not depend on registration order ----
  {
    const auto build = [](bool reversed) {
      dcd::Directory d;
      dcd::WorkerSession s; s.boot = dcd::WorkerBootId(1); s.worker = dcd::WorkerId(1); s.node = dcd::NodeId(1); s.tag = "w";
      d.register_worker_session(s);
      for (std::uint64_t i = 1; i <= 8; ++i) {
        dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(reversed ? 9 - i : i); c.kind = dcd::CacheKind::PROCESS_LOCAL; c.node = s.node; c.worker = s.worker; c.worker_boot = s.boot;
        c.domain = dcd::MemoryDomain::HOST_MEMORY; c.authority = tst::env(d, s.boot, 1);
        d.register_cache(c);
      }
      std::vector<std::uint8_t> out; d.save_to_buffer(out);
      return out;
    };
    const auto a = build(false);
    const auto b = build(true);
    DCD_CHECK(a.size() > 44);
    DCD_CHECK(a == b);
  }
  return dcdtest::summary("test_persistence");
}
