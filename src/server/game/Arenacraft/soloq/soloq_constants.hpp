
#pragma once

#include <stdint.h>
#include "DBCStructure.h"

namespace arenacraft::soloq::constants
{
    constexpr uint32_t BRACKET_ID = 14;     
    static PvPDifficultyEntry* SOLOQ_DIFFICULTY_ENTRY = new PvPDifficultyEntry(0, BRACKET_ID, 80, 80, 0);
}