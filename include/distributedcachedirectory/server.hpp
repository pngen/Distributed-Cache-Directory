#pragma once
#include "directory.hpp"
#include "transport.hpp"
#include "protocol.hpp"
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace distributedcachedirectory {
namespace server {

// A coordinator that owns a Directory and serves framed-TCP requests from
// workers. Directory mutations/queries are serialized by dir_mutex_; socket I/O
// happens outside that lock (the handler thread does the read before locking).
class CoordinatorServer {
 public:
  CoordinatorServer();
  ~CoordinatorServer();

  bool start(unsigned short port, const std::string& persist_path, std::string& err);
  void run();                 // blocking accept loop (call from coordinator main)
  void stop();
  bool running() const { return running_; }
  unsigned short port() const;
  std::shared_ptr<Directory> directory() { return dir_; }

 private:
  void handle_connection(transport::TcpStream&& stream);
  void respond_ack(transport::FrameChannel& ch, const AckResult& a);
  void respond_error(transport::FrameChannel& ch, DirectoryError e, const std::string& m);

  // Handler threads started by run(). Each lists the channel it serves in
  // live_channels_ while it runs. stop() interrupts every listed channel and
  // joins every thread, so no handler outlives the server it works on.
  struct Connection {
    std::thread thread;
    std::atomic<bool> done{false};
  };
  // Joins and drops the handler threads that have finished.
  void reap_finished_locked();

  std::shared_ptr<Directory> dir_;
  transport::TcpListener listener_;
  std::mutex dir_mutex_;
  std::string persist_path_;
  std::atomic<bool> running_{false};
  unsigned short port_{0};
  std::mutex conn_mutex_;
  std::list<Connection> connections_;
  std::vector<transport::FrameChannel*> live_channels_;
  bool stopping_{false};
};

}  // namespace server
}  // namespace distributedcachedirectory
