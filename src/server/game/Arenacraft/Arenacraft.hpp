#include <stdexcept>

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

    namespace fns {
        PlayerRole GetRoleForClassAndSpec(ClassId classId, uint32_t specIndex)
        {
            switch(classId)
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
                    switch(specIndex)
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
                    switch(specIndex)
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
                    switch(specIndex)
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
                    switch(specIndex)
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

};