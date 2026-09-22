/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright
 * information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CrashHandler.h"
#include "Define.h"
#include "GitRevision.h"

#if AC_PLATFORM != AC_PLATFORM_WINDOWS

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <fcntl.h>
#include <initializer_list>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

// Set next to the Crash() macro in Errors.cpp; holds the assertion text (if any)
extern "C"
{
  extern char const* AcoreAssertionFailedMessage;
}

namespace
{
constexpr int  MAX_CRASH_FRAMES = 64;
constexpr auto CRASH_STACK_SIZE = 64 * 1024;

char g_applicationName[64] = "ac";
char g_crashDir[512]       = {0};
bool g_installed           = false;

// Guards against re-entering the handler (e.g. a second fault while printing)
std::atomic<bool> g_handling{false};

char const* SignalName(int sig)
{
  switch (sig)
  {
    case SIGSEGV:
      return "SIGSEGV";
    case SIGBUS:
      return "SIGBUS";
    case SIGILL:
      return "SIGILL";
    case SIGFPE:
      return "SIGFPE";
    case SIGABRT:
      return "SIGABRT";
    default:
      return "signal";
  }
}

// write() can be short, especially on a pipe (container stderr); loop it.
void WriteAll(int fd, char const* data, size_t length)
{
  while (length > 0)
  {
    ssize_t written = ::write(fd, data, length);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return;
    data += written;
    length -= static_cast<size_t>(written);
  }
}

char const* FaultCodeName(int sig, int code)
{
  if (sig == SIGSEGV)
  {
    if (code == SEGV_MAPERR)
      return "SEGV_MAPERR (address not mapped)";
    if (code == SEGV_ACCERR)
      return "SEGV_ACCERR (invalid permissions)";
  }
  else if (sig == SIGBUS)
  {
    if (code == BUS_ADRALN)
      return "BUS_ADRALN (invalid address alignment)";
    if (code == BUS_ADRERR)
      return "BUS_ADRERR (nonexistent physical address)";
    if (code == BUS_OBJERR)
      return "BUS_OBJERR (object-specific hardware error)";
  }
  else if (sig == SIGILL)
  {
    if (code == ILL_ILLOPC)
      return "ILL_ILLOPC (illegal opcode)";
    if (code == ILL_ILLOPN)
      return "ILL_ILLOPN (illegal operand)";
    if (code == ILL_ILLADR)
      return "ILL_ILLADR (illegal addressing mode)";
    if (code == ILL_PRVOPC)
      return "ILL_PRVOPC (privileged opcode)";
    if (code == ILL_PRVREG)
      return "ILL_PRVREG (privileged register)";
    if (code == ILL_COPROC)
      return "ILL_COPROC (coprocessor error)";
    if (code == ILL_BADSTK)
      return "ILL_BADSTK (internal stack error)";
  }
  else if (sig == SIGFPE)
  {
    if (code == FPE_INTDIV)
      return "FPE_INTDIV (integer divide by zero)";
    if (code == FPE_INTOVF)
      return "FPE_INTOVF (integer overflow)";
    if (code == FPE_FLTDIV)
      return "FPE_FLTDIV (floating point divide by zero)";
    if (code == FPE_FLTOVF)
      return "FPE_FLTOVF (floating point overflow)";
    if (code == FPE_FLTUND)
      return "FPE_FLTUND (floating point underflow)";
    if (code == FPE_FLTRES)
      return "FPE_FLTRES (floating point inexact result)";
    if (code == FPE_FLTINV)
      return "FPE_FLTINV (invalid floating point operation)";
    if (code == FPE_FLTSUB)
      return "FPE_FLTSUB (subscript out of range)";
  }

  if (code == SI_USER)
    return "SI_USER (sent by kill/raise)";
  if (code == SI_TKILL)
    return "SI_TKILL (sent by tkill/tgkill)";
  if (code == SI_KERNEL)
    return "SI_KERNEL";

  return "unknown";
}

// snprintf that appends at `offset` in `out` and returns the new length.
size_t Append(char* out, size_t capacity, size_t offset, char const* fmt, ...)
{
  if (offset >= capacity)
    return offset;

  va_list args;
  va_start(args, fmt);
  int written = std::vsnprintf(out + offset, capacity - offset, fmt, args);
  va_end(args);
  if (written < 0)
    return offset;

  size_t n = static_cast<size_t>(written);
  if (n > capacity - offset - 1)
    n = capacity - offset - 1;
  return offset + n;
}

// Fault address, siginfo code and the faulting CPU registers. The register
// layout is architecture specific; production is aarch64 while dev machines
// are usually x86_64, so both are covered.
size_t AppendHardwareContext(char* out, size_t capacity, int sig, siginfo_t* info, void* context)
{
  size_t n = 0;

  bool faultAddressMeaningful = sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE || sig == SIGTRAP;
  if (faultAddressMeaningful && info)
    n = Append(out, capacity, n, "fault address: 0x%llx\n", static_cast<unsigned long long>(
                                                                   reinterpret_cast<uintptr_t>(info->si_addr)));
  else
    n = Append(out, capacity, n, "fault address: (n/a)\n");

  if (info)
    n = Append(out, capacity, n, "si_code:       %d (%s)\n", info->si_code, FaultCodeName(sig, info->si_code));

#if defined(__x86_64__)
  if (ucontext_t* uc = static_cast<ucontext_t*>(context))
  {
    greg_t* g = uc->uc_mcontext.gregs;
    n = Append(out, capacity, n, "\nregisters (x86_64):\n");
    n = Append(out, capacity, n, "  rip: %016llx  rsp: %016llx  rbp: %016llx\n",
               static_cast<unsigned long long>(g[REG_RIP]), static_cast<unsigned long long>(g[REG_RSP]),
               static_cast<unsigned long long>(g[REG_RBP]));
    n = Append(out, capacity, n, "  rax: %016llx  rbx: %016llx  rcx: %016llx  rdx: %016llx\n",
               static_cast<unsigned long long>(g[REG_RAX]), static_cast<unsigned long long>(g[REG_RBX]),
               static_cast<unsigned long long>(g[REG_RCX]), static_cast<unsigned long long>(g[REG_RDX]));
    n = Append(out, capacity, n, "  rsi: %016llx  rdi: %016llx  r8:  %016llx  r9:  %016llx\n",
               static_cast<unsigned long long>(g[REG_RSI]), static_cast<unsigned long long>(g[REG_RDI]),
               static_cast<unsigned long long>(g[REG_R8]), static_cast<unsigned long long>(g[REG_R9]));
    n = Append(out, capacity, n, "  r10: %016llx  r11: %016llx  r12: %016llx  r13: %016llx\n",
               static_cast<unsigned long long>(g[REG_R10]), static_cast<unsigned long long>(g[REG_R11]),
               static_cast<unsigned long long>(g[REG_R12]), static_cast<unsigned long long>(g[REG_R13]));
    n = Append(out, capacity, n, "  r14: %016llx  r15: %016llx  eflags: %llx\n",
               static_cast<unsigned long long>(g[REG_R14]), static_cast<unsigned long long>(g[REG_R15]),
               static_cast<unsigned long long>(g[REG_EFL]));
  }
#elif defined(__aarch64__)
  if (ucontext_t* uc = static_cast<ucontext_t*>(context))
  {
    n = Append(out, capacity, n, "\nregisters (aarch64):\n");
    n = Append(out, capacity, n, "  pc: %016llx  sp: %016llx  pstate: %016llx\n",
               static_cast<unsigned long long>(uc->uc_mcontext.pc),
               static_cast<unsigned long long>(uc->uc_mcontext.sp),
               static_cast<unsigned long long>(uc->uc_mcontext.pstate));
    for (int i = 0; i < 31; i += 4)
    {
      n = Append(out, capacity, n, "  x%d: %016llx", i, static_cast<unsigned long long>(uc->uc_mcontext.regs[i]));
      if (i + 1 < 31)
        n = Append(out, capacity, n, "  x%d: %016llx", i + 1,
                   static_cast<unsigned long long>(uc->uc_mcontext.regs[i + 1]));
      if (i + 2 < 31)
        n = Append(out, capacity, n, "  x%d: %016llx", i + 2,
                   static_cast<unsigned long long>(uc->uc_mcontext.regs[i + 2]));
      if (i + 3 < 31)
        n = Append(out, capacity, n, "  x%d: %016llx", i + 3,
                   static_cast<unsigned long long>(uc->uc_mcontext.regs[i + 3]));
      n = Append(out, capacity, n, "\n");
    }
  }
#else
  (void)context;
#endif

  return n;
}

void CrashHandler(int sig, siginfo_t* info, void* context)
{
  if (g_handling.exchange(true))
  {
    // We faulted while reporting an earlier crash: fall back to the default
    // action so the process still terminates with the right signal.
    ::signal(sig, SIG_DFL);
    ::raise(sig);
    return;
  }

  char path[768] = {0};
  if (g_crashDir[0])
  {
    std::time_t now = std::time(nullptr);
    std::tm     tm{};
    localtime_r(&now, &tm);
    char stamp[32] = {0};
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
    std::snprintf(path, sizeof(path), "%s/%s_crash_%s_%d.log", g_crashDir, g_applicationName, stamp,
                  static_cast<int>(::getpid()));
  }

  int fileFd = -1;
  if (path[0])
    fileFd = ::open(path, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);

  // stderr first (docker logs), then the file (persisted via the volume)
  auto emit = [&](char const* data, size_t length)
  {
    WriteAll(STDERR_FILENO, data, length);
    if (fileFd >= 0)
      WriteAll(fileFd, data, length);
  };

  char header[512] = {0};
  int  headerLen   = std::snprintf(header, sizeof(header),
                                   "\n=== %s crashed ===\n"
                                   "signal:   %d (%s)\n"
                                   "revision: %s\n"
                                   "pid:      %d\ntid:      %ld\n"
                                   "report:   %s\n",
                                   g_applicationName, sig, SignalName(sig), GitRevision::GetFullVersion(),
                                   static_cast<int>(::getpid()), static_cast<long>(::syscall(SYS_gettid)),
                                   path[0] ? path : "<stderr only>");
  if (headerLen > 0)
    emit(header, static_cast<size_t>(headerLen));

  if (AcoreAssertionFailedMessage)
  {
    emit("\n", 1);
    emit(AcoreAssertionFailedMessage, std::strlen(AcoreAssertionFailedMessage));
    emit("\n", 1);
  }

  char   hardware[4096];
  size_t hardwareLength = AppendHardwareContext(hardware, sizeof(hardware), sig, info, context);
  if (hardwareLength > 0)
  {
    emit("\n", 1);
    emit(hardware, hardwareLength);
  }

  void* frames[MAX_CRASH_FRAMES];
  int   frameCount = ::backtrace(frames, MAX_CRASH_FRAMES);

  // Raw addresses first: these are enough to re-symbolize offline (addr2line /
  // gdb) against the matching binary, even if symbol names come out mangled.
  emit("raw addresses:\n", 15);
  for (int i = 0; i < frameCount; ++i)
  {
    char line[64] = {0};
    int  lineLen  = std::snprintf(line, sizeof(line), "  #%-2d %p\n", i, frames[i]);
    if (lineLen > 0)
      emit(line, static_cast<size_t>(lineLen));
  }

  // Symbolized trace (best effort): uses the dynamic symbol table, so the
  // executable must be linked with -rdynamic. Names may be C++ mangled; run
  // them through c++filt.
  emit("backtrace:\n", 11);
  if (fileFd >= 0)
    ::backtrace_symbols_fd(frames, frameCount, fileFd);
  ::backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);

