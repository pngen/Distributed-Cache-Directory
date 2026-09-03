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
  return dcdtest::summary("test_persistence");
}
