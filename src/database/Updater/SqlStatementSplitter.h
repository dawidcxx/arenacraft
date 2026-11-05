/*
 * This file is part of the Arenacraft Project. See AUTHORS file for Copyright
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

#ifndef SQLSTATEMENTSPLITTER_H
#define SQLSTATEMENTSPLITTER_H

#include <string>
#include <string_view>
#include <vector>

namespace Acore::Database
{
/// Splits a .sql file into individual statements. Strips `--` and `#` line
/// comments as well as plain block comments, while keeping mysql conditional
/// comments (the `/*!` variant, e.g. used for LOCK/UNLOCK KEYS directives)
/// and everything inside single/double/backtick quotes intact.
[[nodiscard]] std::vector<std::string> SplitSqlStatements(std::string_view sqlText);
} // namespace Acore::Database

#endif // SQLSTATEMENTSPLITTER_H
