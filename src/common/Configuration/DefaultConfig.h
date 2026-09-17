/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

#include <string>
#include <utility>
#include <vector>

namespace DefaultConfig
{
    /// Built-in option table replacing worldserver.conf / authserver.conf /
    /// module .conf files. Every key is overridable at runtime via an
    /// AC_<OPTION> environment variable (e.g. StartPlayerLevel -> AC_START_PLAYER_LEVEL).
    std::vector<std::pair<std::string, std::string>> const& Options();
}

#endif
