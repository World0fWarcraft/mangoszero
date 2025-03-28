#include "Config/Config.h"
#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "AccountMgr.h"
#include "RandomPlayerbotMgr.h"

class LoginQueryHolder;
class CharacterHandler;

/**
 * @brief Constructor for PlayerbotHolder.
 */
PlayerbotHolder::PlayerbotHolder() : PlayerbotAIBase()
{
    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); spellId++)
    {
        sSpellStore.LookupEntry(spellId);
    }
}

/**
 * @brief Destructor for PlayerbotHolder.
 */
PlayerbotHolder::~PlayerbotHolder()
{
    LogoutAllBots();
}

/**
 * @brief Updates the AI internal state.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotHolder::UpdateAIInternal(uint32 elapsed)
{
}

/**
 * @brief Updates the sessions for all player bots.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotHolder::UpdateSessions(uint32 elapsed)
{
    for (PlayerBotMap::const_iterator itr = GetPlayerBotsBegin(); itr != GetPlayerBotsEnd(); ++itr)
    {
        Player* const bot = itr->second;
        if (bot->IsBeingTeleported())
        {
            bot->GetPlayerbotAI()->HandleTeleportAck();
        }
        else if (bot->IsInWorld())
        {
            bot->GetSession()->HandleBotPackets();
        }
    }
}

/**
 * @brief Logs out all player bots.
 */
void PlayerbotHolder::LogoutAllBots()
{
    while (true)
    {
        PlayerBotMap::const_iterator itr = GetPlayerBotsBegin();
        if (itr == GetPlayerBotsEnd())
        {
            break;
        }
        Player* bot = itr->second;
        LogoutPlayerBot(bot->GetObjectGuid().GetRawValue());
    }
}

/**
 * @brief Logs out a specific player bot.
 * @param guid The GUID of the player bot to log out.
 */
void PlayerbotHolder::LogoutPlayerBot(uint64 guid)
{
    Player* bot = GetPlayerBot(guid);
    if (bot)
    {
        bot->GetPlayerbotAI()->TellMaster("Goodbye!");
        //bot->SaveToDB();

        WorldSession* botWorldSessionPtr = bot->GetSession();
        playerBots.erase(guid);    // deletes bot player ptr inside this WorldSession PlayerBotMap
        botWorldSessionPtr->LogoutPlayer(true); // this will delete the bot Player object and PlayerbotAI object
        delete botWorldSessionPtr;  // finally delete the bot's WorldSession
    }
}

/**
 * @brief Gets a player bot by its GUID.
 * @param playerGuid The GUID of the player bot.
 * @return Pointer to the player bot, or nullptr if not found.
 */
Player* PlayerbotHolder::GetPlayerBot(uint64 playerGuid) const
{
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? nullptr : it->second;
}

/**
 * @brief Handles the login of a player bot.
 * @param bot Pointer to the player bot.
 */
void PlayerbotHolder::OnBotLogin(Player* const bot)
{
    PlayerbotAI* ai = new PlayerbotAI(bot);
    bot->SetPlayerbotAI(ai);
    OnBotLoginInternal(bot);

    playerBots[bot->GetObjectGuid().GetRawValue()] = bot;

    Player* master = ai->GetMaster();
    if (master)
    {
        ObjectGuid masterGuid = master->GetObjectGuid();
        if (master->GetGroup() &&
            !master->GetGroup()->IsLeader(masterGuid))
            master->GetGroup()->ChangeLeader(masterGuid);
    }

    Group* group = bot->GetGroup();
    if (group)
    {
        bool groupValid = false;
        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::MemberSlotList::const_iterator i = slots.begin(); i != slots.end(); ++i)
        {
            ObjectGuid member = i->guid;
            uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(member);
            if (!sPlayerbotAIConfig.IsInRandomAccountList(account))
            {
                groupValid = true;
                break;
            }
        }

        if (!groupValid)
        {
            WorldPacket p;
            string member = bot->GetName();
            p << uint32(PARTY_OP_LEAVE) << member << uint32(0);
            bot->GetSession()->HandleGroupDisbandOpcode(p);
        }
    }

    ai->ResetStrategies();
    ai->TellMaster("Hello!");
}

