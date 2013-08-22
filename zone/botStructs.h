#ifndef BOT_STRUCTS
#define BOT_STRUCTS

#ifdef BOTS

#include "../common/types.h"

#include <sstream>

using namespace std;

struct BotsAvailableList {
	uint32 BotID;
	char BotName[64];
	uint16 BotClass;
	uint8 BotLevel;
	uint16 BotRace;
};

struct BotGroup {
	uint32 BotGroupID;
	uint32 BotID;
};

struct BotGroupList {
	std::string BotGroupName;
	std::string BotGroupLeaderName;
};

struct SpawnedBotsList {
	char BotName[64];
	char ZoneName[64];
	uint32 BotLeaderCharID;
};

struct BotSpell {
	int16 SpellId;
	int SpellIndex;
	sint16 ManaCost;
};

#endif // BOTS

#endif // BOT_STRUCTS
