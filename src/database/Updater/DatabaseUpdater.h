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

#ifndef DATABASEUPDATER_H
#define DATABASEUPDATER_H

#include "Define.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace Acore::Database
{
struct DatabaseUpdateResult
{
  uint32 applied        = 0;
  uint32 skipped        = 0;
  uint32 hashMismatches = 0;
};

/// Applies pending files from `updateDirectory` to the database described by
/// `connectionInfo`. The database must already be seeded (the `updates` table
/// must exist). Returns false on a fatal error.
[[nodiscard]] bool ApplyDatabaseUpdates(std::string_view label, std::string_view connectionInfo,
                                        std::filesystem::path const& updateDirectory, bool dryRun,
                                        DatabaseUpdateResult& result);
} // namespace Acore::Database

#endif // DATABASEUPDATER_H
