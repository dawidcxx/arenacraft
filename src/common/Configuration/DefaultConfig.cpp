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

#include "DefaultConfig.h"

namespace DefaultConfig
{
    std::vector<std::pair<std::string, std::string>> const& Options()
    {
        static std::vector<std::pair<std::string, std::string>> const options = {

            // realm / general
            {"RealmID", "1"},
            {"SkipCinematics", "1"},

            // instant-80 arena server
            {"StartPlayerLevel", "80"},
            {"StartHeroicPlayerLevel", "80"},
            {"CharacterCreating.MinLevelForHeroicCharacter", "1"},
            {"StartPlayerMoney", "1000000000"},
            {"StartHeroicPlayerMoney", "1000000000"},
            {"PlayerStart.MapsExplored", "1"},
            {"PlayerStart.AllReputation", "1"},

            // single-faction-less server: both factions share chat and channels
            {"AllowTwoSide.Interaction.Chat", "1"},
            {"AllowTwoSide.Interaction.Channel", "1"},

            // progression is pointless at max level - no XP from any source
            {"Rate.XP.Kill", "0"},
            {"Rate.XP.Quest", "0"},
            {"Rate.XP.Quest.DF", "0"},
            {"Rate.XP.Explore", "0"},
            {"Rate.XP.Pet", "0"},
            {"Rate.XP.BattlegroundKillWSG", "0"},
            {"Rate.XP.BattlegroundKillAB", "0"},
            {"Rate.XP.BattlegroundKillAV", "0"},
            {"Rate.XP.BattlegroundKillEOTS", "0"},
            {"Rate.XP.BattlegroundKillIC", "0"},
            {"Rate.XP.BattlegroundKillSOTA", "0"},

            // preserve previous worldserver.conf.dist behavior where the
            // in-code default differed
            {"Instance.SharedNormalHeroicId", "1"},
            {"ListenRange.Say", "40"},
            {"ListenRange.TextEmote", "40"},

            // logging - console only, file appenders are gone.
            // Appender args: <type>,<level>,<flags>,<args>; levels are
            // Fatal=1 .. Trace=6. Console colors: "1 9 3 6 5 8" (per-level).
            {"Appender.Console", "1,4,0,1 9 3 6 5 8"},
            {"Logger.root", "2,Console"},
            {"Logger.server", "4,Console"},
            {"Logger.diff", "3,Console"},
            {"Logger.mmaps", "4,Console"},
            {"Logger.sql", "4,Console"},
            {"Logger.sql.sql", "2,Console"},
            {"Logger.time.update", "4,Console"},
            {"Logger.module", "4,Console"},
            {"Logger.spells.scripts", "2,Console"},
            {"Logger.scripts.hotswap", "4,Console"}};

        return options;
    }
}
