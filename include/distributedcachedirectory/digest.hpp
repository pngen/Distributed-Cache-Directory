#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace distributedcachedirectory {
namespace digest {

using Sha256 = std::array<std::uint8_t, 32>;

// CRC-32 (IEEE 802.3 / zlib polynomial 0xEDB88320).
inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t seed = 0xFFFFFFFFu) {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
      }
      t[i] = c;
    }
    return t;
  }();
  std::uint32_t c = seed;
  for (std::size_t i = 0; i < len; ++i) {
    c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8u);
  }
  return c ^ 0xFFFFFFFFu;
}

inline std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
  return crc32(bytes.data(), bytes.size());
}

// Canonical SHA-256 (FIPS 180-4). Conforms to the public NIST vector for the
// empty input (e3b0c442...).
inline Sha256 sha256(std::span<const std::uint8_t> bytes) {
  Sha256 out{};
  std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  // Zero-padded message with a hard-coded max length cap for bounded input.
  const std::size_t blen = (bytes.size() + 8 + 1 + 63) / 64 * 64;
  std::vector<std::uint8_t> msg(bytes.begin(), bytes.end());
  msg.push_back(0x80u);
  const std::uint64_t bit_len = static_cast<std::uint64_t>(bytes.size()) * 8u;
  while (msg.size() < blen) msg.push_back(0u);
  for (int i = 0; i < 8; ++i) {
    msg[blen - 8 + i] = static_cast<std::uint8_t>(bit_len >> (56 - 8 * i));
  }
  static const std::uint32_t k[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
      0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
      0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
      0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
      0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
      0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
      0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
  for (std::size_t oi = 0; oi < blen; oi += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(msg[oi + 4 * i]) << 24u) |
             (static_cast<std::uint32_t>(msg[oi + 4 * i + 1]) << 16u) |
             (static_cast<std::uint32_t>(msg[oi + 4 * i + 2]) << 8u) |
             (static_cast<std::uint32_t>(msg[oi + 4 * i + 3]));
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = ((w[i - 15] >> 7u) | (w[i - 15] << 25u)) ^
                               ((w[i - 15] >> 18u) | (w[i - 15] << 14u)) ^ (w[i - 15] >> 3u);
      const std::uint32_t s1 = ((w[i - 2] >> 17u) | (w[i - 2] << 15u)) ^
                               ((w[i - 2] >> 19u) | (w[i - 2] << 13u)) ^ (w[i - 2] >> 10u);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t s1 = ((e >> 6u) | (e << 26u)) ^ ((e >> 11u) | (e << 21u)) ^
                               ((e >> 25u) | (e << 7u));
      const std::uint32_t ch = (e & f) ^ (~e & g);
      const std::uint32_t t1 = hh + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = ((a >> 2u) | (a << 30u)) ^ ((a >> 13u) | (a << 19u)) ^
                               ((a >> 22u) | (a << 10u));
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = s0 + maj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  for (int i = 0; i < 8; ++i) {
    out[4 * i] = static_cast<std::uint8_t>(h[i] >> 24u);
    out[4 * i + 1] = static_cast<std::uint8_t>(h[i] >> 16u);
    out[4 * i + 2] = static_cast<std::uint8_t>(h[i] >> 8u);
    out[4 * i + 3] = static_cast<std::uint8_t>(h[i]);
  }
  return out;
}

inline Sha256 sha256(const void* data, std::size_t len) {
  return sha256(std::span<const std::uint8_t>(static_cast<const std::uint8_t*>(data), len));
}

// Constant-time byte equality of two digests.
inline bool digest_equal(const Sha256& a, const Sha256& b) {
  std::uint8_t diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

inline std::string hex(const Sha256& d) {
  static const char* hexc = "0123456789abcdef";
  std::string s;
  s.reserve(64);
  for (std::uint8_t b : d) {
    s.push_back(hexc[b >> 4u]);
    s.push_back(hexc[b & 0x0Fu]);
  }
  return s;
}

inline std::string hex32(std::uint32_t v) {
  static const char* hexc = "0123456789abcdef";
  std::string s;
  s.reserve(8);
  for (int i = 28; i >= 0; i -= 4) s.push_back(hexc[(v >> i) & 0xFu]);
  return s;
}

}  // namespace digest
}  // namespace distributedcachedirectory
