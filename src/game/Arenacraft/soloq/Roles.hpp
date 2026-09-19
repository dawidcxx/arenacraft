#pragma once

#include "Types.hpp"

#include <optional>

namespace arenacraft::soloq
{
std::optional<Role> roleFor(Classes classId, uint8_t specIndex);
}
