#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace distributedcachedirectory {
namespace serde {

// Bounded little-endian byte writer. Used to build the canonical/semantic byte
// images that feed SHA-256 and the persistence format.
class ByteWriter {
 public:
  std::vector<std::uint8_t>& bytes() { return buf_; }

  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v) {
    buf_.push_back(static_cast<std::uint8_t>(v));
    buf_.push_back(static_cast<std::uint8_t>(v >> 8u));
  }
  void u32(std::uint32_t v) {
    buf_.push_back(static_cast<std::uint8_t>(v));
    buf_.push_back(static_cast<std::uint8_t>(v >> 8u));
    buf_.push_back(static_cast<std::uint8_t>(v >> 16u));
    buf_.push_back(static_cast<std::uint8_t>(v >> 24u));
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) buf_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  void f64(double v) {
    static_assert(sizeof(double) == 8);
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    u64(bits);
  }
  void bytes(std::span<const std::uint8_t> b) { buf_.insert(buf_.end(), b.begin(), b.end()); }
  void string(const std::string& s) {
    u64(static_cast<std::uint64_t>(s.size()));
    bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
  }

 private:
  std::vector<std::uint8_t> buf_;
};

// Bounded little-endian byte reader. Every read is bounds-checked; a failed
// read returns false and leaves the reader in an unspecified-but-safe state.
class ByteReader {
 public:
  explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

  bool u8(std::uint8_t& v) {
    if (!need(1)) return false;
    v = data_[pos_++];
    return true;
  }
  bool u16(std::uint16_t& v) {
    std::uint8_t a = 0, b = 0;
    if (!u8(a) || !u8(b)) return false;
    v = static_cast<std::uint16_t>(a) | (static_cast<std::uint16_t>(b) << 8u);
    return true;
  }
  bool u32(std::uint32_t& v) {
    std::uint8_t a = 0, b = 0, c = 0, d = 0;
    if (!u8(a) || !u8(b) || !u8(c) || !u8(d)) return false;
    v = static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8u) |
        (static_cast<std::uint32_t>(c) << 16u) | (static_cast<std::uint32_t>(d) << 24u);
    return true;
  }
  bool u64(std::uint64_t& v) {
    std::uint8_t b[8];
    for (int i = 0; i < 8; ++i) if (!u8(b[i])) return false;
    v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<std::uint64_t>(b[i]) << (8 * i));
    return true;
  }
  bool f64(double& v) {
    std::uint64_t bits = 0;
    if (!u64(bits)) return false;
    std::memcpy(&v, &bits, sizeof(v));
    return true;
  }
  bool bytes(std::span<const std::uint8_t>& out, std::size_t n) {
    if (n > data_.size() - pos_) return false;
    out = data_.subspan(pos_, n);
    pos_ += n;
    return true;
  }
  bool string(std::string& s, std::size_t max_len) {
    std::uint64_t n = 0;
    if (!u64(n)) return false;
    if (n > max_len) return false;
    std::span<const std::uint8_t> b;
    if (!bytes(b, static_cast<std::size_t>(n))) return false;
    s.assign(reinterpret_cast<const char*>(b.data()), b.size());
    return true;
  }

  std::size_t remaining() const { return data_.size() - pos_; }
  bool at_end() const { return pos_ == data_.size(); }
  std::size_t tell() const { return pos_; }

 private:
  bool need(std::size_t n) const { return n <= data_.size() - pos_; }
  std::span<const std::uint8_t> data_;
  std::size_t pos_{0};
};

}  // namespace serde
}  // namespace distributedcachedirectory