bool PlayerbotHolder::ProcessBotCommand(string cmd, ObjectGuid guid, bool admin, uint32 masterAccountId)
{
    if (!sPlayerbotAIConfig.enabled || guid.IsEmpty())
    {
        return false;
    }

    bool isRandomBot = sRandomPlayerbotMgr.IsRandomBot(guid);
    bool isRandomAccount = sPlayerbotAIConfig.IsInRandomAccountList(sObjectMgr.GetPlayerAccountIdByGUID(guid));

    if (isRandomAccount && !isRandomBot && !admin)
    {
        return false;
    }

    if (cmd == "add" || cmd == "login")
    {
        if (sObjectMgr.GetPlayer(guid))
        {
            return false;
        }

        AddPlayerBot(guid.GetRawValue(), masterAccountId);
        return true;
    }
    else if (cmd == "remove" || cmd == "logout" || cmd == "rm")
    {
        if (!GetPlayerBot(guid.GetRawValue()))
        {
            return false;
        }

        LogoutPlayerBot(guid.GetRawValue());
        return true;
    }

    if (admin)
    {
        Player* bot = GetPlayerBot(guid.GetRawValue());
        if (!bot)
        {
            return false;
        }

        Player* master = bot->GetPlayerbotAI()->GetMaster();
        if (master)
        {
            if (cmd == "init=white" || cmd == "init=common")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_NORMAL);
                factory.CleanRandomize();
                return true;
            }
            else if (cmd == "init=green" || cmd == "init=uncommon")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_UNCOMMON);
                factory.CleanRandomize();
                return true;
            }
            else if (cmd == "init=blue" || cmd == "init=rare")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_RARE);
                factory.CleanRandomize();
                return true;
            }
            else if (cmd == "init=epic" || cmd == "init=purple")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_EPIC);
                factory.CleanRandomize();
                return true;
            }
        }

        if (cmd == "update")
        {
            PlayerbotFactory factory(bot, bot->getLevel());
            factory.Refresh();
            return true;
        }
        else if (cmd == "random")
        {
            sRandomPlayerbotMgr.Randomize(bot);
            return true;
        }
    }

    return false;
}

bool ChatHandler::HandlePlayerbotCommand(char* args)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        PSendSysMessage("|cffff0000Playerbot system is currently disabled!");
        SetSentErrorMessage(true);
        return false;
    }

    if (!m_session)
    {
        PSendSysMessage("You may only add bots from an active session");
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();
    PlayerbotMgr* mgr = player->GetPlayerbotMgr();
    if (!mgr)
    {
        PSendSysMessage("you cannot control bots yet");
        SetSentErrorMessage(true);
        return false;
    }

    list<string> messages = mgr->HandlePlayerbotCommand(args, player);
    if (messages.empty())
    {
        return true;
    }

    for (list<string>::iterator i = messages.begin(); i != messages.end(); ++i)
    {
        PSendSysMessage(i->c_str());
    }
    SetSentErrorMessage(true);
    return false;
}

