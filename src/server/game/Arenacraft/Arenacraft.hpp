#pragma once

#include <stdexcept>
#include <unordered_map>
#include <functional>
#include <random>
#include <algorithm>

namespace arenacraft
{
    enum ClassId
    {
        CLASS_WARRIOR,
        CLASS_ROGUE,
        CLASS_DEATH_KNIGHT,
        CLASS_HUNTER,
        CLASS_MAGE,
        CLASS_WARLOCK,
        CLASS_DRUID,
        CLASS_SHAMAN,
        CLASS_PALADIN,
        CLASS_PRIEST
    };

    enum PlayerRole
    {
        ROLE_MELEE,
        ROLE_CASTER,
        ROLE_HEALER
    };

    namespace fns
    {

        const char *GetClassName(ClassId classId)
        {
            switch (classId)
            {
            case CLASS_WARRIOR:
                return "Warrior";
            case CLASS_ROGUE:
                return "Rogue";
            case CLASS_DEATH_KNIGHT:
                return "Death Knight";
            case CLASS_HUNTER:
                return "Hunter";
            case CLASS_MAGE:
                return "Mage";
            case CLASS_WARLOCK:
                return "Warlock";
            case CLASS_DRUID:
                return "Druid";
            case CLASS_SHAMAN:
                return "Shaman";
            case CLASS_PALADIN:
                return "Paladin";
            case CLASS_PRIEST:
                return "Priest";
            default:
                throw new std::out_of_range("Invalid class index");
            }
        }

        PlayerRole GetRoleForClassAndSpec(ClassId classId, uint32_t specIndex)
        {
            switch (classId)
            {
            case CLASS_WARRIOR:
            case CLASS_ROGUE:
            case CLASS_DEATH_KNIGHT:
            case CLASS_HUNTER:
                return ROLE_MELEE;
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                return ROLE_CASTER;
            case CLASS_DRUID:
                switch (specIndex)
                {
                case 0:
                    return ROLE_CASTER;
                case 1:
                    return ROLE_MELEE;
                case 2:
                    return ROLE_HEALER;
                default:
                    throw new std::out_of_range("Invalid spec index");
                }
            case CLASS_SHAMAN:
                switch (specIndex)
                {
                case 0:
                    return ROLE_CASTER;
                case 1:
                    return ROLE_MELEE;
                case 2:
                    return ROLE_HEALER;
                default:
                    throw new std::out_of_range("Invalid spec index");
                }
            case CLASS_PALADIN:
                switch (specIndex)
                {
                case 0:
                    return ROLE_HEALER;
                case 1:
                    return ROLE_MELEE;
                case 2:
                    return ROLE_MELEE;
                default:
                    throw new std::out_of_range("Invalid spec index");
                }
            case CLASS_PRIEST:
                switch (specIndex)
                {
                case 0:
                    return ROLE_HEALER;
                case 1:
                    return ROLE_HEALER;
                case 2:
                    return ROLE_CASTER;
                default:
                    throw new std::out_of_range("Invalid spec index");
                }
            default:
                throw new std::out_of_range("Invalid class index");
            }
        }

    }

    namespace algorythm
    {
        template <typename K, typename V>
        void random_walk(const std::unordered_map<K, V> &map, std::function<void(const K &, const V &)> callback)
        {
            // Collect keys into a vector
            std::vector<K> keys;
            for (const auto &pair : map)
            {
                keys.push_back(pair.first);
            }

            std::mt19937 g(keys.size());
            std::shuffle(keys.begin(), keys.end(), g);

            // Iterate over the keys in random order and call the callback
            for (const K &key : keys)
            {
                callback(key, map.at(key));
            }
        }
    }

};