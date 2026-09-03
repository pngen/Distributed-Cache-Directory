#pragma once
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

// Windows system headers define macros (NEAR, FAR, ERROR, min, max, ...) that
// collide with the directory library enumerators/types. Undefine them so the
// distributedcachedirectory headers included by the test are unaffected.
#undef ERROR
#undef FAILED
#undef SUCCEEDED
#undef NEAR
#undef FAR
#undef min
#undef max
#undef interface
#undef NO_ERROR
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef OPTIONAL
#undef OPTIONAL
#endif

// Minimal child-process helper for the multiprocess authority proof. Uses real
// OS processes and real loopback TCP. Coordinates via stdin/stdout pipes.
struct Child {
  HANDLE hproc = nullptr;
  HANDLE hstdin_w = nullptr;
  HANDLE hstdout_r = nullptr;
  DWORD pid = 0;

  static Child spawn(const std::string& exe, const std::vector<std::string>& args, std::string& err) {
    Child c;
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = nullptr;
    HANDLE in_r=nullptr,in_w=nullptr,out_r=nullptr,out_w=nullptr;
    if (!CreatePipe(&in_r, &in_w, &sa, 0)) { err="CreatePipe stdin"; return c; }
    if (!CreatePipe(&out_r, &out_w, &sa, 0)) { err="CreatePipe stdout"; return c; }
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    std::string cmdline = "\"" + exe + "\"";
    for (auto& a : args) cmdline += " \"" + a + "\"";
    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_r; si.hStdOutput = out_w; si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    std::vector<char> buf(cmdline.begin(), cmdline.end()); buf.push_back('\0');
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
      err = "CreateProcess failed"; CloseHandle(in_r); CloseHandle(in_w); CloseHandle(out_r); CloseHandle(out_w); return c;
    }
    CloseHandle(in_r); CloseHandle(out_w);
    c.hproc = pi.hProcess; c.hstdin_w = in_w; c.hstdout_r = out_r; c.pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    return c;
  }

  bool write_stdin(const std::string& s) {
    DWORD written = 0;
    return WriteFile(hstdin_w, s.data(), static_cast<DWORD>(s.size()), &written, nullptr) != 0;
  }
  // Read whatever is currently available (non-blocking).
  std::string read_available() {
    std::string out;
    if (!hstdout_r) return out;
    DWORD avail = 0;
    if (!PeekNamedPipe(hstdout_r, nullptr, 0, nullptr, &avail, nullptr)) return out;
    if (avail == 0) return out;
    std::vector<char> tmp(avail);
    DWORD got = 0;
    if (ReadFile(hstdout_r, tmp.data(), avail, &got, nullptr) == 0) return out;
    out.assign(tmp.data(), got);
    return out;
  }
  bool wait(DWORD ms) { return WaitForSingleObject(hproc, ms) == WAIT_OBJECT_0; }
  void terminate() { if (hproc) TerminateProcess(hproc, 1); }
  void kill() { terminate(); if (hproc) { WaitForSingleObject(hproc, 2000); CloseHandle(hproc); hproc=nullptr; } if (hstdin_w){CloseHandle(hstdin_w);hstdin_w=nullptr;} if (hstdout_r){CloseHandle(hstdout_r);hstdout_r=nullptr;} }
  DWORD exit_code() { DWORD c=0; GetExitCodeProcess(hproc, &c); return c; }
};
