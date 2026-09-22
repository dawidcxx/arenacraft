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

#include <doctest/doctest.h>

#include "CrashHandler.h"

#if defined(__linux__)

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

TEST_CASE("Acore::InstallCrashHandler writes a report and re-raises the signal")
{
  std::string dirTemplate = "/tmp/ac_crash_handler_test_XXXXXX";
  std::string dir         = dirTemplate;
  REQUIRE(::mkdtemp(dir.data()) != nullptr);
  REQUIRE(::setenv("AC_CRASH_DIR", dir.c_str(), 1) == 0);

  pid_t pid = ::fork();
  REQUIRE(pid >= 0);

  if (pid == 0)
  {
    // child: install the handler and fault on purpose
    Acore::InstallCrashHandler("crashhandler_test");
    ::raise(SIGSEGV);
    ::_exit(0); // only reached if the handler failed to terminate us
  }

  int status = 0;
  REQUIRE(::waitpid(pid, &status, 0) == pid);
  CHECK(WIFSIGNALED(status));
  if (WIFSIGNALED(status))
    CHECK(WTERMSIG(status) == SIGSEGV);

  bool foundReport = false;
  for (auto const& entry : std::filesystem::directory_iterator(dir))
  {
    if (!entry.is_regular_file())
      continue;

    std::ifstream report(entry.path());
    std::string   contents((std::istreambuf_iterator<char>(report)), std::istreambuf_iterator<char>());

    if (contents.find("crashhandler_test crashed") != std::string::npos &&
        contents.find("raw addresses:") != std::string::npos && contents.find("backtrace:") != std::string::npos)
    {
      foundReport = true;
      break;
    }
  }

  CHECK(foundReport);

  std::filesystem::remove_all(dir);
  ::unsetenv("AC_CRASH_DIR");
}

#endif // __linux__
