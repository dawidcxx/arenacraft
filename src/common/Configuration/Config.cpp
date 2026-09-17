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

#include "Config.h"
#include "DefaultConfig.h"
#include "Log.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
#include <windows.h>
#elif AC_PLATFORM == AC_PLATFORM_APPLE
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace
{
std::unordered_map<std::string /*name*/, std::string /*value*/> _configOptions;
std::unordered_map<std::string /*name*/, std::string /*value*/> _envVarCache;
std::mutex                                                      _configLock;

// Converts ini keys to the environment variable key (upper snake case).
// Example of conversions:
//   SomeConfig => SOME_CONFIG
//   myNestedConfig.opt1 => MY_NESTED_CONFIG_OPT_1
//   LogDB.Opt.ClearTime => LOG_DB_OPT_CLEAR_TIME
std::string IniKeyToEnvVarKey(std::string const& key)
{
  std::string result;

  const char* str = key.c_str();
  std::size_t n   = key.length();

  char curr;
  bool isEnd;
  bool nextIsUpper;
  bool currIsNumeric;
  bool nextIsNumeric;

  for (std::size_t i = 0; i < n; ++i)
  {
    curr = str[i];
    if (curr == ' ' || curr == '.' || curr == '-')
    {
      result += '_';
      continue;
    }

    isEnd = i == n - 1;
    if (!isEnd)
    {
      nextIsUpper = isupper(str[i + 1]);

      // handle "aB" to "A_B"
      if (!isupper(curr) && nextIsUpper)
      {
        result += static_cast<char>(std::toupper(curr));
        result += '_';
        continue;
      }

      currIsNumeric = isNumeric(curr);
      nextIsNumeric = isNumeric(str[i + 1]);

      // handle "a1" to "a_1"
      if (!currIsNumeric && nextIsNumeric)
      {
        result += static_cast<char>(std::toupper(curr));
        result += '_';
        continue;
      }

      // handle "1a" to "1_a"
      if (currIsNumeric && !nextIsNumeric)
      {
        result += static_cast<char>(std::toupper(curr));
        result += '_';
        continue;
      }
    }

    result += static_cast<char>(std::toupper(curr));
  }
  return result;
}

std::string GetEnvVarName(std::string const& configName) { return "AC_" + IniKeyToEnvVarKey(configName); }

Optional<std::string> EnvVarForIniKey(std::string const& key)
{
  std::string envKey = GetEnvVarName(key);
  char*       val    = std::getenv(envKey.c_str());
  if (!val)
    return std::nullopt;

  return std::string(val);
}

// Check the _envVarCache if the env var is there
// if not, check the env for the value
Optional<std::string> GetEnvFromCache(std::string const& configName, std::string const& envVarName)
{
  auto                  foundInCache = _envVarCache.find(envVarName);
  Optional<std::string> foundInEnv;
  // If it's not in the cache
  if (foundInCache == _envVarCache.end())
  {
    // Check the env itself
    foundInEnv = EnvVarForIniKey(configName);
    if (foundInEnv)
    {
      // If it's found in the env, put it in the cache
      _envVarCache.emplace(envVarName, *foundInEnv);
    }
    // Return the result of checking env
    return foundInEnv;
  }

  return foundInCache->second;
}

std::string_view Trim(std::string_view s)
{
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
    s.remove_suffix(1);
  return s;
}

bool SetEnvVarIfAbsent(std::string const& key, std::string const& value)
{
  if (std::getenv(key.c_str()))
    return false;

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
  return _putenv_s(key.c_str(), value.c_str()) == 0;
#else
  return setenv(key.c_str(), value.c_str(), 0) == 0;
#endif
}
} // namespace

ConfigMgr* ConfigMgr::instance()
{
  static ConfigMgr instance;
  return &instance;
}

bool ConfigMgr::LoadAppConfigs(bool isReload /*= false*/)
{
  std::lock_guard<std::mutex> lock(_configLock);

  _configOptions.clear();
  for (auto const& [name, value] : DefaultConfig::Options())
    _configOptions.emplace(name, value);

  fmt::print("Config: using built-in defaults ({} options), overridable via AC_* environment variables{}\n",
             _configOptions.size(), isReload ? " (reload)" : "");

  return true;
}

bool ConfigMgr::Reload()
{
  return LoadAppConfigs(true);
}

std::optional<std::size_t> ConfigMgr::ApplyEnvFile(std::string const& path)
{
  std::FILE* file = std::fopen(path.c_str(), "r");
  if (!file)
    return std::nullopt;

  std::size_t loaded = 0;
  char        line[4096];
  while (std::fgets(line, sizeof(line), file))
  {
    std::string_view entry = Trim(line);
    if (entry.empty() || entry.front() == '#')
      continue;

    if (entry.starts_with("export "))
      entry.remove_prefix(7);

    auto const sep = entry.find('=');
    if (sep == std::string_view::npos)
      continue;

    std::string const key(Trim(entry.substr(0, sep)));
    std::string_view  value = Trim(entry.substr(sep + 1));

    // strip a single pair of surrounding quotes
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
      value = value.substr(1, value.size() - 2);

    if (key.empty() || !SetEnvVarIfAbsent(key, std::string(value)))
      continue;

    ++loaded;
  }
  std::fclose(file);

  fmt::print("Config: loaded {} environment variables from '{}'\n", loaded, path);
  return loaded;
}

