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

#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr
{
  ConfigMgr()                            = default;
  ConfigMgr(ConfigMgr const&)            = delete;
  ConfigMgr& operator=(ConfigMgr const&) = delete;
  ~ConfigMgr()                           = default;

public:
  /// Seeds the option map with built-in defaults (DefaultConfig::Options).
  bool LoadAppConfigs(bool isReload = false);

  void Configure();

  static ConfigMgr* instance();

  /// Re-seeds the option map from built-in defaults (".reload config" no-op).
  bool Reload();

  /// Loads KEY=VALUE pairs from a dotenv-style file into the process
  /// environment (existing environment variables are never overwritten).
  /// Returns nullopt if the file cannot be opened, otherwise the number of
  /// variables set.
  std::optional<std::size_t> ApplyEnvFile(std::string const& path);

  std::string const GetDataPath();

  /// Directory containing the running executable - all relative dir/file
  /// options (DataDir, LogsDir, PidFile, ...) resolve against it at runtime.
  static std::string GetExecutableDir();

  std::vector<std::string> GetKeysByString(std::string const& name);

  template <class T> T GetOption(std::string const& name, T const& def, bool showLogs = true) const;

  bool isDryRun() { return dryRun; }
  void setDryRun(bool mode) { dryRun = mode; }

private:
  template <class T> T GetValueDefault(std::string const& name, T const& def, bool showLogs = true) const;

  bool dryRun = false;
};

#define sConfigMgr ConfigMgr::instance()

#endif
