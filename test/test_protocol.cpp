#include "distributedcachedirectory/protocol.hpp"
#include "setup.hpp"
#include "test_framework.hpp"
#include "distributedcachedirectory/digest.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  using namespace dcd::protocol;
  std::vector<std::uint8_t> payload = {1,2,3,4,5};
  auto frame = encode_frame(MessageKind::QUERY, payload);
  Frame f; std::string err; std::size_t consumed = 0;
  DCD_CHECK(decode_frame(frame, f, err, consumed));
  DCD_CHECK(f.kind == MessageKind::QUERY);
  DCD_CHECK(f.payload == payload);
  DCD_CHECK_EQ(consumed, frame.size());
  // Bad magic.
  { auto b = frame; b[0] ^= 0x11; Frame f2; std::string e2; std::size_t c2; DCD_CHECK(!decode_frame(b, f2, e2, c2)); DCD_CHECK(e2.find("magic") != std::string::npos); }
  // Unsupported version.
  { auto b = frame; b[4] = 9; Frame f2; std::string e2; std::size_t c2; DCD_CHECK(!decode_frame(b, f2, e2, c2)); DCD_CHECK(e2.find("version") != std::string::npos); }
  // Checksum mismatch.
  { auto b = frame; b.back() ^= 1; Frame f2; std::string e2; std::size_t c2; DCD_CHECK(!decode_frame(b, f2, e2, c2)); DCD_CHECK(e2.find("checksum") != std::string::npos); }
  // Truncation.
  { std::vector<std::uint8_t> b(frame.begin(), frame.begin()+8); Frame f2; std::string e2; std::size_t c2; DCD_CHECK(!decode_frame(b, f2, e2, c2)); DCD_CHECK(e2.find("truncation") != std::string::npos); }
  // Oversized payload.
  { auto b = frame; b[9]=0xFF; b[10]=0xFF; b[11]=0xFF; b[12]=0x7F; Frame f2; std::string e2; std::size_t c2; DCD_CHECK(!decode_frame(b, f2, e2, c2)); DCD_CHECK(e2.find("oversized") != std::string::npos || e2.find("truncation") != std::string::npos); }
  // Ack / error round-trips.
  dcd::AckResult a; a.ok = true; a.message = "hello";
  auto ap = serialize_ack(a); dcd::AckResult a2; DCD_CHECK(deserialize_ack(ap, a2)); DCD_CHECK(a2.ok && a2.message == "hello");
  auto ep = serialize_error(dcd::DirectoryError::STALE_EPOCH, "old"); dcd::DirectoryError e; std::string m;
  DCD_CHECK(deserialize_error(ep, e, m)); DCD_CHECK(e == dcd::DirectoryError::STALE_EPOCH);
  // Record round-trip (use a committed record so all fields are populated).
  auto t = tst::register_one(dcd::StateId(2), dcd::StateGeneration(3));
  const auto& rec0 = t.dir.records().front();
  auto rp = serialize_record(rec0); dcd::DirectoryRecord r2;
  DCD_CHECK(deserialize_record(rp, r2));
  DCD_CHECK(r2.state_id == rec0.state_id && r2.replica == rec0.replica && r2.domain == rec0.domain && r2.state_generation == rec0.state_generation && r2.logical_bytes == rec0.logical_bytes);
  return dcdtest::summary("test_protocol");
}