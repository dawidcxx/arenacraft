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

#include "DatabaseUpdater.h"
#include "CryptoHash.h"
#include "Field.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "SqlStatementSplitter.h"
#include "Timer.h"
#include "Util.h"
#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace
{
class DatabaseUpdaterConnection final : public MySQLConnection
{
public:
  explicit DatabaseUpdaterConnection(MySQLConnectionInfo& info) : MySQLConnection(info) {}
  ~DatabaseUpdaterConnection() override = default;

private:
  void DoPrepareStatements() override {}
};

struct AppliedUpdate
{
  std::string hash;
  std::string state;
};

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
  if (left.size() != right.size())
    return false;

  for (std::size_t i = 0; i < left.size(); ++i)
  {
    if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i])))
      return false;
  }

  return true;
}

bool ReadFile(std::filesystem::path const& path, std::string& out)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open())
    return false;

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  out = buffer.str();
  return true;
}

std::string Sha1Hex(std::string const& contents)
{
  return ByteArrayToHexStr(Acore::Crypto::SHA1::GetDigestOf(contents));
}

std::string EscapeString(MySQLConnection& connection, std::string const& value)
{
  std::string escaped(value.size() * 2 + 1, '\0');
  std::size_t length = connection.EscapeString(escaped.data(), value.c_str(), value.size());
  escaped.resize(length);
  return escaped;
}

std::vector<std::filesystem::path> ListSqlFiles(std::filesystem::path const& directory)
{
  std::vector<std::filesystem::path> files;
  std::error_code                    error;

  if (!std::filesystem::is_directory(directory, error))
    return files;

  for (std::filesystem::directory_iterator itr(directory, error), end; !error && itr != end; itr.increment(error))
  {
    if (!itr->is_regular_file(error))
      continue;

    if (itr->path().extension() == ".sql")
      files.push_back(itr->path());
  }

  std::sort(files.begin(), files.end(), [](std::filesystem::path const& left, std::filesystem::path const& right)
            { return left.filename().string() < right.filename().string(); });

  return files;
}
} // namespace

namespace Acore::Database
{
bool ApplyDatabaseUpdates(std::string_view label, std::string_view connectionInfo,
                          std::filesystem::path const& updateDirectory, bool dryRun, DatabaseUpdateResult& result)
{
  MySQLConnectionInfo info(connectionInfo);
  if (info.database.empty())
  {
    fmt::print(stderr, "[{}] invalid database connection info (expected host;port;user;password;database)\n", label);
    return false;
  }

  DatabaseUpdaterConnection connection(info);
  if (connection.Open() != 0)
  {
    fmt::print(stderr, "[{}] could not open database '{}'\n", label, info.database);
    return false;
  }

  {
    std::unique_ptr<ResultSet> table(connection.Query("SHOW TABLES LIKE 'updates'"));
    if (!table || table->GetRowCount() == 0)
    {
      fmt::print(stderr, "[{}] database '{}' has not been seeded; run scripts/seed_db first\n", label, info.database);
      return false;
    }
  }

  std::unordered_map<std::string, AppliedUpdate> applied;
  {
    std::unique_ptr<ResultSet> rows(connection.Query("SELECT `name`, `hash`, `state` FROM `updates`"));
    if (!rows)
    {
      fmt::print(stderr, "[{}] could not read the updates table\n", label);
      return false;
    }

    while (rows->NextRow())
    {
      Field* fields = rows->Fetch();
      applied.emplace(fields[0].Get<std::string>(),
                      AppliedUpdate{fields[1].Get<std::string>(), fields[2].Get<std::string>()});
    }
  }

  for (std::filesystem::path const& path : ListSqlFiles(updateDirectory))
  {
    std::string const file = path.filename().string();
    auto const        itr  = applied.find(file);

    if (itr != applied.end())
    {
      if (itr->second.state != "ARCHIVED")
      {
        std::string contents;
        if (!ReadFile(path, contents))
        {
          fmt::print(stderr, "[{}] failed to read {}\n", label, path.string());
          return false;
        }

        if (!EqualsIgnoreCase(Sha1Hex(contents), itr->second.hash))
        {
          ++result.hashMismatches;
          fmt::print(stderr, "[{}] WARN {}: sha1 changed since it was applied, skipping\n", label, file);
        }
      }

      ++result.skipped;
      continue;
    }

    std::string contents;
    if (!ReadFile(path, contents))
    {
      fmt::print(stderr, "[{}] failed to read {}\n", label, path.string());
      return false;
    }

    std::vector<std::string> const statements = SplitSqlStatements(contents);
    if (dryRun)
    {
      fmt::print("[{}] would apply {} ({} statements) [dry run]\n", label, file, statements.size());
      ++result.applied;
      continue;
    }

    uint32 const start = getMSTime();
    for (std::string const& statement : statements)
    {
      if (!connection.Execute(statement))
      {
        fmt::print(stderr, "[{}] failed to apply {}\n", label, file);
        return false;
      }
    }

    uint32 const      speed = getMSTimeDiff(start, getMSTime());
    std::string const hash  = Sha1Hex(contents);
    std::string const insert =
        fmt::format("INSERT INTO `updates` (`name`, `hash`, `state`, `speed`) VALUES ('{}', '{}', 'RELEASED', {})",
                    EscapeString(connection, file), EscapeString(connection, hash), speed);

    if (!connection.Execute(insert))
    {
      fmt::print(stderr, "[{}] failed to record {} in the updates table\n", label, file);
      return false;
    }

    ++result.applied;
    fmt::print("[{}] applied {} ({} statements, {}ms)\n", label, file, statements.size(), speed);
  }

  return true;
}
} // namespace Acore::Database
