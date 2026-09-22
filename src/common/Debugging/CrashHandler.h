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

#ifndef _ACORE_CRASH_HANDLER_H_
#define _ACORE_CRASH_HANDLER_H_

namespace Acore
{
/// Install handlers for fatal signals (SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT)
/// that write a backtrace before the process dies. Reports go to stderr and,
/// when the AC_CRASH_DIR environment variable is set, to a timestamped file in
/// that directory. No-op on Windows.
///
/// @param applicationName Short name used for crash file naming, e.g. "worldserver"
void InstallCrashHandler(char const* applicationName);
} // namespace Acore

#endif
