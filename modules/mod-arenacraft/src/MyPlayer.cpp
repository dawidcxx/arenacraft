/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

// Add player scripts
class MyPlayer : public PlayerScript
{
public:
    MyPlayer() : PlayerScript("MyPlayer") { }

    void OnCreate(Player* player) override
    {

        // set HS to starting zone in grizzly hills
        WorldLocation loc(4035.6206, -3756.447, 116.24431, 4.15822);
        player->SetHomebind(loc, 395);
        player->AddItem(6948, 1);
        
        // Add mount
        player->AddItem(46778, 1);
        
    }

    
};

// Add all scripts in one
void AddMyPlayerScripts()
{
    new MyPlayer();
};