list<string> PlayerbotHolder::HandlePlayerbotCommand(char* args, Player* master)
{
    list<string> messages;

    if (!*args)
    {
        messages.push_back("usage: add/init/remove PLAYERNAME");
        return messages;
    }

    char *cmd = strtok ((char*)args, " ");
    char *charname = strtok (NULL, " ");
    if (!cmd || !charname)
    {
        messages.push_back("usage: add/init/remove PLAYERNAME");
        return messages;
    }

    std::string cmdStr = cmd;
    std::string charnameStr = charname;

    set<string> bots;
    if (charnameStr == "*" && master)
    {
        Group* group = master->GetGroup();
        if (!group)
        {
            messages.push_back("you must be in group");
            return messages;
        }

        Group::MemberSlotList slots = group->GetMemberSlots();
        for (Group::member_citerator i = slots.begin(); i != slots.end(); i++)
        {
            ObjectGuid member = i->guid;

            if (member == master->GetObjectGuid())
            {
                continue;
            }

            string bot;
            if (sObjectMgr.GetPlayerNameByGUID(member, bot))
            {
                bots.insert(bot);
            }
        }
    }

    if (charnameStr == "!" && master && master->GetSession()->GetSecurity() > SEC_GAMEMASTER)
    {
        for (PlayerBotMap::const_iterator i = GetPlayerBotsBegin(); i != GetPlayerBotsEnd(); ++i)
        {
            Player* bot = i->second;
            if (bot && bot->IsInWorld())
            {
                bots.insert(bot->GetName());
            }
        }
    }

    vector<string> chars = split(charnameStr, ',');
    for (vector<string>::iterator i = chars.begin(); i != chars.end(); i++)
    {
        string s = *i;

        uint32 accountId = GetAccountId(s);
        if (!accountId)
        {
            bots.insert(s);
            continue;
        }

        QueryResult* results = CharacterDatabase.PQuery(
            "SELECT `name` FROM `characters` WHERE `account` = '%u'",
            accountId);
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                string charName = fields[0].GetCppString();
                bots.insert(charName);
            } while (results->NextRow());

            delete results;
        }
    }

    for (set<string>::iterator i = bots.begin(); i != bots.end(); ++i)
    {
        string bot = *i;
        ostringstream out;
        out << cmdStr << ": " << bot << " - ";

        ObjectGuid member = sObjectMgr.GetPlayerGuidByName(bot);
        bool result = false;
        if (master && member != master->GetObjectGuid())
        {
            result = ProcessBotCommand(cmdStr, member, master->GetSession()->GetSecurity() >= SEC_GAMEMASTER, master->GetSession()->GetAccountId());
        }
        else if (!master)
        {
            result = ProcessBotCommand(cmdStr, member, true, -1);
        }

        out << (result ? "ok" : "not allowed");
        messages.push_back(out.str());
    }

    return messages;
}

/**
 * @brief Gets the account ID for a given name.
 * @param name The name of the account.
 * @return The account ID.
 */
uint32 PlayerbotHolder::GetAccountId(string name)
{
    uint32 accountId = 0;

    QueryResult* results = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", name.c_str());
    if (results)
    {
        Field* fields = results->Fetch();
        accountId = fields[0].GetUInt32();
        delete results;
    }

    return accountId;
}




/**
 * @brief Constructor for PlayerbotMgr.
 * @param master Pointer to the master player.
 */
PlayerbotMgr::PlayerbotMgr(Player* const master) : PlayerbotHolder(), master(master)
{
}

/**
 * @brief Destructor for PlayerbotMgr.
 */
PlayerbotMgr::~PlayerbotMgr()
{
}

/**
 * @brief Updates the AI internal state.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotMgr::UpdateAIInternal(uint32 elapsed)
{
    SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
}

/**
 * @brief Handles a command from the master.
 * @param type The type of the command.
 * @param text The text of the command.
 */
void PlayerbotMgr::HandleCommand(uint32 type, const string& text)
{
    Player* master = GetMaster();
    if (!master)
    {
        return;
    }

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == master)
        {
            bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
        }
    }
}

/**
 * @brief Handles an incoming packet from the master.
 * @param packet The incoming packet.
 */
void PlayerbotMgr::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    // Iterate through all player bots and handle the incoming packet
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
    }

    // Iterate through all random player bots and handle the incoming packet if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
        }
    }

    // Handle specific packet opcodes
    switch (packet.GetOpcode())
    {
        // If the master is logging out, log out all bots
        case CMSG_LOGOUT_REQUEST:
        {
            LogoutAllBots();
            return;
        }
    }
}

/**
 * @brief Handles an outgoing packet to the master.
 * @param packet The outgoing packet.
 */
void PlayerbotMgr::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    // Iterate through all player bots and handle the outgoing packet
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
    }

    // Iterate through all random player bots and handle the outgoing packet if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
        }
    }
}

/**
 * @brief Saves all player bots to the database.
 */
void PlayerbotMgr::SaveToDB()
{
    // Iterate through all player bots and save them to the database
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->SaveToDB();
    }

    // Iterate through all random player bots and save them to the database if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->SaveToDB();
        }
    }
}

/**
 * @brief Internal handler for bot login.
 * @param bot Pointer to the player bot.
 */
void PlayerbotMgr::OnBotLoginInternal(Player * const bot)
{
    // Set the master for the bot and reset its strategies
    bot->GetPlayerbotAI()->SetMaster(master);
    bot->GetPlayerbotAI()->ResetStrategies();
}