template <class T> T ConfigMgr::GetValueDefault(std::string const& name, T const& def, bool showLogs /*= true*/) const
{
  std::string strValue;

  auto const&           itr        = _configOptions.find(name);
  bool                  notFound   = itr == _configOptions.end();
  auto                  envVarName = GetEnvVarName(name);
  Optional<std::string> envVar     = GetEnvFromCache(name, envVarName);
  if (envVar)
  {
    // apply the env value to the map so the override is logged exactly once
    if (notFound)
    {
      if (showLogs)
        LOG_INFO("server.loading", "> Config: Found config value '{}' from environment variable '{}'.", name,
                 envVarName);
      _configOptions.emplace(name, *envVar);
    }
    else if (itr->second != *envVar)
    {
      if (showLogs)
        LOG_INFO("server.loading", "> Config: Found config value '{}' from environment variable '{}'.", name,
                 envVarName);
      itr->second = *envVar;
    }

    strValue = *envVar;
  }
  else if (notFound)
    return def;
  else
    strValue = itr->second;

  auto value = Acore::StringTo<T>(strValue);
  if (!value)
  {
    if (showLogs)
    {
      LOG_ERROR("server.loading", "> Config: Bad value defined for name '{}', going to use '{}' instead", name,
                Acore::ToString(def));
    }

    return def;
  }

  return *value;
}

template <>
std::string ConfigMgr::GetValueDefault<std::string>(std::string const& name, std::string const& def,
                                                    bool showLogs /*= true*/) const
{
  auto const&           itr        = _configOptions.find(name);
  bool                  notFound   = itr == _configOptions.end();
  auto                  envVarName = GetEnvVarName(name);
  Optional<std::string> envVar     = GetEnvFromCache(name, envVarName);
  if (envVar)
  {
    if (notFound)
    {
      if (showLogs)
        LOG_INFO("server.loading", "> Config: Found config value '{}' from environment variable '{}'.", name,
                 envVarName);
      _configOptions.emplace(name, *envVar);
    }
    else if (itr->second != *envVar)
    {
      if (showLogs)
        LOG_INFO("server.loading", "> Config: Found config value '{}' from environment variable '{}'.", name,
                 envVarName);
      itr->second = *envVar;
    }

    return *envVar;
  }
  else if (notFound)
    return def;

  return itr->second;
}

template <class T> T ConfigMgr::GetOption(std::string const& name, T const& def, bool showLogs /*= true*/) const
{
  return GetValueDefault<T>(name, def, showLogs);
}

template <> bool ConfigMgr::GetOption<bool>(std::string const& name, bool const& def, bool showLogs /*= true*/) const
{
  std::string val = GetValueDefault(name, std::string(def ? "1" : "0"), showLogs);

  auto boolVal = Acore::StringTo<bool>(val);
  if (!boolVal)
  {
    if (showLogs)
    {
      LOG_ERROR("server.loading", "> Config: Bad value defined for name '{}', going to use '{}' instead", name,
                def ? "true" : "false");
    }

    return def;
  }

  return *boolVal;
}

std::vector<std::string> ConfigMgr::GetKeysByString(std::string const& name)
{
  std::lock_guard<std::mutex> lock(_configLock);

  std::vector<std::string> keys;

  for (auto const& [optionName, key] : _configOptions)
  {
    if (!optionName.compare(0, name.length(), name))
    {
      keys.emplace_back(optionName);
    }
  }

  return keys;
}

std::string ConfigMgr::GetExecutableDir()
{
  char buffer[4096];
#if AC_PLATFORM == AC_PLATFORM_WINDOWS
  DWORD const len = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
  if (!len)
    return "./";
#elif AC_PLATFORM == AC_PLATFORM_APPLE
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0)
    return "./";
#else
  ssize_t const len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len <= 0)
    return "./";
  buffer[len] = '\0';
#endif

  std::error_code ec;
  std::filesystem::path const exePath = std::filesystem::weakly_canonical(buffer, ec);
  return exePath.parent_path().generic_string() + "/";
}

// Data directory resolved relative to the executable ("data" means
// <exe dir>/data). Absolute paths are used as-is.
std::string const ConfigMgr::GetDataPath()
{
  std::string dataPath = GetOption<std::string>("DataDir", "data");

  std::error_code ec;
  std::filesystem::path const path(dataPath);
  if (!path.is_absolute())
    dataPath = std::filesystem::weakly_canonical(GetExecutableDir() + dataPath, ec).generic_string();

  return dataPath;
}

void ConfigMgr::Configure() {}

#define TEMPLATE_CONFIG_OPTION(__typename)                                                                             \
  template __typename ConfigMgr::GetOption<__typename>(std::string const& name, __typename const& def,                 \
                                                       bool showLogs /*= true*/) const;

TEMPLATE_CONFIG_OPTION(std::string)
TEMPLATE_CONFIG_OPTION(uint8)
TEMPLATE_CONFIG_OPTION(int8)
TEMPLATE_CONFIG_OPTION(uint16)
TEMPLATE_CONFIG_OPTION(int16)
TEMPLATE_CONFIG_OPTION(uint32)
TEMPLATE_CONFIG_OPTION(int32)
TEMPLATE_CONFIG_OPTION(uint64)
TEMPLATE_CONFIG_OPTION(int64)
TEMPLATE_CONFIG_OPTION(float)

#undef TEMPLATE_CONFIG_OPTION