  emit("\n=== end crash report ===\n", 26);

  if (fileFd >= 0)
    ::close(fileFd);

  // Restore the default action and re-raise so the exit status reflects the
  // fatal signal (a supervisor can then restart us). The signal we are
  // handling is blocked while this handler runs, so it has to be unblocked
  // before raising or the re-raise would only sit pending.
  ::signal(sig, SIG_DFL);
  sigset_t set;
  ::sigemptyset(&set);
  ::sigaddset(&set, sig);
  ::sigprocmask(SIG_UNBLOCK, &set, nullptr);
  ::raise(sig);

  ::_exit(128 + sig);
}
} // namespace

void Acore::InstallCrashHandler(char const* applicationName)
{
  if (g_installed)
    return;
  g_installed = true;

  if (applicationName && *applicationName)
    std::snprintf(g_applicationName, sizeof(g_applicationName), "%s", applicationName);

  if (char const* dir = std::getenv("AC_CRASH_DIR"); dir && *dir)
  {
    std::snprintf(g_crashDir, sizeof(g_crashDir), "%s", dir);
    ::mkdir(g_crashDir, 0755); // best effort; ignored if it already exists
  }

  // A crashing process may be mid-printf; keep the report from sitting in
  // a buffer that never gets flushed.
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  // SIGSEGV from a stack overflow cannot run on the exhausted stack, so give
  // the handler its own.
  static char altStack[CRASH_STACK_SIZE];
  stack_t     stack{};
  stack.ss_sp    = altStack;
  stack.ss_size  = sizeof(altStack);
  stack.ss_flags = 0;
  ::sigaltstack(&stack, nullptr);

  struct sigaction action{};
  action.sa_sigaction = &CrashHandler;
  action.sa_flags     = SA_SIGINFO | SA_ONSTACK;
  ::sigemptyset(&action.sa_mask);
  for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
    ::sigaddset(&action.sa_mask, sig);

  for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
    ::sigaction(sig, &action, nullptr);
}

#else // AC_PLATFORM_WINDOWS

void Acore::InstallCrashHandler(char const* /*applicationName*/) {}

#endif
