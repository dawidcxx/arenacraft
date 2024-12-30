#include "ScriptMgr.h"
#include "Battleground.h"
#include <RedisConn.h>
#include <cstdint>
#include "ArenaTeamMgr.h"

struct ArenaTeamMate
{
    std::string characterName;
    uint8_t classId;
};

struct ArenaStartedEvent
{
    uint32_t instanceId;
    u_int8_t arenaType;
    std::vector<ArenaTeamMate> team1;
    std::vector<ArenaTeamMate> team2;
};

struct ArenaEndedEvent
{
    uint32_t instanceId;
};

std::string ArenaStartedEventToJson(const ArenaStartedEvent &event);
std::string ArenaEndedEventToJson(const ArenaEndedEvent &event);

class ArenaNotificationService : public ArenaScript
{
private:
public:
    ArenaNotificationService() : ArenaScript("ArenaNotificationService")
    {
        std::cout << "ArenaNotificationService loaded" << std::endl;
    }
    ~ArenaNotificationService()
    {
    }

    void OnArenaEnd(Battleground *bg) override
    {
        ArenaEndedEvent event;
        event.instanceId = bg->GetInstanceID();
        sRedisConn.publishMessage("arena-ended", ArenaEndedEventToJson(event).c_str());
    }

    void OnArenaStart(Battleground *bg) override
    {
        auto arenaTypeId = bg->GetArenaType();
        auto team1Id = bg->GetArenaTeamIdForTeam(TeamId::TEAM_ALLIANCE);
        auto team2Id = bg->GetArenaTeamIdForTeam(TeamId::TEAM_HORDE);
        auto team1 = sArenaTeamMgr->GetArenaTeamById(team1Id);
        auto team2 = sArenaTeamMgr->GetArenaTeamById(team1Id);

        ArenaStartedEvent event;
        event.instanceId = bg->GetInstanceID();
        event.arenaType = arenaTypeId;
        for (const auto &member : team1->GetMembers())
        {
            ArenaTeamMate mate;
            mate.characterName = member.Name;
            mate.classId = member.Class;
            event.team1.push_back(mate);
        }
        for (const auto &member : team2->GetMembers())
        {
            ArenaTeamMate mate;
            mate.characterName = member.Name;
            mate.classId = member.Class;
            event.team2.push_back(mate);
        }
        sRedisConn.publishMessage("arena-started", ArenaStartedEventToJson(event).c_str());
    }
};

std::string ArenaEndedEventToJson(const ArenaEndedEvent &event)
{
    std::string json = "{";
    json += "\"instanceId\": " + std::to_string(event.instanceId);
    json += "}";
    return json;
}

std::string ArenaStartedEventToJson(const ArenaStartedEvent &event)
{
    std::string json = "{";
    json += "\"instanceId\": " + std::to_string(event.instanceId) + ",";
    json += "\"team1\": [";
    for (const auto &mate : event.team1)
    {
        json += "{";
        json += "\"characterName\": \"" + (mate.characterName) + "\",";
        json += "\"classId\": " + std::to_string(mate.classId);
        json += "},";
    }
    json.pop_back();
    json += "],";
    json += "\"team2\": [";
    for (const auto &mate : event.team2)
    {
        json += "{";
        json += "\"characterName\": \"" + (mate.characterName) + "\",";
        json += "\"classId\": " + std::to_string(mate.classId);
        json += "},";
    }
    json.pop_back();
    json += "]";
    json += "}";
    return json;
}

void AddArenaNotificationServiceScripts()
{
    new ArenaNotificationService();
}