#pragma once

#include <source_location>
#include <string_view>

namespace logging
{
enum class Level
{
  info,
  error,
};

void write(Level level,
           std::string_view message,
           std::source_location where = std::source_location::current());
} // namespace logging
