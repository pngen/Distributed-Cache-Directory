#include "distributedcachedirectory/server.hpp"
#include <cstdio>
#include <csignal>
#include <string>

namespace {
bool g_stop = false;
void on_sig(int) { g_stop = true; }
}

int main(int argc, char** argv) {
  unsigned short port = 0;
  std::string persist;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = static_cast<unsigned short>(std::atoi(argv[++i]));
    else if (a == "--persist" && i + 1 < argc) persist = argv[++i];
    else if (a == "--help") { std::printf("usage: %s --port N [--persist FILE]\n", argv[0]); return 0; }
  }
  std::signal(SIGINT, on_sig); std::signal(SIGTERM, on_sig);
  distributedcachedirectory::server::CoordinatorServer server;
  std::string err;
  if (!server.start(port, persist, err)) { std::fprintf(stderr, "start failed: %s\n", err.c_str()); return 1; }
  std::printf("DCD_PORT=%u\n", server.port());
  std::printf("DCD_EPOCH=%llu\n", static_cast<unsigned long long>(server.directory()->epoch().value()));
  std::fflush(stdout);
  while (!g_stop) server.run();
  server.stop();
  return 0;
}
