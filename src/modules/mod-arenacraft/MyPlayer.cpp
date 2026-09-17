/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

static void LearnWeaponSkills(Player *player);

// Add player scripts
class MyPlayer : public PlayerScript
{
public:
    MyPlayer() : PlayerScript("MyPlayer") {}

    void OnFirstLogin(Player *player) override
    {

        // set HS to starting zone in grizzly hills
        WorldLocation loc(4035.6206, -3756.447, 116.24431, 4.15822);
        player->SetHomebind(loc, 395);
        player->AddItem(6948, 1);

        // Add mount
        player->learnSpell(65917);

        LearnWeaponSkills(player);
    }
};

// Add all scripts in one
void AddMyPlayerScripts()
{
    new MyPlayer();
};

enum WeaponProficiencies
{
    BLOCK = 107,
    BOWS = 264,
    CROSSBOWS = 5011,
    DAGGERS = 1180,
    FIST_WEAPONS = 15590,
    GUNS = 266,
    ONE_H_AXES = 196,
    ONE_H_MACES = 198,
    ONE_H_SWORDS = 201,
    POLEARMS = 200,
    SHOOT = 5019,
    STAVES = 227,
    TWO_H_AXES = 197,
    TWO_H_MACES = 199,
    TWO_H_SWORDS = 202,
    WANDS = 5009,
    THROW_WAR = 2567
};

static void LearnWeaponSkills(Player *player)
{
    using ClassToWeapons = std::unordered_map<uint8, std::vector<WeaponProficiencies>>;
    static ClassToWeapons classToWeaponLookup = {
        {CLASS_WARRIOR, {
                            THROW_WAR,
                            TWO_H_SWORDS,
                            TWO_H_MACES,
                            TWO_H_AXES,
                            STAVES,
                            POLEARMS,
                            ONE_H_SWORDS,
                            ONE_H_MACES,
                            ONE_H_AXES,
                            GUNS,
                            FIST_WEAPONS,
                            DAGGERS,
                            CROSSBOWS,
                            BOWS,
                            BLOCK,
                        }},
        {CLASS_PRIEST, {
                           WANDS,
                           STAVES,
                           SHOOT,
                           ONE_H_MACES,
                           DAGGERS,
                       }},
        {CLASS_PALADIN, {
                            TWO_H_SWORDS,
                            TWO_H_MACES,
                            TWO_H_AXES,
                            POLEARMS,
                            ONE_H_SWORDS,
                            ONE_H_MACES,
                            ONE_H_AXES,
                            BLOCK,
                        }},
        {CLASS_ROGUE, {
                          ONE_H_SWORDS,
                          ONE_H_MACES,
                          ONE_H_AXES,
                          GUNS,
                          FIST_WEAPONS,
                          DAGGERS,
                          CROSSBOWS,
                          BOWS,
                      }},
        {CLASS_DEATH_KNIGHT, {
                                 TWO_H_SWORDS,
                                 TWO_H_MACES,
                                 TWO_H_AXES,
                                 POLEARMS,
                                 ONE_H_SWORDS,
                                 ONE_H_MACES,
                                 ONE_H_AXES,
                             }},
        {CLASS_MAGE, {
                         WANDS,
                         STAVES,
                         SHOOT,
                         ONE_H_SWORDS,
                         DAGGERS,
                     }},

        {CLASS_SHAMAN, {
                           TWO_H_MACES,
                           TWO_H_AXES,
                           STAVES,
                           ONE_H_MACES,
                           ONE_H_AXES,
                           FIST_WEAPONS,
                           DAGGERS,
                           BLOCK,
                       }},
        {CLASS_HUNTER, {
                           THROW_WAR,
                           TWO_H_SWORDS,
                           TWO_H_AXES,
                           STAVES,
                           POLEARMS,
                           ONE_H_SWORDS,
                           ONE_H_AXES,
                           GUNS,
                           FIST_WEAPONS,
                           DAGGERS,
                           CROSSBOWS,
                           BOWS,
                       }},
        {CLASS_DRUID, {
                          TWO_H_MACES,
                          STAVES,
                          POLEARMS,
                          ONE_H_MACES,
                          FIST_WEAPONS,
                          DAGGERS,
                      }},
        {CLASS_WARLOCK, {
                            WANDS,
                            STAVES,
                            SHOOT,
                            ONE_H_SWORDS,
                            DAGGERS,
                        }}};
    auto playerClassWepSkills = classToWeaponLookup.find(player->getClass())->second;
    for (auto playerClassWepSkill : playerClassWepSkills)
    {
        if (player->HasSpell(playerClassWepSkill))
            continue;
        player->learnSpell(playerClassWepSkill);
    }
    player->UpdateSkillsToMaxSkillsForLevel();
}