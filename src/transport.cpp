#include "distributedcachedirectory/transport.hpp"
#include "distributedcachedirectory/digest.hpp"
#include "distributedcachedirectory/protocol.hpp"
#include <cstring>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define DCD_WSA
#endif

namespace distributedcachedirectory {
namespace transport {

bool winsock_init() {
#ifdef DCD_WSA
  static bool done = [] { WSADATA d; return ::WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
  return done;
#else
  return true;
#endif
}

TcpStream::TcpStream(unsigned long long sock) : sock_(sock) {}
TcpStream::~TcpStream() { close(); }
TcpStream::TcpStream(TcpStream&& other) noexcept : sock_(other.sock_) { other.sock_ = ~0ull; }
TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
  if (this != &other) { close(); sock_ = other.sock_; other.sock_ = ~0ull; } return *this;
}
bool TcpStream::valid() const { return sock_ != ~0ull; }
void TcpStream::close() {
  if (sock_ != ~0ull) {
#ifdef DCD_WSA
    ::shutdown((SOCKET)sock_, SD_BOTH); ::closesocket((SOCKET)sock_);
#else
    ::close(sock_);
#endif
    sock_ = ~0ull;
  }
}
bool TcpStream::blocking_read_exact(std::uint8_t* buf, std::size_t n) {
  std::size_t got = 0;
  while (got < n) {
#ifdef DCD_WSA
    int r = ::recv((SOCKET)sock_, reinterpret_cast<char*>(buf + got), static_cast<int>(n - got), 0);
#else
    ssize_t r = ::recv(sock_, buf + got, n - got, 0);
#endif
    if (r <= 0) return false;
    got += static_cast<std::size_t>(r);
  }
  return true;
}
bool TcpStream::blocking_write_all(const std::uint8_t* buf, std::size_t n) {
  std::size_t sent = 0;
  while (sent < n) {
#ifdef DCD_WSA
    int r = ::send((SOCKET)sock_, reinterpret_cast<const char*>(buf + sent), static_cast<int>(n - sent), 0);
#else
    ssize_t r = ::send(sock_, buf + sent, n - sent, 0);
#endif
    if (r <= 0) return false;
    sent += static_cast<std::size_t>(r);
  }
  return true;
}

bool TcpListener::bind(const std::string& host, unsigned short port, std::string& err) {
  if (!winsock_init()) { err = "winsock init failed"; return false; }
  close();
#ifdef DCD_WSA
  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) { err = "socket failed"; return false; }
  int reuse = 1; ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
  if (host.empty() || host == "0.0.0.0") addr.sin_addr.s_addr = INADDR_ANY;
  else if (host == "127.0.0.1" || host == "localhost") addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  else { ::inet_pton(AF_INET, host.c_str(), &addr.sin_addr); }
  if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) { ::closesocket(s); err = "bind failed"; return false; }
  if (::listen(s, 16) == SOCKET_ERROR) { ::closesocket(s); err = "listen failed"; return false; }
  sock_ = s;
  sockaddr_in actual{}; int len = sizeof(actual); ::getsockname(s, reinterpret_cast<sockaddr*>(&actual), &len);
  port_ = ntohs(actual.sin_port);
#endif
  return true;
}
std::optional<TcpStream> TcpListener::accept(std::string& err) {
#ifdef DCD_WSA
  sockaddr_in caddr{}; int clen = sizeof(caddr);
  SOCKET c = ::accept((SOCKET)sock_, reinterpret_cast<sockaddr*>(&caddr), &clen);
  if (c == INVALID_SOCKET) { if (err.empty()) err = "accept failed"; return std::nullopt; }
  return TcpStream(static_cast<unsigned long long>(c));
#else
  err = "not supported"; return std::nullopt;
#endif
}
void TcpListener::close() {
#ifdef DCD_WSA
  if (sock_ != ~0ull) { ::closesocket((SOCKET)sock_); sock_ = ~0ull; }
#endif
}

bool FrameChannel::send(std::uint8_t kind, const std::uint8_t* payload, std::uint32_t len, std::string& err) {
  if (!stream_.valid()) { err = "stream not valid"; return false; }
  std::lock_guard<std::mutex> lock(write_mutex_);
  auto frame = protocol::encode_frame(static_cast<protocol::MessageKind>(kind), std::span<const std::uint8_t>(payload, len));
  bool ok = stream_.blocking_write_all(frame.data(), frame.size());
  if (!ok) err = "send failed";
  return ok;
}
bool FrameChannel::send(std::uint8_t kind, const std::vector<std::uint8_t>& payload, std::string& err) {
  return send(kind, payload.data(), static_cast<std::uint32_t>(payload.size()), err);
}
bool FrameChannel::receive(std::uint8_t& kind, std::vector<std::uint8_t>& payload, std::string& err) {
  return receive_timeout(kind, payload, err, -1);
}
bool FrameChannel::receive_timeout(std::uint8_t& kind, std::vector<std::uint8_t>& payload, std::string& err, int timeout_ms) {
  if (!stream_.valid()) { err = "stream not valid"; return false; }
#ifdef DCD_WSA
  if (timeout_ms >= 0) {
    fd_set rf; FD_ZERO(&rf); FD_SET((SOCKET)stream_.native(), &rf);
    timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
    int sel = ::select(0, &rf, nullptr, nullptr, &tv);
    if (sel == 0) { err = "timeout"; return false; }
    if (sel < 0) { err = "select failed"; return false; }
  }
#endif
  std::uint8_t hdr[17];
  if (!stream_.blocking_read_exact(hdr, 17)) { err = "eof/connection closed"; return false; }
  std::uint32_t len = 0; std::memcpy(&len, hdr + 9, 4);
  if (len > protocol::MAX_PAYLOAD) { err = "oversized payload"; return false; }
  payload.resize(len);
  if (!stream_.blocking_read_exact(payload.data(), len)) { err = "eof/connection closed (payload)"; return false; }
  std::vector<std::uint8_t> full(hdr, hdr + 17);
  full.insert(full.end(), payload.begin(), payload.end());
  protocol::Frame frame; std::string derr; std::size_t consumed = 0;
  if (!protocol::decode_frame(full, frame, derr, consumed)) { err = derr; return false; }
  kind = static_cast<std::uint8_t>(frame.kind);
  payload = std::move(frame.payload);
  return true;
}

}  // namespace transport
}  // namespace distributedcachedirectory
