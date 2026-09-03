#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace distributedcachedirectory {
namespace transport {

// One-time Winsock initialization (idempotent).
bool winsock_init();

// A TCP stream (client socket or accepted listener socket).
class TcpStream {
 public:
  TcpStream() = default;
  explicit TcpStream(unsigned long long sock);
  ~TcpStream();
  TcpStream(const TcpStream&) = delete;
  TcpStream& operator=(const TcpStream&) = delete;
  TcpStream(TcpStream&& other) noexcept;
  TcpStream& operator=(TcpStream&& other) noexcept;

  bool valid() const;
  bool blocking_read_exact(std::uint8_t* buf, std::size_t n);
  bool blocking_write_all(const std::uint8_t* buf, std::size_t n);
  void close();
  unsigned long long native() const { return sock_; }

 private:
  unsigned long long sock_{~0ull};
};

class TcpListener {
 public:
  bool bind(const std::string& host, unsigned short port, std::string& err);
  std::optional<TcpStream> accept(std::string& err);
  void close();
  unsigned short port() const { return port_; }
  bool valid() const { return sock_ != ~0ull; }
 private:
  unsigned long long sock_{~0ull};
  unsigned short port_{0};
};

// A framed frame channel over a TcpStream. Serializes writes per channel.
class FrameChannel {
 public:
  FrameChannel() = default;
  FrameChannel(TcpStream&& stream) : stream_(std::move(stream)) {}

  TcpStream& stream() { return stream_; }
  bool valid() const { return stream_.valid(); }

  bool send(std::uint8_t kind, const std::uint8_t* payload, std::uint32_t len, std::string& err);
  bool send(std::uint8_t kind, const std::vector<std::uint8_t>& payload, std::string& err);
  bool receive(std::uint8_t& kind, std::vector<std::uint8_t>& payload, std::string& err);
  bool receive_timeout(std::uint8_t& kind, std::vector<std::uint8_t>& payload, std::string& err, int timeout_ms);
  void close() { stream_.close(); }

 private:
  TcpStream stream_;
  std::mutex write_mutex_;
};

}  // namespace transport
}  // namespace distributedcachedirectory
