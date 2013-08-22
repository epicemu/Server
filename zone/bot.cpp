#ifdef BOTS

#include "bot.h"
#include "object.h"
#include "doors.h"
#include "QuestParserCollection.h"

// This constructor is used during the bot create command
Bot::Bot(NPCType npcTypeData, Client* botOwner) : NPC(&npcTypeData, 0, 0, 0, 0, 0, 0, false), rest_timer(1) {
	if(botOwner) {
		this->SetBotOwner(botOwner);
		this->_botOwnerCharacterID = botOwner->CharacterID();
	}
	else {
		this->SetBotOwner(0);
		this->_botOwnerCharacterID = 0;
	}

	_guildRank = 0;
	_guildId = 0;

	_lastTotalPlayTime = 0;
	_startTotalPlayTime = time(&_startTotalPlayTime);
	_lastZoneId = 0;

	_baseMR = npcTypeData.MR;
	_baseCR = npcTypeData.CR;
	_baseDR = npcTypeData.DR;
	_baseFR = npcTypeData.FR;
	_basePR = npcTypeData.PR;
	_baseCorrup = npcTypeData.Corrup;
	_baseAC = npcTypeData.AC;
	_baseSTR = npcTypeData.STR;
	_baseSTA = npcTypeData.STA;
	_baseDEX = npcTypeData.DEX;
	_baseAGI = npcTypeData.AGI;
	_baseINT = npcTypeData.INT;
	_baseWIS = npcTypeData.WIS;
	_baseCHA = npcTypeData.CHA;
	_baseATK = npcTypeData.ATK;
	_baseRace = npcTypeData.race;
	_baseGender = npcTypeData.gender;
	RestRegenHP = 0;
	RestRegenMana = 0;
	
	SetBotID(0);
	SetBotSpellID(0);
	SetSpawnStatus(false);
	SetBotArcher(false);
	SetBotCharmer(false);
	SetPetChooser(false);
	SetRangerAutoWeaponSelect(false);
	rest_timer.Disable();

	SetFollowDistance(184);

	// Do this once and only in this constructor
	GenerateAppearance();

	GenerateBaseStats();
	GenerateArmorClass();

	// Calculate HitPoints Last As It Uses Base Stats
	cur_hp = GenerateBaseHitPoints();
	cur_mana = GenerateBaseManaPoints();

	hp_regen = CalcHPRegen();
	mana_regen = CalcManaRegen();

	for (int i = 0; i < MaxSpellTimer; i++)
	{
		spellRecastTimers[i] = 0;
	}

	strcpy(this->name, this->GetCleanName());
}

// This constructor is used when the bot is loaded out of the database
Bot::Bot(uint32 botID, uint32 botOwnerCharacterID, uint32 botSpellsID, double totalPlayTime, int32 lastZoneId, NPCType npcTypeData) : NPC(&npcTypeData, 0, 0, 0, 0, 0, 0, false), rest_timer(1) {
	this->_botOwnerCharacterID = botOwnerCharacterID;

	if(this->_botOwnerCharacterID > 0) {
		this->SetBotOwner(entity_list.GetClientByCharID(this->_botOwnerCharacterID));
	}

	_guildRank = 0;
	_guildId = 0;

	_lastTotalPlayTime = totalPlayTime;
	_startTotalPlayTime = time(&_startTotalPlayTime);
	_lastZoneId = lastZoneId;

	_baseMR = npcTypeData.MR;
	_baseCR = npcTypeData.CR;
	_baseDR = npcTypeData.DR;
	_baseFR = npcTypeData.FR;
	_basePR = npcTypeData.PR;
	_baseCorrup = npcTypeData.Corrup;
	_baseAC = npcTypeData.AC;
	_baseSTR = npcTypeData.STR;
	_baseSTA = npcTypeData.STA;
	_baseDEX = npcTypeData.DEX;
	_baseAGI = npcTypeData.AGI;
	_baseINT = npcTypeData.INT;
	_baseWIS = npcTypeData.WIS;
	_baseCHA = npcTypeData.CHA;
	_baseATK = npcTypeData.ATK;
	_baseRace = npcTypeData.race;
	_baseGender = npcTypeData.gender;
	RestRegenHP = 0;
	RestRegenMana = 0;

	SetBotID(botID);
	SetBotSpellID(botSpellsID);
	SetSpawnStatus(false);
	SetBotArcher(false);
	SetBotCharmer(false);
	SetPetChooser(false);
	SetRangerAutoWeaponSelect(false);
	rest_timer.Disable();

	SetFollowDistance(184);

	strcpy(this->name, this->GetCleanName());

	LoadGuildMembership(&_guildId, &_guildRank, &_guildName);

	std::string TempErrorMessage;

	EquipBot(&TempErrorMessage);

	if(!TempErrorMessage.empty()) {
		// TODO: log error message to zone error log
		if(GetBotOwner())
			GetBotOwner()->Message(13, TempErrorMessage.c_str());
	}

	for (int i = 0; i < MaxSpellTimer; i++)
	{
		spellRecastTimers[i] = 0;
	}

	GenerateBaseStats();

	// Load saved buffs
	LoadBuffs();

	CalcBotStats(false);

	hp_regen = CalcHPRegen();
	mana_regen = CalcManaRegen();

	cur_hp = max_hp;
	cur_mana = max_mana;
}

Bot::~Bot() {
	AI_Stop();

	if(HasGroup())
		Bot::RemoveBotFromGroup(this, GetGroup());

	if(HasPet())
		GetPet()->Depop();

	entity_list.RemoveBot(GetID());
}

void Bot::SetBotID(uint32 botID) {
	this->_botID = botID;
	this->npctype_id = botID;
}

void Bot::SetBotSpellID(uint32 newSpellID) {
	this->npc_spells_id = newSpellID;
}

uint32 Bot::GetBotArcheryRange() {
	uint32 result = 0;

	ItemInst* rangeItem = GetBotItem(SLOT_RANGE);

	if(!rangeItem)
		return 0;

	const Item_Struct* botweapon = rangeItem->GetItem();
	
	uint32 archeryMaterial;
	uint32 archeryColor;
	uint32 archeryBowID;
	uint32 archeryAmmoID; 

	if(botweapon && botweapon->ItemType == ItemTypeBow) {
		uint32 range = 0;

		archeryMaterial = atoi(botweapon->IDFile + 2);
		archeryBowID = botweapon->ID;
		archeryColor = botweapon->Color;
		range =+ botweapon->Range;

		rangeItem = GetBotItem(SLOT_AMMO);
		if(rangeItem)
			botweapon = rangeItem->GetItem();

		if(!botweapon || (botweapon->ItemType != ItemTypeArrow)) {
			return 0;
		}

		range += botweapon->Range;
		
		archeryAmmoID = botweapon->ID;

		result = range;
	}

	return result;
}

void Bot::ChangeBotArcherWeapons(bool isArcher) {
	if((GetClass()==WARRIOR) || (GetClass()==PALADIN) || (GetClass()==RANGER)
		|| (GetClass()==SHADOWKNIGHT) || (GetClass()==ROGUE)) 
	{
		if(!isArcher) {
			BotAddEquipItem(SLOT_PRIMARY, GetBotItemBySlot(SLOT_PRIMARY));		
			BotAddEquipItem(SLOT_SECONDARY, GetBotItemBySlot(SLOT_SECONDARY));
			//archerbot->SendWearChange(MATERIAL_PRIMARY);
			//archerbot->SendWearChange(MATERIAL_SECONDARY);
			Say("My blade is ready.");
		}
		else {
			//archerbot->SendWearChange(MATERIAL_PRIMARY);
			//archerbot->SendWearChange(MATERIAL_SECONDARY);
			BotRemoveEquipItem(SLOT_PRIMARY);
			BotRemoveEquipItem(SLOT_SECONDARY);
			//archerbot->SendBotArcheryWearChange(MATERIAL_PRIMARY, archeryMaterial, archeryColor);
			BotAddEquipItem(SLOT_AMMO, GetBotItemBySlot(SLOT_AMMO));
			BotAddEquipItem(SLOT_SECONDARY, GetBotItemBySlot(SLOT_RANGE));
			Say("My bow is true and ready.");
		}
	}
	else {
		Say("I don't know how to use a bow.");
	}
}

void Bot::Sit() {
	if(IsMoving()) {
		moved = false;
		// SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
		SendPosition();
		SetMoving(false);
		tar_ndx = 0;
	}

	SetAppearance(eaSitting);
}

void Bot::Stand() {
	SetAppearance(eaStanding);
}

bool Bot::IsSitting() {
	bool result = false;

	if(GetAppearance() == eaSitting && !IsMoving())
		result = true;

	return result;
}

bool Bot::IsStanding() {
	bool result = false;

	if(GetAppearance() == eaStanding)
		result = true;

	return result;
}

NPCType Bot::FillNPCTypeStruct(uint32 botSpellsID, std::string botName, std::string botLastName, uint8 botLevel, uint16 botRace, uint8 botClass, uint8 gender, float size, uint32 face, uint32 hairStyle, uint32 hairColor, uint32 eyeColor, uint32 eyeColor2, uint32 beardColor, uint32 beard, uint32 drakkinHeritage, uint32 drakkinTattoo, uint32 drakkinDetails, sint16 mr, sint16 cr, sint16 dr, sint16 fr, sint16 pr, sint16 corrup, sint16 ac, uint16 str, uint16 sta, uint16 dex, uint16 agi, uint16 _int, uint16 wis, uint16 cha, uint16 attack) {
	NPCType BotNPCType;
	int CopyLength = 0;

	CopyLength = botName.copy(BotNPCType.name, 63);
	BotNPCType.name[CopyLength] = '\0';
	CopyLength = 0;

	CopyLength = botLastName.copy(BotNPCType.lastname, 69);
	BotNPCType.lastname[CopyLength] = '\0';
	CopyLength = 0;

	BotNPCType.npc_spells_id = botSpellsID;
	BotNPCType.level = botLevel;
	BotNPCType.race = botRace;
	BotNPCType.class_ = botClass;
	BotNPCType.gender = gender;
	BotNPCType.size = size;
	BotNPCType.luclinface = face;
	BotNPCType.hairstyle = hairStyle;
	BotNPCType.haircolor = hairColor;
	BotNPCType.eyecolor1 = eyeColor;
	BotNPCType.eyecolor2 = eyeColor2;
	BotNPCType.beardcolor = beardColor;
	BotNPCType.beard = beard;
	BotNPCType.drakkin_heritage = drakkinHeritage;
	BotNPCType.drakkin_tattoo = drakkinTattoo;
	BotNPCType.drakkin_details = drakkinDetails;
	BotNPCType.MR = mr;
	BotNPCType.CR = cr;
	BotNPCType.DR = dr;
	BotNPCType.FR = fr;
	BotNPCType.PR = pr;
	BotNPCType.Corrup = corrup;
	BotNPCType.AC = ac;
	BotNPCType.STR = str;
	BotNPCType.STA = sta;
	BotNPCType.DEX = dex;
	BotNPCType.AGI = agi;
	BotNPCType.INT = _int;
	BotNPCType.WIS = wis;
	BotNPCType.CHA = cha;
	BotNPCType.ATK = attack;

	BotNPCType.npc_id = 0;
	BotNPCType.texture = 0;
	BotNPCType.d_meele_texture1 = 0;
	BotNPCType.d_meele_texture2 = 0;
	BotNPCType.qglobal = false;
	BotNPCType.attack_speed = 0;
	BotNPCType.runspeed = 1.25;
	BotNPCType.bodytype = 1;
	BotNPCType.findable = 0;
	BotNPCType.hp_regen = 1;
	BotNPCType.mana_regen = 1;
	BotNPCType.maxlevel = botLevel;

	return BotNPCType;
}

NPCType Bot::CreateDefaultNPCTypeStructForBot(std::string botName, std::string botLastName, uint8 botLevel, uint16 botRace, uint8 botClass, uint8 gender) {
	NPCType Result;
	int CopyLength = 0;

	CopyLength = botName.copy(Result.name, 63);
	Result.name[CopyLength] = '\0';
	CopyLength = 0;

	CopyLength = botLastName.copy(Result.lastname, 69);
	Result.lastname[CopyLength] = '\0';
	CopyLength = 0;

	Result.level = botLevel;
	Result.race = botRace;
	Result.class_ = botClass;
	Result.gender = gender;

	// default values
	Result.maxlevel = botLevel;
	Result.size = 6.0;
	Result.npc_id = 0;
	Result.cur_hp = 0;
	Result.drakkin_details = 0;
	Result.drakkin_heritage = 0;
	Result.drakkin_tattoo = 0;
	Result.runspeed = 1.25;
	Result.bodytype = 1;
	Result.findable = 0;
	Result.hp_regen = 1;
	Result.mana_regen = 1;
	Result.texture = 0;
	Result.d_meele_texture1 = 0;
	Result.d_meele_texture2 = 0;
	Result.qglobal = false;
	Result.npc_spells_id = 0;
	Result.attack_speed = 0;
	Result.STR = 75;
	Result.STA = 75;
	Result.DEX = 75;
	Result.AGI = 75;
	Result.WIS = 75;
	Result.INT = 75;
	Result.CHA = 75;
	Result.ATK = 75;
	Result.MR = 25;
	Result.FR = 25;
	Result.DR = 15;
	Result.PR = 15;
	Result.CR = 25;
	Result.Corrup = 15;
	Result.AC = 12;

	return Result;
}

void Bot::GenerateBaseStats() {
	int BotSpellID = 0;

	// base stats
	uint16 Strength = _baseSTR;
	uint16 Stamina = _baseSTA;
	uint16 Dexterity = _baseDEX;
	uint16 Agility = _baseAGI;
	uint16 Wisdom = _baseWIS;
	uint16 Intelligence = _baseINT;
	uint16 Charisma = _baseCHA;
	uint16 Attack = _baseATK;
	sint16 MagicResist = _baseMR;
	sint16 FireResist = _baseFR;
	sint16 DiseaseResist = _baseDR;
	sint16 PoisonResist = _basePR;
	sint16 ColdResist = _baseCR;
	sint16 CorruptionResist = _baseCorrup;

	switch(this->GetClass()) {
			case 1: // Warrior
				Strength += 10;
				Stamina += 20;
				Agility += 10;
				Dexterity += 10;
				Attack += 12;
				MagicResist += 2;
				break;
			case 2: // Cleric
				BotSpellID = 701;
				Strength += 5;
				Stamina += 5;
				Agility += 10;
				Wisdom += 30;
				Attack += 8;
				break;
			case 3: // Paladin
				BotSpellID = 708;
				Strength += 15;
				Stamina += 5;
				Wisdom += 15;
				Charisma += 10;
				Dexterity += 5;
				Attack += 17;
				DiseaseResist += 8;
				break;
			case 4: // Ranger
				BotSpellID = 710;
				Strength += 15;
				Stamina += 10;
				Agility += 10;
				Wisdom += 15;
				Attack += 17;
				FireResist += 4;
				ColdResist += 4;
				break;
			case 5: // Shadowknight
				BotSpellID = 709;
				Strength += 10;
				Stamina += 15;
				Intelligence += 20;
				Charisma += 5;
				Attack += 17;
				PoisonResist += 4;
				DiseaseResist += 4;
				break;
			case 6: // Druid
				BotSpellID = 707;
				Stamina += 15;
				Wisdom += 35;
				Attack += 5;
				break;
			case 7: // Monk
				Strength += 5;
				Stamina += 15;
				Agility += 15;
				Dexterity += 15;
				Attack += 17;
				break;
			case 8: // Bard
				BotSpellID = 711;
				Strength += 15;
				Dexterity += 10;
				Charisma += 15;
				Intelligence += 10;
				Attack += 17;
				break;
			case 9: // Rogue
				Strength += 10;
				Stamina += 20;
				Agility += 10;
				Dexterity += 10;
				Attack += 12;
				PoisonResist += 8;
				break;
			case 10: // Shaman
				BotSpellID = 706;
				Stamina += 10;
				Wisdom += 30;
				Charisma += 10;
				Attack += 28;
				break;
			case 11: // Necromancer
				BotSpellID = 703;
				Dexterity += 10;
				Agility += 10;
				Intelligence += 30;
				Attack += 5;
				break;
			case 12: // Wizard
				BotSpellID = 702;
				Stamina += 20;
				Intelligence += 30;
				Attack += 5;
				break;
			case 13: // Magician
				BotSpellID = 704;
				Stamina += 20;
				Intelligence += 30;
				Attack += 5;
				break;
			case 14: // Enchanter
				BotSpellID = 705;
				Intelligence += 25;
				Charisma += 25;
				Attack += 5;
				break;
			case 15: // Beastlord
				BotSpellID = 712;
				Stamina += 10;
				Agility += 10;
				Dexterity += 5;
				Wisdom += 20;
				Charisma += 5;
				Attack += 31;
				break;
			case 16: // Berserker
				Strength += 10;
				Stamina += 15;
				Dexterity += 15;
				Agility += 10;
				Attack += 25;
				break;
	}

	float BotSize = GetSize();

	switch(this->GetRace()) {
			case 1: // Humans have no race bonus
				break;
			case 2: // Barbarian
				Strength += 28;
				Stamina += 20;
				Agility += 7;
				Dexterity -= 5;
				Wisdom -= 5;
				Intelligence -= 10;
				Charisma -= 20;
				BotSize = 7.0;
				ColdResist += 10;
				break;
			case 3: // Erudite
				Strength -= 15;
				Stamina -= 5;
				Agility -= 5;
				Dexterity -= 5;
				Wisdom += 8;
				Intelligence += 32;
				Charisma -= 5;
				MagicResist += 5;
				DiseaseResist -= 5;
				break;
			case 4: // Wood Elf
				Strength -= 10;
				Stamina -= 10;
				Agility += 20;
				Dexterity += 5;
				Wisdom += 5;
				BotSize = 5.0;
				break;
			case 5: // High Elf
				Strength -= 20;
				Stamina -= 10;
				Agility += 10;
				Dexterity -= 5;
				Wisdom += 20;
				Intelligence += 12;
				Charisma += 5;
				break;
			case 6: // Dark Elf
				Strength -= 15;
				Stamina -= 10;
				Agility += 15;
				Wisdom += 8;
				Intelligence += 24;
				Charisma -= 15;
				BotSize = 5.0;
				break;
			case 7: // Half Elf
				Strength -= 5;
				Stamina -= 5;
				Agility += 15;
				Dexterity += 10;
				Wisdom -= 15;
				BotSize = 5.5;
				break;
			case 8: // Dwarf
				Strength += 15;
				Stamina += 15;
				Agility -= 5;
				Dexterity += 15;
				Wisdom += 8;
				Intelligence -= 15;
				Charisma -= 30;
				BotSize = 4.0;
				MagicResist -= 5;
				PoisonResist += 5;
				break;
			case 9: // Troll
				Strength += 33;
				Stamina += 34;
				Agility += 8;
				Wisdom -= 15;
				Intelligence -= 23;
				Charisma -= 35;
				BotSize = 8.0;
				FireResist -= 20;
				break;
			case 10: // Ogre
				Strength += 55;
				Stamina += 77;
				Agility -= 5;
				Dexterity -= 5;
				Wisdom -= 8;
				Intelligence -= 15;
				Charisma -= 38;
				BotSize = 9.0;
				break;
			case 11: // Halfling
				Strength -= 5;
				Agility += 20;
				Dexterity += 15;
				Wisdom += 5;
				Intelligence -= 8;
				Charisma -= 25;
				BotSize = 3.5;
				PoisonResist += 5;
				DiseaseResist += 5;
				break;
			case 12: // Gnome
				Strength -= 15;
				Stamina -= 5;
				Agility += 10;
				Dexterity += 10;
				Wisdom -= 8;
				Intelligence += 23;
				Charisma -= 15;
				BotSize = 3.0;
				break;
			case 128: // Iksar
				Strength -= 5;
				Stamina -= 5;
				Agility += 15;
				Dexterity += 10;
				Wisdom += 5;
				Charisma -= 20;
				MagicResist -= 5;
				FireResist -= 5;
				break;
			case 130: // Vah Shir
				Strength += 15;
				Agility += 15;
				Dexterity -= 5;
				Wisdom -= 5;
				Intelligence -= 10;
				Charisma -= 10;
				BotSize = 7.0;
				MagicResist -= 5;
				FireResist -= 5;
				break;
			case 330: // Froglok
				Strength -= 5;
				Stamina += 5;
				Agility += 25;
				Dexterity += 25;
				Charisma -= 25;
				BotSize = 5.0;
				MagicResist -= 5;
				FireResist -= 5;
				break;
			case 522: // Drakkin
				Strength -= 5;
				Stamina += 5;
				Agility += 10;
				Intelligence += 10;
				Wisdom += 5;
				BotSize = 5.0;
				PoisonResist += 2;
				DiseaseResist += 2;
				MagicResist += 2;
				FireResist += 2;
				ColdResist += 2;
				break;
	}

	this->STR = Strength;
	this->STA = Stamina;
	this->DEX = Dexterity;
	this->AGI = Agility;
	this->WIS = Wisdom;
	this->INT = Intelligence;
	this->CHA = Charisma;
	this->ATK = Attack;
	this->MR = MagicResist;
	this->FR = FireResist;
	this->DR = DiseaseResist;
	this->PR = PoisonResist;
	this->CR = ColdResist;
	this->Corrup = CorruptionResist;
	SetBotSpellID(BotSpellID);
	this->size = BotSize;
}

void Bot::GenerateAppearance() {
	// Randomize facial appearance
	int iFace = 0;
	if(this->GetRace() == 2) { // Barbarian w/Tatoo
		iFace = MakeRandomInt(0, 79);
	}
	else {
		iFace = MakeRandomInt(0, 7);
	}

	int iHair = 0;
	int iBeard = 0;
	int iBeardColor = 1;
	if(this->GetRace() == 522) {
		iHair = MakeRandomInt(0, 8);
		iBeard = MakeRandomInt(0, 11);
		iBeardColor = MakeRandomInt(0, 3);
	}
	else if(this->GetGender()) {
		iHair = MakeRandomInt(0, 2);
		if(this->GetRace() == 8) { // Dwarven Females can have a beard
			if(MakeRandomInt(1, 100) < 50) {
				iFace += 10;
			}
		}
	}
	else {
		iHair = MakeRandomInt(0, 3);
		iBeard = MakeRandomInt(0, 5);
		iBeardColor = MakeRandomInt(0, 19);
	}

	int iHairColor = 0;
	if(this->GetRace() == 522) {
		iHairColor = MakeRandomInt(0, 3);
	}
	else {
		iHairColor = MakeRandomInt(0, 19);
	}

	int8 iEyeColor1 = (int8)MakeRandomInt(0, 9);
	int8 iEyeColor2 = 0;
	if(this->GetRace() == 522) {
		iEyeColor1 = iEyeColor2 = (int8)MakeRandomInt(0, 11);
	}
	else if(MakeRandomInt(1, 100) > 96) {
		iEyeColor2 = MakeRandomInt(0, 9);
	}
	else {
		iEyeColor2 = iEyeColor1;
	}

	int iHeritage = 0;
	int iTattoo = 0;
	int iDetails = 0;
	if(this->GetRace() == 522) {
		iHeritage = MakeRandomInt(0, 6);
		iTattoo = MakeRandomInt(0, 7);
		iDetails = MakeRandomInt(0, 7);
	}

	this->luclinface = iFace;
	this->hairstyle = iHair;
	this->beard = iBeard;
	this->beardcolor = iBeardColor;
	this->haircolor = iHairColor;
	this->eyecolor1 = iEyeColor1;
	this->eyecolor2 = iEyeColor2;
	this->drakkin_heritage = iHeritage;
	this->drakkin_tattoo = iTattoo;
	this->drakkin_details = iDetails;

}

sint16 Bot::acmod()
{
	int agility = GetAGI();
	int level = GetLevel();
	if(agility < 1 || level < 1)
		return 0;

	if(agility <= 74)
	{
		if(agility == 1)
			return -24;
		else if(agility <=3)
			return -23;
		else if(agility == 4)
			return -22;
		else if(agility <=6)
			return -21;
		else if(agility <=8)
			return -20;
		else if(agility == 9)
			return -19;
		else if(agility <=11)
			return -18;
		else if(agility == 12)
			return -17;
		else if(agility <=14)
			return -16;
		else if(agility <=16)
			return -15;
		else if(agility == 17)
			return -14;
		else if(agility <=19)
			return -13;
		else if(agility == 20)
			return -12;
		else if(agility <=22)
			return -11;
		else if(agility <=24)
			return -10;
		else if(agility == 25)
			return -9;
		else if(agility <=27)
			return -8;
		else if(agility == 28)
			return -7;
		else if(agility <=30)
			return -6;
		else if(agility <=32)
			return -5;
		else if(agility == 33)
			return -4;
		else if(agility <=35)
			return -3;
		else if(agility == 36)
			return -2;
		else if(agility <=38)
			return -1;
		else if(agility <=65)
			return 0;
		else if(agility <=70)
			return 1;
		else if(agility <=74)
			return 5;
	}
	else if(agility <= 137)
	{
		if(agility == 75)
		{
			if(level <= 6)
				return 9;
			else if(level <= 19)
				return 23;
			else if(level <= 39)
				return 33;
			else
				return 39;
		}
		else if(agility >= 76 && agility <= 79)
		{
			if(level <= 6)
				return 10;
			else if(level <= 19)
				return 23;
			else if(level <= 39)
				return 33;
			else
				return 40;
		}
		else if(agility == 80)
		{
			if(level <= 6)
				return 11;
			else if(level <= 19)
				return 24;
			else if(level <= 39)
				return 34;
			else
				return 41;
		}
		else if(agility >= 81 && agility <= 85)
		{
			if(level <= 6)
				return 12;
			else if(level <= 19)
				return 25;
			else if(level <= 39)
				return 35;
			else
				return 42;
		}
		else if(agility >= 86 && agility <= 90)
		{
			if(level <= 6)
				return 12;
			else if(level <= 19)
				return 26;
			else if(level <= 39)
				return 36;
			else
				return 42;
		}
		else if(agility >= 91 && agility <= 95)
		{
			if(level <= 6)
				return 13;
			else if(level <= 19)
				return 26;
			else if(level <= 39)
				return 36;
			else
				return 43;
		}
		else if(agility >= 96 && agility <= 99){
			if(level <= 6)
				return 14;
			else if(level <= 19)
				return 27;
			else if(level <= 39)
				return 37;
			else 
				return 44;
		}
		else if(agility == 100 && level >= 7)
		{
			if(level <= 19)
				return 28;
			else if (level <= 39)
				return 38;
			else
				return 45;
		}
		else if(level <= 6)
		{
			return 15;
		}
		//level is >6
		else if(agility >= 101 && agility <= 105)
		{
			if(level <= 19)
				return 29;
			else if(level <= 39)
				return 39;// not verified
			else
				return 45;
		}
		else if(agility >= 106 && agility <= 110)
		{
			if(level <= 19)
				return 29;
			else if(level <= 39)
				return 39;// not verified
			else
				return 46;
		}
		else if(agility >= 111 && agility <= 115)
		{
			if(level <= 19)
				return 30;
			else if(level <= 39)
				return 40;// not verified
			else
				return 47;
		}
		else if(agility >= 116 && agility <= 119)
		{
			if(level <= 19)
				return 31;
			else if(level <= 39)
				return 41;
			else
				return 47;
		}
		else if(level <= 19)
		{
			return 32;
		}
		//level is > 19
		else if(agility == 120)
		{
			if(level <= 39)
				return 42;
			else
				return 48;
		}
		else if(agility <= 125)
		{
			if(level <= 39)
				return 42;
			else
				return 49;
		}
		else if(agility <= 135)
		{
			if(level <= 39)
				return 42;
			else
				return 50;
		}
		else {
			if(level <= 39)
				return 42;
			else
				return 51;
		}
	}
	else if(agility <= 300)
	{
		if(level <= 6) {
			if(agility <= 139)
				return(21);
			else if(agility == 140)
				return(22);
			else if(agility <= 145)
				return(23);
			else if(agility <= 150)
				return(23);
			else if(agility <= 155)
				return(24);
			else if(agility <= 159)
				return(25);
			else if(agility == 160)
				return(26);
			else if(agility <= 165)
				return(26);
			else if(agility <= 170)
				return(27);
			else if(agility <= 175)
				return(28);
			else if(agility <= 179)
				return(28);
			else if(agility == 180)
				return(29);
			else if(agility <= 185)
				return(30);
			else if(agility <= 190)
				return(31);
			else if(agility <= 195)
				return(31);
			else if(agility <= 199)
				return(32);
			else if(agility <= 219)
				return(33);
			else if(agility <= 239)
				return(34);
			else
				return(35);
		}
		else if(level <= 19)
		{
			if(agility <= 139)
				return(34);
			else if(agility == 140)
				return(35);
			else if(agility <= 145)
				return(36);
			else if(agility <= 150)
				return(37);
			else if(agility <= 155)
				return(37);
			else if(agility <= 159)
				return(38);
			else if(agility == 160)
				return(39);
			else if(agility <= 165)
				return(40);
			else if(agility <= 170)
				return(40);
			else if(agility <= 175)
				return(41);
			else if(agility <= 179)
				return(42);
			else if(agility == 180)
				return(43);
			else if(agility <= 185)
				return(43);
			else if(agility <= 190)
				return(44);
			else if(agility <= 195)
				return(45);
			else if(agility <= 199)
				return(45);
			else if(agility <= 219)
				return(46);
			else if(agility <= 239)
				return(47);
			else
				return(48);
		}
		else if(level <= 39)
		{
			if(agility <= 139)
				return(44);
			else if(agility == 140)
				return(45);
			else if(agility <= 145)
				return(46);
			else if(agility <= 150)
				return(47);
			else if(agility <= 155)
				return(47);
			else if(agility <= 159)
				return(48);
			else if(agility == 160)
				return(49);
			else if(agility <= 165)
				return(50);
			else if(agility <= 170)
				return(50);
			else if(agility <= 175)
				return(51);
			else if(agility <= 179)
				return(52);
			else if(agility == 180)
				return(53);
			else if(agility <= 185)
				return(53);
			else if(agility <= 190)
				return(54);
			else if(agility <= 195)
				return(55);
			else if(agility <= 199)
				return(55);
			else if(agility <= 219)
				return(56);
			else if(agility <= 239)
				return(57);
			else
				return(58);
		}
		else
		{	//lvl >= 40
			if(agility <= 139)
				return(51);
			else if(agility == 140)
				return(52);
			else if(agility <= 145)
				return(53);
			else if(agility <= 150)
				return(53);
			else if(agility <= 155)
				return(54);
			else if(agility <= 159)
				return(55);
			else if(agility == 160)
				return(56);
			else if(agility <= 165)
				return(56);
			else if(agility <= 170)
				return(57);
			else if(agility <= 175)
				return(58);
			else if(agility <= 179)
				return(58);
			else if(agility == 180)
				return(59);
			else if(agility <= 185)
				return(60);
			else if(agility <= 190)
				return(61);
			else if(agility <= 195)
				return(61);
			else if(agility <= 199)
				return(62);
			else if(agility <= 219)
				return(63);
			else if(agility <= 239)
				return(64);
			else
				return(65);
		}
	}
	else
	{
		//seems about 21 agil per extra AC pt over 300...
		return (65 + ((agility-300) / 21));
	}
#if EQDEBUG >= 11
	LogFile->write(EQEMuLog::Error, "Error in Bot::acmod(): Agility: %i, Level: %i",agility,level);
#endif
	return 0;
}

void Bot::GenerateArmorClass()
{
	/// new formula
	int avoidance = 0;
	avoidance = (acmod() + ((GetSkill(DEFENSE)*16)/9));
	if(avoidance < 0)
		avoidance = 0;

	int mitigation = 0;
	if(GetClass() == WIZARD || GetClass() == MAGICIAN || GetClass() == NECROMANCER || GetClass() == ENCHANTER)
	{
		mitigation = GetSkill(DEFENSE)/4 + (itembonuses.AC+1);
		mitigation -= 4;
	}
	else
	{
		mitigation = GetSkill(DEFENSE)/3 + ((itembonuses.AC*4)/3);
		if(GetClass() == MONK)
			mitigation += GetLevel() * 13/10;	//the 13/10 might be wrong, but it is close...
	}
	int displayed = 0;
	displayed += ((avoidance+mitigation)*1000)/847;	//natural AC

	//Iksar AC, untested
	if(GetRace() == IKSAR)
	{
		displayed += 12;
		int iksarlevel = GetLevel();
		iksarlevel -= 10;
		if(iksarlevel > 25)
			iksarlevel = 25;
		if(iksarlevel > 0)
			displayed += iksarlevel * 12 / 10;
	}

	//spell AC bonuses are added directly to natural total
	displayed += spellbonuses.AC;

	this->AC = displayed;
}

uint16 Bot::GetPrimarySkillValue()
{
	SkillType skill = HIGHEST_SKILL;  //because NULL == 0, which is 1H Slashing, & we want it to return 0 from GetSkill
	bool equiped = m_inv.GetItem(SLOT_PRIMARY);

	if(!equiped)
	{
		skill = HAND_TO_HAND;
	}
	else
	{
		uint8 type = m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType;  //is this the best way to do this?
		switch(type)
		{
			case ItemType1HS: // 1H Slashing
			{
				skill = _1H_SLASHING;
				break;
			}
			case ItemType2HS: // 2H Slashing
			{
				skill = _2H_SLASHING;
				break;
			}
			case ItemTypePierce: // Piercing
			{
				skill = PIERCING;
				break;
			}
			case ItemType1HB: // 1H Blunt
			{
				skill = _1H_BLUNT;
				break;
			}
			case ItemType2HB: // 2H Blunt
			{
				skill = _2H_BLUNT;
				break;
			}
			case ItemType2HPierce: // 2H Piercing
			{
				skill = PIERCING;
				break;
			}
			case ItemTypeHand2Hand: // Hand to Hand
			{
				skill = HAND_TO_HAND;
				break;
			}
			default: // All other types default to Hand to Hand
			{
				skill = HAND_TO_HAND;
				break;
			}
		}
	}

	return GetSkill(skill);
}

uint16 Bot::GetTotalATK()
{
	int16 AttackRating = 0;
	int16 WornCap = itembonuses.ATK;

	if(IsBot()) {
		AttackRating = ((WornCap * 1.342) + (GetSkill(OFFENSE) * 1.345) + ((GetSTR() - 66) * 0.9) + (GetPrimarySkillValue() * 2.69));
		AttackRating += aabonuses.ATK + GroupLeadershipAAOffenseEnhancement();

		if (AttackRating < 10)
			AttackRating = 10;
	}
	else
		AttackRating = GetATK();
		
	AttackRating += spellbonuses.ATK;

	return AttackRating;
}

uint16 Bot::GetATKRating()
{
	int16 AttackRating = 0;
	if(IsBot()) {
		AttackRating = (GetSkill(OFFENSE) * 1.345) + ((GetSTR() - 66) * 0.9) + (GetPrimarySkillValue() * 2.69);

		if (AttackRating < 10)
			AttackRating = 10;
	}
	return AttackRating;
}

sint32 Bot::GenerateBaseHitPoints()
{
	// Calc Base Hit Points
	int new_base_hp = 0;
	int16 lm = GetClassLevelFactor();
	int16 Post255;
	int16 NormalSTA = GetSTA();

	if(GetOwner() && GetOwner()->CastToClient() && GetOwner()->CastToClient()->GetClientVersion() >= EQClientSoD && RuleB(Character, SoDClientUseSoDHPManaEnd))
	{
		float SoDPost255;

		if(((NormalSTA - 255) / 2) > 0)
			SoDPost255 = ((NormalSTA - 255) / 2);
		else
			SoDPost255 = 0;

		int hp_factor = GetClassHPFactor();

		if(level < 41)
		{
			new_base_hp = (5 + (GetLevel() * hp_factor / 12) + ((NormalSTA - SoDPost255) * GetLevel() * hp_factor / 3600));
		}
		else if(level < 81)
		{
			new_base_hp = (5 + (40 * hp_factor / 12) + ((GetLevel() - 40) * hp_factor / 6) +
				((NormalSTA - SoDPost255) * hp_factor / 90) +
				((NormalSTA - SoDPost255) * (GetLevel() - 40) * hp_factor / 1800));
		}
		else
		{
			new_base_hp = (5 + (80 * hp_factor / 8) + ((GetLevel() - 80) * hp_factor / 10) +
				((NormalSTA - SoDPost255) * hp_factor / 90) +
				((NormalSTA - SoDPost255) * hp_factor / 45));
		}
	}
	else
	{
		if((NormalSTA-255)/2 > 0)
			Post255 = (NormalSTA-255)/2;
		else
			Post255 = 0;

		new_base_hp = (5)+(GetLevel()*lm/10) + (((NormalSTA-Post255)*GetLevel()*lm/3000)) + ((Post255*1)*lm/6000);
	}
	this->base_hp = new_base_hp;

	return new_base_hp;
}

void Bot::GenerateAABonuses() {
	// General AA bonus
	uint8 botClass = GetClass();
	uint8 botLevel = GetLevel();
	memset(&aabonuses, 0, sizeof(StatBonuses));

	if(botLevel >= 51) {
		//level 51 = 1 AA level

		aabonuses.STR += GetAA(aaInnateStrength) * 2;		// Innate Strength AAs
		aabonuses.STA += GetAA(aaInnateStamina) * 2;		// Innate Stamina AAs
		aabonuses.AGI += GetAA(aaInnateAgility) * 2;		// Innate Agility AAs
		aabonuses.DEX += GetAA(aaInnateDexterity) * 2;		// Innate Dexterity AA
		aabonuses.INT += GetAA(aaInnateIntelligence) * 2;	// Innate Intelligence AAs
		aabonuses.WIS += GetAA(aaInnateWisdom) * 2;			// Innate Wisdom AAs

		//aabonuses.Corrup += GetAA(aaInnateCorrup) * 2;
		if(botLevel >=65){
			int botAAlevels = botLevel - 64;
			if(botAAlevels > 5)
				botAAlevels = 5;

			aabonuses.Corrup += botAAlevels * 2;	// Innate Corruption Protection AAs
		}

		//HPRegen
		aabonuses.HPRegen += GetAA(aaInnateRegeneration) * 1;	// Innate Regeneration, Convalescence, Healthy Aura, Natural Healing, Body and Mind Rejuvenation AAs
		aabonuses.HPRegen += GetAA(aaBodyAndMindRejuvenation) * 1;	// Body and Mind Rejuvenation AA

		//ManaRegen
		aabonuses.ManaRegen += GetAA(aaMentalClarity) * 1;		//Mental Clarity
		aabonuses.ManaRegen += GetAA(aaBodyAndMindRejuvenation) * 1;   // Body and Mind Rejuvenation AA

		//Stat Caps
		int32 planarPowerLevels = GetAA(aaPlanarPower);
		aabonuses.STRCapMod += GetAA(aaPlanarPower) * 5;	// Planar Power AAs
		aabonuses.STACapMod += GetAA(aaPlanarPower) * 5;	
		aabonuses.AGICapMod += GetAA(aaPlanarPower) * 5;	
		aabonuses.DEXCapMod += GetAA(aaPlanarPower) * 5;	
		aabonuses.INTCapMod += GetAA(aaPlanarPower) * 5;	
		aabonuses.WISCapMod += GetAA(aaPlanarPower) * 5;	
		aabonuses.CHACapMod += GetAA(aaPlanarPower) * 5;	

		aabonuses.INTCapMod += GetAA(aaInnateEnlightenment) * 10;	// Innate Enlightenment AAs
		aabonuses.WISCapMod += GetAA(aaInnateEnlightenment) * 10;	

		//Resist Caps
		aabonuses.FRCapMod += GetAA(aaDiscordantDefiance) * 5;	// Discordant Defiance AAs	
		aabonuses.CRCapMod += GetAA(aaDiscordantDefiance) * 5;		
		aabonuses.MRCapMod += GetAA(aaDiscordantDefiance) * 5;	
		aabonuses.PRCapMod += GetAA(aaDiscordantDefiance) * 5;	
		aabonuses.DRCapMod += GetAA(aaDiscordantDefiance) * 5;	
		aabonuses.CorrupCapMod += GetAA(aaDiscordantDefiance) * 5;

		//HP% (MaxHP)
		switch(GetAA(aaNaturalDurability))
		{
			case 1: 
				aabonuses.MaxHP += 200;	// Natural Durability AA 1
				break;
			case 2: 
				aabonuses.MaxHP += 500;	// Natural Durability AA 2
				break;
			case 3: 
				aabonuses.MaxHP += 1000;	// Natural Durability AA 3
				break;
			case 4: 
			case 5: 
			case 6: 
				aabonuses.MaxHP += (1000 + (GetAA(aaNaturalDurability)-3) * 100);	// SoD AA 1,2,3
				break;
		}
		aabonuses.MaxHP += GetAA(aaPhysicalEnhancement) * 200;
		aabonuses.MaxHP += GetAA(aaPlanarDurability) * 125;

		//HP
		aabonuses.HP += GetAA(aaSturdiness) * 100;

		//Mana  (SoF)
		if(botLevel >= 65) {
			aabonuses.Mana += (int)(((botLevel - 60))/5) > 5 ? 250 : (int)(((botLevel - 60))/5) * 50;  // Mental Stamina AAs	
			if(botLevel >= 85) {
				aabonuses.Mana += 250;	// Mental Stamina AAs	
			}
		}

		//Melee Mitigation
		switch(GetAA(aaCombatStability)){
			case 1:
				aabonuses.MeleeMitigation += 0.02;
				break;
			case 2:
				aabonuses.MeleeMitigation += 0.05;
				break;
			case 3:
				aabonuses.MeleeMitigation += 0.10;
				break;
			case 4: 
			case 5: 
			case 6: 
			case 7:
			case 8: 
				aabonuses.MeleeMitigation += (0.10 + (GetAA(aaCombatStability)-3) * 0.03);	// Innate Defense
				break;
			case 9: 
			case 10: 
			case 11: 
			case 12: 
			case 13:
				aabonuses.MeleeMitigation += (0.25 + (GetAA(aaCombatStability)-8) * 0.02);	// Defensive Instincts
				break;
			case 14: 
			case 15: 
			case 16: 
			case 17: 
			case 18:
				aabonuses.MeleeMitigation += (0.35 + (GetAA(aaCombatStability)-13) * 0.02);	// Thick Skin
				break;
		}
		aabonuses.MeleeMitigation += GetAA(aaPhysicalEnhancement) * 0.02;

		//HPRegen Cap (not in aabonuses)
		if(botLevel >= 71) {
			//level 71 = 1 AA level
			uint8 botAAlevels = level - 70;
			//aabonuses.HPRegenCap += botAAlevels * 1;	// Energetic Attunement AAs	
		}

		//ManaRegen Cap
		aabonuses.ItemManaRegenCap += GetAA(aaExpansiveMind);

		//Packrat
		aabonuses.Packrat += GetAA(aaPackrat);
	}
}

int32 Bot::GetAA(int32 aa_id) const {
	int maxAAExpansion = RuleI(Bots, BotAAExpansion);
	int botLevel = GetLevel();
	int botClass = GetClass();
	int aaLevel = 0;

	if(botLevel >= 51 && maxAAExpansion > ExpansionNone) {
	
		switch(aa_id) {
			case aaInnateStrength:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Strength - PoP Advanced
					}
				}
				break;
			case aaInnateStamina:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Stamina - PoP Advanced
					}
				}
				break;
			case aaInnateAgility:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Agility - PoP Advanced
					}
				}
				break;
			case aaInnateDexterity:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Dexterity - PoP Advanced
					}
				}
				break;
			case aaInnateIntelligence:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Intelligence - PoP Advanced
					}
				}
				break;
			case aaInnateWisdom:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Wisdom - PoP Advanced
					}
				}
				break;
			case aaInnateCharisma:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Advanced Innate Charisma - PoP Advanced
					}
				}
				break;
			case aaInnateFireProtection:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Warding of Solusek - PoP Advanced
					}
				}
				break;
			case aaInnateColdProtection:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Blessing of Eci - PoP Advanced
					}
				}
				break;
			case aaInnateMagicProtection:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Marrs Protection - PoP Advanced
					}
				}
				break;
			case aaInnatePoisonProtection:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Shroud of the Faceless - PoP Advanced
					}
				}
				break;
			case aaInnateDiseaseProtection:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 5;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += 2 * (botLevel - 60) > 10 ? 10 : 2 * (botLevel - 60);  //Bertoxxulous Gift - PoP Advanced
					}
				}
				break;
			case aaInnateRunSpeed:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionGoD) {
						aaLevel += 2;  //Swift Journey - GoD AA
					}
				}
				break;
			case aaInnateRegeneration:
				if(botLevel >= 51 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(IsWarriorClass() && botLevel >= 55) {
						aaLevel += 3;   //Natural Healing
					}
					if(botLevel >= 61 && maxAAExpansion > ExpansionGoD) {
						aaLevel += 2;  //Convalescence - GoD AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionGoD) {
							aaLevel += (botLevel - 65) > 5 ? 5 : botLevel - 65;  //Healthy Aura - GoD AA
							if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
								aaLevel += (botLevel - 70) > 5 ? 5 : botLevel - 70;  //Fast Healing - TSS AA
								if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
									aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoF AA
									if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
										aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;  //SoD AA
									}
								}
							}
						}
					}
				}
			break;
			case aaHealingAdept:
				if((botClass == BEASTLORD)||(botClass == CLERIC)||(botClass == DRUID)||(botClass == PALADIN)||(botClass == RANGER)||(botClass == SHAMAN) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 61) > 3 ? 3 : botLevel - 61;  //Advanced Healing Adept - PoP Ability AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
							aaLevel += (int)(((botLevel - 65)+1)/2) > 3 ? 3 : (int)(((botLevel - 65)+1)/2);  //Healing Adept Mastery - OoW AA
							if(botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
								aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //UF AA
							}
						}
					}
				}
				break;
			case aaHealingGift:
				if((botClass == BEASTLORD)||(botClass == CLERIC)||(botClass == DRUID)||(botClass == PALADIN)||(botClass == RANGER)||(botClass == SHAMAN) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 61) > 3 ? 3 : botLevel - 61;  //Advanced Healing Gift - PoP Ability AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
							aaLevel += (int)(((botLevel - 65)+1)/2) > 3 ? 3 : (int)(((botLevel - 65)+1)/2);  //Healing Gift Mastery - OoW AA
							if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
								aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);  //Healing Surge - TSS AA
								if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
									aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);  //SoF AA
									if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
										aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //SoD AA
										if(botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
											aaLevel += 3;  //UF AA
										}
									}
								}
							}
						}
					}
				}
				break;
			case aaSpellCastingMastery:
				if((botClass == WIZARD || botClass == ENCHANTER || botClass == MAGICIAN || botClass == NECROMANCER || botClass == DRUID || botClass == SHAMAN || botClass == CLERIC) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				else if((botClass == PALADIN || botClass == RANGER || botClass == SHADOWKNIGHT || botClass == BEASTLORD) && botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
					aaLevel = 3;  //UF AAs
				}
				break;
			case aaSpellCastingReinforcement:
				if((botClass == WIZARD || botClass == ENCHANTER || botClass == MAGICIAN || botClass == NECROMANCER || botClass == DRUID || botClass == SHAMAN || botClass == CLERIC || botClass == RANGER || botClass == SHADOWKNIGHT || botClass == PALADIN || botClass == BEASTLORD) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				break;
			case aaMentalClarity:
				if(!((botClass == WARRIOR)||(botClass == MONK)||(botClass == ROGUE)||(botClass == BERSERKER)) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (botLevel - 70) > 5 ? 5 : botLevel - 70;  //TSS AA
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoF AA
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;  //SoD AA
								if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
									aaLevel += 5;  //UF AA
								}
							}
						}
					}
				}
			break;
			case aaSpellCastingFury:
				if(!((botClass == WARRIOR || botClass == MONK || botClass == ROGUE || botClass == BERSERKER)) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;  //spell casting fury
					
					if(botClass == WIZARD && botLevel >= 59) {
						aaLevel += (botLevel - 58) > 3 ? 3 : botLevel - 58;   // spell casting fury mastery - SoL AA
					}
				}
				break;
			case aaChanellingFocus:
			break;
			case aaSpellCastingSubtlety:
				if((botClass == WIZARD || botClass == NECROMANCER || botClass == MAGICIAN || botClass == ENCHANTER || botClass == CLERIC || botClass == DRUID || botClass == SHAMAN) && botLevel >= 55 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);  //TSS AA
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);  //SoF AA
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //SoD AA
							}
						}
					}
				}
				else if(botClass == BEASTLORD && botLevel >= 60 && maxAAExpansion >= ExpansionUF) {
					aaLevel = (botLevel - 59) > 3 ? 3 : botLevel - 59;  //UF AAs
				}
				break;
			case aaSpellCastingExpertise:
			break;
			case aaSpellCastingDeftness:
				if((botClass == WIZARD || botClass == NECROMANCER || botClass == MAGICIAN || botClass == SHADOWKNIGHT) && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				break;
			case aaNaturalDurability:
				if(botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
					if(botLevel >= 60 && maxAAExpansion >= ExpansionSoD) {
						aaLevel += 3;  //SoD AA
					}
				}
				break;
			case aaNaturalHealing:
				//Innate Regeneration
			break;
			case aaCombatFury:
				if(IsWarriorClass() && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 54) > 3 ? 3 : botLevel - 54;
					if(botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 61) > 3 ? 3 : botLevel - 61;  //Fury of the Ages- PoP Ability AA
					}
				}
				break;
			case aaFearResistance:
				if(IsWarriorClass() && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				break;
			case aaFinishingBlow:
				if(IsWarriorClass() && botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 54) > 3 ? 3 : botLevel - 54;  //SoL
					if(botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 61) > 3 ? 3 : botLevel - 61;  //Coup de Grace - PoP Ability AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
							aaLevel += (botLevel - 65) > 3 ? 3 : botLevel - 65;  //Deathblow - OoW AA
							if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
								aaLevel += (botLevel - 70) > 3 ? 3 : botLevel - 70;  //Mercy Kill - TSS AA
								if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
									aaLevel += (botLevel - 75) > 3 ? 3 : botLevel - 75;  //SoF AA
									if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
										aaLevel += (botLevel - 80) > 3 ? 3 : botLevel - 80;  //SoD AA
										if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
											aaLevel += 3;  //UF AA
										}
									}
								}
							}
						}
					}
				}
				break;
			case aaCombatStability:
				//aabonuses
				if(botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;  //SoL
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 60) > 5 ? 5 : botLevel - 60;  //Innate Defense - PoP Ability AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {  
							aaLevel += (botLevel - 5) > 5 ? 5 : botLevel - 65;   //Defensive Instincts - OoW AA
							if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
								aaLevel += 5;  //Thick Skin DoDH AA
								if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
									aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoF AAs
									if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {  
										aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;   //SoD AAs
										if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
											aaLevel += 5;  //UF AAs
										}
									}
								}	
							}
						}
					}		
				}
				break;
			case aaCombatAgility:
				if(botLevel >= 55 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3; //SoL AA
					if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 60) > 5 ? 5 : botLevel - 60;  //Lightning Reflexes - PoP Ability AA
						if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
							aaLevel += (botLevel - 60) > 5 ? 5 : botLevel - 60;  //Reflexive Mastery - OoW AA
							if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
								aaLevel += 5;  //Precognition - DoDH AA
								if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
									aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoF AAs
									if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {  
										aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;   //SoD AAs
										if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
											aaLevel += 5;  //UF AAs
										}
									}
								}
							}
						}
					}
				}
				break;
			case aaMassGroupBuff:
			break;
			case aaDivineResurrection:
			break;
			case aaInnateInvisToUndead:
			break;
			case aaCelestialRegeneration:
			break;
			case aaBestowDivineAura:
			break;
			case aaTurnUndead:
			break;
			case aaPurifySoul:
			break;
			case aaQuickEvacuation:
				if((botClass == WIZARD || botClass == DRUID) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaExodus:
			break;
			case aaQuickDamage:
				if((botClass == WIZARD || botClass == MAGICIAN || botClass == DRUID) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaEnhancedRoot:
				if(botClass == DRUID && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaDireCharm:
			break;
			case aaCannibalization:
			break;
			case aaQuickBuff:
				if((botClass == CLERIC || botClass == DRUID || botClass == SHAMAN || botClass == BEASTLORD || botClass == RANGER || botClass == ENCHANTER || botClass == PALADIN) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaAlchemyMastery:
			break;
			case aaRabidBear:
			break;
			case aaManaBurn:
			break;
			case aaImprovedFamiliar:
			break;
			case aaNexusGate:
			break;      
			case aaPermanentIllusion:
			break;
			case aaGatherMana:
			break;
			case aaMendCompanion:
			break;
			case aaQuickSummoning:
				if(botClass == MAGICIAN && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaFrenziedBurnout:
			break;
			case aaElementalFormFire:
			break;
			case aaElementalFormWater:
			break;
			case aaElementalFormEarth:
			break;
			case aaElementalFormAir:
			break;
			case aaImprovedReclaimEnergy:
			break;
			case aaTurnSummoned:
			break;
			case aaElementalPact:
				if(botClass == MAGICIAN && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaLifeBurn:
			break;
			case aaDeadMesmerization:
			break;
			case aaFearstorm:
			break;
			case aaFleshToBone:
			break;
			case aaCallToCorpse:
			break;
			case aaDivineStun:
			break;
			case aaImprovedLayOnHands:
			break;
			case aaSlayUndead:
				if(botClass == PALADIN && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += 3;   //Vanquish undead - DoDH AA
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);  //TSS AA
						}
					}
				}
				break;
			case aaActOfValor:
			break;
			case aaHolySteed:
			break;
			case aaFearless:
			break;
			case aa2HandBash:
				if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaInnateCamouflage:
			break;
			case aaAmbidexterity:
				if((botClass == WARRIOR || botClass == RANGER || botClass == ROGUE || botClass == MONK || botClass == BARD || botClass == BEASTLORD) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaArcheryMastery:
				if(botClass == RANGER && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaFletchingMastery:
			break;
			case aaEndlessQuiver:
			break;
			case aaUnholySteed:
			break;
			case aaImprovedHarmTouch:
			break;
			case aaLeechTouch:
			break;
			case aaDeathPeace:
			break;
			case aaSoulAbrasion:
				if(botClass == SHADOWKNIGHT && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaInstrumentMastery:
				if(botClass == BARD && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
					
					if(botLevel >= 65) {
						aaLevel += 1;   // Improved Instrument Mastery AA
					}
				}
				break;
			case aaJamFest:
			break;
			case aaSonicCall:
			break;
			case aaCriticalMend:
			break;
			case aaPurifyBody:
			break;
			case aaChainCombo:
			break;
			case aaRapidFeign:
			break;
			case aaReturnKick:
				if(botClass == MONK && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaEscape:
			break;
			case aaPoisonMastery:
			break;
			case aaDoubleRiposte:
				if((botClass == WARRIOR || botClass == BERSERKER || botClass == BEASTLORD || botClass == MONK || botClass == PALADIN || botClass == RANGER || botClass == ROGUE || botClass == SHADOWKNIGHT) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;  
					if(botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
						aaLevel += (botLevel - 61) > 3 ? 3 : botLevel - 61;   // flash of steel - PoP Ability AA
					}
				}
				break;
			case aaQuickHide:
			break;
			case aaQuickThrow:
			break;
			case aaPurgePoison:
			break;
			case aaFlurry:
				if((botClass == WARRIOR || botClass == BERSERKER) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaRampage:
			break;
			case aaAreaTaunt:
			break;
			case aaWarcry:
			break;
			case aaBandageWound:
			break;
			case aaSpellCastingReinforcementMastery:
				if((botClass == WIZARD || botClass == ENCHANTER || botClass == MAGICIAN || botClass == NECROMANCER || botClass == DRUID || botClass == SHAMAN || botClass == CLERIC || botClass == RANGER || botClass == SHADOWKNIGHT || botClass == PALADIN || botClass == BEASTLORD) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				break;
			case aaSpellCastingFuryMastery:
				// spell casting fury
			break;
			case aaExtendedNotes:
				if(botClass == BARD && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
				}
				break;
			case aaDragonPunch:
				if(botClass == MONK && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaStrongRoot:
			break;
			case aaSingingMastery:
				if(botClass == BARD && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = (botLevel - 58) > 3 ? 3 : botLevel - 58;
					
					if(botLevel >= 65) {
						aaLevel += 1;   // Improved Singing Mastery AA
					}
				}
				break;
			case aaBodyAndMindRejuvenation:
				if((botClass == BARD || botClass == RANGER || botClass == PALADIN || botClass == SHADOWKNIGHT || botClass == BEASTLORD) && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
			break;
			case aaPhysicalEnhancement:
				if(IsWarriorClass() && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaAdvTrapNegotiation:
			break;
			case aaAcrobatics:
			break;
			case aaScribbleNotes:
			break;
			case aaChaoticStab:
				if(botClass == ROGUE && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 1;
				}
				break;
			case aaPetDiscipline:
			break;
			case aaHobbleofSpirits:
			break;
			case aaFrenzyofSpirit:
			break;
			case aaParagonofSpirit:
			break;
			case aaAdvancedInnateStrength:
				//Innate Strength
			break;
			case aaAdvancedInnateStamina:
				//Innate Stamina
			break;
			case aaAdvancedInnateAgility:
				//Innate Agility
			break;
			case aaAdvancedInnateDexterity:
				//Innate Dexterity
			break;
			case aaAdvancedInnateIntelligence:
				//Innate Intelligence
			break;
			case aaAdvancedInnateWisdom:
				//Innate Wisdom
			break;
			case aaAdvancedInnateCharisma:
				//Innate Charisma
			break;
			case aaWardingofSolusek:
				//Innate Fire Protection
			break;
			case aaBlessingofEci:
				//Innate Cold Protection
			break;
			case aaMarrsProtection:
				//Innate Magic Protection
			break;
			case aaShroudofTheFaceless:
				//Innate Poison Protection
			break;
			case aaBertoxxulousGift:
				//Innate Disease Protection
			break;
			case aaNewTanaanCraftingMastery:
			break;
			case aaPlanarPower:
				if(botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 5 ? 5 : botLevel - 60;  //PoP Advance
					if(botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
						aaLevel = (botLevel - 65) > 5 ? 5 : botLevel - 65;  //Chaotic Potential - OoW
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel = (botLevel - 70) > 5 ? 5 : botLevel - 70;  //Potential of Serpent Spine - TSS
							if(botLevel >= 76 && maxAAExpansion >= ExpansionSoD) {
								aaLevel = (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoD
							}
						}
					}
				}
				break;
			case aaPlanarDurability:
				if(IsWarriorClass() && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2); 
				}
				break;
			case aaInnateEnlightenment:
				if((botClass == CLERIC || botClass == DRUID || botClass == SHAMAN || botClass == ENCHANTER || botClass == MAGICIAN || botClass == NECROMANCER || botClass == WIZARD) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 5 ? 5 : botLevel - 60;
				}
			break;
			case aaAdvancedSpellCastingMastery:
			break;
			case aaAdvancedHealingAdept:
				//Healing Adept
			break;
			case aaAdvancedHealingGift:
				//Healing Gift
			break;
			case aaCoupdeGrace:
				//Finishing Blow
			break;
			case aaFuryoftheAges:
				//Combat Fury
			break;
			case aaMasteryofthePast:
			break;
			case aaLightningReflexes:
				//Combat Agility
			break;
			case aaInnateDefense:
				//aabonuses
				//aaCombatStability
			break;
			case aaRadiantCure:
			break;
			case aaHastenedDivinity:
			break;
			case aaHastenedTurning:
			break;
			case aaHastenedPurificationofSoul:
			break;
			case aaHastenedGathering:
			break;
			case aaHastenedRabidity:
			break;
			case aaHastenedExodus:
			break;
			case aaHastenedRoot:
			break;
			case aaHastenedMending:
			break;
			case aaHastenedBanishment:
			break;
			case aaHastenedInstigation:
			break;
			case aaFuriousRampage:
			break;
			case aaHastenedPurificationoftheBody:
			break;
			case aaHastyExit:
			break;
			case aaHastenedPurification:
			break;
			case aaFlashofSteel:
				//double riposte
			break;
			case aaDivineArbitration:
			break;
			case aaWrathoftheWild:
			break;
			case aaVirulentParalysis:
			break;
			case aaHarvestofDruzzil:
			break;
			case aaEldritchRune:
			break;
			case aaServantofRo:
			break;
			case aaWaketheDead:
			break;
			case aaSuspendedMinion:
			break;
			case aaSpiritCall:
			break;
			case aaCelestialRenewal:
			break;
			case aaAllegiantFamiliar:
			break;
			case aaHandofPiety:
			break;
			case aaMithanielsBinding:
			break;
			case aaMendingoftheTranquil:
			break;
			case aaRagingFlurry:
				if((botClass == WARRIOR || botClass == BERSERKER) && botLevel >= 63 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 62) > 3 ? 3 : botLevel - 62;
				}
				break;
			case aaGuardianoftheForest:
			break;
			case aaSpiritoftheWood:
			break;
			case aaBestialFrenzy:
				if(botClass == BEASTLORD && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 5 ? 5 : botLevel - 60;   
				}
				break;
			case aaHarmoniousAttack:
				if(botClass == BARD && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 5 ? 5 : botLevel - 60;
				}
				break;
			case aaKnightsAdvantage:
				if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
					if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += 3;   //Knights Expertise - DoDH AA
					} 
				} 
				break;  
			case aaFerocity:
				if((botClass == WARRIOR || botClass == RANGER || botClass == ROGUE || botClass == MONK || botClass == BERSERKER) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
					if(botLevel >= 70){
						aaLevel += 3;  //  Relentless Assault
					}
				}
				break;
			case aaViscidRoots:
			break;
			case aaSionachiesCrescendo:
				if(botClass == BARD && botLevel >= 63 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 62) > 3 ? 3 : botLevel - 62;
				}
				break;
			case aaAyonaesTutelage:
			break;
			case aaFeignedMinion:
			break;
			case aaUnfailingDivinity:
			break;
			case aaAnimationEmpathy:
			break;
			case aaRushtoJudgement:
			break;
			case aaLivingShield:
			break;
			case aaConsumptionoftheSoul:
				if(botClass == SHADOWKNIGHT && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
				}
				break;
			case aaBoastfulBellow:
			break;
			case aaFervrentBlessing:
			break;
			case aaTouchoftheWicked:
			break;
			case aaPunishingBlade:
				if((botClass == WARRIOR || botClass == RANGER || botClass == MONK || botClass == BERSERKER) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH){
						aaLevel += 3;  // Wicked Blade
					}
				}
				break;
			case aaSpeedoftheKnight:
				if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);  //PoP Ability
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH){
						aaLevel += 3;  // Swift Blade - DoDH
						if(botLevel >= 76 && maxAAExpansion >= ExpansionTSS){
							aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);  // Quick Blade - TSS
						}
					}
				}
				break;
			case aaShroudofStealth:
			break;
			case aaNimbleEvasion:
			break;
			case aaTechniqueofMasterWu:
				if(botClass == MONK && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 5 ? 5 : botLevel - 60;
				}
				break;
			case aaHostoftheElements:
			break;
			case aaCallofXuzl:
			break;
			case aaHastenedStealth:
			break;
			case aaIngenuity:
				if((botClass == WARRIOR || botClass == BERSERKER || botClass == ROGUE || botClass == MONK) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
				}
				break;
			case aaFleetofFoot:
				if(botClass == BARD && botLevel >= 62 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 61)+1)/2) > 2 ? 2 : (int)(((botLevel - 61)+1)/2);
				}
				break;
			case aaFadingMemories:
			break;
			case aaTacticalMastery:
				if((botClass == WARRIOR || botClass == BERSERKER) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (botLevel - 60) > 3 ? 3 : botLevel - 60;
					
					if(botLevel >= 68 && maxAAExpansion >= ExpansionOoW) {
						aaLevel += (botLevel - 67) > 3 ? 3 : botLevel - 67;  // Overwhelming Attack - OoW AA
						
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);   // Overbear - TSS AA
						}
					}
				}
				else if((botClass == BARD || botClass == BEASTLORD || botClass == MONK || botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 83 && maxAAExpansion >= ExpansionUF) {
					aaLevel = (botLevel - 82) > 3 ? 3 : botLevel - 82;   //UF AAs
				}
				break;
			case aaTheftofLife:
				if((botClass == NECROMANCER || botClass == SHADOWKNIGHT) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
					if(botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
						aaLevel += (botLevel - 64) > 3 ? 3 : botLevel - 64;   //Advanced Theft of Life
						if(botLevel >= 68 && maxAAExpansion >= ExpansionOoW) {
							aaLevel += (botLevel - 67) > 2 ? 2 : botLevel - 67;   // Soul Thief
						}
					}
				}
				break;
			case aaFuryofMagic:
				if((botClass == CLERIC || botClass == DRUID || botClass == SHAMAN || botClass == ENCHANTER || botClass == MAGICIAN || botClass == NECROMANCER || botClass == WIZARD) && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
						
					if(botClass == WIZARD) {
						aaLevel += (botLevel - 60) > 3 ? 3 : botLevel - 60;   // fury of magic mastery - PoP AA
					}
					else {
						aaLevel += (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);    //fury of magic - PoP AA
					}
						
					if(botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
		
						if(botClass == WIZARD) {
							aaLevel += (botLevel - 64) > 3 ? 3 : botLevel - 64;   // advanced fury of magic mastery - GoD AA
						}
						else {
							aaLevel += 3;    //fury of magic mastery - GoD AA
						}
							
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);     //TSS AA
							
							if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
								aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);   //SoF AA  
								
								if(botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
									aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);   //UF AA
								}
							}
						}	
					}
				}
				else if ((botClass == BEASTLORD || botClass == RANGER || botClass == SHADOWKNIGHT || botClass == PALADIN || botClass == BARD) && botLevel >= 66 && maxAAExpansion >= ExpansionOoW) {
					aaLevel += (int)(((botLevel - 65)+1)/2) > 3 ? 3 : (int)(((botLevel - 65)+1)/2);    //fury of magic - OoW AA
				}
				break;
			case aaFuryofMagicMastery2:
			break;
			case aaProjectIllusion:
			break;
			case aaHeadshot:
				if(botClass == RANGER && botLevel >= 60 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = 1;
					if(botLevel >= 61 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);  //Improved Headshot, etc.
						
						if(botLevel >= 67 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 66)+1)/2) > 3 ? 3 : (int)(((botLevel - 66)+1)/2);  // TSS AA
							
							if(botLevel >= 73 && maxAAExpansion >= ExpansionSoF) {
								aaLevel += (int)(((botLevel - 72)+1)/2) > 3 ? 3 : (int)(((botLevel - 72)+1)/2);  // SoF AA
								
								if(botLevel >= 79 && maxAAExpansion >= ExpansionSoD) {
									aaLevel += (int)(((botLevel - 78)+1)/2) > 3 ? 3 : (int)(((botLevel - 78)+1)/2);  //SoD AAs
									
									if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
										aaLevel += (int)(((botLevel - 84)+1)/2) > 3 ? 3 : (int)(((botLevel - 84)+1)/2);   //UF AAs
									}
								}
							}
						}
					}
				}
				break;
			case aaEntrap:
			break;
			case aaUnholyTouch:
				if(botClass == SHADOWKNIGHT && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = (int)(((botLevel - 60)+1)/2) > 3 ? 3 : (int)(((botLevel - 60)+1)/2);
				}
				break;
			case aaTotalDomination:
			break;
			case aaStalwartEndurance:
			break;
			case aaQuickSummoning2:
			break;
			case aaMentalClarity2:
			break;
			case aaInnateRegeneration2:
			break;
			case aaManaBurn2:
			break;
			case aaExtendedNotes2:
			break;
			case aaSionachiesCrescendo2:
			break;
			case aaImprovedReclaimEnergy2:
			break;
			case aaSwiftJourney:
				//Innate Run Speed
			break;
			case aaConvalescence:
				//Innate Regeneration
			break;
			case aaLastingBreath:
			break;
			case aaPackrat:
				if(botLevel >= 61 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;   //GoD AAs
					if(botLevel >= 65) {
						aaLevel += 3;   //GoD AAs
						if(botLevel >= 70 && maxAAExpansion >= ExpansionSoD) {
							aaLevel += 5;  //SoD AAs
						}
					}
				}
				break;
			case aaHeightenedEndurance:
			break;
			case aaWeaponAffinity:
				if(IsWarriorClass() && botLevel >= 55 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 54) > 5 ? 5 : botLevel - 54;
				}
				break;
			case aaSecondaryForte:
			break;
			case aaPersistantCasting:
			break;
			case aaTuneofPursuance:
			break;
			case aaImprovedInstrumentMastery:
				//instrument mastery
			break;
			case aaImprovedSingingMastery:
				//singing mastery
			break;
			case aaExultantBellowing:
			break;
			case aaEchoofTaelosia:
			break;
			case aaInternalMetronome:
			break;
			case aaPiousSupplication:
			break;
			case aaBeastialAlignment:
			break;
			case aaWrathofXuzl:
			break;
			case aaFeralSwipe:
			break;
			case aaWardersFury:
				if(botClass == BEASTLORD && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (botLevel - 70) > 3 ? 3 : botLevel - 70;   
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += aaLevel += (botLevel - 75) > 3 ? 3 : botLevel - 75;
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += aaLevel += (botLevel - 80) > 3 ? 3 : botLevel - 80;
							}
						}
					}
				}
				break;
			case aaWardersAlacrity:
				if(botClass == BEASTLORD && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += (botLevel - 69) > 3 ? 3 : botLevel - 69;
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);
							if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
								aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);
								if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
									aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //SoD AAs
									if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
										aaLevel += 3;  //UF AAs
									}
								}
							}
						}
					}
				}
				break;
			case aaPetAffinity:
			break;
			case aaMasteryofthePast2:
			break;
			case aaSpellCastingSubtlety2:
			break;
			case aaTouchoftheDivine:
			break;
			case aaDivineAvatar:
			break;
			case aaExquisiteBenediction:
			break;
			case aaQuickenedCuring:
			break;
			case aaNaturesBoon:
			break;
			case aaAdvancedTracking:
			break;
			case aaCriticalAffliction:
			break;
			case aaFuryofMagicMastery:
				if(!(botClass == WARRIOR || botClass == MONK || botClass == ROGUE || botClass == BERSERKER) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
				}
				break;
			case aaDoppelganger:
			break;
			case aaEnchancedForgetfulness:
			break;
			case aaMesmerizationMastery:
			break;
			case aaQuickMassGroupBuff:
			break;
			case aaSharedHealth:
			break;
			case aaElementalFury:
				if(botClass == MAGICIAN && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (botLevel - 70) > 3 ? 3 : botLevel - 70;
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += aaLevel += (botLevel - 75) > 3 ? 3 : botLevel - 75;
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += aaLevel += (botLevel - 80) > 3 ? 3 : botLevel - 80;
							}
						}
					}
				}
				break;
			case aaElementalAlacrity:
				if(botClass == MAGICIAN && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += (botLevel - 69) > 3 ? 3 : botLevel - 69;
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);
							if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
								aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);
								if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
									aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);   //SoD AAs
									if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
										aaLevel += 3;  //UF AAs
									}
								}
							}
						}
					}
				}
				break;
			case aaElementalAgility:
				if(botClass == MAGICIAN && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
				}
				break;
			case aaElementalDurability:
				if(botClass == MAGICIAN && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
				}
				break;
			case aaSinisterStrikes:
			break;
			case aaStrikethrough:
				if((botClass == MONK || botClass == ROGUE) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 3 ? 3 : botLevel - 64;
				}
				break;
			case aaStonewall:
			break;
			case aaRapidStrikes:
				if(botClass == MONK && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
				}
				break;
			case aaKickMastery:
				if(botClass == MONK && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
				}
				break;
			case aaHightenedAwareness:
				if(botClass == MONK && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
				}
				break;
			case aaDestructiveForce:
			break;
			case aaSwarmofDecay:
			break;
			case aaDeathsFury:
				if((botClass == NECROMANCER || botClass == SHADOWKNIGHT) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (botLevel - 70) > 3 ? 3 : botLevel - 70;
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += aaLevel += (botLevel - 75) > 3 ? 3 : botLevel - 75;
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += aaLevel += (botLevel - 80) > 3 ? 3 : botLevel - 80;
							}
						}
					}
				}
				break;
			case aaQuickeningofDeath:
				if(botClass == NECROMANCER && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
					if(botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += (botLevel - 69) > 3 ? 3 : botLevel - 69;
						if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
							aaLevel += (int)(((botLevel - 70)+1)/2) > 3 ? 3 : (int)(((botLevel - 70)+1)/2);
							if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
								aaLevel += (int)(((botLevel - 75)+1)/2) > 3 ? 3 : (int)(((botLevel - 75)+1)/2);
								if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
									aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //SoD
									if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
										aaLevel += 3;  //UF
									}
								}
							}
						}
					}
				}
				break;
			case aaAdvancedTheftofLife:
				// Theft of Life
			break;
			case aaTripleBackstab:
				if(botClass == ROGUE && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 3 ? 3 : botLevel - 64;
				}
				break;
			case aaHastenedPiety:
			break;
			case aaImmobilizingBash:
				if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
					if(botLevel >= 78 && maxAAExpansion >= ExpansionSoF) {
						aaLevel += (botLevel - 77) > 3 ? 3 : botLevel - 77;  //SoF
						if(botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
							aaLevel += (int)(((botLevel - 80)+1)/2) > 3 ? 3 : (int)(((botLevel - 80)+1)/2);  //UF
						}
					}
				}
				break;
			case aaViciousSmash:
				if((botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 5;
					if(botLevel >= 75 && maxAAExpansion >= ExpansionUF) {
						aaLevel += 5;  //UF
					}
				}
				break;
			case aaRadiantCure2:
			break;
			case aaPurification:
			break;
			case aaPrecisionofthePathfinder:
				if(botClass == RANGER && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 3 ? 3 : botLevel - 64;
				}
				break;
			case aaCoatofThistles:
				if(botClass == RANGER && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = 3;
				}
				break;
			case aaFlamingArrows:
			break;
			case aaFrostArrows:
			break;
			case aaSeizedOpportunity:
			break;
			case aaTrapCircumvention:
			break;
			case aaImprovedHastyExit:
			break;
			case aaVirulentVenom:
			break;
			case aaImprovedConsumptionofSoul:
			break;
			case aaIntenseHatred:
			break;
			case aaAdvancedSpiritCall:
			break;
			case aaCalloftheAncients:
			break;
			case aaSturdiness:
				if(botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					if(botClass == WARRIOR) {
						aaLevel = (botLevel - 64) > 5 ? 5 : botLevel - 64;
					}
					if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
						aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //General Sturdiness SoF AAs
						if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
							aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;  //SoD AAs
							if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
								aaLevel += 5;  //UF AAs
							}
						}
					}
				}
			break;
			case aaWarlordsTenacity:
			break;
			case aaStrengthenedStrike:
				if((botClass == RANGER || botClass == WARRIOR) && botLevel >= 65 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 64) > 3 ? 3 : botLevel - 64;
				}
				break;
			case aaExtendedShielding:
			break;
			case aaRosFlamingFamiliar:
			break;
			case aaEcisIcyFamiliar:
			break;
			case aaDruzzilsMysticalFamiliar:
			break;
			case aaAdvancedFuryofMagicMastery:
				//fury of magic
			break;
			case aaWardofDestruction:
			break;
			case aaFrenziedDevastation:
			break;
			case aaCombatFury2:
			break;
			case aaCombatFury3:
			break;
			case aaCombatFury4:
			break;
			case aaFuryoftheAges2:
			break;
			case aaFuryoftheAges3:
			break;
			case aaFuryoftheAges4:
			break;
			case aaPlanarDurability2:
			break;
			case aaInnateEnlightenment2:
			break;
			case aaDireCharm2:
			break;
			case aaDireCharm3:
			break;
			case aaTouchoftheDivine2:
			break;
			case aaTouchofDecay:
			break;
			case aaCalloftheAncients2:
			break;
			case aaImprovedVision:
			break;
			case aaEternalBreath:
			break;
			case aaBlacksmithingMastery:
			break;
			case aaBakingMastery:
			break;
			case aaBrewingMastery:
			break;
			case aaFletchingMastery2:
			break;
			case aaPotteryMastery:
			break;
			case aaTailoringMastery:
			break;
			case aaSalvage:
			break;
			case aaOrigin:
			break;
			case aaChaoticPotential:
			break;
			case aaDiscordantDefiance:
				if(botLevel >= 66 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 65) > 10 ? 10 : botLevel - 65;  //GoD AAs
					if(botLevel >= 80 && maxAAExpansion >= ExpansionSoD) {
						aaLevel += 5;   //SoD AAs
					}
				}
				break;
			case aaTrialsofMataMuram:
			break;
			case aaMysticalAttuning:
			break;
			case aaDelayDeath:
			break;
			case aaHealthyAura:
				//Innate Regeneration
			break;
			case aaFitness:
			break;
			case aaVeteransWrath:
			break;
			case aaVeteransWrath2:
			break;
			case aaVeteransWrath3:
			break;
			case aaVeteransWrath4:
			break;
			case aaDeathblow:
			break;
			case aaReflexiveMastery:
				//CombatAgility
			break;
			case aaDefensiveInstincts:
				//Combat Stability
				break;
			case aaMnemonicRetention:
			break;
			case aaExpansiveMind:
				if(botLevel >= 66 && maxAAExpansion >= ExpansionGoD) {
					aaLevel = (botLevel - 65) > 5 ? 5 : botLevel - 65;
					if(botLevel >= 71 && maxAAExpansion >= ExpansionTSS) {
						aaLevel += (botLevel - 70) > 5 ? 5 : botLevel - 70;  //Labyrinthine Mind - TSS AAs
						if(botLevel >= 76 && maxAAExpansion >= ExpansionSoF) {
							aaLevel += (botLevel - 75) > 5 ? 5 : botLevel - 75;  //SoF AAs	
							if(botLevel >= 81 && maxAAExpansion >= ExpansionSoD) {
								aaLevel += (botLevel - 80) > 5 ? 5 : botLevel - 80;  //SoD AAs	
								if(botLevel >= 85 && maxAAExpansion >= ExpansionUF) {
									aaLevel += 5;  //UF AAs
								}
							}
						}
					}
				}
			break;
			case aaSleightofHand:
			break;
			case aaSleightofHand2:
			break;
			case aaHealingAdeptMastery:
			break;
			case aaHealingGiftMastery:
			break;
			case aaArcaneTongues:
			break;
			case aaMasterofDisguise:
			break;
			case aaSlipperyAttacks:
				if(IsWarriorClass() && botLevel > 65 && maxAAExpansion >= ExpansionOoW) {
					aaLevel = (botLevel - 65) > 5 ? 5 : botLevel - 65;
				}
				break;
			case aaImprovedCriticalAffliction:
			break;
			case aaFortifiedBellowing:
			break;
			case aaFuryofMagic2:
			break;
			case aaDanceofBlades:
				if(botClass == BARD && botLevel >= 68 && maxAAExpansion >= ExpansionOoW) {
					aaLevel = 3;
				}
				break;
			case aaShieldofNotes:
			break;
			case aaRoarofThunder:
			break;
			case aaPersistentMinion:
			break;
			case aaPerfectionofSpirit:
			break;
			case aaReplentishCompanion:
			break;
			case aaAdvancedPetDiscipline:
			break;
			case aaThrowingMastery:
				if(botClass == BERSERKER && botLevel >= 59 && maxAAExpansion >= ExpansionSoL) {
					aaLevel = 3;
				}
				break;
			case aaBlurofAxes:
				if(botClass == BERSERKER && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = 3;
				}
				break;
			case aaHastenedWarCry:
			break;
			case aaDeadAim:
				if(botClass == BERSERKER && botLevel >= 61 && maxAAExpansion >= ExpansionPoP) {
					aaLevel = 3;
				}
				break;
			case aaFrenziedDefense:
			break;
			case aaTirelessSprint:
			break;
			case aaDesperation:
			break;
			case aaUntamedRage:
			break;
			case aaEchoingCries:
			break;
			case aaViciousFrenzy:
			break;
			case aaCrazedOnslaught:
			break;
			case aaOverwhelmingAttack:
			break;
			case aaFuriousRage:
			break;
			case aaBloodPact:
			break;
			case aaShieldingResistance:
			break;
			case aaHealingBoon:
			break;
			case aaResplendentCure:
			break;
			case aaCelestialHammer:
			break;
			case aaDivineRetribution:
			break;
			case aaCelestialRejuvination:
			break;
			case aaFerventBenediction:
			break;
			case aaSanctuary:
			break;
			case aaDestructiveFury:
				if(!(botClass == WARRIOR || botClass == MONK || botClass == ROGUE || botClass == BERSERKER) && botLevel >= 68 && maxAAExpansion >= ExpansionOoW) {
					aaLevel = (botLevel - 67) > 3 ? 3 : botLevel - 67;
					
					if(botClass == WIZARD && botLevel >= 70 && maxAAExpansion >= ExpansionDoDH) {
						aaLevel += 3;   // mastery of fury - DoDH AA
					}
				}
				break;
			case aaDestructiveFury2:
			break;
			case aaBoonoftheForest:
			break;
			case aaSpiritoftheGrove:
			break;
			case aaCalloftheWild:
			break;
			case aaSecondaryRecall:
			break;
			case aaNaturesBounty:
			break;
			case aaStasis:
			break;
			case aaColorShock:
			break;
			case aaMindOverMatter:
			break;
			case aaSoothingWords:
			break;
			case aaElementalSwarm:
			break;
			case aaHeartofFlames:
			break;
			case aaHeartofVapor:
			break;
			case aaHeartofIce:
			break;
			case aaHeartofStone:
			break;
			case aaImitateDeath:
			break;
			case aaCripplingStrike:
			break;
			case aaStunningKick:
			break;
			case aaEyeGouge:
			break;
			case aaIronKicks:
			break;
			case aaStyleoftheMimic:
			break;
			case aaDeathPeace2:
			break;
			case aaArmyoftheDead:
			break;
			case aaCelestialStun:
			break;
			case aaHandofDevotion:
			break;
			case aaSteadfastWill:
			break;
			case aaShieldBlock:
				if((botClass == WARRIOR || botClass == PALADIN || botClass == SHADOWKNIGHT) && botLevel >= 67 && maxAAExpansion >= ExpansionOoW) {
					aaLevel = (botLevel - 66) > 3 ? 3 : botLevel - 66;
					if(botLevel >= 77 && maxAAExpansion >= ExpansionSoF) {
						aaLevel += (botLevel - 76) > 3 ? 3 : botLevel - 76;   //SoF AAs
						if(botLevel >= 81 && maxAAExpansion >= ExpansionUF) {
							aaLevel += 3;   // UF AAs
						}
					}
				}
				break;
			case aaScoutsEfficiency:
			break;
			case aaGuardianoftheGlade:
			break;
			case aaTrackingMastery:
			break;
			case aaFlurryofKnives:
			break;
			case aaPrecision:
			break;
			case aaNervesofSteel:
			break;
			case aaTouchoftheCursed:
			break;
			case aaSpiritualCorrosion:
			break;
			case aaSoulThief:
				//Theft of Life
			break;
			case aaSpiritualChanneling:
			break;
			case aaBoonoftheAncients:
			break;
			case aaAncestralAid:
			break;
			case aaResoluteDefiance:
			break;
			case aaPresstheAttack:
			break;
			case aaMindCrash:
			break;
			case aaProlongedDestruction:
			break;
			case aaRosGreaterFamiliar:
			break;
			case aaEcisGreaterFamiliar:
			break;
			case aaDruzzilsGreaterFamiliar:
			break;
			case aaTeleportBind:
			break;
			case aaDevotedFamiliar:
			break;
			case aaAuspiceoftheHunter:
			break;
			case aaSavageSpirit:
			break;
			case aaPresstheAttack2:
			break;
			case aaCripplingStrike2:
			break;
			case aaStunningKick2:
			break;
			case aaEyeGouge2:
			break;
	        
			//Dragons of Norrath
			//good info here: http://www.eqthieves.com/exp-don-progression.htm and here: http://everquest.allakhazam.com/db/guides.html?guide=811
			case aaGiftoftheDarkReign:
			break;
			case aaTenacityoftheDarkReign:
			break;
			case aaEmbraceoftheDarkReign:
			break;
			case aaPoweroftheDarkReign:
			break;
			case aaFervoroftheDarkReign:
			break;
			case aaGiftoftheKeepers:
			break;
			case aaValoroftheKeepers:
			break;
			case aaEmbraceoftheKeepers:
			break;
			case aaPoweroftheKeepers:
			break;
			case aaSanctityoftheKeepers:
			break;

			//Depths of Darkhollow

			//the following 5 look to be used as flags for completion of the Blood Raids for access to the Demiplane of Blood
			//quest info here: http://everquest.allakhazam.com/db/quest.html?quest=3582
			//"You must also complete the five Blood Raids in any order: The Council of Nine, Emperor Draygun, Bloodeye, Matriarch Shyra, Sendaii, the Hive Queen"
			//"The AA's you receive are: Curse of Blood (1/5), Affliction of Blood (2/5), Torment of Blood (3/5), Temptation of Blood (4/5), Invitation of Blood (5/5)."
			case aaCurseofBlood:
			break;
			case aaAfflictionofBlood:
			break;
			case aaTormentofBlood:
			break;
			case aaTemptationofBlood:
			break;
			case aaInvitationofBlood:
			break;

			case aaTurnUndead2:
			break;
			case aaWrackUndead:
			break;
			case aaEradicateUndead:
			break;
			case aaInnateSeeInvis:
			break;
			case aaProlongedMortality:
			break;
			case aaPrecognition:
				//Combat Agility
			break;
			case aaThickSkin:
				//Combat Stability
				break;
			case aaSilentCasting:
			break;
			case aaSilentCasting2:
			break;
			case aaHastenedMindCrash:
			break;
			case aaFieldDressing:
			break;
			case aaBandageWounds:
			break;
			case aaCascadingRage:
			break;
			case aaElementalFerocity:
			break;
			case aaGiftofMana:
			break;
			case aaRuneofShadows:
			break;
			case aaChannelingMastery:
			break;
			case aaConservation:
			break;
			case aaCryofBattle:
			break;
			case aaWardofPurity:
			break;
			case aaTurnSummoned2:
			break;
			case aaWrackSummoned:
			break;
			case aaEradicateSummoned:
			break;
			case aaWardersSavagery:
			break;
			case aaShackleofSpirits:
			break;
			case aaHastenedThunder:
			break;
			case aaTranslocationalAnchor:
			break;
			case aaStealthyGetaway:
			break;
			case aaPyromancy:
			break;
			case aaMasteryofFury:
			break;
			case aaAbundantHealing:
			break;
			case aaGreaterAvatar:
			break;
			case aaSharedCamouflage:
			break;
			case aaConvergenceofSpirits:
			break;
			case aaNaturesGuardian:
			break;
			case aaEdictofCommand:
			break;
			case aaExtendedBurnout:
			break;
			case aaGuardianofRo:
			break;
			case aaBloodMagic:
			break;
			case aaGraverobbing:
			break;
			case aaAfflictionMastery:
			break;
			case aaGreaterRabidBear:
			break;
			case aaAncestralGuard:
			break;
			case aaCloakofLight:
			break;
			case aaVanquishUndead:
			break;
			case aaCloakofShadows:
			break;
			case aaWillfulDeath:
			break;
			case aaSwiftBlade:
			break;
			case aaWickedBlade:
				//Puniching Blade
			break;
			case aaForcedOpening:
			break;
			case aaAppraisal:
			break;
			case aaPreciseStrikes:
			break;
			case aaHastenedDeath:
			break;
			case aaUnflinchingResolve:
			break;
			case aaWeightlessSteps:
			break;
			case aaHastenedBlades:
			break;
			case aaImprovedHarmoniousAttack:
			break;
			case aaImprovedBestialFrenzy:
			break;
			case aaSongofStone:
			break;
			case aaDeepSleep:
			break;
			case aaCompanionsGift:
			break;
			case aaHastenedDefiance:
			break;
			case aaDauntlessPerseverance:
			break;
			case aaConcentration:
			break;
			case aaEnhancedAggression:
			break;
			case aaCallofChallenge:
			break;
			case aaCacophony:
			break;
			case aaImprovedHeadshot:
				//Headshot
			break;
			case aaAnatomy:
			break;
			case aaFetterofSpirits:
			break;
			case aaTrickShot:
			break;
			case aaLightningStrikes:
			break;
			case aaRelentlessAssault:
				// Ferocity
			break;
			case aaKnightsExpertise:
			break;
			case aaSelosEnduringCadence:
			break;
			case aaHarmTouch:
			break;
			case aaLayonHands:
			break;
			case aaLayonHandsRank16:
			break;
		}

	}

	return aaLevel;
}

bool Bot::IsValidRaceClassCombo() {
	bool Result = false;

	switch(GetRace()) {
		case 1: // Human
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 4: // Ranger
				case 5: // Shadowknight
				case 6: // Druid
				case 7: // Monk
				case 8: // Bard
				case 9: // Rogue
				case 11: // Necromancer
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
		case 2: // Barbarian
			switch(GetClass()) {
				case 1: // Warrior
				case 9: // Rogue
				case 10: // Shaman
				case 15: // Beastlord
				case 16: // Berserker
					Result = true;
					break;
			}
			break;
		case 3: // Erudite
			switch(GetClass()) {
				case 2: // Cleric
				case 3: // Paladin
				case 5: // Shadowknight
				case 11: // Necromancer
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
		case 4: // Wood Elf
			switch(GetClass()) {
				case 1: // Warrior
				case 4: // Ranger
				case 6: // Druid
				case 8: // Bard
				case 9: // Rogue
					Result = true;
					break;
			}
			break;
		case 5: // High Elf
			switch(GetClass()) {
				case 2: // Cleric
				case 3: // Paladin
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
		case 6: // Dark Elf
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 5: // Shadowknight
				case 9: // Rogue
				case 11: // Necromancer
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
		case 7: // Half Elf
			switch(GetClass()) {
				case 1: // Warrior
				case 3: // Paladin
				case 4: // Ranger
				case 6: // Druid
				case 8: // Bard
				case 9: // Rogue
					Result = true;
					break;
			}
			break;
		case 8: // Dwarf
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 9: // Rogue
				case 16: // Berserker
					Result = true;
					break;
			}
			break;
		case 9: // Troll
			switch(GetClass()) {
				case 1: // Warrior
				case 5: // Shadowknight
				case 10: // Shaman
				case 15: // Beastlord
				case 16: // Berserker
					Result = true;
					break;
			}
			break;
		case 10: // Ogre
			switch(GetClass()) {
				case 1: // Warrior
				case 5: // Shadowknight
				case 10: // Shaman
				case 15: // Beastlord
				case 16: // Berserker
					Result = true;
					break;
			}
			break;
		case 11: // Halfling
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 4: // Ranger
				case 6: // Druid
				case 9: // Rogue
					Result = true;
					break;
			}
			break;
		case 12: // Gnome
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 5: // Shadowknight
				case 9: // Rogue
				case 11: // Necromancer
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
		case 128: // Iksar
			switch(GetClass()) {
				case 1: // Warrior
				case 5: // Shadowknight
				case 7: // Monk
				case 10: // Shaman
				case 11: // Necromancer
				case 15: // Beastlord
					Result = true;
					break;
			}
			break;
		case 130: // Vah Shir
			switch(GetClass()) {
				case 1: // Warrior
				case 8: // Bard
				case 9: // Rogue
				case 10: // Shaman
				case 15: // Beastlord
				case 16: // Berserker
					Result = true;
					break;
			}
			break;
		case 330: // Froglok
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 5: // Shadowknight
				case 9: // Rogue
				case 10: // Shaman
				case 11: // Necromancer
				case 12: // Wizard
					Result = true;
					break;
			}
			break;
		case 522: // Drakkin
			switch(GetClass()) {
				case 1: // Warrior
				case 2: // Cleric
				case 3: // Paladin
				case 4: // Ranger
				case 5: // Shadowknight
				case 6: // Druid
				case 7: // Monk
				case 8: // Bard
				case 9: // Rogue
				case 11: // Necromancer
				case 12: // Wizard
				case 13: // Magician
				case 14: // Enchanter
					Result = true;
					break;
			}
			break;
	}

	return Result;
}

bool Bot::IsValidName() {
	bool Result = false;
	std::string TempBotName = std::string(this->GetCleanName());

	for(int iCounter = 0; iCounter < TempBotName.length(); iCounter++) {
		if(isalpha(TempBotName[iCounter]) || TempBotName[iCounter] == '_') {
			Result = true;
		}
	}

	return Result;
}

bool Bot::IsBotNameAvailable(std::string* errorMessage) {
	bool Result = false;

	if(this->GetCleanName()) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT COUNT(id) FROM vwBotCharacterMobs WHERE name LIKE '%s'", this->GetCleanName()), TempErrorMessageBuffer, &DatasetResult)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			uint32 ExistingNameCount = 0;

			while(DataRow = mysql_fetch_row(DatasetResult)) {
				ExistingNameCount = atoi(DataRow[0]);
				break;
			}

			if(ExistingNameCount == 0)
				Result = true;

			mysql_free_result(DatasetResult);
		}

		safe_delete(Query);
	}

	return Result;
}

bool Bot::Save() {
	bool Result = false;
	std::string errorMessage;

	char* Query = 0;
	char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
	int32 affectedRows = 0;

	if(this->GetBotID() == 0) {
		// New bot record
		uint32 TempNewBotID = 0;
		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "INSERT INTO bots (BotOwnerCharacterID, BotSpellsID, Name, LastName, BotLevel, Race, Class, Gender, Size, Face, LuclinHairStyle, LuclinHairColor, LuclinEyeColor, LuclinEyeColor2, LuclinBeardColor, LuclinBeard, DrakkinHeritage, DrakkinTattoo, DrakkinDetails, MR, CR, DR, FR, PR, Corrup, AC, STR, STA, DEX, AGI, _INT, WIS, CHA, ATK, LastSpawnDate, TotalPlayTime, LastZoneId) VALUES('%u', '%u', '%s', '%s', '%u', '%i', '%i', '%i', '%f', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', NOW(), 0, %i)", this->_botOwnerCharacterID, this->GetBotSpellID(), this->GetCleanName(), this->lastname, this->GetLevel(), GetRace(), GetClass(), GetGender(), GetSize(), this->GetLuclinFace(), this->GetHairStyle(), GetHairColor(), this->GetEyeColor1(), this->GetEyeColor2(), this->GetBeardColor(), this->GetBeard(), this->GetDrakkinHeritage(), this->GetDrakkinTattoo(), this->GetDrakkinDetails(), GetMR(), GetCR(), GetDR(), GetFR(), GetPR(), GetCorrup(), GetAC(), GetSTR(), GetSTA(), GetDEX(), GetAGI(), GetINT(), GetWIS(), GetCHA(), GetATK(), _lastZoneId), TempErrorMessageBuffer, 0, &affectedRows, &TempNewBotID)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			SetBotID(TempNewBotID);
			Result = true;
		}
	}
	else {
		// Update existing bot record
		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "UPDATE bots SET BotOwnerCharacterID = '%u', BotSpellsID = '%u', Name = '%s', LastName = '%s', BotLevel = '%u', Race = '%i', Class = '%i', Gender = '%i', Size = '%f', Face = '%i', LuclinHairStyle = '%i', LuclinHairColor = '%i', LuclinEyeColor = '%i', LuclinEyeColor2 = '%i', LuclinBeardColor = '%i', LuclinBeard = '%i', DrakkinHeritage = '%i', DrakkinTattoo = '%i', DrakkinDetails = '%i', MR = '%i', CR = '%i', DR = '%i', FR = '%i', PR = '%i', Corrup = '%i', AC = '%i', STR = '%i', STA = '%i', DEX = '%i', AGI = '%i', _INT = '%i', WIS = '%i', CHA = '%i', ATK = '%i', LastSpawnDate = NOW(), TotalPlayTime = '%u', LastZoneId = %i WHERE BotID = '%u'", _botOwnerCharacterID, this->GetBotSpellID(), this->GetCleanName(), this->lastname, this->GetLevel(), _baseRace, this->GetClass(), _baseGender, GetSize(), this->GetLuclinFace(), this->GetHairStyle(), GetHairColor(), this->GetEyeColor1(), this->GetEyeColor2(), this->GetBeardColor(), this->GetBeard(), this->GetDrakkinHeritage(), GetDrakkinTattoo(), GetDrakkinDetails(), _baseMR, _baseCR, _baseDR, _baseFR, _basePR, _baseCorrup, _baseAC, _baseSTR, _baseSTA, _baseDEX, _baseAGI, _baseINT, _baseWIS, _baseCHA, _baseATK, GetTotalPlayTime(), _lastZoneId, GetBotID()), TempErrorMessageBuffer, 0, &affectedRows)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			Result = true;
			time(&_startTotalPlayTime);
		}
	}

	safe_delete(Query);

	if(!errorMessage.empty() || (Result && affectedRows != 1)) {
		if(GetBotOwner() && !errorMessage.empty())
			GetBotOwner()->Message(13, errorMessage.c_str());
		else if(GetBotOwner())
			GetBotOwner()->Message(13, std::string("Unable to save bot to the database.").c_str());

		Result = false;
	}
	else {
		SaveBuffs();
		SavePet();
	}

	return Result;
}

// Returns the current total play time for the bot
uint32 Bot::GetTotalPlayTime() {
	uint32 Result = 0;

	double TempTotalPlayTime = 0;

	time_t currentTime = time(&currentTime);

	TempTotalPlayTime = difftime(currentTime, _startTotalPlayTime);

	TempTotalPlayTime += _lastTotalPlayTime;

	Result = (uint32)TempTotalPlayTime;

	return Result;
}

void Bot::SaveBuffs() {
	std::string errorMessage;
	char* Query = 0;
	char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
	int BuffCount = 0;
	int InsertCount = 0;

	uint32 buff_count = GetMaxTotalSlots();
	while(BuffCount < BUFF_COUNT) {
		if(buffs[BuffCount].spellid > 0 && buffs[BuffCount].spellid != SPELL_UNKNOWN) {
			if(InsertCount == 0) {
				// Remove any existing buff saves
				if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botbuffs WHERE BotId = %u", GetBotID()), TempErrorMessageBuffer)) {
					errorMessage = std::string(TempErrorMessageBuffer);
					safe_delete(Query);
					Query = 0;
					break;
				}
			}

			int IsPersistent = 0;

			if(buffs[BuffCount].persistant_buff)
				IsPersistent = 1;
			else
				IsPersistent = 0;

			if(!database.RunQuery(Query, MakeAnyLenString(&Query, "INSERT INTO botbuffs (BotId, SpellId, CasterLevel, DurationFormula, "
                "TicsRemaining, PoisonCounters, DiseaseCounters, CurseCounters, CorruptionCounters, HitCount, MeleeRune, MagicRune, "
                "DeathSaveSuccessChance, CasterAARank, Persistent) VALUES (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u);", 
                GetBotID(), buffs[BuffCount].spellid, buffs[BuffCount].casterlevel, spells[buffs[BuffCount].spellid].buffdurationformula, 
                buffs[BuffCount].ticsremaining, 
                CalculatePoisonCounters(buffs[BuffCount].spellid) > 0 ? buffs[BuffCount].counters : 0, 
                CalculateDiseaseCounters(buffs[BuffCount].spellid) > 0 ? buffs[BuffCount].counters : 0, 
                CalculateCurseCounters(buffs[BuffCount].spellid) > 0 ? buffs[BuffCount].counters : 0, 
                CalculateCorruptionCounters(buffs[BuffCount].spellid) > 0 ? buffs[BuffCount].counters : 0, 
                buffs[BuffCount].numhits, buffs[BuffCount].melee_rune, buffs[BuffCount].magic_rune, 
                buffs[BuffCount].deathSaveSuccessChance, 
                buffs[BuffCount].deathsaveCasterAARank, IsPersistent), TempErrorMessageBuffer)) {
				errorMessage = std::string(TempErrorMessageBuffer);
				safe_delete(Query);
				Query = 0;
				break;
			}
			else {
				safe_delete(Query);
				Query = 0;
				InsertCount++;
			}
		}

		BuffCount++;
	}

	if(!errorMessage.empty()) {
		// TODO: Record this error message to zone error log
	}
}

void Bot::LoadBuffs() {
	std::string errorMessage;
	char* Query = 0;
	char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
	MYSQL_RES* DatasetResult;
	MYSQL_ROW DataRow;

	bool BuffsLoaded = false;

	if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT SpellId, CasterLevel, DurationFormula, TicsRemaining, PoisonCounters, DiseaseCounters, CurseCounters, CorruptionCounters, HitCount, MeleeRune, MagicRune, DeathSaveSuccessChance, CasterAARank, Persistent FROM botbuffs WHERE BotId = %u", GetBotID()), TempErrorMessageBuffer, &DatasetResult)) {
		errorMessage = std::string(TempErrorMessageBuffer);
	}
	else {
		int BuffCount = 0;

		while(DataRow = mysql_fetch_row(DatasetResult)) {
			if(BuffCount == BUFF_COUNT)
				break;

			buffs[BuffCount].spellid = atoi(DataRow[0]);
			buffs[BuffCount].casterlevel = atoi(DataRow[1]);
			buffs[BuffCount].ticsremaining = atoi(DataRow[3]);

            if(CalculatePoisonCounters(buffs[BuffCount].spellid) > 0) {
                buffs[BuffCount].counters = atoi(DataRow[4]);
            } else if(CalculateDiseaseCounters(buffs[BuffCount].spellid) > 0) {
                buffs[BuffCount].counters = atoi(DataRow[5]);
            } else if(CalculateCurseCounters(buffs[BuffCount].spellid) > 0) {
                buffs[BuffCount].counters = atoi(DataRow[6]);
            } else if(CalculateCorruptionCounters(buffs[BuffCount].spellid) > 0) {
                buffs[BuffCount].counters = atoi(DataRow[7]);
            } 
			buffs[BuffCount].numhits = atoi(DataRow[8]);
			buffs[BuffCount].melee_rune = atoi(DataRow[9]);
			buffs[BuffCount].magic_rune = atoi(DataRow[10]);
			buffs[BuffCount].deathSaveSuccessChance = atoi(DataRow[11]);
			buffs[BuffCount].deathsaveCasterAARank = atoi(DataRow[12]);
			buffs[BuffCount].casterid = 0;

			bool IsPersistent = false;

			if(atoi(DataRow[13]))
				IsPersistent = true;

			buffs[BuffCount].persistant_buff = IsPersistent;

			BuffCount++;
		}

		mysql_free_result(DatasetResult);

		BuffsLoaded = true;
	}

	safe_delete(Query);
	Query = 0;

	if(errorMessage.empty() && BuffsLoaded) {
		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botbuffs WHERE BotId = %u", GetBotID()), TempErrorMessageBuffer)) {
			errorMessage = std::string(TempErrorMessageBuffer);
			safe_delete(Query);
			Query = 0;
		}
	}

	if(!errorMessage.empty()) {
		// TODO: Record this error message to zone error log
	}
}

uint32 Bot::GetPetSaveId() {
	uint32 Result = 0;
	std::string errorMessage;
	char* Query = 0;
	char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
	MYSQL_RES* DatasetResult;
	MYSQL_ROW DataRow;

	if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select BotPetsId from botpets where BotId = %u;", GetBotID()), TempErrorMessageBuffer, &DatasetResult)) {
		errorMessage = std::string(TempErrorMessageBuffer);
	}
	else {
		while(DataRow = mysql_fetch_row(DatasetResult)) {
			Result = atoi(DataRow[0]);
			break;
		}

		mysql_free_result(DatasetResult);
	}

	safe_delete(Query);

	if(!errorMessage.empty()) {
		// TODO: Record this error message to zone error log
	}

	return Result;
}

void Bot::LoadPet() {
	uint32 PetSaveId = GetPetSaveId();

	if(PetSaveId > 0 && !GetPet() && PetSaveId <= SPDAT_RECORDS) {
		std::string petName;
		int16 petMana = 0;
		int16 petHitPoints = 0;
		uint32 botPetId = 0;

		LoadPetStats(&petName, &petMana, &petHitPoints, &botPetId, PetSaveId);

		MakePet(botPetId, spells[botPetId].teleport_zone, petName.c_str());

		if(GetPet() && GetPet()->IsNPC()) {
			NPC *pet = GetPet()->CastToNPC();
			SpellBuff_Struct petBuffs[BUFF_COUNT];
			int32 petItems[MAX_WORN_INVENTORY];

			LoadPetBuffs(petBuffs, PetSaveId);
			LoadPetItems(petItems, PetSaveId);

			pet->SetPetState(petBuffs, petItems);
			pet->CalcBonuses();
			pet->SetHP(petHitPoints);
			pet->SetMana(petMana);
		}

		DeletePetStats(PetSaveId);
	}
}

void Bot::LoadPetStats(std::string* petName, int16* petMana, int16* petHitPoints, uint32* botPetId, uint32 botPetSaveId) {
	if(botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		bool statsLoaded = false;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select PetId, Name, Mana, HitPoints from botpets where BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer, &DatasetResult)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				*botPetId = atoi(DataRow[0]);
				*petName = std::string(DataRow[1]);
				*petMana = atoi(DataRow[2]);
				*petHitPoints = atoi(DataRow[3]);
				break;
			}

			mysql_free_result(DatasetResult);

			statsLoaded = true;
		}

		safe_delete(Query);
		Query = 0;

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

void Bot::LoadPetBuffs(SpellBuff_Struct* petBuffs, uint32 botPetSaveId) {
	if(petBuffs && botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		bool BuffsLoaded = false;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT SpellId, CasterLevel, Duration FROM botpetbuffs WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer, &DatasetResult)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			int BuffCount = 0;

			while(DataRow = mysql_fetch_row(DatasetResult)) {
				if(BuffCount == BUFF_COUNT)
					break;

				petBuffs[BuffCount].spellid = atoi(DataRow[0]);
				petBuffs[BuffCount].level = atoi(DataRow[1]);
				petBuffs[BuffCount].duration = atoi(DataRow[2]);

				BuffCount++;
			}

			mysql_free_result(DatasetResult);

			BuffsLoaded = true;
		}

		safe_delete(Query);
		Query = 0;

		if(errorMessage.empty() && BuffsLoaded) {
			if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botpetbuffs WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer)) {
				errorMessage = std::string(TempErrorMessageBuffer);
				safe_delete(Query);
				Query = 0;
			}
		}

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

void Bot::LoadPetItems(int32* petItems, uint32 botPetSaveId) {
	if(petItems && botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		bool itemsLoaded = false;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT ItemId FROM botpetinventory WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer, &DatasetResult)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			int ItemCount = 0;

			while(DataRow = mysql_fetch_row(DatasetResult)) {
				if(ItemCount == MAX_WORN_INVENTORY)
					break;

				petItems[ItemCount] = atoi(DataRow[0]);

				ItemCount++;
			}

			mysql_free_result(DatasetResult);

			itemsLoaded = true;
		}

		safe_delete(Query);
		Query = 0;

		if(errorMessage.empty() && itemsLoaded) {
			if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botpetinventory WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer)) {
				errorMessage = std::string(TempErrorMessageBuffer);
				safe_delete(Query);
				Query = 0;
			}
		}

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

void Bot::SavePet() {
	if(GetPet() && !GetPet()->IsFamiliar() && GetPet()->CastToNPC()->GetPetSpellID() /*&& !dead*/) {
		NPC *pet = GetPet()->CastToNPC();
		int16 petMana = pet->GetMana();
		int16 petHitPoints = pet->GetHP();
		uint32 botPetId = pet->CastToNPC()->GetPetSpellID();
		char* tempPetName = new char[64];
		SpellBuff_Struct petBuffs[BUFF_COUNT];
		int32 petItems[MAX_WORN_INVENTORY];

		pet->GetPetState(petBuffs, petItems, tempPetName);
		
		uint32 existingBotPetSaveId = GetPetSaveId();

		if(existingBotPetSaveId > 0) {
			// Remove any existing pet buffs
			DeletePetBuffs(existingBotPetSaveId);

			// Remove any existing pet items
			DeletePetItems(existingBotPetSaveId);
		}

		// Save pet stats and get a new bot pet save id
		uint32 botPetSaveId = SavePetStats(std::string(tempPetName), petMana, petHitPoints, botPetId);

		// Save pet buffs
		SavePetBuffs(petBuffs, botPetSaveId);

		// Save pet items
		SavePetItems(petItems, botPetSaveId);

		if(tempPetName)
			safe_delete_array(tempPetName);
	}
}

uint32 Bot::SavePetStats(std::string petName, int16 petMana, int16 petHitPoints, uint32 botPetId) {
	uint32 Result = 0;

	std::string errorMessage;
	char* Query = 0;
	char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];

	if(!database.RunQuery(Query, MakeAnyLenString(&Query, "REPLACE INTO botpets SET PetId = %u, BotId = %u, Name = '%s', Mana = %u, HitPoints = %u;", botPetId, GetBotID(), petName.c_str(), petMana, petHitPoints), TempErrorMessageBuffer, 0, 0, &Result)) {
		errorMessage = std::string(TempErrorMessageBuffer);
	}

	safe_delete(Query);
	Query = 0;

	return Result;
}

void Bot::SavePetBuffs(SpellBuff_Struct* petBuffs, uint32 botPetSaveId) {
	if(petBuffs && botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		int BuffCount = 0;

		while(BuffCount < BUFF_COUNT) {
			if(petBuffs[BuffCount].spellid > 0 && petBuffs[BuffCount].spellid != SPELL_UNKNOWN) {
				if(!database.RunQuery(Query, MakeAnyLenString(&Query, "INSERT INTO botpetbuffs (BotPetsId, SpellId, CasterLevel, Duration) VALUES(%u, %u, %u, %u);", botPetSaveId, petBuffs[BuffCount].spellid, petBuffs[BuffCount].level, petBuffs[BuffCount].duration), TempErrorMessageBuffer)) {
					errorMessage = std::string(TempErrorMessageBuffer);
					safe_delete(Query);
					Query = 0;
					break;
				}
				else {
					safe_delete(Query);
					Query = 0;
				}
			}

			BuffCount++;
		}

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

void Bot::SavePetItems(int32* petItems, uint32 botPetSaveId) {
	if(petItems && botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		int ItemCount = 0;

		while(ItemCount < MAX_WORN_INVENTORY) {
			if(petItems[ItemCount] > 0) {
				if(!database.RunQuery(Query, MakeAnyLenString(&Query, "INSERT INTO botpetinventory (BotPetsId, ItemId) VALUES(%u, %u);", botPetSaveId, petItems[ItemCount]), TempErrorMessageBuffer)) {
					errorMessage = std::string(TempErrorMessageBuffer);
					safe_delete(Query);
					Query = 0;
					break;
				}
				else {
					safe_delete(Query);
					Query = 0;
					ItemCount++;
				}
			}

			ItemCount++;
		}

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

void Bot::DeletePetBuffs(uint32 botPetSaveId) {
	if(botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botpetbuffs WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}

		safe_delete(Query);
		Query = 0;
	}
}

void Bot::DeletePetItems(uint32 botPetSaveId) {
	if(botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botpetinventory WHERE BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}

		safe_delete(Query);
		Query = 0;
	}
}

void Bot::DeletePetStats(uint32 botPetSaveId) {
	if(botPetSaveId > 0) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE from botpets where BotPetsId = %u;", botPetSaveId), TempErrorMessageBuffer)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}

		safe_delete(Query);
		Query = 0;
	}
}

bool Bot::Process()
{
	_ZP(Bot_Process);
	
	if(IsStunned() && stunned_timer.Check())
	{
		this->stunned = false;
		this->stunned_timer.Disable();
	}

	if(!GetBotOwner())
		return false;

	if (GetDepop())
	{
		_botOwner = 0;
		_botOwnerCharacterID = 0;
		_previousTarget = 0;
		return false;
	}

	SpellProcess();

	if(tic_timer.Check())
	{
		//6 seconds, or whatever the rule is set to has passed, send this position to everyone to avoid ghosting
		if(!IsMoving() && !IsEngaged())
		{
			SendPosition();
			if(IsSitting())
			{
				if(!rest_timer.Enabled())
				{
					rest_timer.Start(RuleI(Character, RestRegenTimeToActivate) * 1000);
				}
			}
		}

		BuffProcess();
		
		CalcRestState();

		if(curfp)
			ProcessFlee();

		if(GetHP() < GetMaxHP())
			SetHP(GetHP() + CalcHPRegen() + RestRegenHP);

		if(GetMana() < GetMaxMana())
			SetMana(GetMana() + CalcManaRegen() + RestRegenMana);

		CalcATK();
	}

	if (sendhpupdate_timer.Check()) {
		SendHPUpdate();

		if(HasPet())
			GetPet()->SendHPUpdate();
	}

	if (IsStunned() || IsMezzed())
		return true;

	//Handle assists...
	/*if(assist_timer.Check() && !Charmed() && GetTarget() != NULL) {
		entity_list.AIYellForHelp(this, GetTarget());
	}*/

	// Bot AI
	AI_Process();

	// Bot Pet AI
	if(HasPet())
		PetAIProcess();

	return true;
}

void Bot::SpellProcess()
{
	// check the rapid recast prevention timer
	if(spellend_timer.Check(false))
	{
		NPC::SpellProcess();

		if(GetClass() == BARD) {
			if (casting_spell_id != 0) 
				casting_spell_id = 0;
		}
	}
}

void Bot::BotMeditate(bool isSitting) {
	if(isSitting) {
		// If the bot is a caster has less than 99% mana while its not engaged, he needs to sit to meditate
		if(GetManaRatio() < 99.0f)
		{
			if(!IsSitting())
				Sit();
		}
		else
		{
			if(IsSitting())
				Stand();
		}
	}
	else
	{
		if(IsSitting())
			Stand();
	}
	if(IsSitting())
	{
		if(!rest_timer.Enabled())
		{
			rest_timer.Start(RuleI(Character, RestRegenTimeToActivate) * 1000);
		}
	}
	else
	{
		rest_timer.Disable();
	}
}

bool Bot::BotRangedAttack(Mob* other) {
	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if((attack_timer.Enabled() && !attack_timer.Check(false)) || (ranged_timer.Enabled() && !ranged_timer.Check())) {
		mlog(COMBAT__RANGED, "Bot Archery attack canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		Message(0, "Error: Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return false;
	}

	ItemInst* rangedItem = GetBotItem(SLOT_RANGE);
	const Item_Struct* RangeWeapon = 0;
	if(rangedItem)
		RangeWeapon = rangedItem->GetItem();

	ItemInst* ammoItem = GetBotItem(SLOT_AMMO);
	const Item_Struct* Ammo = 0;
	if(ammoItem)
		Ammo = ammoItem->GetItem();

	if(!RangeWeapon || !Ammo)
		return false;

	
	mlog(COMBAT__RANGED, "Shooting %s with bow %s (%d) and arrow %s (%d)", other->GetCleanName(), RangeWeapon->Name, RangeWeapon->ID, Ammo->Name, Ammo->ID);
	
	if(!IsAttackAllowed(other) || 
		IsCasting() || 
		DivineAura() ||
		IsStunned() ||
		IsMezzed() ||
		(GetAppearance() == eaDead))
	{
		return false;
	}
	
	SendItemAnimation(other, Ammo, ARCHERY);

	// Hit?
	if(!other->CheckHitChance(other, ARCHERY, SLOT_RANGE)) {
		mlog(COMBAT__RANGED, "Ranged attack missed %s.", other->GetCleanName());
		other->Damage(this, 0, SPELL_UNKNOWN, ARCHERY);
	}
	else {
		mlog(COMBAT__RANGED, "Ranged attack hit %s.", other->GetCleanName());
		
		if(!TryHeadShot(other, ARCHERY)) {
			sint16 WDmg = GetWeaponDamage(other, RangeWeapon);
			sint16 ADmg = GetWeaponDamage(other, Ammo);
			if((WDmg > 0) || (ADmg > 0)){
				if(WDmg < 0)
					WDmg = 0;
				if(ADmg < 0)
					ADmg = 0;

				uint32 MaxDmg = (2*(WDmg+ADmg)*GetDamageTable(ARCHERY)) / 100;

				switch(GetAA(aaArcheryMastery)) {
					case 1:
						MaxDmg = MaxDmg * 130/100;
						break;
					case 2:
						MaxDmg = MaxDmg * 160/100;
						break;
					case 3:
						MaxDmg = MaxDmg * 2;
						break;
				}

				mlog(COMBAT__RANGED, "Bow DMG %d, Arrow DMG %d, Max Damage %d.", WDmg, ADmg, MaxDmg);
				
				if(GetClass()==RANGER && other->IsNPC() && !other->IsMoving() && !other->IsRooted() && GetLevel() > 50){
					MaxDmg *= 2;
					mlog(COMBAT__RANGED, "Ranger. Target is stationary, doubling max damage to %d", MaxDmg);
				}

				sint32 TotalDmg = 0;

				if (MaxDmg == 0)
					MaxDmg = 1;

				if(RuleB(Combat, UseIntervalAC))
					TotalDmg = MaxDmg;
				else
					TotalDmg = MakeRandomInt(1, MaxDmg);

				int minDmg = 1;
				if(GetLevel() > 25){
					//twice, for ammo and weapon
					TotalDmg += (2*((GetLevel()-25)/3));
					minDmg += (2*((GetLevel()-25)/3));
				}

				other->MeleeMitigation(this, TotalDmg, minDmg);
				ApplyMeleeDamageBonus(ARCHERY, TotalDmg);
				TryCriticalHit(other, ARCHERY, TotalDmg);
				other->Damage(this, TotalDmg, SPELL_UNKNOWN, ARCHERY);
			}
			else {
				other->Damage(this, -5, SPELL_UNKNOWN, ARCHERY);
			}
		}
	}

	//try proc on hits and misses
	if(other && (other->GetHP() > -10)) {
		TryWeaponProc(RangeWeapon, other);
	}

	return true;
}

bool Bot::CheckBotDoubleAttack(bool tripleAttack) {
	// If you don't have the double attack skill, return
	if(!GetSkill(DOUBLE_ATTACK) && !(GetClass() == BARD || GetClass() == BEASTLORD))
		return false;
	
	// You start with no chance of double attacking
	int chance = 0;
	
	// Used for maxSkill and triple attack calcs
	int8 classtype = GetClass();
	
	// The current skill level
	uint16 skill = GetSkill(DOUBLE_ATTACK);

	// Discipline bonuses give you 100% chance to double attack
	sint16 buffs = spellbonuses.DoubleAttackChance + itembonuses.DoubleAttackChance;
	
	Mob* BotOwner = this->GetBotOwner();

	// The maximum value for the Class based on the server rule of MaxLevel
	if(!BotOwner || BotOwner->qglobal || (GetAppearance() == eaDead))
		return false;
	int16 maxSkill = BotOwner->CastToClient()->MaxSkill(DOUBLE_ATTACK, classtype, RuleI(Character, MaxLevel));

	// AA bonuses for the melee classes
	int32 aaBonus =
		GetAA(aaBestialFrenzy) +
		GetAA(aaHarmoniousAttack) +
		GetAA(aaKnightsAdvantage)*10 +
		GetAA(aaFerocity)*10;
	
	// Bard Dance of Blades Double Attack bonus is not cumulative
	if(GetAA(aaDanceofBlades)) {
		aaBonus += 500;
	}

	// Half of Double Attack Skill used to check chance for Triple Attack
	if(tripleAttack) {
		// Only some Double Attack classes get Triple Attack
		if((classtype == MONK) || (classtype == WARRIOR) || (classtype == RANGER) || (classtype == BERSERKER)) {
			// We only get half the skill, but should get all the bonuses
			chance = (skill/2) + buffs + aaBonus;
		}
		else {
			return false;
		}
	}
	else {
		// This is the actual Double Attack chance
		chance = skill + buffs + aaBonus;
	}
	
	// If your chance is greater than the RNG you are successful! Always have a 5% chance to fail at max skills+bonuses.
	if(chance > MakeRandomInt(0, (maxSkill + itembonuses.DoubleAttackChance + aaBonus)*1.05)) {
		return true;
	}

	return false;
}

void Bot::SetTarget(Mob* mob) {
	if(mob != this) {
		if(mob != GetTarget())
			_previousTarget = GetTarget();

		NPC::SetTarget(mob);
	}
}

float Bot::GetMaxMeleeRangeToTarget(Mob* target) {
	float result = 0;
	
	if(target) {
		float size_mod = GetSize();
		float other_size_mod = target->GetSize();

		if(GetRace() == 49 || GetRace() == 158 || GetRace() == 196) //For races with a fixed size
			size_mod = 60.0f;
		else if (size_mod < 6.0)
			size_mod = 8.0f;

		if(target->GetRace() == 49 || target->GetRace() == 158 || target->GetRace() == 196) //For races with a fixed size
			other_size_mod = 60.0f;
		else if (other_size_mod < 6.0)
			other_size_mod = 8.0f;

		if (other_size_mod > size_mod) {
			size_mod = other_size_mod;
		}

		// this could still use some work, but for now it's an improvement....

		if (size_mod > 29)
			size_mod *= size_mod;
		else if (size_mod > 19)
			size_mod *= size_mod * 2;
		else
			size_mod *= size_mod * 4;

		// prevention of ridiculously sized hit boxes
		if (size_mod > 10000)
			size_mod = size_mod / 7;

		result = size_mod;
	}

	return result;
}

// AI Processing for the Bot object
void Bot::AI_Process() {
	_ZP(Mob_BOT_Process);

	if(!IsAIControlled())
		return;

	int8 botClass = GetClass();
	uint8 botLevel = GetLevel();

	if(IsCasting() && (botClass != BARD))
		return;

	// A bot wont start its AI if not grouped
	if(!GetBotOwner() || !IsGrouped()) {
		return;
	}

	Mob* BotOwner = GetBotOwner();

	// The bots need an owner
	if(!BotOwner)
		return;

	try {
		if(BotOwner->CastToClient()->IsDead()) {
			SetTarget(0);
			SetBotOwner(0);
			return;
		}
	}
	catch(...) {
		SetTarget(0);
		SetBotOwner(0);
		return;
	}

	if(!IsEngaged()) {
		if(GetFollowID()) {
			if(BotOwner && BotOwner->CastToClient()->AutoAttackEnabled() && BotOwner->GetTarget() &&
				BotOwner->GetTarget()->IsNPC() && BotOwner->GetTarget()->GetHateAmount(BotOwner)) {
					AddToHateList(BotOwner->GetTarget(), 1);
					
					if(HasPet())
						GetPet()->AddToHateList(BotOwner->GetTarget(), 1);
			}
			else {
				Group* g = GetGroup();

				if(g) {
					for(int counter = 0; counter < g->GroupCount(); counter++) {
						if(g->members[counter]) {
							if(g->members[counter]->IsEngaged() && g->members[counter]->GetTarget()) {
								AddToHateList(g->members[counter]->GetTarget(), 1);

								if(HasPet())
									GetPet()->AddToHateList(g->members[counter]->GetTarget(), 1);

								break;
							}
						}
					}
				}
			}
		}
	}

	if(IsEngaged())
	{
		_ZP(Mob_BOT_Process_IsEngaged);

		if(rest_timer.Enabled())
			rest_timer.Disable();

		if(IsRooted())
			SetTarget(hate_list.GetClosest(this));
		else
			SetTarget(hate_list.GetTop(this));

		if(!GetTarget())
			return;

		if(HasPet())
			GetPet()->SetTarget(GetTarget());

		FaceTarget(GetTarget());

		if(DivineAura())
			return;

		// Let's check if we have a los with our target.
		// If we don't, our hate_list is wiped.
		// Else, it was causing the bot to aggro behind wall etc... causing massive trains.
		if(!CheckLosFN(GetTarget()) || GetTarget()->IsMezzed() || !IsAttackAllowed(GetTarget())) {
			WipeHateList();

			if(IsMoving()) {
				SetHeading(0);
				SetRunAnimSpeed(0);

				if(moved) {
					moved = false;
					SendPosition();
					SetMoving(false);
				}
			}

			return;
		}

		bool atCombatRange = false;
		
		float meleeDistance = GetMaxMeleeRangeToTarget(GetTarget());

		if(botClass == SHADOWKNIGHT || botClass == PALADIN || botClass == WARRIOR) {
			meleeDistance = meleeDistance * .30;
		}
		else {
			meleeDistance *= (float)MakeRandomFloat(.50, .85);
		}
		
		bool atArcheryRange = IsArcheryRange(GetTarget());

		if(GetRangerAutoWeaponSelect()) {
			bool changeWeapons = false;

			if(atArcheryRange && !IsBotArcher()) {
				SetBotArcher(true);
				changeWeapons = true;
			}
			else if(!atArcheryRange && IsBotArcher()) {
				SetBotArcher(false);
				changeWeapons = true;
			}

			if(changeWeapons)
				ChangeBotArcherWeapons(IsBotArcher());
		}

		if(IsBotArcher() && atArcheryRange) {
			if(IsMoving()) {
				SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
				SetRunAnimSpeed(0);

				if(moved) {
					moved = false;
					SendPosition();
					SetMoving(false);
				}
			}

			atCombatRange = true;
		}
		else if(IsBotCaster() && GetLevel() > 12) {
			if(IsBotCasterCombatRange(GetTarget()))
				atCombatRange = true;
		}
		else if(DistNoRoot(*GetTarget()) <= meleeDistance) {
			atCombatRange = true;
		}

		if(atCombatRange) {	
			if(IsMoving()) {
				SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
				SetRunAnimSpeed(0);
				
				if(moved) {
					moved = false;
					SendPosition();
					SetMoving(false);
				}
			}

			if(AImovement_timer->Check()) {
				if(!IsMoving() && GetClass() == ROGUE && !BehindMob(GetTarget(), GetX(), GetY())) {
					// Move the rogue to behind the mob
					float newX = 0;
					float newY = 0;
					float newZ = 0;

					if(PlotPositionAroundTarget(GetTarget(), newX, newY, newZ)) {
						CalculateNewPosition2(newX, newY, newZ, GetRunspeed());
						return;
					}
				}
				else if(!IsMoving() && GetClass() != ROGUE && (DistNoRootNoZ(*GetTarget()) < GetTarget()->GetSize())) {
					// If we are not a rogue trying to backstab, let's try to adjust our melee range so we don't appear to be bunched up
					float newX = 0;
					float newY = 0;
					float newZ = 0;

					if(PlotPositionAroundTarget(GetTarget(), newX, newY, newZ, false)) {
						CalculateNewPosition2(newX, newY, newZ, GetRunspeed());
						return;
					}
				}

				if(IsMoving())
					SendPosUpdate();
				else
					SendPosition();
			}

			if(IsBotArcher() && ranged_timer.Check(false)) {
				if(GetTarget()->GetHPRatio() < 98)
					BotRangedAttack(GetTarget());
			}
			else if(!IsBotArcher() && (!(IsBotCaster() && GetLevel() > 12)) && GetTarget() && !IsStunned() && !IsMezzed() && (GetAppearance() != eaDead)) {
				// we can't fight if we don't have a target, are stun/mezzed or dead..
				// Stop attacking if the target is enraged
				if(IsEngaged() && !BehindMob(GetTarget(), GetX(), GetY()) && GetTarget()->IsEnraged())
					return;

				// First, special attack per class (kick, backstab etc..)
				DoClassAttacks(GetTarget());

				//try main hand first
				if(attack_timer.Check()) {
					Attack(GetTarget(), SLOT_PRIMARY);

					bool tripleSuccess = false;

					if(BotOwner && GetTarget() && CanThisClassDoubleAttack()) {

						if(BotOwner && CheckBotDoubleAttack()) {
							Attack(GetTarget(), SLOT_PRIMARY, true);
						}

						if(BotOwner && GetTarget() && SpecAttacks[SPECATK_TRIPLE] && CheckBotDoubleAttack(true)) {
							tripleSuccess = true;
							Attack(GetTarget(), SLOT_PRIMARY, true);
						}

						//quad attack, does this belong here??
						if(BotOwner && GetTarget() && SpecAttacks[SPECATK_QUAD] && CheckBotDoubleAttack(true)) {
							Attack(GetTarget(), SLOT_PRIMARY, true);
						}
					}

					// Handle Flurrys
					if((GetAA(aaFlurry) > 0 || spellbonuses.FlurryChance > 0 || itembonuses.FlurryChance > 0)) {

						int32 flurrychance = (itembonuses.FlurryChance + spellbonuses.FlurryChance) * 10;
                                        	switch (GetAA(aaFlurry))
                                        	{
                                        	case 1:
                                                	flurrychance += 10;
                                                	break;
                                        	case 2:
                                                	flurrychance += 25;
                                                	break;
                                        	case 3:
                                                	flurrychance += 50;
                                                	break;
                                        	}

                                        	if(tripleSuccess) {
                                                	tripleSuccess = false;
                                                	switch (GetAA(aaRagingFlurry)) {
                                                	case 1:
                                                        	flurrychance += 10;
                                                        	break;
                                                	case 2:
                                                        	flurrychance += 25;
                                                        	break;
                                                	case 3:
                                                        	flurrychance += 50;
                                                        	break;
                                                	}
                                        	}


						if(MakeRandomInt(0, 999) < flurrychance) {
							Message_StringID(MT_NPCFlurry, 128);
							Attack(GetTarget(), SLOT_PRIMARY, true);
							Attack(GetTarget(), SLOT_PRIMARY, true);
						}
					}

					if(GetTarget() && GetAA(aaRapidStrikes)) { // Rapid Strikes AA
						int chance_xhit1 = 0;
                                        int chance_xhit2 = 0;
                                        switch (GetAA(aaRapidStrikes))
                                        {
                                        	case 1:
                                                	chance_xhit1 = 10;
                                                	chance_xhit2 = 2;
                                                	break;
                                        	case 2:
                                                	chance_xhit1 = 12;
                                                	chance_xhit2 = 4;
                                                	break;
                                        	case 3:
                                                	chance_xhit1 = 14;
                                                	chance_xhit2 = 6;
                                                	break;
                                        	case 4:
                                                	chance_xhit1 = 16;
                                                	chance_xhit2 = 8;
                                                	break;
                                        	case 5:
                                                	chance_xhit1 = 20;
                                                	chance_xhit2 = 10;
                                                	break;
                                        	}

						if(MakeRandomInt(1,100) < chance_xhit1)
							Attack(GetTarget(), SLOT_PRIMARY, true);

						if(GetTarget() && (MakeRandomInt(1,100) < chance_xhit2))
							Attack(GetTarget(), SLOT_PRIMARY, true);
					}

					// Handle Punishing Blade and Speed of the Knight and Wicked Blade
					if(GetTarget() && (GetAA(aaPunishingBlade) > 0 || GetAA(aaSpeedoftheKnight) > 0)) {
						if(botLevel >= 61) {
							ItemInst* weapon = GetBotItem(SLOT_PRIMARY);
							if(weapon) {
								if( weapon->GetItem()->ItemType == ItemType2HS ||
									weapon->GetItem()->ItemType == ItemType2HB ||
									weapon->GetItem()->ItemType == ItemType2HPierce )
								{
									int extatk = GetAA(aaPunishingBlade)*5;
										extatk += GetAA(aaSpeedoftheKnight)*5;

									if(MakeRandomInt(0, 100) < extatk) {
										Attack(GetTarget(), SLOT_PRIMARY, true);
									}
								}
							}
						}
					}
				}

				//now off hand
				if(GetTarget() && attack_dw_timer.Check() && CanThisClassDualWield()) {
					const ItemInst* instweapon = GetBotItem(SLOT_SECONDARY);
					const Item_Struct* weapon = 0;
					//can only dual wield without a weapon if you're a monk
					if(instweapon || (botClass == MONK)) {
						if(instweapon)
							weapon = instweapon->GetItem();

						int weapontype = NULL;
						bool bIsFist = true;

						if(weapon) {
							weapontype = weapon->ItemType;
							bIsFist = false;
						}

						if(bIsFist || ((weapontype != ItemType2HS) && (weapontype != ItemType2HPierce) && (weapontype != ItemType2HB))) {
							float DualWieldProbability = (GetSkill(DUAL_WIELD) + botLevel) / 400.0f;

							if(GetAA(aaAmbidexterity))
								DualWieldProbability += 0.1f;

							//discipline effects:
							DualWieldProbability += (spellbonuses.DualWieldChance + itembonuses.DualWieldChance) / 100.0f;

							float random = MakeRandomFloat(0, 1);

							if (random < DualWieldProbability) { // Max 78% of DW
								Attack(GetTarget(), SLOT_SECONDARY);

								if(GetTarget() && CanThisClassDoubleAttack() && CheckBotDoubleAttack()) {
									Attack(GetTarget(), SLOT_SECONDARY);
								}
							}
						}
					}
				}
			}
		} // end in combat range
		else {
			if(GetTarget()->IsFeared() && !spellend_timer.Enabled()){
				// This is a mob that is fleeing either because it has been feared or is low on hitpoints
				AI_PursueCastCheck();
			}

			if (AImovement_timer->Check()) {
				if(!IsRooted()) {
					mlog(AI__WAYPOINTS, "Pursuing %s while engaged.", GetTarget()->GetCleanName());
					CalculateNewPosition2(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ(), GetRunspeed());
					return;
				}

				if(IsMoving())
					SendPosUpdate();
				else
					SendPosition();
			}
		} // end not in combat range

		if(!IsMoving() && !spellend_timer.Enabled()) {
			AI_EngagedCastCheck();
			BotMeditate(false);
		}
	} // end IsEngaged()
	else {
		// Not engaged in combat
		SetTarget(0);

		if(!IsMoving() && AIthink_timer->Check() && !spellend_timer.Enabled()) {
			if(!AI_IdleCastCheck() && !IsCasting())
				BotMeditate(true);
		}

		if(AImovement_timer->Check()) {
			if(GetFollowID()) {
				Mob* follow = entity_list.GetMob(GetFollowID());

				if(follow) {
					float dist = DistNoRoot(*follow);
					float speed = follow->GetRunspeed();

					if(dist < GetFollowDistance() + 1000) 
						speed = follow->GetWalkspeed();

					SetRunAnimSpeed(0);

					if(dist > GetFollowDistance()) {
						CalculateNewPosition2(follow->GetX(), follow->GetY(), follow->GetZ(), speed);
						if(rest_timer.Enabled())
							rest_timer.Disable();
						return;
					} 
					else
					{						
						if(moved)
						{
							moved=false;
							SendPosition();
							SetMoving(false);
						}
					}
				}
			}
		}
	}
}

// AI Processing for a Bot object's pet
void Bot::PetAIProcess() {
	_ZP(Bot_PET_Process);

	if( !HasPet() || !GetPet() || !GetPet()->IsNPC())
		return;

	Mob* BotOwner = this->GetBotOwner();
	NPC* botPet = this->GetPet()->CastToNPC();

	if(!botPet->GetOwner() || !botPet->GetID() || !botPet->GetOwnerID()) {
		Kill();
		return;
	} 

	if (!botPet->IsAIControlled())
		return;

	if(botPet->GetAttackTimer().Check(false))
		return;

	if (botPet->IsCasting())
		return;

	// Return if the owner of the bot pet isnt a bot.
	if (!botPet->GetOwner()->IsBot())
		return;

	if (IsEngaged()) {
		_ZP(Bot_PET_Process_IsEngaged);
		
		if (botPet->IsRooted())
			botPet->SetTarget(hate_list.GetClosest(botPet));
		else
			botPet->SetTarget(hate_list.GetTop(botPet));

		// Let's check if we have a los with our target.
		// If we don't, our hate_list is wiped.
		// It causes some cpu stress but without it, it was causing the bot/pet to aggro behind wall, floor etc... 
		if(!botPet->CheckLosFN(botPet->GetTarget()) || botPet->GetTarget()->IsMezzed() || !botPet->IsAttackAllowed(GetTarget())) {
			botPet->WipeHateList();
			botPet->SetTarget(botPet->GetOwner());

			return;
		}

		botPet->FaceTarget(botPet->GetTarget());

		// Lets see if we can let the main tank build a little aggro
		/*if(GetBotRaidID()) {
			BotRaids *br = entity_list.GetBotRaidByMob(GetOwner());
			if(br) {
				if(br->GetBotMainTank() && (br->GetBotMainTank() != this)) {
					if(br->GetBotMainTarget() && (br->GetBotMainTarget()->GetHateAmount(br->GetBotMainTank()) < 5000)) {
						if(GetTarget() == br->GetBotMainTarget()) {
							return;
						}
					}
				}
			}
		}*/

		bool is_combat_range = botPet->CombatRange(botPet->GetTarget());

		// Ok, we're engaged, each class type has a special AI
		// Only melee class will go to melee. Casters and healers will stay behind, following the leader by default.
		// I should probably make the casters staying in place so they can cast..

		// Ok, we 're a melee or any other class lvl<12. Yes, because after it becomes hard to go in melee for casters.. even for bots..
		if( is_combat_range ) {
			botPet->GetAIMovementTimer()->Check();
			
			if(botPet->IsMoving()) {
				botPet->SetRunAnimSpeed(0);
				botPet->SetHeading(botPet->GetTarget()->GetHeading());
				if(moved) {
					moved=false;
					botPet->SendPosition();
					botPet->SetMoving(false);
				}
			}

			if(!botPet->IsMoving()) {
				float newX = 0;
				float newY = 0;
				float newZ = 0;

				if(botPet->GetClass() == ROGUE && !botPet->BehindMob(botPet->GetTarget(), botPet->GetX(), botPet->GetY())) {
					// Move the rogue to behind the mob
					if(botPet->PlotPositionAroundTarget(botPet->GetTarget(), newX, newY, newZ)) {
						botPet->CalculateNewPosition2(newX, newY, newZ, botPet->GetRunspeed());
						return;
					}
				}
				else if(GetTarget() == botPet->GetTarget() && !botPet->BehindMob(botPet->GetTarget(), botPet->GetX(), botPet->GetY())) {
					// If the bot owner and the bot are fighting the same mob, then move the pet to the rear arc of the mob
					if(botPet->PlotPositionAroundTarget(botPet->GetTarget(), newX, newY, newZ)) {
						botPet->CalculateNewPosition2(newX, newY, newZ, botPet->GetRunspeed());
						return;
					}
				}
				else if(botPet->DistNoRootNoZ(*botPet->GetTarget()) < botPet->GetTarget()->GetSize()) {
					// Let's try to adjust our melee range so we don't appear to be bunched up
					bool isBehindMob = false;

					if(botPet->BehindMob(botPet->GetTarget(), botPet->GetX(), botPet->GetY()))
						isBehindMob = true;

					if(botPet->PlotPositionAroundTarget(botPet->GetTarget(), newX, newY, newZ, isBehindMob)) {
						botPet->CalculateNewPosition2(newX, newY, newZ, botPet->GetRunspeed());
						return;
					}
				}
			}
			
			// we can't fight if we don't have a target, are stun/mezzed or dead..
			if(botPet->GetTarget() && !botPet->IsStunned() && !botPet->IsMezzed() && (botPet->GetAppearance() != eaDead)) {
				// check the delay on the attack
				if(botPet->GetAttackTimer().Check()) {		
					// Stop attacking while we are on a front arc and the target is enraged
					if(!botPet->BehindMob(botPet->GetTarget(), botPet->GetX(), botPet->GetY()) && botPet->GetTarget()->IsEnraged())
						return;

					if(botPet->Attack(GetTarget(), 13))			// try the main hand
						if (botPet->GetTarget())					// Do we still have a target?
						{
							// We're a pet so we re able to dual attack
							sint32 RandRoll = MakeRandomInt(0, 99);	
							if (botPet->CanThisClassDoubleAttack() && (RandRoll < (botPet->GetLevel() + NPCDualAttackModifier)))	
							{
								if(botPet->Attack(botPet->GetTarget(), 13)) 
								{}
							}
						}

						if (botPet->GetOwner()->IsBot()) {
							int aa_chance = 0;
							int aa_skill = 0;
							// Magician AA
							aa_skill += botPet->GetOwner()->GetAA(aaElementalAlacrity);
							// Necromancer AA
							aa_skill += botPet->GetOwner()->GetAA(aaQuickeningofDeath);
							// Beastlord AA
							aa_skill += botPet->GetOwner()->GetAA(aaWardersAlacrity);

							if(aa_skill >= 1) {
								aa_chance += (aa_skill > 5 ? 5 : aa_skill) * 4;
							}
							if(aa_skill >= 6) {
								aa_chance += (aa_skill-5 > 3 ? 3 : aa_skill-5) * 7;
							}
							if(aa_skill >= 9) {
								aa_chance += (aa_skill-8 > 3 ? 3 : aa_skill-8) * 3;
							}
							if(aa_skill >= 12) {
								aa_chance += (aa_skill - 11) * 1;
							}	

							//aa_chance += botPet->GetOwner()->GetAA(aaCompanionsAlacrity) * 3;

							if (MakeRandomInt(1, 100) < aa_chance)
								Flurry();
						}

						// Ok now, let's check pet's offhand. 
						if (botPet->GetAttackDWTimer().Check() && botPet->GetOwnerID() && botPet->GetOwner() && ((botPet->GetOwner()->GetClass() == MAGICIAN) || (botPet->GetOwner()->GetClass() == NECROMANCER) || (botPet->GetOwner()->GetClass() == SHADOWKNIGHT) || (botPet->GetOwner()->GetClass() == BEASTLORD))) 
						{
							if(botPet->GetOwner()->GetLevel() >= 24)
							{
								float DualWieldProbability = (botPet->GetSkill(DUAL_WIELD) + botPet->GetLevel()) / 400.0f;
								DualWieldProbability -= MakeRandomFloat(0, 1);
								if(DualWieldProbability < 0){
									botPet->Attack(botPet->GetTarget(), 14);
									if (botPet->CanThisClassDoubleAttack())
									{
										sint32 RandRoll = MakeRandomInt(0, 99);
										if (RandRoll < (botPet->GetLevel() + 20))
										{
											botPet->Attack(botPet->GetTarget(), 14);
										}
									}
								}
							}
						}
						if(!botPet->GetOwner())
							return;

						// Special attack
						botPet->DoClassAttacks(botPet->GetTarget()); 
				}
				// See if the pet can cast any spell
				botPet->AI_EngagedCastCheck();
			}	
		}// end of the combat in range
		else{
			// Now, if we cannot reach our target
			if (!botPet->HateSummon()) 
			{
				if(botPet->GetTarget() && botPet->AI_PursueCastCheck()) 
				{}
				else if (botPet->GetTarget() && botPet->GetAIMovementTimer()->Check()) 
				{
					botPet->SetRunAnimSpeed(0);
					if(!botPet->IsRooted()) {
						mlog(AI__WAYPOINTS, "Pursuing %s while engaged.", botPet->GetTarget()->GetCleanName());
						botPet->CalculateNewPosition2(botPet->GetTarget()->GetX(), botPet->GetTarget()->GetY(), botPet->GetTarget()->GetZ(), botPet->GetOwner()->GetRunspeed());
						return;
					}
					else {
						botPet->SetHeading(botPet->GetTarget()->GetHeading());
						if(moved) {
							moved=false;
							botPet->SendPosition();
							botPet->SetMoving(false);
						}
					}
				}
			}
		}
	}
	else{
		// Franck: EQoffline
		// Ok if we're not engaged, what's happening..
		if(botPet->GetTarget() != botPet->GetOwner()) {
			botPet->SetTarget(botPet->GetOwner());
		}

		if(!IsMoving()) {
			botPet->AI_IdleCastCheck();
		}

		if(botPet->GetAIMovementTimer()->Check()) {
			switch(pStandingPetOrder) {
				case SPO_Follow:
					{
						float dist = botPet->DistNoRoot(*botPet->GetTarget());
						botPet->SetRunAnimSpeed(0);
						if(dist > 184) {
							botPet->CalculateNewPosition2(botPet->GetTarget()->GetX(), botPet->GetTarget()->GetY(), botPet->GetTarget()->GetZ(), botPet->GetTarget()->GetRunspeed());
							return;
						}
						else {
							botPet->SetHeading(botPet->GetTarget()->GetHeading());
							if(moved) {
								moved=false;
								botPet->SendPosition();
								botPet->SetMoving(false);
							}
						}
					}
					break;
				case SPO_Sit:
					botPet->SetAppearance(eaSitting);
					break;
				case SPO_Guard:
					botPet->NextGuardPosition();
					break;
			}
		}
	}
}

void Bot::Depop() {
	WipeHateList();
	
	entity_list.RemoveFromHateLists(this);
	
	if(HasGroup())
		Bot::RemoveBotFromGroup(this, GetGroup());

	if(HasPet()) {
		GetPet()->Depop();
	}
	
	_botOwner = 0;
	_botOwnerCharacterID = 0;
	_previousTarget = 0;
	
	NPC::Depop(false);
}

bool Bot::DeleteBot(std::string* errorMessage) {
	bool Result = false;
	int TempCounter = 0;

	if(this->GetBotID() > 0) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];

		// TODO: These queries need to be ran together as a transaction.. ie, if one or more fail then they all will fail to commit to the database.

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM botinventory WHERE botid = '%u'", this->GetBotID()), TempErrorMessageBuffer)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else
			TempCounter++;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "DELETE FROM bots WHERE BotID = '%u'", this->GetBotID()), TempErrorMessageBuffer)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else
			TempCounter++;

		if(TempCounter == 2)
			Result = true;
	}

	return Result;
}

void Bot::Spawn(Client* botCharacterOwner, std::string* errorMessage) {
	if(GetBotID() > 0 && _botOwnerCharacterID > 0 && botCharacterOwner && botCharacterOwner->CharacterID() == _botOwnerCharacterID) {
		// Rename the bot name to make sure that Mob::GetName() matches Mob::GetCleanName() so we dont have a bot named "Jesuschrist001"
		strcpy(name, GetCleanName());

		// Get the zone id this bot spawned in
		_lastZoneId = GetZoneID();

		if(this->Save())
			this->GetBotOwner()->CastToClient()->Message(0, "%s saved.", this->GetCleanName());
		else
			this->GetBotOwner()->CastToClient()->Message(13, "%s save failed!", this->GetCleanName());

		// Spawn the bot at the bow owner's loc
		this->x_pos = botCharacterOwner->GetX();
		this->y_pos = botCharacterOwner->GetY();
		this->z_pos = botCharacterOwner->GetZ();
		
		// Make the bot look at the bot owner
		FaceTarget(botCharacterOwner);

		// Level the bot to the same level as the bot owner
		//this->SetLevel(botCharacterOwner->GetLevel());

		entity_list.AddBot(this, true, true);

		// Load pet
		LoadPet();

		this->SendPosition();

		uint32 itemID = 0;
		int8 materialFromSlot = 0xFF;
		for(int i=0; i<22; ++i) {
			itemID = GetBotItemBySlot(i);
			if(itemID != 0) {
				materialFromSlot = Inventory::CalcMaterialFromSlot(i);
				if(materialFromSlot != 0xFF) {
					this->SendWearChange(materialFromSlot);
				}
			}
		}
	}
}

// Saves the specified item as an inventory record in the database for this bot.
void Bot::SetBotItemInSlot(uint32 slotID, uint32 itemID, const ItemInst* inst, std::string *errorMessage) {
	char errbuf[MYSQL_ERRMSG_SIZE];
	char *query = 0;
	uint32 augslot[5] = { 0, 0, 0, 0, 0 };

	if(this->GetBotID() > 0 && slotID >= 0 && itemID > 0) {
		if (inst && inst->IsType(ItemClassCommon)) {
			for(int i=0; i<5; ++i) {
				ItemInst* auginst = inst->GetItem(i);
				augslot[i] = (auginst && auginst->GetItem()) ? auginst->GetItem()->ID : 0;
			}
		}
		if(!database.RunQuery(query, MakeAnyLenString(&query,
			"REPLACE INTO botinventory "
			"	(botid,slotid,itemid,charges,instnodrop,color,"
			"	augslot1,augslot2,augslot3,augslot4,augslot5)"
			" VALUES(%lu,%lu,%lu,%lu,%lu,%lu,"
			"	%lu,%lu,%lu,%lu,%lu)",
			(unsigned long)this->GetBotID(), (unsigned long)slotID, (unsigned long)itemID, (unsigned long)inst->GetCharges(), (unsigned long)(inst->IsInstNoDrop() ? 1:0),(unsigned long)inst->GetColor(),
			(unsigned long)augslot[0],(unsigned long)augslot[1],(unsigned long)augslot[2],(unsigned long)augslot[3],(unsigned long)augslot[4]), errbuf)) {
				*errorMessage = std::string(errbuf);
		}

		safe_delete_array(query);
	}
}

// Deletes the inventory record for the specified item from the database for this bot.
void Bot::RemoveBotItemBySlot(uint32 slotID, std::string *errorMessage) {
	char errbuf[MYSQL_ERRMSG_SIZE];
	char *query = 0;

	if(this->GetBotID() > 0 && slotID >= 0) {
		if(!database.RunQuery(query, MakeAnyLenString(&query, "DELETE FROM botinventory WHERE botid=%i AND slotid=%i", this->GetBotID(), slotID), errbuf)){
			*errorMessage = std::string(errbuf);
		}
		safe_delete_array(query);
		m_inv.DeleteItem(slotID);
	}
}

// Retrieves all the inventory records from the database for this bot.
void Bot::GetBotItems(std::string* errorMessage, Inventory &inv) {

	if(this->GetBotID() > 0) {
		char errbuf[MYSQL_ERRMSG_SIZE];
		char* query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(query, MakeAnyLenString(&query, "SELECT slotid,itemid,charges,color,augslot1,augslot2,augslot3,augslot4,augslot5,instnodrop FROM botinventory WHERE botid=%i order by slotid", this->GetBotID()), errbuf, &DatasetResult)) {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				sint16 slot_id	= atoi(DataRow[0]);
				uint32 item_id	= atoi(DataRow[1]);
				int16 charges	= atoi(DataRow[2]);
				uint32 color	= atoul(DataRow[3]);
				uint32 aug[5];
				aug[0] = (uint32)atoul(DataRow[4]);
				aug[1] = (uint32)atoul(DataRow[5]);
				aug[2] = (uint32)atoul(DataRow[6]);
				aug[3] = (uint32)atoul(DataRow[7]);
				aug[4] = (uint32)atoul(DataRow[8]);
				bool instnodrop	= (DataRow[9] && (int16)atoi(DataRow[9])) ? true : false;

				ItemInst* inst = database.CreateItem(item_id, charges, aug[0], aug[1], aug[2], aug[3], aug[4]);
				if(inst) {
					sint16 put_slot_id = SLOT_INVALID;
					if(instnodrop || ((slot_id >= 0) && (slot_id <= 21) && inst->GetItem()->Attuneable))
						inst->SetInstNoDrop(true);
					if(color > 0)
						inst->SetColor(color);
					if(charges==255)
						inst->SetCharges(-1);
					else
						inst->SetCharges(charges);
					if((slot_id >= 8000) && (slot_id <= 8999)) {
						// do nothing
					}
					else {
						put_slot_id = inv.PutItem(slot_id, *inst);
					}
					safe_delete(inst);

					// Save ptr to item in inventory
					if (put_slot_id == SLOT_INVALID) {
						LogFile->write(EQEMuLog::Error,
							"Warning: Invalid slot_id for item in inventory: botid=%i, item_id=%i, slot_id=%i",
							this->GetBotID(), item_id, slot_id);
					}
				}
				else {
					LogFile->write(EQEMuLog::Error,
						"Warning: botid %i has an invalid item_id %i in inventory slot %i",
						this->GetBotID(), item_id, slot_id);
				}
			}
			mysql_free_result(DatasetResult);
		}
		else 
			*errorMessage = std::string(errbuf);

		safe_delete_array(query);
	}
}

// Returns the inventory record for this bot from the database for the specified equipment slot.
uint32 Bot::GetBotItemBySlot(uint32 slotID) {
	uint32 Result = 0;

	if(this->GetBotID() > 0 && slotID >= 0) {
		char* query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(query, MakeAnyLenString(&query, "SELECT itemid FROM botinventory WHERE botid=%i AND slotid=%i", GetBotID(), slotID), 0, &DatasetResult)) {
			if(mysql_num_rows(DatasetResult) == 1) {
				DataRow = mysql_fetch_row(DatasetResult);
				if(DataRow)
					Result = atoi(DataRow[0]);
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(query);
	}

	return Result;
}

// Returns the number of inventory records the bot has in the database.
uint32 Bot::GetBotItemsCount(std::string *errorMessage) {
	uint32 Result = 0;

	if(this->GetBotID() > 0) {
		char errbuf[MYSQL_ERRMSG_SIZE];
		char* query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(query, MakeAnyLenString(&query, "SELECT COUNT(*) FROM botinventory WHERE botid=%i", this->GetBotID()), errbuf, &DatasetResult)) {
			if(mysql_num_rows(DatasetResult) == 1) {
				DataRow = mysql_fetch_row(DatasetResult);
				if(DataRow)
					Result = atoi(DataRow[0]);
			}

			mysql_free_result(DatasetResult);
		}
		else
			*errorMessage = std::string(errbuf);

		safe_delete_array(query);
	}

	return Result;
}

bool Bot::MesmerizeTarget(Mob* target) {
	bool Result = false;

	if(target) {
		int mezid = 0;
		int mezlevel = GetLevel();

		if(mezlevel >= 69) {
			mezid = 5520;
		}
		else if(mezlevel == 68) {
			mezid = 8035;
		}
		else if(mezlevel == 67) {
			mezid = 5503;
		}
		else if(mezlevel >= 64) {
			mezid = 3358;
		}
		else if(mezlevel == 63) {
			mezid = 3354;
		}
		else if(mezlevel >= 61) {
			mezid = 3341;
		}
		else if(mezlevel == 60) {
			mezid = 2120;
		}
		else if(mezlevel == 59) {
			mezid = 1692;
		}
		else if(mezlevel >= 54) {
			mezid = 1691;
		}
		else if(mezlevel >= 47) {
			mezid = 190;
		}
		else if(mezlevel >= 30) {
			mezid = 188;
		}
		else if(mezlevel >= 13) {
			mezid = 187;
		}
		else if(mezlevel >= 2) {
			mezid = 292;
		}
		if(mezid > 0) {
			int32 DontRootMeBeforeTime = 0;
			CastSpell(mezid, target->GetID(), 1, -1, -1, &DontRootMeBeforeTime);
			target->SetDontRootMeBefore(DontRootMeBeforeTime);
			Result = true;
		}
	}

	return Result;
}

void Bot::SetLevel(uint8 in_level, bool command) {
	if(in_level > 0) {
		Mob::SetLevel(in_level, command);
	}
}

void Bot::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) {
	if(ns) {
		Mob::FillSpawnStruct(ns, ForWho);

		ns->spawn.afk = 0;
		ns->spawn.lfg = 0;
		ns->spawn.anon = 0;
		ns->spawn.gm = 0;
		if(IsInAGuild())
			ns->spawn.guildID = GuildID();
		else
			ns->spawn.guildID = 0xFFFFFFFF;		// 0xFFFFFFFF = NO GUILD, 0 = Unknown Guild
		ns->spawn.is_npc = 0;				// 0=no, 1=yes
		ns->spawn.is_pet = 0;
		ns->spawn.guildrank = 0;
		ns->spawn.showhelm = 1;
		ns->spawn.flymode = 0;
		ns->spawn.size = 0;
		ns->spawn.NPC = 0;					// 0=player,1=npc,2=pc corpse,3=npc corpse

		const Item_Struct* item = 0;
		const ItemInst* inst = 0;

		uint32 spawnedbotid = 0;
		spawnedbotid = this->GetBotID();

		inst = GetBotItem(SLOT_HANDS);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_HANDS]	= item->Material;
				ns->spawn.colors[MATERIAL_HANDS].color = GetEquipmentColor(MATERIAL_HANDS);
			}
		}

		inst = GetBotItem(SLOT_HEAD);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_HEAD] = item->Material;
				ns->spawn.colors[MATERIAL_HEAD].color = GetEquipmentColor(MATERIAL_HEAD);
			}
		}

		inst = GetBotItem(SLOT_ARMS);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_ARMS] = item->Material;
				ns->spawn.colors[MATERIAL_ARMS].color = GetEquipmentColor(MATERIAL_ARMS);
			}
		}

		inst = GetBotItem(SLOT_BRACER01);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_BRACER] = item->Material;
				ns->spawn.colors[MATERIAL_BRACER].color	= GetEquipmentColor(MATERIAL_BRACER);
			}
		}

		inst = GetBotItem(SLOT_BRACER02);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_BRACER] = item->Material;
				ns->spawn.colors[MATERIAL_BRACER].color	= GetEquipmentColor(MATERIAL_BRACER);
			}
		}

		inst = GetBotItem(SLOT_CHEST);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_CHEST]	= item->Material;
				ns->spawn.colors[MATERIAL_CHEST].color = GetEquipmentColor(MATERIAL_CHEST);
			}
		}

		inst = GetBotItem(SLOT_LEGS);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_LEGS] = item->Material;
				ns->spawn.colors[MATERIAL_LEGS].color = GetEquipmentColor(MATERIAL_LEGS);
			}
		}

		inst = GetBotItem(SLOT_FEET);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				ns->spawn.equipment[MATERIAL_FEET] = item->Material;
				ns->spawn.colors[MATERIAL_FEET].color = GetEquipmentColor(MATERIAL_FEET);
			}
		}
		
		inst = GetBotItem(SLOT_PRIMARY);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				if(strlen(item->IDFile) > 2)
					ns->spawn.equipment[MATERIAL_PRIMARY] = atoi(&item->IDFile[2]);
					ns->spawn.colors[MATERIAL_PRIMARY].color = GetEquipmentColor(MATERIAL_PRIMARY);
			}
		}

		inst = GetBotItem(SLOT_SECONDARY);
		if(inst) {
			item = inst->GetItem();
			if(item) {
				if(strlen(item->IDFile) > 2)
					ns->spawn.equipment[MATERIAL_SECONDARY] = atoi(&item->IDFile[2]);
					ns->spawn.colors[MATERIAL_SECONDARY].color = GetEquipmentColor(MATERIAL_SECONDARY);
			}
		}
	}
}

uint32 Bot::GetBotIDByBotName(std::string botName) {
	uint32 Result = 0;

	if(!botName.empty()) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;
		std::string errorMessage;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT BotID FROM bots WHERE Name = '%s'", botName.c_str()), TempErrorMessageBuffer, &DatasetResult)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				Result = atoi(DataRow[0]);
				break;
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);

		if(!errorMessage.empty()) {
			// TODO: Log this error to zone error log
		}
	}

	return Result;
}

Bot* Bot::LoadBot(uint32 botID, std::string* errorMessage) {
	Bot* Result = 0;

	if(botID > 0) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT BotOwnerCharacterID, BotSpellsID, Name, LastName, BotLevel, Race, Class, Gender, Size, Face, LuclinHairStyle, LuclinHairColor, LuclinEyeColor, LuclinEyeColor2, LuclinBeardColor, LuclinBeard, DrakkinHeritage, DrakkinTattoo, DrakkinDetails, MR, CR, DR, FR, PR, Corrup, AC, STR, STA, DEX, AGI, _INT, WIS, CHA, ATK, BotCreateDate, LastSpawnDate, TotalPlayTime, LastZoneId FROM bots WHERE BotID = '%u'", botID), TempErrorMessageBuffer, &DatasetResult)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				NPCType DefaultNPCTypeStruct = CreateDefaultNPCTypeStructForBot(std::string(DataRow[2]), std::string(DataRow[3]), atoi(DataRow[4]), atoi(DataRow[5]), atoi(DataRow[6]), atoi(DataRow[7]));
				NPCType TempNPCStruct = FillNPCTypeStruct(atoi(DataRow[1]), std::string(DataRow[2]), std::string(DataRow[3]), atoi(DataRow[4]), atoi(DataRow[5]), atoi(DataRow[6]), atoi(DataRow[7]), atof(DataRow[8]), atoi(DataRow[9]), atoi(DataRow[10]), atoi(DataRow[11]), atoi(DataRow[12]), atoi(DataRow[13]), atoi(DataRow[14]), atoi(DataRow[15]), atoi(DataRow[16]), atoi(DataRow[17]), atoi(DataRow[18]), DefaultNPCTypeStruct.MR, DefaultNPCTypeStruct.CR, DefaultNPCTypeStruct.DR, DefaultNPCTypeStruct.FR, DefaultNPCTypeStruct.PR, DefaultNPCTypeStruct.Corrup, DefaultNPCTypeStruct.AC, DefaultNPCTypeStruct.STR, DefaultNPCTypeStruct.STA, DefaultNPCTypeStruct.DEX, DefaultNPCTypeStruct.AGI, DefaultNPCTypeStruct.INT, DefaultNPCTypeStruct.WIS, DefaultNPCTypeStruct.CHA, DefaultNPCTypeStruct.ATK);
				Result = new Bot(botID, atoi(DataRow[0]), atoi(DataRow[1]), atof(DataRow[36]), atoi(DataRow[37]), TempNPCStruct);
				break;
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return Result;
}

std::list<uint32> Bot::GetGroupedBotsByGroupId(uint32 groupId, std::string* errorMessage) {
	std::list<uint32> Result;

	if(groupId > 0) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select g.mobid as BotID from vwGroups as g join bots as b on g.mobid = b.BotId and g.mobtype = 'B' where g.groupid = %u", groupId), TempErrorMessageBuffer, &DatasetResult)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				if(DataRow) {
					Result.push_back(atoi(DataRow[0]));
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return Result;
} 

// Load and spawn all zoned bots by bot owner character
void Bot::LoadAndSpawnAllZonedBots(Client* botOwner) {
	if(botOwner) {
		if(botOwner->HasGroup()) {
			Group* g = botOwner->GetGroup();
			if(g) {
				uint32 TempGroupId = g->GetID();
				std::string errorMessage;
				std::list<uint32> ActiveBots = Bot::GetGroupedBotsByGroupId(botOwner->GetGroup()->GetID(), &errorMessage);

				if(errorMessage.empty() && !ActiveBots.empty()) {
					for(list<uint32>::iterator itr = ActiveBots.begin(); itr != ActiveBots.end(); itr++) {
						Bot* activeBot = Bot::LoadBot(*itr, &errorMessage);

						if(!errorMessage.empty()) {
							safe_delete(activeBot);
							break;
						}

						if(activeBot) {
							activeBot->Spawn(botOwner, &errorMessage);

							g->UpdatePlayer(activeBot);
							
							if(g->GetLeader())
								activeBot->SetFollowID(g->GetLeader()->GetID());
						}

						if(activeBot && !botOwner->HasGroup())
							database.SetGroupID(activeBot->GetCleanName(), 0, activeBot->GetBotID());
					}
				}

				// Catch all condition for error messages destined for the zone error log
				if(!errorMessage.empty()) {
					// TODO: Log this error message to zone error log
				}
			}
		}
	}
}

// Returns TRUE if there is atleast 1 bot in the specified group
bool Bot::GroupHasBot(Group* group) {
	bool Result = false;

	if(group) {
		for(int Counter = 0; Counter < MAX_GROUP_MEMBERS; Counter++) {
			if (group->members[Counter] == NULL) {
				continue;
			}

			if(group->members[Counter]->IsBot()) {
				Result = true;
				break;
			}
		}
	}

	return Result;
}

std::list<BotsAvailableList> Bot::GetBotList(uint32 botOwnerCharacterID, std::string* errorMessage) {
	std::list<BotsAvailableList> Result;

	if(botOwnerCharacterID > 0) {
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT BotID, Name, Class, BotLevel, Race FROM bots WHERE BotOwnerCharacterID = '%u'", botOwnerCharacterID), TempErrorMessageBuffer, &DatasetResult)) {
			*errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				if(DataRow) {
					BotsAvailableList TempAvailableBot;
					TempAvailableBot.BotID = atoi(DataRow[0]);
					strcpy(TempAvailableBot.BotName, DataRow[1]);
					TempAvailableBot.BotClass = atoi(DataRow[2]);
					TempAvailableBot.BotLevel = atoi(DataRow[3]);
					TempAvailableBot.BotRace = atoi(DataRow[4]);

					Result.push_back(TempAvailableBot);
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return Result;
}

std::list<SpawnedBotsList> Bot::ListSpawnedBots(uint32 characterID, std::string* errorMessage) {
	std::list<SpawnedBotsList> Result;
	char ErrBuf[MYSQL_ERRMSG_SIZE];
	char* Query = 0;
	MYSQL_RES* DatasetResult;
	MYSQL_ROW DataRow;

	if(characterID > 0) {
		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT bot_name, zone_name FROM botleader WHERE leaderid=%i", characterID), ErrBuf, &DatasetResult)) {
			*errorMessage = std::string(ErrBuf);
		}
		else {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);
					SpawnedBotsList TempSpawnedBotsList;
					TempSpawnedBotsList.BotLeaderCharID = characterID;
					strcpy(TempSpawnedBotsList.BotName, DataRow[0]);
					strcpy(TempSpawnedBotsList.ZoneName, DataRow[1]);

					Result.push_back(TempSpawnedBotsList);
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return Result;
}

void Bot::SaveBotGroup(Group* botGroup, std::string botGroupName, std::string* errorMessage) {
	if(botGroup && !botGroupName.empty()) {
		char errbuf[MYSQL_ERRMSG_SIZE];
		char *query = 0;
		
		Mob* tempGroupLeader = botGroup->GetLeader();

		if(tempGroupLeader->IsBot()) {
			uint32 botGroupId = 0;

			uint32 botGroupLeaderBotId = tempGroupLeader->CastToBot()->GetBotID();
		
			if(!database.RunQuery(query, MakeAnyLenString(&query, "INSERT into botgroup (BotGroupLeaderBotId, BotGroupName) values (%u, '%s')", botGroupLeaderBotId, botGroupName.c_str()), errbuf, 0, 0, &botGroupId)) {
				*errorMessage = std::string(errbuf);
			}
			else {
				if(botGroupId > 0) {
					for(int counter = 0; counter < botGroup->GroupCount(); counter++) {
						Mob* tempBot = botGroup->members[counter];

						if(tempBot && tempBot->IsBot()) {
							uint32 botGroupMemberBotId = tempBot->CastToBot()->GetBotID();

							safe_delete_array(query);

							if(!database.RunQuery(query, MakeAnyLenString(&query, "INSERT into botgroupmembers (BotGroupId, BotId) values (%u, %u)", botGroupId, botGroupMemberBotId), errbuf)) {
								*errorMessage = std::string(errbuf);
							}
						}
					}
				}
			}

			safe_delete_array(query);
		}
	}
}

void Bot::DeleteBotGroup(std::string botGroupName, std::string* errorMessage) {
	char errbuf[MYSQL_ERRMSG_SIZE];
	char *query = 0;

	if(!botGroupName.empty()) {
		uint32 botGroupId = GetBotGroupIdByBotGroupName(botGroupName, errorMessage);

		if(errorMessage->empty() && botGroupId > 0) {
			if(!database.RunQuery(query, MakeAnyLenString(&query, "DELETE FROM botgroupmembers WHERE BotGroupId = %u", botGroupId), errbuf)) {
				*errorMessage = std::string(errbuf);
			}
			else {
				safe_delete_array(query);

				if(!database.RunQuery(query, MakeAnyLenString(&query, "DELETE FROM botgroup WHERE BotGroupId = %u", botGroupId), errbuf)) {
					*errorMessage = std::string(errbuf);
				}
			}

			safe_delete_array(query);
		}
	}
}

std::list<BotGroup> Bot::LoadBotGroup(std::string botGroupName, std::string* errorMessage) {
	std::list<BotGroup> Result;
	char ErrBuf[MYSQL_ERRMSG_SIZE];
	char* Query = 0;
	MYSQL_RES* DatasetResult;
	MYSQL_ROW DataRow;

	if(!botGroupName.empty()) {
		uint32 botGroupId = GetBotGroupIdByBotGroupName(botGroupName, errorMessage);

		if(botGroupId > 0) {
			if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select BotId from botgroupmembers where BotGroupId = %u", botGroupId), ErrBuf, &DatasetResult)) {
				*errorMessage = std::string(ErrBuf);
			}
			else {
				uint32 RowCount = mysql_num_rows(DatasetResult);

				if(RowCount > 0) {
					for(int iCounter = 0; iCounter < RowCount; iCounter++) {
						DataRow = mysql_fetch_row(DatasetResult);

						if(DataRow) {
							BotGroup tempBotGroup;
							tempBotGroup.BotGroupID = botGroupId;
							tempBotGroup.BotID = atoi(DataRow[0]);

							Result.push_back(tempBotGroup);
						}
					}
				}

				mysql_free_result(DatasetResult);
			}

			safe_delete_array(Query);
		}
	}

	return Result;
}

std::list<BotGroupList> Bot::GetBotGroupListByBotOwnerCharacterId(uint32 botOwnerCharacterId, std::string* errorMessage) {
	std::list<BotGroupList> result;

	if(botOwnerCharacterId > 0) {
		char ErrBuf[MYSQL_ERRMSG_SIZE];
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select BotGroupName, BotGroupLeaderName from vwBotGroups where BotOwnerCharacterId = %u", botOwnerCharacterId), ErrBuf, &DatasetResult)) {
			*errorMessage = std::string(ErrBuf);
		}
		else {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);

					if(DataRow) {
						BotGroupList botGroupList;
						botGroupList.BotGroupName = std::string(DataRow[0]);
						botGroupList.BotGroupLeaderName = std::string(DataRow[1]);

						result.push_back(botGroupList);
					}
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return result;
}

bool Bot::DoesBotGroupNameExist(std::string botGroupName) {
	bool result = false;
	
	if(!botGroupName.empty()) {
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(Query, MakeAnyLenString(&Query, "select BotGroupId from vwBotGroups where BotGroupName = '%s'", botGroupName.c_str()), 0, &DatasetResult)) {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);

					if(DataRow) {
						uint32 tempBotGroupId = atoi(DataRow[0]);
						std::string tempBotGroupName = std::string(DataRow[1]);

						if(botGroupName == tempBotGroupName) {
							result = tempBotGroupId;
							break;
						}
					}
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return result;
}

uint32 Bot::CanLoadBotGroup(uint32 botOwnerCharacterId, std::string botGroupName, std::string* errorMessage) {
	uint32 result = 0;

	if(botOwnerCharacterId > 0 && !botGroupName.empty()) {
		char ErrBuf[MYSQL_ERRMSG_SIZE];
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select BotGroupId, BotGroupName from vwBotGroups where BotOwnerCharacterId = %u", botOwnerCharacterId), ErrBuf, &DatasetResult)) {
			*errorMessage = std::string(ErrBuf);
		}
		else {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);

					if(DataRow) {
						uint32 tempBotGroupId = atoi(DataRow[0]);
						std::string tempBotGroupName = std::string(DataRow[1]);

						if(botGroupName == tempBotGroupName) {
							result = tempBotGroupId;
							break;
						}
					}
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return result;
}

uint32 Bot::GetBotGroupIdByBotGroupName(std::string botGroupName, std::string* errorMessage) {
	uint32 result = 0;
	
	if(!botGroupName.empty()) {
		char ErrBuf[MYSQL_ERRMSG_SIZE];
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "select BotGroupId from vwBotGroups where BotGroupName = '%s'", botGroupName.c_str()), ErrBuf, &DatasetResult)) {
			*errorMessage = std::string(ErrBuf);
		}
		else {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);

					if(DataRow) {
						result = atoi(DataRow[0]);
						break;
					}
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return result;
}

uint32 Bot::GetBotGroupLeaderIdByBotGroupName(std::string botGroupName) {
	uint32 result = 0;
	
	if(!botGroupName.empty()) {
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(Query, MakeAnyLenString(&Query, "select BotGroupLeaderBotId from vwBotGroups where BotGroupName = '%s'", botGroupName.c_str()), 0, &DatasetResult)) {
			uint32 RowCount = mysql_num_rows(DatasetResult);

			if(RowCount > 0) {
				for(int iCounter = 0; iCounter < RowCount; iCounter++) {
					DataRow = mysql_fetch_row(DatasetResult);

					if(DataRow) {
						result = atoi(DataRow[0]);
						break;
					}
				}
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete_array(Query);
	}

	return result;
}

uint32 Bot::AllowedBotSpawns(uint32 botOwnerCharacterID, std::string* errorMessage) {
	uint32 Result = 0;

	if(botOwnerCharacterID > 0) {
		char ErrBuf[MYSQL_ERRMSG_SIZE];
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT value FROM quest_globals WHERE name='bot_spawn_limit' and charid=%i", botOwnerCharacterID), ErrBuf, &DatasetResult)) {
			if(mysql_num_rows(DatasetResult) == 1) {
				DataRow = mysql_fetch_row(DatasetResult);
				if(DataRow)
					Result = atoi(DataRow[0]);
			}

			mysql_free_result(DatasetResult);
		}
		else
			*errorMessage = std::string(ErrBuf);

		safe_delete_array(Query);
	}

	return Result;
}
	
uint32 Bot::SpawnedBotCount(uint32 botOwnerCharacterID, std::string* errorMessage) {
	uint32 Result = 0;

	if(botOwnerCharacterID > 0) {
		std::list<Bot*> SpawnedBots = entity_list.GetBotsByBotOwnerCharacterID(botOwnerCharacterID);

		Result = SpawnedBots.size();
	}

	return Result;
}

uint32 Bot::GetBotOwnerCharacterID(uint32 botID, std::string* errorMessage) {
	uint32 Result = 0;

	if(botID > 0) {
		char ErrBuf[MYSQL_ERRMSG_SIZE];
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT BotOwnerCharacterID FROM bots WHERE BotID = %u", botID), ErrBuf, &DatasetResult)) {
			if(mysql_num_rows(DatasetResult) == 1) {
				if(DataRow = mysql_fetch_row(DatasetResult))
					Result = atoi(DataRow[0]);
			}

			mysql_free_result(DatasetResult);
		}
		else
			*errorMessage = std::string(ErrBuf);

		safe_delete_array(Query);
	}

	return Result;
}

std::string Bot::ClassIdToString(uint16 classId) {
	std::string Result;

	if(classId > 0 && classId < 17) {
		switch(classId) {
			case 1:
				Result = std::string("Warrior");
				break;
			case 2:
				Result = std::string("Cleric");
				break;
			case 3:
				Result = std::string("Paladin");
				break;
			case 4:
				Result = std::string("Ranger");
				break;
			case 5:
				Result = std::string("Shadowknight");
				break;
			case 6:
				Result = std::string("Druid");
				break;
			case 7:
				Result = std::string("Monk");
				break;
			case 8:
				Result = std::string("Bard");
				break;
			case 9:
				Result = std::string("Rogue");
				break;
			case 10:
				Result = std::string("Shaman");
				break;
			case 11:
				Result = std::string("Necromancer");
				break;
			case 12:
				Result = std::string("Wizard");
				break;
			case 13:
				Result = std::string("Magician");
				break;
			case 14:
				Result = std::string("Enchanter");
				break;
			case 15:
				Result = std::string("Beastlord");
				break;
			case 16:
				Result = std::string("Berserker");
				break;
		}
	}

	return Result;
}

std::string Bot::RaceIdToString(uint16 raceId) {
	std::string Result;

	if(raceId > 0) {
		switch(raceId) {
			case 1:
				Result = std::string("Human");
				break;
			case 2:
				Result = std::string("Barbarian");
				break;
			case 3:
				Result = std::string("Erudite");
				break;
			case 4:
				Result = std::string("Wood Elf");
				break;
			case 5:
				Result = std::string("High Elf");
				break;
			case 6:
				Result = std::string("Dark Elf");
				break;
			case 7:
				Result = std::string("Half Elf");
				break;
			case 8:
				Result = std::string("Dwarf");
				break;
			case 9:
				Result = std::string("Troll");
				break;
			case 10:
				Result = std::string("Ogre");
				break;
			case 11:
				Result = std::string("Halfling");
				break;
			case 12:
				Result = std::string("Gnome");
				break;
			case 128:
				Result = std::string("Iksar");
				break;
			case 130:
				Result == std::string("Vah Shir");
				break;
			case 330:
				Result = std::string("Froglok");
				break;
			case 522:
				Result = std::string("Drakkin");
				break;
		}
	}

	return Result;
}

void Bot::SendBotArcheryWearChange(int8 material_slot, uint32 material, uint32 color) {
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = GetID();
	wc->material = material;
	wc->color.color = color;
	wc->wear_slot_id = material_slot;

	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
}

// Returns the item id that is in the bot inventory collection for the specified slot.
ItemInst* Bot::GetBotItem(uint32 slotID) {
	ItemInst* item = m_inv.GetItem(slotID);
	if(item){
		return item;
	}

	return NULL;
}

// Adds the specified item it bot to the NPC equipment array and to the bot inventory collection.
void Bot::BotAddEquipItem(int slot, uint32 id) {
	if(slot > 0 && id > 0) {
		int8 materialFromSlot = Inventory::CalcMaterialFromSlot(slot);

		if(materialFromSlot != 0xFF) {
			equipment[slot] = id; // npc has more than just material slots. Valid material should mean valid inventory index
			SendWearChange(materialFromSlot);
		}
	}
}

// Erases the specified item from bot the NPC equipment array and from the bot inventory collection.
void Bot::BotRemoveEquipItem(int slot) {
	if(slot > 0) {
		int8 materialFromSlot = Inventory::CalcMaterialFromSlot(slot);

		if(materialFromSlot != 0xFF) {
			equipment[slot] = 0; // npc has more than just material slots. Valid material should mean valid inventory index
			SendWearChange(materialFromSlot);
			if(materialFromSlot == MATERIAL_CHEST)
				SendWearChange(MATERIAL_ARMS);
		}
	}
}

void Bot::BotTradeSwapItem(Client* client, sint16 lootSlot, const ItemInst* inst, const ItemInst* inst_swap, uint32 equipableSlots, std::string* errorMessage, bool swap) {
	
	if(!errorMessage->empty())
		return;

	client->PushItemOnCursor(*inst_swap, true);
	
	// Remove the item from the bot and from the bot's database records
	RemoveBotItemBySlot(lootSlot, errorMessage);

	if(!errorMessage->empty())
		return;

	this->BotRemoveEquipItem(lootSlot);

	if(swap) {
		BotTradeAddItem(inst->GetItem()->ID, inst, inst->GetCharges(), equipableSlots, lootSlot, errorMessage);

		if(!errorMessage->empty())
			return;
	}
}

void Bot::BotTradeAddItem(uint32 id, const ItemInst* inst, sint16 charges, uint32 equipableSlots, int16 lootSlot, std::string* errorMessage, bool addToDb) {
	if(addToDb) {
		this->SetBotItemInSlot(lootSlot, id, inst, errorMessage);
		if(!errorMessage->empty())
			return;
		m_inv.PutItem(lootSlot, *inst);
	}

	this->BotAddEquipItem(lootSlot, id);
}

bool Bot::Bot_Command_Resist(int resisttype, int level) {
	int resistid = 0;
	switch(resisttype) {
		case 1: // Poison Cleric
			if(level >= 30) {
				resistid = 62;
			}
			else if(level >= 6) {
				resistid = 227;
			}
			break;
		case 2: // Disease Cleric
			if(level >= 36) {
				resistid = 63;
			}
			else if(level >= 11) {
				resistid = 226;
			}
			break;
		case 3: // Fire Cleric
			if(level >= 33) {
				resistid = 60;
			}
			else if(level >= 8) {
				resistid = 224;
			}
			break;
		case 4: // Cold Cleric
			if(level >= 38) {
				resistid = 61;
			}
			else if(level >= 13) {
				resistid = 225;
			}
			break;
		case 5: // Magic Cleric
			if(level >= 43) {
				resistid = 64;
			}
			else if(level >= 16) {
				resistid = 228;
			}
			break;
		case 6: // Magic Enchanter
			if(level >= 37) {
				resistid = 64;
			}
			else if(level >= 17) {
				resistid = 228;
			}
			break;
		case 7: // Poison Druid
			if(level >= 44) {
				resistid = 62;
			}
			else if(level >= 19) {
				resistid = 227;
			}
			break;
		case 8: // Disease Druid
			if(level >= 44) {
				resistid = 63;
			}
			else if(level >= 19) {
				resistid = 226;
			}
			break;
		case 9: // Fire Druid
			if(level >= 20) {
				resistid = 60;
			}
			else if(level >= 1) {
				resistid = 224;
			}
			break;
		case 10: // Cold Druid
			if(level >= 30) {
				resistid = 61;
			}
			else if(level >= 9) {
				resistid = 225;
			}
			break;
		case 11: // Magic Druid
			if(level >= 49) {
				resistid = 64;
			}
			else if(level >= 34) {
				resistid = 228;
			}
			break;
		case 12: // Poison Shaman
			if(level >= 35) {
				resistid = 62;
			}
			else if(level >= 20) {
				resistid = 227;
			}
			break;
		case 13: // Disease Shaman
			if(level >= 30) {
				resistid = 63;
			}
			else if(level >= 8) {
				resistid = 226;
			}
			break;
		case 14: // Fire Shaman
			if(level >= 27) {
				resistid = 60;
			}
			else if(level >= 5) {
				resistid = 224;
			}
			break;
		case 15: // Cold Shaman
			if(level >= 24) {
				resistid = 61;
			}
			else if(level >= 1) {
				resistid = 225;
			}
			break;
		case 16: // Magic Shaman
			if(level >= 43) {
				resistid = 64;
			}
			else if(level >= 19) {
				resistid = 228;
			}
			break;
	}

	if(resistid > 0) {
		Group* g = GetGroup();
		if(g) {
			for(int k=0; k<MAX_GROUP_MEMBERS; k++) {
				if(g->members[k]) {
					SpellOnTarget(resistid, g->members[k]);
				}
			}
			return true;
		}
	}

	return false;
}

bool Bot::RemoveBotFromGroup(Bot* bot, Group* group) {
	bool Result = false;

	if(bot && group) {
		if(bot->HasGroup()) {
			if(!group->IsLeader(bot)) {
				bot->SetFollowID(0);

				if(group->DelMember(bot))
					database.SetGroupID(bot->GetCleanName(), 0, bot->GetBotID());

				if(group->GroupCount() <= 1)
					group->DisbandGroup();
			}
			else {
				for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if(!group->members[i])
						continue;

					group->members[i]->SetFollowID(0);
				}

				group->DisbandGroup();
				database.SetGroupID(bot->GetCleanName(), 0, bot->GetBotID());
			}

			Result = true;
		}
	}

	return Result;
}

bool Bot::AddBotToGroup(Bot* bot, Group* group) {
	bool Result = false;

	if(bot && group) {
		if(!bot->HasGroup()) {
			// Add bot to this group
			if(group->AddMember(bot)) {
				if(group->GetLeader()) {
					bot->SetFollowID(group->GetLeader()->GetID());

					// Need to send this only once when a group is formed with a bot so the client knows it is also the group leader
					if(group->GroupCount() == 2 && group->GetLeader()->IsClient()) {
						group->UpdateGroupAAs();
						Mob *TempLeader = group->GetLeader();
						group->SendUpdate(groupActUpdate, TempLeader);
					}
				}

				Result = true;
			}
		}
	}

	return Result;
}

bool Bot::BotGroupCreate(std::string botGroupLeaderName) {
	bool Result = false;

	if(!botGroupLeaderName.empty()) {
		Bot* botGroupLeader = entity_list.GetBotByBotName(botGroupLeaderName);

		if(botGroupLeader)
			Result = BotGroupCreate(botGroupLeader);
	}

	return Result;
}

bool Bot::BotGroupCreate(Bot* botGroupLeader) {
	bool Result = false;

	if(botGroupLeader && !botGroupLeader->HasGroup()) {
		Group* newGroup = new Group(botGroupLeader);
		
		if(newGroup) {
			entity_list.AddGroup(newGroup);
			database.SetGroupID(botGroupLeader->GetName(), newGroup->GetID(), botGroupLeader->GetBotID());
			database.SetGroupLeaderName(newGroup->GetID(), botGroupLeader->GetName());

			botGroupLeader->SetFollowID(botGroupLeader->GetBotOwner()->GetID());

			Result = true;
		}
	}

	return Result;
}

bool Bot::Bot_Command_CharmTarget(int charmtype, Mob *target) {
	int charmid = 0;
	int charmlevel = GetLevel();
	if(target) {
		switch(charmtype) {
			case 1: // Enchanter
				if((charmlevel >= 64) && (charmlevel <= 75)) {
					charmid = 3355;
				}
				else if((charmlevel >= 62) && (charmlevel <= 63)) {
					charmid = 3347;
				}
				else if((charmlevel >= 60) && (charmlevel <= 61)) {
					charmid = 1707;
				}
				else if((charmlevel >= 53) && (charmlevel <= 59)) {
					charmid = 1705;
				}
				else if((charmlevel >= 37) && (charmlevel <= 52)) {
					charmid = 183;
				}
				else if((charmlevel >= 23) && (charmlevel <= 36)) {
					charmid = 182;
				}
				else if((charmlevel >= 11) && (charmlevel <= 22)) {
					charmid = 300;
				}
				break;
			case 2: // Necromancer
				if((charmlevel >= 60) && (charmlevel <= 75)) {
					charmid = 1629;
				}
				else if((charmlevel >=47) && (charmlevel <= 59)) {
					charmid = 198;
				}
				else if((charmlevel >= 31) && (charmlevel <= 46)) {
					charmid = 197;
				}
				else if((charmlevel >= 18) && (charmlevel <= 30)) {
					charmid = 196;
				}
				break;
			case 3: // Druid
				if((charmlevel >= 63) && (charmlevel <= 75)) {
					charmid = 3445;
				}
				else if((charmlevel >= 55) && (charmlevel <= 62)) {
					charmid = 1556;
				}
				else if((charmlevel >= 52) && (charmlevel <= 54)) {
					charmid = 1553;
				}
				else if((charmlevel >= 43) && (charmlevel <= 51)) {
					charmid = 142;
				}
				else if((charmlevel >= 33) && (charmlevel <= 42)) {
					charmid = 141;
				}
				else if((charmlevel >= 23) && (charmlevel <= 32)) {
					charmid = 260;
				}
				else if((charmlevel >= 13) && (charmlevel <= 22)) {
					charmid = 242;
				}
				break;
		}
		if(charmid > 0) {
			int32 DontRootMeBeforeTime = 0;
			CastSpell(charmid, target->GetID(), 1, -1, -1, &DontRootMeBeforeTime);
			target->SetDontRootMeBefore(DontRootMeBeforeTime);
			return true;
		}
	}
	return false;
}

bool Bot::Bot_Command_DireTarget(int diretype, Mob *target) {
	int direid = 0;
	int direlevel = GetLevel();
	if(target) {
		switch(diretype) {
			case 1: // Enchanter
				if(direlevel >= 65) {
					direid = 5874;
				}
				else if(direlevel >= 55) {
					direid = 2761;
				}
				break;
			case 2: // Necromancer
				if(direlevel >= 65) {
					direid = 5876;
				}
				else if(direlevel >= 55) {
					direid = 2759;
				}
				break;
			case 3: // Druid
				if(direlevel >= 65) {
					direid = 5875;
				}
				else if(direlevel >= 55) {
					direid = 2760;
				}
				break;
		}
		if(direid > 0) {
			int32 DontRootMeBeforeTime = 0;
			CastSpell(direid, target->GetID(), 1, -1, -1, &DontRootMeBeforeTime);
			target->SetDontRootMeBefore(DontRootMeBeforeTime);
			return true;
		}
	}
	return false;
}

bool Bot::Bot_Command_CalmTarget(Mob *target) {
	if(target) {
		int calmid = 0;
		int calmlevel = GetLevel();
		if((calmlevel >= 67) && (calmlevel <= 75)) {
			calmid = 5274;
		}
		else if((calmlevel >= 62) && (calmlevel <= 66)) {
			calmid = 3197;
		}
		else if((calmlevel >= 35) && (calmlevel <= 61)) {
			calmid = 45;
		}
		else if((calmlevel >= 18) && (calmlevel <= 34)) {
			calmid = 47;
		}
		else if((calmlevel >= 6) && (calmlevel <= 17)) {
			calmid = 501;
		}
		else if((calmlevel >= 1) && (calmlevel <= 5)) {
			calmid = 208;
		}
		if(calmid > 0) {
			int32 DontRootMeBeforeTime = 0;
			CastSpell(calmid, target->GetID(), 1, -1, -1, &DontRootMeBeforeTime);
			target->SetDontRootMeBefore(DontRootMeBeforeTime);
			return true;
		}
	}
	return false;
}

bool Bot::Bot_Command_RezzTarget(Mob *target) {
	if(target) {
		int rezid = 0;
		int rezlevel = GetLevel();
		if(rezlevel >= 56) {
			rezid = 1524;
		}
		else if(rezlevel >= 47) {
			rezid = 392;
		}
		else if(rezlevel >= 42) {
			rezid = 2172;
		}
		else if(rezlevel >= 37) {
			rezid = 388;
		}
		else if(rezlevel >= 32) {
			rezid = 2171;
		}
		else if(rezlevel >= 27) {
			rezid = 391;
		}
		else if(rezlevel >= 22) {
			rezid = 2170;
		}
		else if(rezlevel >= 18) {
			rezid = 2169;
		}
		if(rezid > 0) {
			int32 DontRootMeBeforeTime = 0;
			CastSpell(rezid, target->GetID(), 1, -1, -1, &DontRootMeBeforeTime);
			target->SetDontRootMeBefore(DontRootMeBeforeTime);
			return true;
		}
	}
	return false;
}

bool Bot::Bot_Command_Cure(int curetype, int level) {
	int cureid = 0;
	switch(curetype) {
		case 1: // Poison
			if(level >= 58) {
				cureid = 1525;
			}
			else if(level >= 48) {
				cureid = 97;
			}
			else if(level >= 22) {
				cureid = 95;
			}
			else if(level >= 1) {
				cureid = 203;
			}
			break;
		case 2: // Disease
			if(level >= 51) {
				cureid = 3693;
			}
			else if(level >= 28) {
				cureid = 96;
			}
			else if(level >= 4) {
				cureid = 213;
			}
			break;
		case 3: // Curse
			if(level >= 54) {
				cureid = 2880;
			}
			else if(level >= 38) {
				cureid = 2946;
			}
			else if(level >= 23) {
				cureid = 4057;
			}
			else if(level >= 8) {
				cureid = 4056;
			}
			break;
		case 4: // Blindness
			if(level >= 3) {
				cureid = 212;
			}
			break;
	}

	if(cureid > 0) {
		Group* g = GetGroup();
		if(g) {
			for(int k=0; k<MAX_GROUP_MEMBERS; k++) {
				if(g->members[k]) {
					SpellOnTarget(cureid, g->members[k]);
				}
			}
			return true;
		}
	}

	return false;
}

// Completes a trade with a client bot owner
void Bot::FinishTrade(Client* client, BotTradeType tradeType) {
	if(client && !client->GetTradeskillObject() && (client->trade->state != Trading)) {
		if(tradeType == BotTradeClientNormal) {
			// Items being traded are found in the normal trade window used to trade between a Client and a Client or NPC
			// Items in this mode are found in slot ids 3000 thru 3003
			PerformTradeWithClient(3000, 3007, client);
		}
		else if(tradeType == BotTradeClientNoDropNoTrade) {
			// Items being traded are found on the Client's cursor slot, slot id 30. This item can be either a single item or it can be a bag.
			// If it is a bag, then we have to search for items in slots 331 thru 340
			PerformTradeWithClient(SLOT_CURSOR, SLOT_CURSOR, client);

			// TODO: Add logic here to test if the item in SLOT_CURSOR is a container type, if it is then we need to call the following:
			// PerformTradeWithClient(331, 340, client);
		}
	}
}

// Perfoms the actual trade action with a client bot owner
void Bot::PerformTradeWithClient(sint16 beginSlotID, sint16 endSlotID, Client* client) {
	if(client) {
		// TODO: Figure out what the actual max slot id is
		const int MAX_SLOT_ID = 3179;
		int32 items[MAX_SLOT_ID] = {0};
		int8 charges[MAX_SLOT_ID] = {0};
		bool botCanWear[MAX_SLOT_ID] = {0};
		
		for(sint16 i=beginSlotID; i<=endSlotID; ++i) {
			bool BotCanWear = false;
			bool UpdateClient = false;
			bool already_returned = false;

			Inventory& clientInventory = client->GetInv();
			const ItemInst* inst = clientInventory[i];
			if(inst) {
				items[i] = inst->GetItem()->ID;
				charges[i] = inst->GetCharges();
			}

			if(i == SLOT_CURSOR)
				UpdateClient = true;

			//EQoffline: will give the items to the bots and change the bot stats
			if(inst && (GetBotOwner() == client->CastToMob()) && !IsEngaged()) {
				std::string TempErrorMessage;
				const Item_Struct* mWeaponItem = inst->GetItem();
				bool failedLoreCheck = false;
				for(int m=0; m<MAX_AUGMENT_SLOTS; ++m) {
					ItemInst *itm = inst->GetAugment(m);
					if(itm)
					{
						if(CheckLoreConflict(itm->GetItem())) {
							failedLoreCheck = true;
						}
					}
				}
				if(CheckLoreConflict(mWeaponItem)) {
					failedLoreCheck = true;
				}
				if(failedLoreCheck) {
					Message_StringID(0, DUP_LORE);
				}
				if(!failedLoreCheck && mWeaponItem && inst->IsEquipable(GetBaseRace(), GetClass()) && (GetLevel() >= mWeaponItem->ReqLevel)) {
					BotCanWear = true;
					botCanWear[i] = BotCanWear;
					ItemInst* swap_item = NULL;

					const char* equipped[22] = {"Charm", "Left Ear", "Head", "Face", "Right Ear", "Neck", "Shoulders", "Arms", "Back",
												"Left Wrist", "Right Wrist", "Range", "Hands", "Primary Hand", "Secondary Hand",
												"Left Finger", "Right Finger", "Chest", "Legs", "Feet", "Waist", "Ammo" };
					bool success = false;
					int how_many_slots = 0;
					for(int j=0; j<22; ++j) {
						if((mWeaponItem->Slots & (1 << j))) {
							how_many_slots++;
							if(!GetBotItem(j)) {
								if(j == SLOT_PRIMARY) {
									if((mWeaponItem->ItemType == ItemType2HS) || (mWeaponItem->ItemType == ItemType2HB) || (mWeaponItem->ItemType == ItemType2HPierce)) {
										if(GetBotItem(SLOT_SECONDARY)) {
											ItemInst* remove_item = GetBotItem(SLOT_SECONDARY);
											BotTradeSwapItem(client, SLOT_SECONDARY, 0, remove_item, remove_item->GetItem()->Slots, &TempErrorMessage, false);
										}
									}
									BotTradeAddItem(mWeaponItem->ID, inst, inst->GetCharges(), mWeaponItem->Slots, j, &TempErrorMessage);
									success = true;
									break;
								}
								else if(j == SLOT_SECONDARY) {
									if(inst->IsWeapon()) {
										if(CanThisClassDualWield()) {
											BotTradeAddItem(mWeaponItem->ID, inst, inst->GetCharges(), mWeaponItem->Slots, j, &TempErrorMessage);
											success = true;
										}
										else {
											Say("I can't Dual Wield yet.");
											--how_many_slots;
										}
									}
									else {
										BotTradeAddItem(mWeaponItem->ID, inst, inst->GetCharges(), mWeaponItem->Slots, j, &TempErrorMessage);
										success = true;
									}
									if(success) {
										if(GetBotItem(SLOT_PRIMARY)) {
											ItemInst* remove_item = GetBotItem(SLOT_PRIMARY);
											if((remove_item->GetItem()->ItemType == ItemType2HS) || (remove_item->GetItem()->ItemType == ItemType2HB) || (remove_item->GetItem()->ItemType == ItemType2HPierce)) {
												BotTradeSwapItem(client, SLOT_PRIMARY, 0, remove_item, remove_item->GetItem()->Slots, &TempErrorMessage, false);
											}
										}
										break;
									}
								}
								else {
									BotTradeAddItem(mWeaponItem->ID, inst, inst->GetCharges(), mWeaponItem->Slots, j, &TempErrorMessage);
									success = true;
									break;
								}
							}
						}
					}
					if(!success) {
						for(int j=0; j<22; ++j) {
							if((mWeaponItem->Slots & (1 << j))) {
								swap_item = GetBotItem(j);
								failedLoreCheck = false;
								for(int k=0; k<MAX_AUGMENT_SLOTS; ++k) {
									ItemInst *itm = swap_item->GetAugment(k);
									if(itm)
									{
										if(client->CheckLoreConflict(itm->GetItem())) {
											failedLoreCheck = true;
										}
									}
								}
								if(client->CheckLoreConflict(swap_item->GetItem())) {
									failedLoreCheck = true;
								}
								if(!failedLoreCheck) {
									if(j == SLOT_PRIMARY) {
										if((mWeaponItem->ItemType == ItemType2HS) || (mWeaponItem->ItemType == ItemType2HB) || (mWeaponItem->ItemType == ItemType2HPierce)) {
											if(GetBotItem(SLOT_SECONDARY)) {
												ItemInst* remove_item = GetBotItem(SLOT_SECONDARY);
												BotTradeSwapItem(client, SLOT_SECONDARY, 0, remove_item, remove_item->GetItem()->Slots, &TempErrorMessage, false);
											}
										}
										BotTradeSwapItem(client, SLOT_PRIMARY, inst, swap_item, mWeaponItem->Slots, &TempErrorMessage);
										success = true;
										break;
									}
									else if(j == SLOT_SECONDARY) {
										if(inst->IsWeapon()) {
											if(CanThisClassDualWield()) {
												BotTradeSwapItem(client, SLOT_SECONDARY, inst, swap_item, mWeaponItem->Slots, &TempErrorMessage);
												success = true;
											}
											else {
												botCanWear[i] = false;
												Say("I can't Dual Wield yet.");
											}
										}
										else {
											BotTradeSwapItem(client, SLOT_SECONDARY, inst, swap_item, mWeaponItem->Slots, &TempErrorMessage);
											success = true;
										}
										if(success && GetBotItem(SLOT_PRIMARY)) {
											ItemInst* remove_item = GetBotItem(SLOT_PRIMARY);
											if((remove_item->GetItem()->ItemType == ItemType2HS) || (remove_item->GetItem()->ItemType == ItemType2HB) || (remove_item->GetItem()->ItemType == ItemType2HPierce)) {
												BotTradeSwapItem(client, SLOT_PRIMARY, 0, remove_item, remove_item->GetItem()->Slots, &TempErrorMessage, false);
											}
										}
										break;
									}
									else {
										BotTradeSwapItem(client, j, inst, swap_item, mWeaponItem->Slots, &TempErrorMessage);
										success = true;
										break;
									}
								}
								else {
									botCanWear[i] = false;
									Message_StringID(0, PICK_LORE);
									break;
								}
							}
						}
					}
					if(success) {
						if(how_many_slots > 1) {
							Message(300, "If you want this item in a different slot, use #bot inventory remove <slot_id> to clear the spot.");
						}
						CalcBotStats();
					}
				}
			}
			if(inst) {
				if(!botCanWear[i]) {
					client->PushItemOnCursor(*inst, true);
				}
				client->DeleteItemInInventory(i, 0, UpdateClient);
			}
		}

		const Item_Struct* item2 = 0;
		for(int y=beginSlotID; y<=endSlotID; ++y) {
			item2 = database.GetItem(items[y]);
			if(item2) {
				if(botCanWear[y]) {
					Say("Thank you for the %s, %s.", item2->Name,  client->GetName());
				}
				else {
					Say("I can't use this %s!", item2->Name);
				}
			}
		}
	}
}

void Bot::Death(Mob *killerMob, sint32 damage, int16 spell_id, SkillType attack_skill) {
	NPC::Death(killerMob, damage, spell_id, attack_skill);

	Save();

	Mob *give_exp = hate_list.GetDamageTop(this);
	Client *give_exp_client = NULL;

	if(give_exp && give_exp->IsClient())
		give_exp_client = give_exp->CastToClient();

	bool IsLdonTreasure = (this->GetClass() == LDON_TREASURE);

	//if (give_exp_client && !IsCorpse() && MerchantType == 0)
	//{
	//	Group *kg = entity_list.GetGroupByClient(give_exp_client);
	//	Raid *kr = entity_list.GetRaidByClient(give_exp_client);

	//	if(!kr && give_exp_client->IsClient() && give_exp_client->GetBotRaidID() > 0) {
	//		BotRaids *br = entity_list.GetBotRaidByMob(give_exp_client->CastToMob());
	//		if(br) {
	//			if(!IsLdonTreasure)
	//				br->SplitExp((EXP_FORMULA), this);

	//			if(br->GetBotMainTarget() == this)
	//				br->SetBotMainTarget(NULL);

	//			/* Send the EVENT_KILLED_MERIT event for all raid members */
	//			if(br->BotRaidGroups[0]) {
	//				for(int j=0; j<MAX_GROUP_MEMBERS; j++) {
	//					if(br->BotRaidGroups[0]->members[j] && br->BotRaidGroups[0]->members[j]->IsClient()) {
	//						parse->Event(EVENT_KILLED_MERIT, GetNPCTypeID(), "killed", this, br->BotRaidGroups[0]->members[j]);
	//						if(RuleB(TaskSystem, EnableTaskSystem)) {
	//							br->BotRaidGroups[0]->members[j]->CastToClient()->UpdateTasksOnKill(GetNPCTypeID());
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//}

	//corpse->Depop();
	if(entity_list.GetCorpseByID(GetID()))
		entity_list.GetCorpseByID(GetID())->Depop();

	Group *g = GetGroup();
	if(g) {
		for(int i=0; i<MAX_GROUP_MEMBERS; i++) {
			if(g->members[i]) {
				if(g->members[i] == this) {
					// If the leader dies, make the next bot the leader
					// and reset all bots followid
					if(g->IsLeader(g->members[i])) {
						if(g->members[i+1]) {
							g->SetLeader(g->members[i+1]);
							g->members[i+1]->SetFollowID(g->members[i]->GetFollowID());
							for(int j=0; j<MAX_GROUP_MEMBERS; j++) {
								if(g->members[j] && (g->members[j] != g->members[i+1])) {
									g->members[j]->SetFollowID(g->members[i+1]->GetID());
								}
							}
						}
					}

					// delete from group data
					g->membername[i][0] = '\0';
					memset(g->membername[i], 0, 64);
					g->members[i]->SetOwnerID(0);
					g->members[i] = NULL;

					// if group members exist below this one, move
					// them all up one slot in the group list
					int j = i+1;
					for(; j<MAX_GROUP_MEMBERS; j++) {
						if(g->members[j]) {
							g->members[j-1] = g->members[j];
							strcpy(g->membername[j-1], g->members[j]->GetCleanName());
							g->membername[j][0] = '\0';
							memset(g->membername[j], 0, 64);
							g->members[j] = NULL;
						}
					}

					// update the client group
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gu = (GroupJoin_Struct*)outapp->pBuffer;
					gu->action = groupActLeave;
					strcpy(gu->membername, GetCleanName());
					if(g) {
						for(int k=0; k<MAX_GROUP_MEMBERS; k++) {
							if(g->members[k] && g->members[k]->IsClient())
								g->members[k]->CastToClient()->QueuePacket(outapp);
						}
					}
					safe_delete(outapp);

					// now that's done, lets see if all we have left is the client
					// and we can clean up the clients raid group and group
					/*if(GetBotRaidID()) {
						BotRaids* br = entity_list.GetBotRaidByMob(this);
						if(br) {
							if(this == br->botmaintank) {
								br->botmaintank = NULL;
							}
							if(this == br->botsecondtank) {
								br->botsecondtank = NULL;
							}
						}
						if(g->GroupCount() == 0) {
							int32 gid = g->GetID();
							if(br) {
								br->RemoveEmptyBotGroup();
							}
							entity_list.RemoveGroup(gid);
						}
						if(br && (br->RaidBotGroupsCount() == 1)) {
							br->RemoveClientGroup(br->GetRaidBotLeader());
						}
						if(br && (br->RaidBotGroupsCount() == 0)) {
							br->DisbandBotRaid();
						}
					}*/
				}
			}
		}
	}

	entity_list.RemoveBot(this->GetID());
}

void Bot::Damage(Mob *from, sint32 damage, int16 spell_id, SkillType attack_skill, bool avoidable, sint8 buffslot, bool iBuffTic) {
	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	//handle EVENT_ATTACK. Resets after we have not been attacked for 12 seconds
	if(attacked_timer.Check()) {
		mlog(COMBAT__HITS, "Triggering EVENT_ATTACK due to attack by %s", from->GetName());
		parse->EventNPC(EVENT_ATTACK, this, from, "", 0);
	}

	attacked_timer.Start(CombatEventTimer_expire);
    
	// TODO: A bot doesnt call this, right?
	/*if (!IsEngaged())
		zone->AddAggroMob();*/

	// if spell is lifetap add hp to the caster
	if (spell_id != SPELL_UNKNOWN && IsLifetapSpell(spell_id)) {
		int healed = GetActSpellHealing(spell_id, damage);
		mlog(COMBAT__DAMAGE, "Applying lifetap heal of %d to %s", healed, GetCleanName());
		HealDamage(healed);
		entity_list.MessageClose(this, true, 300, MT_Spells, "%s beams a smile at %s", GetCleanName(), from->GetCleanName() );
	}

	CommonDamage(from, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);

	SendHPUpdate();

	// Aggro the bot's group members
	if(IsGrouped())
	{
		Group *g = GetGroup();
		if(g)
		{
			for(int i=0; i<MAX_GROUP_MEMBERS; i++)
			{
				if(g->members[i] && g->members[i]->IsBot() && from && !g->members[i]->CheckAggro(from))
				{
					g->members[i]->AddToHateList(from, 1);
				}
			}
		}
	}
}

void Bot::AddToHateList(Mob* other, sint32 hate, sint32 damage, bool iYellForHelp, bool bFrenzy, bool iBuffTic)
{
	Mob::AddToHateList(other, hate, damage, iYellForHelp, bFrenzy, iBuffTic);
}

bool Bot::Attack(Mob* other, int Hand, bool FromRiposte, bool IsStrikethrough)
{
	_ZP(Bot_Attack);

	if (!other) {
		SetTarget(NULL);
		LogFile->write(EQEMuLog::Error, "A null Mob object was passed to Bot::Attack for evaluation!");
		return false;
	}
	
	if(!GetTarget() || GetTarget() != other)
		SetTarget(other);
	
	mlog(COMBAT__ATTACKS, "Attacking %s with hand %d %s", other?other->GetCleanName():"(NULL)", Hand, FromRiposte?"(this is a riposte)":"");
	
	if ((IsCasting() && (GetClass() != BARD)) ||
		other == NULL ||
		(GetHP() < 0) ||
		(!IsAttackAllowed(other)))
	{
		if(this->GetOwnerID())
			entity_list.MessageClose(this, 1, 200, 10, "%s says, '%s is not a legal target master.'", this->GetCleanName(), this->GetTarget()->GetCleanName());
		if(other) {
			RemoveFromHateList(other);
			mlog(COMBAT__ATTACKS, "I am not allowed to attack %s", other->GetCleanName());
		}
		return false;
	}

	if(DivineAura()) {//cant attack while invulnerable
		mlog(COMBAT__ATTACKS, "Attack canceled, Divine Aura is in effect.");
		return false;
	}
	
	// TODO: Uncomment this block after solved the bug that is assigning a null value to GetTarget() for bots while in combat. Appears to happen at random, but frequently.
	/*if(HasGroup() && _previousTarget != GetTarget()) {
		std::ostringstream attackMessage;
		attackMessage << "Attacking " << other->GetCleanName() << ".";

		GetGroup()->GroupMessage(this, 0, 100, attackMessage.str().c_str());
	}*/

	FaceTarget(GetTarget());

	ItemInst* weapon = NULL;
	if(Hand == SLOT_PRIMARY)
		weapon = GetBotItem(SLOT_PRIMARY);
	if(Hand == SLOT_SECONDARY)
		weapon = GetBotItem(SLOT_SECONDARY);

	if(weapon != NULL) {
		if (!weapon->IsWeapon()) {
			mlog(COMBAT__ATTACKS, "Attack canceled, Item %s (%d) is not a weapon.", weapon->GetItem()->Name, weapon->GetID());
			return(false);
		}
		mlog(COMBAT__ATTACKS, "Attacking with weapon: %s (%d)", weapon->GetItem()->Name, weapon->GetID());
	} else {
		mlog(COMBAT__ATTACKS, "Attacking without a weapon.");
	}
	
	// calculate attack_skill and skillinuse depending on hand and weapon
	// also send Packet to near clients
	SkillType skillinuse;
	AttackAnimation(skillinuse, Hand, weapon);
	mlog(COMBAT__ATTACKS, "Attacking with %s in slot %d using skill %d", weapon?weapon->GetItem()->Name:"Fist", Hand, skillinuse);
	
	/// Now figure out damage
	int damage = 0;
	int weapon_damage = GetWeaponDamage(other, weapon);
	
	//if weapon damage > 0 then we know we can hit the target with this weapon
	//otherwise we cannot and we set the damage to -5 later on
	if(weapon_damage > 0){
		
		//Berserker Berserk damage bonus
		if((GetHPRatio() < 30) && (GetClass() == BERSERKER)){
			int bonus = 3 + GetLevel()/10;		//unverified
			weapon_damage = weapon_damage * (100+bonus) / 100;
			mlog(COMBAT__DAMAGE, "Berserker damage bonus increases DMG to %d", weapon_damage);
		}

		//try a finishing blow.. if successful end the attack
		if(TryFinishingBlow(other, skillinuse)) {
			return (true);
		}
		
		//damage formula needs some work
		int min_hit = 1;
		int max_hit = (2*weapon_damage*GetDamageTable(skillinuse)) / 100;

		if(GetLevel() < 10 && max_hit > 20)
			max_hit = 20;
		else if(GetLevel() < 20 && max_hit > 40)
			max_hit = 40;

		//if mainhand only, get the bonus damage from level
		if((Hand == SLOT_PRIMARY) && (GetLevel() >= 28) && IsWarriorClass())
		{
			int8 ucDamageBonus = GetWeaponDamageBonus( weapon ? weapon->GetItem() : (const Item_Struct*) NULL );

			min_hit += (int) ucDamageBonus;
			max_hit += (int) ucDamageBonus;
		}

		min_hit = min_hit * (100 + itembonuses.MinDamageModifier + spellbonuses.MinDamageModifier) / 100;

		if(Hand == SLOT_SECONDARY) {
			if((GetClass() == WARRIOR) ||
				(GetClass() == ROGUE) ||
				(GetClass() == MONK) ||
				(GetClass() == RANGER) ||
				(GetClass() == BARD) ||
				(GetClass() == BEASTLORD))
			{
				if(GetAA(aaSinisterStrikes)) { // Sinister Strikes AA
					int sinisterBonus = MakeRandomInt(5, 10);
					min_hit += (min_hit * sinisterBonus / 100);
					max_hit += (max_hit * sinisterBonus / 100);
				}
			}
		}

		if(max_hit < min_hit)
			max_hit = min_hit;

		if(RuleB(Combat, UseIntervalAC))
			damage = max_hit;
		else
			damage = MakeRandomInt(min_hit, max_hit);

		mlog(COMBAT__DAMAGE, "Damage calculated to %d (min %d, max %d, str %d, skill %d, DMG %d, lv %d)",
			damage, min_hit, max_hit, GetSTR(), GetSkill(skillinuse), weapon_damage, GetLevel());

		//check to see if we hit..
		if(!other->CheckHitChance(other, skillinuse, Hand)) {
			mlog(COMBAT__ATTACKS, "Attack missed. Damage set to 0.");
			damage = 0;
			other->AddToHateList(this, 0);
		} else {	//we hit, try to avoid it
			other->AvoidDamage(this, damage);
			other->MeleeMitigation(this, damage, min_hit);
			ApplyMeleeDamageBonus(skillinuse, damage);
			TryCriticalHit(other, skillinuse, damage);
			mlog(COMBAT__DAMAGE, "Final damage after all reductions: %d", damage);

			if(damage != 0){
				sint32 hate = max_hit;
				mlog(COMBAT__HITS, "Generating hate %d towards %s", hate, GetCleanName());
				// now add done damage to the hate list
				other->AddToHateList(this, hate);
			}
			else
				other->AddToHateList(this, 0);
		}

		//riposte
		bool slippery_attack = false; // Part of hack to allow riposte to become a miss, but still allow a Strikethrough chance (like on Live)
		if (damage == -3)  {
			if(FromRiposte) {
				return false;
			}
			else {
				
				
				if ((Hand == SLOT_SECONDARY) && GetAA(aaSlipperyAttacks) > 0) {// Do we even have it & was attack with mainhand? If not, don't bother with other calculations
					if (MakeRandomInt(0, 100) < (GetAA(aaSlipperyAttacks) * 20)) {
						damage = 0; // Counts as a miss
						slippery_attack = true;
					}
					else 
						DoRiposte(other);
				}
				else 
					DoRiposte(other);
			}
		}
		
		int aaStrikethroughBonus = 0;
		
		aaStrikethroughBonus += GetAA(aaStrikethrough) * 2;
		aaStrikethroughBonus += GetAA(aaTacticalMastery) * 2;

		//strikethrough..
		if (((damage < 0) || slippery_attack) && !FromRiposte && !IsStrikethrough) { // Hack to still allow Strikethrough chance w/ Slippery Attacks AA
			if(MakeRandomInt(0, 100) < (itembonuses.StrikeThrough + spellbonuses.StrikeThrough + aaStrikethroughBonus)) {
				Attack(other, Hand, true); // Strikethrough only gives another attempted hit
				return false;
			}
		}
	}
	else{
		damage = -5;
	}
	
	///////////////////////////////////////////////////////////
	//////    Send Attack Damage
	///////////////////////////////////////////////////////////
	other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);
	if(damage > 0 && (spellbonuses.MeleeLifetap || itembonuses.MeleeLifetap)) {
		mlog(COMBAT__DAMAGE, "Melee lifetap healing for %d damage.", damage);
		//heal self for damage done..
		HealDamage(damage);
	}
	
	//break invis when you attack
	if(invisible) {
		mlog(COMBAT__ATTACKS, "Removing invisibility due to melee attack.");
		BuffFadeByEffect(SE_Invisibility);
		BuffFadeByEffect(SE_Invisibility2);
		invisible = false;
	}
	if(invisible_undead) {
		mlog(COMBAT__ATTACKS, "Removing invisibility vs. undead due to melee attack.");
		BuffFadeByEffect(SE_InvisVsUndead);
		BuffFadeByEffect(SE_InvisVsUndead2);
		invisible_undead = false;
	}
	if(invisible_animals){
		mlog(COMBAT__ATTACKS, "Removing invisibility vs. animals due to melee attack.");
		BuffFadeByEffect(SE_InvisVsAnimals);
		invisible_animals = false;
	}

	if(hidden || improved_hidden){
		hidden = false;
		improved_hidden = false;
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
		SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
		sa_out->spawn_id = GetID();
		sa_out->type = 0x03;
		sa_out->parameter = 0;
		entity_list.QueueClients(this, outapp, true);
		safe_delete(outapp);
	}
	
	////////////////////////////////////////////////////////////
	////////  PROC CODE
	////////  Kaiyodo - Check for proc on weapon based on DEX
	///////////////////////////////////////////////////////////
	if(other->GetHP() > -10 && !FromRiposte && this) {
		TryWeaponProc(weapon, other);
	}
	
	if(damage > 0) {
		// Give the opportunity to throw back a defensive proc, if we are successful in affecting damage on our target
		other->TriggerDefensiveProcs(this);
		return true;
	}
	else {
		return false;
	}
}

sint16 Bot::GetBotFocusEffect(BotfocusType bottype, int16 spell_id) {
	if (IsBardSong(spell_id))
		return 0;

	const Item_Struct* TempItem = 0;
	const ItemInst* TempInst = 0;
	sint16 Total = 0;
	sint16 realTotal = 0;

	//item focus
	for(int x=0; x<=21; ++x) {
		TempInst = GetBotItem(x);
		if(TempInst) {
			TempItem = TempInst->GetItem();
			if (TempItem && TempItem->Focus.Effect > 0 && TempItem->Focus.Effect != SPELL_UNKNOWN) {
				Total = CalcBotFocusEffect(bottype, TempItem->Focus.Effect, spell_id);
				if(Total > realTotal) {
					realTotal = Total;
				}
			}
		}
	}

	//Spell Focus
	sint16 Total2 = 0;
	sint16 realTotal2 = 0;

	for (int y = 0; y < BUFF_COUNT; y++) {
		int16 focusspellid = buffs[y].spellid;
		
		if (focusspellid == 0 || focusspellid >= SPDAT_RECORDS)
			continue;

		Total2 = CalcBotFocusEffect(bottype, focusspellid, spell_id);
		
		if(Total2 > realTotal2) {
			realTotal2 = Total2;
		}
	}

	if(bottype == BotfocusReagentCost && IsSummonPetSpell(spell_id) && GetAA(aaElementalPact))
		return 100;

	if(bottype == BotfocusReagentCost && (IsEffectInSpell(spell_id, SE_SummonItem) || IsSacrificeSpell(spell_id))){
		return 0;
	//Summon Spells that require reagents are typically imbue type spells, enchant metal, sacrifice and shouldn't be affected
	//by reagent conservation for obvious reasons.
	}

	return realTotal + realTotal2;
}

sint16 Bot::CalcBotFocusEffect(BotfocusType bottype, int16 focus_id, int16 spell_id) {
	const SPDat_Spell_Struct &focus_spell = spells[focus_id];
	const SPDat_Spell_Struct &spell = spells[spell_id];

	sint16 value = 0;
	int lvlModifier = 100;

	for (int i = 0; i < EFFECT_COUNT; i++) {
		switch (focus_spell.effectid[i]) {
		case SE_Blank:
			break;

		//check limits

		//missing limits:
		//SE_LimitTarget

		case SE_LimitResist:{
			if(focus_spell.base[i]){
				if(spell.resisttype != focus_spell.base[i])
					return(0);
			}
			break;
		}

		case SE_LimitInstant:{
			if(spell.buffduration)
				return(0);
			break;
		}

		case SE_LimitMaxLevel:{
			int lvldiff = (spell.classes[(GetClass()%16) - 1]) - focus_spell.base[i];

			if(lvldiff > 0){ //every level over cap reduces the effect by spell.base2[i] percent
				lvlModifier -= spell.base2[i]*lvldiff;
				if(lvlModifier < 1)
					return 0;
			}
			break;
		}

		case SE_LimitMinLevel:
			if (spell.classes[(GetClass()%16) - 1] < focus_spell.base[i])
				return(0);
			break;

		case SE_LimitCastTime:
			if (spells[spell_id].cast_time < (uint16)focus_spell.base[i])
				return(0);
			break;

		case SE_LimitSpell:
			if(focus_spell.base[i] < 0) {	//exclude spell
				if (spell_id == (focus_spell.base[i]*-1))
					return(0);
			} else {
				//this makes the assumption that only one spell can be explicitly included...
				if (spell_id != focus_spell.base[i])
					return(0);
			}
			break;

		case SE_LimitMinDur:
				if (focus_spell.base[i] > CalcBuffDuration_formula(GetLevel(), spell.buffdurationformula, spell.buffduration))
					return(0);
			break;

		case SE_LimitEffect:
			if(focus_spell.base[i] < 0){
				if(IsEffectInSpell(spell_id,focus_spell.base[i])){ //we limit this effect, can't have
					return 0;
				}
			}
			else{
				if(!IsEffectInSpell(spell_id,focus_spell.base[i])){ //we limit this effect, must have
					return 0;
				}
			}
			break;


		case SE_LimitSpellType:
			switch( focus_spell.base[i] )
			{
				case 0:
					if (!IsDetrimentalSpell(spell_id))
						return 0;
					break;
				case 1:
					if (!IsBeneficialSpell(spell_id))
						return 0;
					break;
				default:
					LogFile->write(EQEMuLog::Normal, "CalcFocusEffect:  unknown limit spelltype %d", focus_spell.base[i]);
			}
			break;

		//handle effects

		case SE_ImprovedDamage:
			switch (focus_spell.max[i])
			{
				case 0:
					if (bottype == BotfocusImprovedDamage && focus_spell.base[i] > value)
					{
						value = focus_spell.base[i];
					}
					break;
				case 1:
					if (bottype == BotfocusImprovedCritical && focus_spell.base[i] > value)
					{
						value = focus_spell.base[i];
					}
					break;
				case 2:
					if (bottype == BotfocusImprovedUndeadDamage && focus_spell.base[i] > value)
					{
						value = focus_spell.base[i];
					}
					break;
				case 3:
					if (bottype == 10 && focus_spell.base[i] > value)
					{
						value = focus_spell.base[i];
					}
					break;
				default: //Resist stuff
					if (bottype == (BotfocusType)focus_spell.max[i] && focus_spell.base[i] > value)
					{
						value = focus_spell.base[i];
					}
					break;
			}
			break;
		case SE_ImprovedHeal:
			if (bottype == BotfocusImprovedHeal && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_IncreaseSpellHaste:
			if (bottype == BotfocusSpellHaste && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_IncreaseSpellDuration:
			if (bottype == BotfocusSpellDuration && BeneficialSpell(spell_id) && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_IncreaseRange:
			if (bottype == BotfocusRange && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_ReduceReagentCost:
			if (bottype == BotfocusReagentCost && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_ReduceManaCost:
			if (bottype == BotfocusManaCost && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
		case SE_PetPowerIncrease:
			if (bottype == BotfocusPetPower && focus_spell.base[i] > value)
			{
				value = focus_spell.base[i];
			}
			break;
#if EQDEBUG >= 6
		//this spits up a lot of garbage when calculating spell focuses
		//since they have all kinds of extra effects on them.
		default:
			LogFile->write(EQEMuLog::Normal, "CalcFocusEffect:  unknown effectid %d", focus_spell.effectid[i]);
#endif
		}
	}

	return(value*lvlModifier/100);
}

//proc chance includes proc bonus
float Bot::GetProcChances(float &ProcBonus, float &ProcChance, int16 weapon_speed) {
	int mydex = GetDEX();
	float AABonus = 0;
	ProcBonus = 0;
	ProcChance = 0;

	switch(GetAA(aaWeaponAffinity)) {
		case 1:
			AABonus = 0.10;
			break;
		case 2:
			AABonus = 0.20;
			break;
		case 3:
			AABonus = 0.30;
			break;
		case 4:
			AABonus = 0.40;
			break;
		case 5:
			AABonus = 0.50;
			break;
	}

	float PermaHaste;
	if(GetHaste() > 0)
		PermaHaste = 1 / (1 + (float)GetHaste()/100);
	else if(GetHaste() < 0)
		PermaHaste = 1 * (1 - (float)GetHaste()/100);
	else
		PermaHaste = 1.0f;

	weapon_speed = ((int)(weapon_speed*(100.0f+attack_speed)*PermaHaste) / 100);


	ProcBonus += (float(itembonuses.ProcChance + spellbonuses.ProcChance) / 1000.0f + AABonus);

	if(RuleB(Combat, AdjustProcPerMinute) == true)
	{
		ProcChance = ((float)weapon_speed * RuleR(Combat, AvgProcsPerMinute) / 600.0f);
		ProcBonus += float(mydex) * RuleR(Combat, ProcPerMinDexContrib) / 100.0f;
		ProcChance = ProcChance + (ProcChance * ProcBonus);
	}
	else
	{
		ProcChance = RuleR(Combat, BaseProcChance) + float(mydex) / RuleR(Combat, ProcDexDivideBy);
		ProcChance = ProcChance + (ProcChance * ProcBonus);
	}

	mlog(COMBAT__PROCS, "Proc chance %.2f (%.2f from bonuses)", ProcChance, ProcBonus);
	return ProcChance;
}

bool Bot::AvoidDamage(Mob* other, sint32 &damage)
{
	/* solar: called when a mob is attacked, does the checks to see if it's a hit
	*  and does other mitigation checks.  'this' is the mob being attacked.
	* 
	* special return values:
	*    -1 - block
	*    -2 - parry
	*    -3 - riposte
	*    -4 - dodge
	* 
	*/
	float skill = 0;
	float bonus = 0;
	float RollTable[4] = {0,0,0,0};
	float roll = 0;
	Mob *attacker=other;
	Mob *defender=this;

	//garunteed hit
	bool ghit = false;
	if((attacker->GetSpellBonuses().MeleeSkillCheck + attacker->GetItemBonuses().MeleeSkillCheck) > 500)
		ghit = true;
	
	//////////////////////////////////////////////////////////
	// make enrage same as riposte
	/////////////////////////////////////////////////////////
	if (IsEnraged() && !other->BehindMob(this, other->GetX(), other->GetY())) {
		damage = -3;
		mlog(COMBAT__DAMAGE, "I am enraged, riposting frontal attack.");
	}
	
	/////////////////////////////////////////////////////////
	// riposte
	/////////////////////////////////////////////////////////
	if (damage > 0 && CanThisClassRiposte() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
        skill = GetSkill(RIPOSTE);
		
		if (!ghit) {	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetDEX()/200);
			bonus = bonus * (100 + defender->GetSpellBonuses().RiposteChance + defender->GetItemBonuses().RiposteChance) / 100.0f;
			RollTable[0] = bonus;
		}
	}
	
	///////////////////////////////////////////////////////	
	// block
	///////////////////////////////////////////////////////

	bool bBlockFromRear = false;
	float aaChance = 0;

	// a successful roll on this does not mean a successful block is forthcoming. only that a chance to block
	// from a direction other than the rear is granted.
	if(GetAA(aaHightenedAwareness))
	{
		switch (GetAA(aaHightenedAwareness)) {
		case 1:
			aaChance = 8;
			break;
		case 2:
			aaChance = 16;
			break;
		case 3:
			aaChance = 24;
			break;
		case 4:
			aaChance = 32;
			break;
		case 5:
			aaChance = 40;
			break;
		}
	}

	if(aaChance > MakeRandomInt(1, 100))
		bBlockFromRear = true;

	if (damage > 0 && CanThisClassBlock() && (!other->BehindMob(this, other->GetX(), other->GetY()) || bBlockFromRear))
	{
		skill = GetSkill(BLOCKSKILL);

		if(!ghit)
		{	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/35.0 + (GetDEX()/200);
			RollTable[1] = RollTable[0] + bonus;
		}
	}
	else
	{
		RollTable[1] = RollTable[0];
	}

	if(damage > 0 && GetAA(aaShieldBlock) && (!other->BehindMob(this, other->GetX(), other->GetY())))
	{
		bool equiped = m_inv.GetItem(SLOT_SECONDARY);
		if(equiped) {
			uint8 shield = m_inv.GetItem(SLOT_SECONDARY)->GetItem()->ItemType;

			if(shield == ItemTypeShield) {
				switch(GetAA(aaShieldBlock)) {
					 case 1:
						RollTable[1] = RollTable[0] + 2.50;
                        break;
	                 case 2:
		                RollTable[1] = RollTable[0] + 5.00;
			            break;
				     case 3:
					    RollTable[1] = RollTable[0] + 10.00;
						break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////		
	// parry
	//////////////////////////////////////////////////////
	if (damage > 0 && CanThisClassParry() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
		skill = GetSkill(PARRY);

		if(!ghit)
		{	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetDEX()/200);
			bonus = bonus * (100 + defender->GetSpellBonuses().ParryChance + defender->GetItemBonuses().ParryChance) / 100.0f;
			RollTable[2] = RollTable[1] + bonus;
		}
	}
	else
	{
		RollTable[2] = RollTable[1];
	}
	
	////////////////////////////////////////////////////////
	// dodge
	////////////////////////////////////////////////////////
	if (damage > 0 && CanThisClassDodge() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
		skill = GetSkill(DODGE);

		if(!ghit)
		{	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetAGI()/200);
			bonus = bonus * (100 + defender->GetSpellBonuses().DodgeChance + defender->GetItemBonuses().DodgeChance) / 100.0f;
			RollTable[3] = RollTable[2] + bonus;
		}
	}
	else
	{
		RollTable[3] = RollTable[2];
	}

	if(damage > 0)
	{
		roll = MakeRandomFloat(0,100);
		if(roll <= RollTable[0]){
			damage = -3;
		}
		else if(roll <= RollTable[1]){
			damage = -1;
		}
		else if(roll <= RollTable[2]){
			damage = -2;
		}
		else if(roll <= RollTable[3]){
			damage = -4;
		}
	}

	mlog(COMBAT__DAMAGE, "Final damage after all avoidances: %d", damage);
	
	if (damage < 0)
		return true;
	return false;
}

int Bot::GetMonkHandToHandDamage(void)
{
	// Kaiyodo - Determine a monk's fist damage. Table data from www.monkly-business.com
	// saved as static array - this should speed this function up considerably
	static int damage[66] = {
		//   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
		99, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
		8, 8, 8, 8, 8, 9, 9, 9, 9, 9,10,10,10,10,10,11,11,11,11,11,
		12,12,12,12,12,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,
		14,14,15,15,15,15 };

		// Have a look to see if we have epic fists on

		uint32 botWeaponId = INVALID_ID;
		botWeaponId = CastToNPC()->GetEquipment(MATERIAL_HANDS);
		if(botWeaponId == 10652) { //Monk Epic ID
			return 9;
		}
		else
		{
			int Level = GetLevel();
			if(Level > 65)
				return 19;
			else
				return damage[Level];
		}

		int Level = GetLevel();
		if (Level > 65)
			return(19);
		else
			return damage[Level];
}

void Bot::TryCriticalHit(Mob *defender, int16 skill, sint32 &damage)
{
	bool slayUndeadCrit = false;

	if(damage < 1) //We can't critical hit if we don't hit.
		return;

	int critChance = RuleI(Combat, MeleeBaseCritChance);

	critChance += RuleI(Combat, ClientBaseCritChance);

	uint16 critMod = 200; 

	if(skill == ARCHERY && GetClass() == RANGER && GetSkill(ARCHERY) >= 65){
		critChance += 6;
	}

	if(GetHPRatio() < 30 && GetClass() == BERSERKER) {
		critChance += RuleI(Combat, BerserkBaseCritChance);
		critMod = 400;
	}
	else if(GetHPRatio() < 30 && GetClass() == WARRIOR) {
		critChance += RuleI(Combat, WarBerBaseCritChance);
	}

	// Bot AA's for CombatFury and FuryoftheAges
	if(GetAA(aaCombatFury)) {
		critChance += GetAA(aaCombatFury) * 2;
	}

	int CritBonus = GetCriticalChanceBonus(skill);
	if(CritBonus > 0) {
		if(critChance == 0) //If we have a bonus to crit in items or spells but no actual chance to crit
			critChance = (CritBonus / 100) + 1; //Give them a small one so skills and items appear to have some effect.
		else
			critChance += (critChance * CritBonus / 100); //crit chance is a % increase to your reg chance
	}

	if(GetAA(aaSlayUndead)){
		if(defender && defender->GetBodyType() == BT_Undead || defender->GetBodyType() == BT_SummonedUndead || defender->GetBodyType() == BT_Vampire){
			switch(GetAA(aaSlayUndead)){
			case 1:
				critMod += 33;
				break;
			case 2:
				critMod += 66;
				break;
			case 3:
				critMod += 100;
				break;
			}
			slayUndeadCrit = true;
		}
	}

	if(critChance > 0){
		if(MakeRandomInt(0, 99) < critChance)
		{
			if (slayUndeadCrit)
			{
				damage = (damage * (critMod * 2.65)) / 100;
				entity_list.MessageClose(this, false, 200, MT_CritMelee, "%s cleanses %s target!(%d)", GetCleanName(), this->GetGender() == 0 ? "his" : this->GetGender() == 1 ? "her" : "its", damage);
				return;
			}
			////Veteran's Wrath AA
			////first, find out of we have it (don't multiply by 0 :-\ )
			int32 AAdmgmod = GetAA(aaVeteransWrath);
			if (AAdmgmod > 0) {
				//now, make sure it's not a special attack
				if (skill == _1H_BLUNT
					|| skill == _2H_BLUNT
					|| skill == _1H_SLASHING
					|| skill == _2H_SLASHING
					|| skill == PIERCING
					|| skill == HAND_TO_HAND
					)
					critMod += AAdmgmod * 3; //AndMetal: guessing
			}

			damage = (damage * critMod) / 100;

			if((GetClass() == WARRIOR || GetClass() == BERSERKER) && GetHPRatio() < 30)
			{
				entity_list.MessageClose(this, false, 200, MT_CritMelee, "%s lands a crippling blow!(%d)", GetCleanName(), damage);
			}
			else
			{
				entity_list.MessageClose(this, false, 200, MT_CritMelee, "%s scores a critical hit!(%d)", GetCleanName(), damage);
			}
		}
	}
}

bool Bot::TryFinishingBlow(Mob *defender, SkillType skillinuse)
{
	int8 aa_item = GetAA(aaFinishingBlow);

	if(aa_item && !defender->IsClient() && defender->GetHPRatio() < 10)
	{
		int chance = 0;
		int levelreq = 0;
		switch(aa_item)
		{
		case 1:
			chance = 2;
			levelreq = 50;
			break;
		case 2:
			chance = 5;
			levelreq = 52;
			break;
		case 3:
			chance = 7;
			levelreq = 54;
			break;
		case 4:
			chance = 7;
			levelreq = 55;
			break;
		case 5:
			chance = 7;
			levelreq = 57;
			break;
		case 6:
			chance = 7;
			levelreq = 59;
			break;
		case 7:
			chance = 7;
			levelreq = 61;
			break;
		case 8:
			chance = 7;
			levelreq = 63;
			break;
		case 9:
			chance = 7;
			levelreq = 65;
			break;
		default:
			break;
		}

		if(chance >= MakeRandomInt(0, 100) && defender->GetLevel() <= levelreq){
			mlog(COMBAT__ATTACKS, "Landed a finishing blow: AA at %d, other level %d", aa_item, defender->GetLevel());
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, FINISHING_BLOW, GetCleanName());
			defender->Damage(this, 32000, SPELL_UNKNOWN, skillinuse);
			return true;
		}
		else
		{
			mlog(COMBAT__ATTACKS, "FAILED a finishing blow: AA at %d, other level %d", aa_item, defender->GetLevel());
			return false;
		}
	}
	return false;
}

void Bot::DoRiposte(Mob* defender) {
	mlog(COMBAT__ATTACKS, "Preforming a riposte");

	defender->Attack(this, SLOT_PRIMARY, true);

	//double riposte
	int DoubleRipChance = 0;
	switch(defender->GetAA(aaDoubleRiposte)) {
		case 1: 
			DoubleRipChance = 15;
			break;
		case 2:
			DoubleRipChance = 35;
			break;
		case 3:
			DoubleRipChance = 50;
			break;
		case 4: 
			DoubleRipChance = 60;
			break;
		case 5:
			DoubleRipChance = 70;
			break;
		case 6:
			DoubleRipChance = 80;
			break;
	}

	if(DoubleRipChance >= MakeRandomInt(0, 100)) {
		mlog(COMBAT__ATTACKS, "Preforming a double riposte (%d percent chance)", DoubleRipChance);

		defender->Attack(this, SLOT_PRIMARY, true);
	}

	if(defender->GetAA(aaReturnKick)){
		int ReturnKickChance = 0;
		switch(defender->GetAA(aaReturnKick)){
			case 1:
				ReturnKickChance = 25;
				break;
			case 2:
				ReturnKickChance = 35;
				break;
			case 3:
				ReturnKickChance = 50;
				break;
		}

		if(ReturnKickChance >= MakeRandomInt(0, 100)) {
			mlog(COMBAT__ATTACKS, "Preforming a return kick (%d percent chance)", ReturnKickChance);
			defender->MonkSpecialAttack(this, FLYING_KICK);
		}
	}			
}

void Bot::MeleeMitigation(Mob *attacker, sint32 &damage, sint32 minhit)
{
	if(damage <= 0)
		return;

	Mob* defender = this;
	float aa_mit = aabonuses.MeleeMitigation;

	if(RuleB(Combat, UseIntervalAC))
	{
		float softcap = 0.0;
		float mitigation_rating = 0.0;
		float attack_rating = 0.0;
		int shield_ac = 0;
		int armor;
		float weight = 0.0;
		if(IsBot())
		{
			armor = GetRawACNoShield(shield_ac);
			weight = (CalcCurrentWeight() / 10.0);
		}

		if(GetClass() == WIZARD || GetClass() == MAGICIAN || GetClass() == NECROMANCER || GetClass() == ENCHANTER)
		{
			softcap = RuleI(Combat, ClothACSoftcap);
		}
		else if(GetClass() == MONK && weight <= 15.0)
		{
			softcap = RuleI(Combat, MonkACSoftcap);
		}
		else if(GetClass() == DRUID || GetClass() == BEASTLORD || GetClass() == MONK)
		{
			softcap = RuleI(Combat, LeatherACSoftcap);
		}
		else if(GetClass() == SHAMAN || GetClass() == ROGUE || GetClass() == BERSERKER || GetClass() == RANGER)
		{
			softcap = RuleI(Combat, ChainACSoftcap);
		}
		else
		{
			softcap = RuleI(Combat, PlateACSoftcap);
		}
		
		softcap += shield_ac;
		armor += shield_ac;
		softcap += (softcap * (aa_mit * RuleR(Combat, AAMitigationACFactor)));
		if(armor > softcap)
		{
			int softcap_armor = armor - softcap;
			if(GetClass() == WARRIOR)
			{
				softcap_armor = softcap_armor * RuleR(Combat, WarriorACSoftcapReturn);
			}
			else if(GetClass() == SHADOWKNIGHT || GetClass() == PALADIN || (GetClass() == MONK && weight <= 15.0))
			{
				softcap_armor = softcap_armor * RuleR(Combat, KnightACSoftcapReturn);
			}
			else if(GetClass() == CLERIC || GetClass() == BARD || GetClass() == BERSERKER || GetClass() == ROGUE || GetClass() == SHAMAN || GetClass() == MONK)
			{
				softcap_armor = softcap_armor * RuleR(Combat, LowPlateChainACSoftcapReturn);
			}
			else if(GetClass() == RANGER || GetClass() == BEASTLORD)
			{
				softcap_armor = softcap_armor * RuleR(Combat, LowChainLeatherACSoftcapReturn);
			}
			else if(GetClass() == WIZARD || GetClass() == MAGICIAN || GetClass() == NECROMANCER || GetClass() == ENCHANTER || GetClass() == DRUID)
			{
				softcap_armor = softcap_armor * RuleR(Combat, CasterACSoftcapReturn);
			}
			else
			{
				softcap_armor = softcap_armor * RuleR(Combat, MiscACSoftcapReturn);
			}
			armor = softcap + softcap_armor;
		}

		mitigation_rating = 0.0;
		if(GetClass() == WIZARD || GetClass() == MAGICIAN || GetClass() == NECROMANCER || GetClass() == ENCHANTER)
		{
			mitigation_rating = ((GetSkill(DEFENSE) + itembonuses.HeroicAGI/10) / 4.0) + armor + 1;
		}
		else
		{
			mitigation_rating = ((GetSkill(DEFENSE) + itembonuses.HeroicAGI/10) / 3.0) + (armor * 1.333333) + 1;
		}
		mitigation_rating *= 0.847;

		if(attacker->IsClient())
		{
			attack_rating = (attacker->CastToClient()->GetATK() + ((attacker->GetSTR()-66) * 0.9) + (attacker->GetSkill(OFFENSE)*1.345));
		}
		else if(attacker->IsBot())
		{
			attack_rating = (attacker->CastToBot()->CalcATK() + ((attacker->GetSTR()-66) * 0.9) + (attacker->GetSkill(OFFENSE)*1.345));
		}
		else
		{
			attack_rating = (attacker->GetATK() + (attacker->GetSkill(OFFENSE)*1.345) + ((attacker->GetSTR()-66) * 0.9));
		}

		float d = 10.0;
		float mit_roll = MakeRandomFloat(0, mitigation_rating);
		float atk_roll = MakeRandomFloat(0, attack_rating);

		if(atk_roll > mit_roll)
		{
			float a_diff = (atk_roll - mit_roll);
			float thac0 = attack_rating * RuleR(Combat, ACthac0Factor);
			d -= 10.0 * (a_diff / thac0);
			float thac0cap = ((attacker->GetLevel() * 9) + 20);
			if(thac0 > thac0cap)
			{
				thac0 = thac0cap;
			}
		}
		else if(mit_roll > atk_roll)
		{
			float m_diff = (mit_roll - atk_roll);
			float thac20 = mitigation_rating * RuleR(Combat, ACthac20Factor);
			d += 10 * (m_diff / thac20);
			float thac20cap = ((defender->GetLevel() * 9) + 20);
			if(thac20 > thac20cap)
			{
				thac20 = thac20cap;
			}
		}

		if(d < 0.0)
		{
			d = 0.0;
		}

		if(d > 20)
		{
			d = 20.0;
		}

		float interval = (damage - minhit) / 20.0;
		damage = damage - ((int)d * interval);
	}
	else{
		////////////////////////////////////////////////////////
		// Scorpious2k: Include AC in the calculation
		// use serverop variables to set values
		int myac = GetAC();
		if (damage > 0 && myac > 0) {
			int acfail=1000;
			char tmp[10];

			if (database.GetVariable("ACfail", tmp, 9)) {
				acfail = (int) (atof(tmp) * 100);
				if (acfail>100) acfail=100;
			}

			if (acfail<=0 || MakeRandomInt(0, 100)>acfail) {
				float acreduction=1;
				int acrandom=300;
				if (database.GetVariable("ACreduction", tmp, 9))
				{
					acreduction=atof(tmp);
					if (acreduction>100) acreduction=100;
				}
		
				if (database.GetVariable("ACrandom", tmp, 9))
				{
					acrandom = (int) ((atof(tmp)+1) * 100);
					if (acrandom>10100) acrandom=10100;
				}
				
				if (acreduction>0) {
					damage -= (int) (GetAC() * acreduction/100.0f);
				}		
				if (acrandom>0) {
					damage -= (myac * MakeRandomInt(0, acrandom) / 10000);
				}
				if (damage<1) damage=1;
				mlog(COMBAT__DAMAGE, "AC Damage Reduction: fail chance %d%%. Failed. Reduction %.3f%%, random %d. Resulting damage %d.", acfail, acreduction, acrandom, damage);
			} else {
				mlog(COMBAT__DAMAGE, "AC Damage Reduction: fail chance %d%%. Did not fail.", acfail);
			}
		}

		damage -= (aa_mit * damage);

		if(damage != 0 && damage < minhit)
			damage = minhit;
	}


	//reduce the damage from shielding item and aa based on the min dmg
	//spells offer pure mitigation
	damage -= (minhit * itembonuses.MeleeMitigation / 100);
	damage -= (damage * spellbonuses.MeleeMitigation / 100);

	if(damage < 0)
		damage = 0;

	mlog(COMBAT__DAMAGE, "Applied %d percent mitigation, remaining damage %d", aa_mit, damage);
}

void Bot::DoSpecialAttackDamage(Mob *who, SkillType skill, sint32 max_damage, sint32 min_damage, sint32 hate_override) {
	//this really should go through the same code as normal melee damage to
	//pick up all the special behavior there

	sint32 hate = max_damage;
	if(hate_override > -1)
		hate = hate_override;

	if(skill == BASH) {
		const ItemInst* inst = GetBotItem(SLOT_SECONDARY);
		const Item_Struct* botweapon = 0;
		if(inst)
			botweapon = inst->GetItem();
		if(botweapon) {
			if(botweapon->ItemType == ItemTypeShield) {
				hate += botweapon->AC;
			}
		}

	}

	if(max_damage > 0) {
		if(skill != THROWING && skill != ARCHERY) {
			who->AvoidDamage(this, max_damage);
		}
		who->MeleeMitigation(this, max_damage, min_damage);
		ApplyMeleeDamageBonus(skill, max_damage);
		TryCriticalHit(who, skill, max_damage);
		if(max_damage > 0) {
			who->AddToHateList(this, hate);
		}
		else
			who->AddToHateList(this, 0);
	}
	who->Damage(this, max_damage, SPELL_UNKNOWN, skill, false);
	
	if(max_damage == -3)
		DoRiposte(who);	
}

void Bot::TryBackstab(Mob *other) {
	if(!other)
		return;

	const ItemInst* inst = GetBotItem(SLOT_PRIMARY);
	const Item_Struct* botpiercer = NULL;
	if(inst)
		botpiercer = inst->GetItem();
	if(!botpiercer || (botpiercer->ItemType != ItemTypePierce)) {
		Say("I can't backstab with this weapon!");
		return;
	}
	
	bool tripleBackstab = false;
	int tripleChance = 0;

	if (GetAA(aaTripleBackstab) > 0) {
		switch (GetAA(aaTripleBackstab)) {
		case 1:
			tripleChance = 10;
			break;
		case 2:
			tripleChance = 20;
			break;
		case 3:
			tripleChance = 30;
			break;
		}
		if (tripleChance > MakeRandomInt(0, 100)) {
			tripleBackstab = true;
		}
	}

	bool seizedOpportunity = false;
	int seizedChance = 0;

	if (GetAA(aaSeizedOpportunity) > 0) {
		switch (GetAA(aaSeizedOpportunity)) {
			case 1:
				seizedChance = 10;
				break;
			case 2:
				seizedChance = 20;
				break;
			case 3:
				seizedChance = 30;
				break;
		}
		if (seizedChance > MakeRandomInt(0, 100)) {
			seizedOpportunity = true;
		}
	}

	if(BehindMob(other, GetX(), GetY()) || seizedOpportunity) // Player is behind other
	{
		if (seizedOpportunity) {
			Message(0,"%s's fierce attack is executed with such grace, %s did not see it coming!", this->GetCleanName(), other->GetCleanName());
		}

		// solar - chance to assassinate
		float chance = (10.0+(GetDEX()/10)); //18.5% chance at 85 dex 40% chance at 300 dex
		if(
			level >= 60 && // player is 60 or higher
			other->GetLevel() <= 45 && // mob 45 or under
			!other->CastToNPC()->IsEngaged() && // not aggro
			other->GetHP()<=32000
			&& other->IsNPC()
			&& MakeRandomFloat(0, 99) < chance // chance
			) {
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, ASSASSINATES, GetName());
			RogueAssassinate(other);
		}
		else {
			RogueBackstab(other);
			if (level > 54) {
				float DoubleAttackProbability = (GetSkill(DOUBLE_ATTACK) + GetLevel()) / 500.0f; // 62.4 max
				// Check for double attack with main hand assuming maxed DA Skill (MS)
				
				if(MakeRandomFloat(0, 1) < DoubleAttackProbability)	// Max 62.4 % chance of DA
				{
					if(other->GetHP() > 0)
						RogueBackstab(other);

					if (tripleBackstab && other->GetHP() > 0) 
					{
						RogueBackstab(other);
					}
				}
			}
		}
	}
	// Chaotic Stab
	else if(GetAA(aaChaoticStab) > 0)
	{
		//we can stab from any angle, we do min damage though.
		RogueBackstab(other, true);
		float DoubleAttackProbability = (GetSkill(DOUBLE_ATTACK) + GetLevel()) / 500.0f; // 62.4 max
		// Check for double attack with main hand assuming maxed DA Skill (MS)
		if(MakeRandomFloat(0, 1) < DoubleAttackProbability)
		{		// Max 62.4 % chance of DA
			if(other->GetHP() > 0)
				RogueBackstab(other, true);

			if (tripleBackstab && other->GetHP() > 0)
			{
				RogueBackstab(other);
			}
		}
	}
	else { //We do a single regular attack if we attack from the front without chaotic stab
		Attack(other, 13);
	}
}

//heko: backstab
void Bot::RogueBackstab(Mob* other, bool min_damage)
{
	sint32 ndamage = 0;
	sint32 max_hit = 0;
	sint32 min_hit = 0;
	sint32 hate = 0;
	sint32 primaryweapondamage = 0;
	sint32 backstab_dmg = 0;
	
	ItemInst* botweaponInst = GetBotItem(SLOT_PRIMARY);
	if(botweaponInst) {
		primaryweapondamage = GetWeaponDamage(other, botweaponInst);
		backstab_dmg = primaryweapondamage;
	}
	else 
	{
		primaryweapondamage = (GetLevel()/7)+1; // fallback incase it's a npc without a weapon, 2 dmg at 10, 10 dmg at 65
		backstab_dmg = primaryweapondamage;
	}

	if(primaryweapondamage > 0){
		if(level > 25){
			max_hit = (((2*backstab_dmg) * GetDamageTable(BACKSTAB) / 100) * 10 * GetSkill(BACKSTAB) / 355)  + ((level-25)/3) + 1;
			hate = 20 * backstab_dmg * GetSkill(BACKSTAB) / 355;
		}
		else{
			max_hit = (((2*backstab_dmg) * GetDamageTable(BACKSTAB) / 100) * 10 * GetSkill(BACKSTAB) / 355) + 1;
			hate = 20 * backstab_dmg * GetSkill(BACKSTAB) / 355;
		}

		// determine minimum hits
		if (level < 51)
		{
			min_hit = (level*15/10);
		}
		else
		{
			// Trumpcard:  Replaced switch statement with formula calc.  This will give minhit increases all the way to 65.
			min_hit = (level * ( level*5 - 105)) / 100;
		}

		if(!other->CheckHitChance(this, BACKSTAB, 0))	{
			ndamage = 0;
		}
		else{
			if(min_damage){
				ndamage = min_hit;
			}
			else
			{
				if (max_hit < min_hit)
					max_hit = min_hit;

				if(RuleB(Combat, UseIntervalAC))
					ndamage = max_hit; 
				else
					ndamage = MakeRandomInt(min_hit, max_hit);

			}
		}
	}
	else{
		ndamage = -5;
	}

	DoSpecialAttackDamage(other, BACKSTAB, ndamage, min_hit, hate);
	DoAnim(animPiercing);
}

void Bot::RogueAssassinate(Mob* other)
{
	ItemInst* botweaponInst = GetBotItem(SLOT_PRIMARY);
	if(botweaponInst) {
		if(GetWeaponDamage(other, botweaponInst)) {
			other->Damage(this, 32000, SPELL_UNKNOWN, BACKSTAB);
		}
		else {
			other->Damage(this, -5, SPELL_UNKNOWN, BACKSTAB);
		}
	}

	DoAnim(animPiercing);	//piercing animation
}

void Bot::DoClassAttacks(Mob *target) {
	if(target == NULL)
		return;	//gotta have a target for all these

	bool taunt_time = taunt_timer.Check();
	bool ca_time = classattack_timer.Check(false);
	bool ka_time = knightattack_timer.Check(false);

	//only check attack allowed if we are going to do something
	if((taunt_time || ca_time || ka_time) && !IsAttackAllowed(target))
		return;

	if(ka_time){
		int knightreuse = 1000; //lets give it a small cooldown actually.
		switch(GetClass()){
			case SHADOWKNIGHT: case SHADOWKNIGHTGM:{
				CastSpell(SPELL_NPC_HARM_TOUCH, target->GetID());
				knightreuse = HarmTouchReuseTime * 1000;
				break;
							   }
			case PALADIN: case PALADINGM:{
				if(GetHPRatio() < 20) {
					CastSpell(SPELL_LAY_ON_HANDS, GetID());
					knightreuse = LayOnHandsReuseTime * 1000;
				} else {
					knightreuse = 2000; //Check again in two seconds.
				}
				break;
						  }
		}
		knightattack_timer.Start(knightreuse); 
	}

	//general stuff, for all classes....
	//only gets used when their primary ability get used too
	if (taunting && HasOwner() && target->IsNPC() && target->GetBodyType() != BT_Undead && taunt_time) {
		Taunt(target->CastToNPC(), false);
	}

	//franck-add: EQoffline. Warrior bots must taunt the target.
	if(((GetClass() == WARRIOR) || (GetClass() == PALADIN) || (GetClass() == SHADOWKNIGHT)) && target->IsNPC() && taunt_time && target) {
		/*bool isTaunting = false;
		BotRaids* br = entity_list.GetBotRaidByMob(this);
		if(br)
		{
			if(br->GetBotMainTank())
			{
				if(br->GetBotMainTank() == this)
				{
					isTaunting = true;
				}
			}
			else if(br->GetBotSecondTank())
			{
				if(br->GetBotSecondTank() == this)
				{
					isTaunting = true;
				}
			}
			else
			{
				if(MakeRandomInt(1, 100) > 50)
				{
					isTaunting = true;
				}
			}
		}
		else
		{
			isTaunting = true;
		}

		if(isTaunting) {
			Say("Taunting %s", target->GetCleanName());
			Taunt(target->CastToNPC(), true);
		}*/
		Say("Taunting %s", target->GetCleanName());
		Taunt(target->CastToNPC(), false);
	}

	if(!ca_time)
		return;

	float HasteModifier = 0;
	if(GetHaste() > 0)
		HasteModifier = 10000 / (100 + GetHaste());
	else if(GetHaste() < 0)
		HasteModifier = (100 - GetHaste());
	else
		HasteModifier = 100;

	int level = GetLevel();
	int reuse = TauntReuseTime * 1000;	//make this very long since if they dont use it once, they prolly never will
	bool did_attack = false;
	//class specific stuff...
	switch(GetClass()) {
		case ROGUE: case ROGUEGM:
			if(level >= 10) {
				TryBackstab(target);
				reuse = BackstabReuseTime * 1000;
				did_attack = true;
			}
			break;
		case MONK: case MONKGM: {
			int8 satype = KICK;
			if(level > 29) {
				satype = FLYING_KICK;
			} else if(level > 24) {
				satype = DRAGON_PUNCH;
			} else if(level > 19) {
				satype = EAGLE_STRIKE;
			} else if(level > 9) {
				satype = TIGER_CLAW;
			} else if(level > 4) {
				satype = ROUND_KICK;
			}
			reuse = MonkSpecialAttack(target, satype);

			int specl = GetAA(aaTechniqueofMasterWu) * 20;

			if(specl == 100 || specl > MakeRandomInt(0,100)) {
				reuse = MonkSpecialAttack(target, satype);
				if(20 > MakeRandomInt(0,100)) {
					reuse = MonkSpecialAttack(target, satype);
				}
			}

			reuse *= 1000;
			did_attack = true;
			break;
				   }
		case WARRIOR: case WARRIORGM:{
			if(level >= RuleI(Combat, NPCBashKickLevel)){
				if(MakeRandomInt(0, 100) > 25) //tested on live, warrior mobs both kick and bash, kick about 75% of the time, casting doesn't seem to make a difference.
				{
					DoAnim(animKick);
					sint32 dmg = 0;

					if(GetWeaponDamage(GetTarget(), m_inv.GetItem(SLOT_FEET)) <= 0){
						dmg = -5;
					}
					else{
						if(target->CheckHitChance(this, KICK, 0)) {
							if(RuleB(Combat, UseIntervalAC))
								dmg = GetKickDamage();
							else
								dmg = MakeRandomInt(1, GetKickDamage());

						}
					}

					DoSpecialAttackDamage(target, KICK, dmg);
					reuse = KickReuseTime * 1000;
					did_attack = true;
				}
				else
				{
					if((GetRace() == OGRE || GetRace() == TROLL || GetRace() == BARBARIAN) // Racial Slam
						|| (m_inv.GetItem(SLOT_SECONDARY) && m_inv.GetItem(SLOT_SECONDARY)->GetItem()->ItemType == ItemTypeShield)  //Using Shield
						|| (m_inv.GetItem(SLOT_PRIMARY) && (m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HS 
							|| m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HB 
							|| m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HPierce) 
							&& GetAA(aa2HandBash) >= 1)) {   //Using 2 hand weapon, but has AA 2 Hand Bash
						DoAnim(animTailRake);
						sint32 dmg = 0;

						if(GetWeaponDamage(GetTarget(), m_inv.GetItem(SLOT_SECONDARY)) <= 0 &&
							GetWeaponDamage(GetTarget(), m_inv.GetItem(SLOT_SHOULDER)) <= 0){
							dmg = -5;
						}
						else{
							if(target->CheckHitChance(this, BASH, 0)) {
								if(RuleB(Combat, UseIntervalAC))
									dmg = GetBashDamage();
								else
									dmg = MakeRandomInt(1, GetBashDamage());
							}
						}

						DoSpecialAttackDamage(target, BASH, dmg);
						reuse = BashReuseTime * 1000;
						did_attack = true;
					}
				}
			}
			break;
					   }
		case BERSERKER: case BERSERKERGM:
			{
				int32 dmg = 1 + (GetSkill(FRENZY) / 100);

				switch (GetAA(aaBlurofAxes)) {
					case 1:
						dmg *= 1.15;
						break;
					case 2:
						dmg *= 1.30;
						break;
					case 3:
						dmg *= 1.50;
						break;
				}

				switch (GetAA(aaViciousFrenzy)) {
					case 1:
						dmg *= 1.05;
						break;
					case 2:
						dmg *= 1.10;
						break;
					case 3:
						dmg *= 1.15;
						break;
					case 4:
						dmg *= 1.20;
						break;
					case 5:
						dmg *= 1.25;
						break;
				}

				while(dmg > 0 && target) 
				{
					if(Attack(target))
						dmg--;
					else
						dmg = 0;
				}
				reuse = FrenzyReuseTime * 1000;
				did_attack = true;
				break;
			}
		case RANGER: case RANGERGM:
		case BEASTLORD: case BEASTLORDGM: {
			//kick
			if(level >= RuleI(Combat, NPCBashKickLevel)){
				DoAnim(animKick);
				sint32 dmg = 0;

				if(GetWeaponDamage(GetTarget(), m_inv.GetItem(SLOT_FEET)) <= 0){
					dmg = -5;
				}
				else{
					if(target->CheckHitChance(this, KICK, 0)) {
						if(RuleB(Combat, UseIntervalAC))
							dmg = GetKickDamage();
						else
							dmg = MakeRandomInt(1, GetKickDamage());
					}
				}

				DoSpecialAttackDamage(target, KICK, dmg);
				reuse = KickReuseTime * 1000;
				did_attack = true;
			}
			break;
						}
		case CLERIC: case CLERICGM: //clerics can bash too.
		case SHADOWKNIGHT: case SHADOWKNIGHTGM:
		case PALADIN: case PALADINGM:
			{
				if(level >= RuleI(Combat, NPCBashKickLevel)){
					if((GetRace() == OGRE || GetRace() == TROLL || GetRace() == BARBARIAN) // Racial Slam
						|| (m_inv.GetItem(SLOT_SECONDARY) && m_inv.GetItem(SLOT_SECONDARY)->GetItem()->ItemType == ItemTypeShield)  //Using Shield
						|| (m_inv.GetItem(SLOT_PRIMARY) && (m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HS 
							|| m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HB 
							|| m_inv.GetItem(SLOT_PRIMARY)->GetItem()->ItemType == ItemType2HPierce) 
							&& GetAA(aa2HandBash) >= 1)) {   //Using 2 hand weapon, but has AA 2 Hand Bash
						DoAnim(animTailRake);
						sint32 dmg = 0;

						if(GetWeaponDamage(GetTarget(), m_inv.GetItem(SLOT_SECONDARY)) <= 0){	
							dmg = -5;
						}
						else{
							if(target->CheckHitChance(this, BASH, 0)) {
								if(RuleB(Combat, UseIntervalAC))
									dmg = GetBashDamage();
								else
									dmg = MakeRandomInt(1, GetBashDamage());
							}
						}

						DoSpecialAttackDamage(target, BASH, dmg);
						reuse = BashReuseTime * 1000;
						did_attack = true;
					}
				}
				break;
			}
	}

	classattack_timer.Start(reuse*HasteModifier/100);
}

bool Bot::TryHeadShot(Mob* defender, SkillType skillInUse) {
	bool Result = false;

	if(defender && (defender->GetBodyType() == BT_Humanoid) && (skillInUse == ARCHERY) && (GetClass() == RANGER) && (GetLevel() >= 62)) {
		int defenderLevel = defender->GetLevel();
		int rangerLevel = GetLevel();
		// Bot Ranger Headshot AA through level 85(Underfoot)
		if( GetAA(aaHeadshot) && ((defenderLevel - 46) <= GetAA(aaHeadshot) * 2) ) {
			// WildcardX: These chance formula's below are arbitrary. If someone has a better formula that is more
			// consistent with live, feel free to update these.
			float AttackerChance = 0.20f + ((float)(rangerLevel - 51) * 0.005f);
			float DefenderChance = (float)MakeRandomFloat(0.00f, 1.00f);
			if(AttackerChance > DefenderChance) {
				mlog(COMBAT__ATTACKS, "Landed a headshot: Attacker chance was %f and Defender chance was %f.", AttackerChance, DefenderChance);
				// WildcardX: At the time I wrote this, there wasnt a string id for something like HEADSHOT_BLOW
				//entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, FINISHING_BLOW, GetName());
				entity_list.MessageClose(this, false, 200, MT_CritMelee, "%s has scored a leathal HEADSHOT!", GetName());
				defender->Damage(this, (defender->GetMaxHP()+50), SPELL_UNKNOWN, skillInUse);
				Result = true;
			}
			else {
				mlog(COMBAT__ATTACKS, "FAILED a headshot: Attacker chance was %f and Defender chance was %f.", AttackerChance, DefenderChance);
			}
		}
	}

	return Result;
}

//offensive spell aggro
sint32 Bot::CheckAggroAmount(int16 spellid) {
	sint32 AggroAmount = Mob::CheckAggroAmount(spellid);
	
	switch (GetAA(aaSpellCastingSubtlety)) {
		case 1:
			AggroAmount = AggroAmount * 95 / 100;
			break;
		case 2:
			AggroAmount = AggroAmount * 90 / 100;
			break;
		case 3:
			AggroAmount = AggroAmount * 80 / 100;
			break;
	}

	sint32 focusAggro = GetBotFocusEffect(BotfocusHateReduction, spellid);
	AggroAmount = (AggroAmount * (100+focusAggro) / 100);

	return AggroAmount;
}

sint32 Bot::CheckHealAggroAmount(int16 spellid, int32 heal_possible) {
	sint32 AggroAmount = Mob::CheckHealAggroAmount(spellid, heal_possible);

	sint32 focusAggro = GetBotFocusEffect(BotfocusHateReduction, spellid);
	
	AggroAmount = (AggroAmount * (100 + focusAggro) / 100);

	return AggroAmount;
}

void Bot::MakePet(int16 spell_id, const char* pettype, const char *petname) {
	Mob::MakePet(spell_id, pettype, petname);
}

void Bot::AI_Stop() {
	NPC::AI_Stop();
	Mob::AI_Stop();
}

//this is called with 'this' as the mob being looked at, and
//iOther the mob who is doing the looking. It should figure out
//what iOther thinks about 'this'
FACTION_VALUE Bot::GetReverseFactionCon(Mob* iOther) {
#if FACTIONS_DEBUG >= 20
	LogFile->write(EQEMuLog::Debug, "called N $s::GetReverseFactionCon(%s)", GetName(), iOther->GetName());
#endif
	
	_ZP(Bot_GetReverseFactionCon);

	if(iOther->IsBot()) {
		return FACTION_ALLY;
	}

	return NPC::GetReverseFactionCon(iOther);
}

Mob* Bot::GetOwnerOrSelf() {
	Mob* Result = 0;

	if(this->GetBotOwner())
		Result = GetBotOwner();
	else
		Result = this;

	return Result;
}

Mob* Bot::GetOwner() {
	Mob* Result = 0;

	Result = GetBotOwner();

	if(!Result) {
		this->SetBotOwner(0);
	}

	return Result;
}

bool Bot::IsBotAttackAllowed(Mob* attacker, Mob* target, bool& hasRuleDefined)
{
	bool Result = false;
	
	if(attacker && target)
	{
		if(attacker->IsClient() && target->IsBot() && attacker->CastToClient()->GetPVP() && target->CastToBot()->GetBotOwner()->CastToClient()->GetPVP())
		{
			hasRuleDefined = true;
			Result = true;
		}
		else if(attacker->IsClient() && target->IsBot())
		{
			hasRuleDefined = true;
			Result = false;
		}
		else if(attacker->IsBot() && target->IsNPC())
		{
			hasRuleDefined = true;
			Result = true;
		}
		else if(attacker->IsBot() && !target->IsNPC())
		{
			hasRuleDefined = true;
			Result = false;
		}
		else if(attacker->IsPet() && attacker->IsFamiliar())
		{
			hasRuleDefined = true;
			Result = false;
		}
		else if(attacker->IsBot() && attacker->CastToBot()->GetBotOwner() && attacker->CastToBot()->GetBotOwner()->CastToClient()->GetPVP())
		{
			if(target->IsBot() && target->GetOwner() && target->GetOwner()->CastToClient()->GetPVP())
			{
				// my target is a bot and it's owner is pvp
				hasRuleDefined = true;

				if(target->GetOwner() == attacker->GetOwner())
				{
					// no attacking if my target's owner is my owner
					Result = false;
				}
				else
				{
					Result = true;
				}
			}
			else if(target->IsClient() && target->CastToClient()->GetPVP())
			{
				// my target is a player and it's pvp
				hasRuleDefined = true;

				if(target == attacker->GetOwner())
				{
					// my target cannot be my owner
					Result = false;
				}
				else
				{
					Result = true;
				}
			}
			else if(target->IsNPC())
			{
				hasRuleDefined = true;
				Result = true;
			}
			else if(!target->IsNPC())
			{
				hasRuleDefined = true;
				Result = false;
			}
		}
	}

	return Result;
}

void Bot::EquipBot(std::string* errorMessage) {
	GetBotItems(errorMessage, m_inv);

	const ItemInst* inst = 0;
	const Item_Struct* item = 0;
	for(int i=0; i<=21; ++i) {
		inst = GetBotItem(i);
		if(inst) {
			item = inst->GetItem();
			BotTradeAddItem(inst->GetID(), inst, inst->GetCharges(), item->Slots, i, errorMessage, false);
			if(!errorMessage->empty())
				return;
		}
	}
}

//// This method is meant to be called by zone or client methods to clean up objects when a client camps, goes LD, zones out or something like that.
//void Bot::DestroyBotRaidObjects(Client* client) {
//	if(client) {
//		if(client->GetBotRaidID() > 0) {
//			BotRaids* br = entity_list.GetBotRaidByMob(client);
//			if(br) {
//				br->RemoveRaidBots();
//				br = NULL;
//			}
//		}
//
//		//BotOrderCampAll(client);
//	}
//}

// Orders all the bots owned by the specified client bot owner to camp out of the game
void Bot::BotOrderCampAll(Client* c) {
	if(c) {
		std::list<Bot*> BotList = entity_list.GetBotsByBotOwnerCharacterID(c->CharacterID());

		for(std::list<Bot*>::iterator botListItr = BotList.begin(); botListItr != BotList.end(); botListItr++) {
			(*botListItr)->Camp();
		}
	}
}

void Bot::ProcessBotOwnerRefDelete(Mob* botOwner) {
	if(botOwner) {
		if(botOwner->IsClient()) {
			std::list<Bot*> BotList = entity_list.GetBotsByBotOwnerCharacterID(botOwner->CastToClient()->CharacterID());

			if(!BotList.empty()) {
				for(std::list<Bot*>::iterator botListItr = BotList.begin(); botListItr != BotList.end(); botListItr++) {
					Bot* tempBot = *botListItr;

					if(tempBot) {
						tempBot->SetTarget(0);
						tempBot->SetBotOwner(0);
					}
				}
			}
		}
	}
}

void Bot::ProcessGuildInvite(Client* guildOfficer, Bot* botToGuild) {
	if(guildOfficer && botToGuild) {
		// Bots can be only guild member rank
		if(!botToGuild->IsInAGuild()) {
			//they are not in this or any other guild, this is an invite
			if (!guild_mgr.CheckPermission(guildOfficer->GuildID(), guildOfficer->GuildRank(), GUILD_INVITE)) {
				guildOfficer->Message(13, "You dont have permission to invite.");
				return;
			}

			// mlog(GUILDS__ACTIONS, "Inviting %s (%d) into guild %s (%d)", botToGuild->GetName(), botToGuild->GetBotID(), guild_mgr.GetGuildName(client->GuildID()), client->GuildID());

			SetBotGuildMembership(botToGuild->GetBotID(), guildOfficer->GuildID(), GUILD_MEMBER);

			//_log(GUILDS__REFRESH, "Sending char refresh for BOT %s from guild %d to world", botToGuild->GetName(), guildOfficer->GuildID();

			ServerPacket* pack = new ServerPacket(ServerOP_GuildCharRefresh, sizeof(ServerGuildCharRefresh_Struct));
			ServerGuildCharRefresh_Struct *s = (ServerGuildCharRefresh_Struct *) pack->pBuffer;
			s->guild_id = guildOfficer->GuildID();
			s->old_guild_id = GUILD_NONE;
			s->char_id = botToGuild->GetBotID();
			worldserver.SendPacket(pack);
			safe_delete(pack);

		} else {
			//they are in some other guild
			guildOfficer->Message(13, "Player is in a guild.");
			return;
		}
	}
}

bool Bot::ProcessGuildRemoval(Client* guildOfficer, std::string botName) {
	bool Result = false;

	if(guildOfficer && !botName.empty()) {
		Bot* botToUnGuild = entity_list.GetBotByBotName(botName);
		if(botToUnGuild) {
			SetBotGuildMembership(botToUnGuild->GetBotID(), 0, 0);
			Result = true;
		}
		else {
			uint32 botId = GetBotIDByBotName(botName);

			if(botId > 0) {
				// Bot is camped or in another zone
				SetBotGuildMembership(botId, 0, 0);
				Result = true;
			}
		}

		if(Result) {
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_GuildManageRemove, sizeof(GuildManageRemove_Struct));
			GuildManageRemove_Struct* gm = (GuildManageRemove_Struct*) outapp->pBuffer;
			gm->guildeqid = guildOfficer->GuildID();
			strcpy(gm->member, botName.c_str());
			guildOfficer->Message(0, "%s successfully removed from your guild.", botName.c_str());
			entity_list.QueueClientsGuild(guildOfficer, outapp, false, gm->guildeqid);
			safe_delete(outapp);
		}
	}

	return Result;
}

void Bot::SetBotGuildMembership(int32 botId, int32 guildid, int8 rank) {
	if(botId > 0) {
		std::string errorMessage;
		char errbuf[MYSQL_ERRMSG_SIZE];
		char *query = 0;
		
		if(guildid > 0) {
			if(!database.RunQuery(query, MakeAnyLenString(&query, "REPLACE INTO botguildmembers SET char_id = %u, guild_id = %u, rank = %u;", botId, guildid, rank), errbuf)) {
				errorMessage = std::string(errbuf);
			}
		}
		else {
			if(!database.RunQuery(query, MakeAnyLenString(&query, "DELETE FROM botguildmembers WHERE char_id = %u;", botId), errbuf)) {
				errorMessage = std::string(errbuf);
			}
		}

		safe_delete_array(query);

		if(!errorMessage.empty()) {
			// TODO: Log this error message to the zone error log
		}
	}
}

void Bot::LoadGuildMembership(int32* guildId, int8* guildRank, std::string* guildName) {
	if(guildId && guildRank && guildName) {
		std::string errorMessage;
		char* Query = 0;
		char TempErrorMessageBuffer[MYSQL_ERRMSG_SIZE];
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;

		if(!database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT gm.guild_id, gm.rank, g.name FROM vwGuildMembers AS gm JOIN guilds AS g ON gm.guild_id = g.id WHERE gm.char_id = %u AND gm.mobtype = 'B';", GetBotID()), TempErrorMessageBuffer, &DatasetResult)) {
			errorMessage = std::string(TempErrorMessageBuffer);
		}
		else {
			while(DataRow = mysql_fetch_row(DatasetResult)) {
				*guildId = atoi(DataRow[0]);
				*guildRank = atoi(DataRow[1]);
				*guildName = std::string(DataRow[2]);
				break;
			}

			mysql_free_result(DatasetResult);
		}

		safe_delete(Query);

		if(!errorMessage.empty()) {
			// TODO: Record this error message to zone error log
		}
	}
}

sint32 Bot::CalcMaxMana() {
	switch(GetCasterClass())
	{
		case 'I': 
		case 'W':
		{
			max_mana = (GenerateBaseManaPoints() + itembonuses.Mana + spellbonuses.Mana + GroupLeadershipAAManaEnhancement());
			break;
		}
		case 'N':
		{
			max_mana = 0;
			break;
		}
		default:
		{
			LogFile->write(EQEMuLog::Debug, "Invalid Class '%c' in CalcMaxMana", GetCasterClass());
			max_mana = 0;
			break;
		}
	}

	if(cur_mana > max_mana) {
		cur_mana = max_mana;
	}
	else if(max_mana < 0) {
		max_mana = 0;
	}

	return max_mana;
}

void Bot::SetAttackTimer() {
	float PermaHaste;
	if(GetHaste() > 0)
		PermaHaste = 1 / (1 + (float)GetHaste()/100);
	else if(GetHaste() < 0)
		PermaHaste = 1 * (1 - (float)GetHaste()/100);
	else
		PermaHaste = 1.0f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetAtTrigger(4000, true);

	Timer* TimerToUse = NULL;
	const Item_Struct* PrimaryWeapon = NULL;

	for (int i=SLOT_RANGE; i<=SLOT_SECONDARY; i++) {

		//pick a timer
		if (i == SLOT_PRIMARY)
			TimerToUse = &attack_timer;
		else if (i == SLOT_RANGE)
			TimerToUse = &ranged_timer;
		else if(i == SLOT_SECONDARY)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		const Item_Struct* ItemToUse = NULL;		
		ItemInst* ci = GetBotItem(i);
		if(ci)
			ItemToUse = ci->GetItem();

		//special offhand stuff
		if(i == SLOT_SECONDARY) {
			//if we have a 2H weapon in our main hand, no dual
			if(PrimaryWeapon != NULL) {
				if(	PrimaryWeapon->ItemClass == ItemClassCommon
					&& (PrimaryWeapon->ItemType == ItemType2HS
					||	PrimaryWeapon->ItemType == ItemType2HB
					||	PrimaryWeapon->ItemType == ItemType2HPierce)) {
						attack_dw_timer.Disable();
						continue;
				}
			}

			//clients must have the skill to use it...
			if(!GetSkill(DUAL_WIELD)) {
				attack_dw_timer.Disable();
				continue;
			}
		}

		//see if we have a valid weapon
		if(ItemToUse != NULL) {
			//check type and damage/delay
			if(ItemToUse->ItemClass != ItemClassCommon 
				|| ItemToUse->Damage == 0 
				|| ItemToUse->Delay == 0) {
					//no weapon
					ItemToUse = NULL;
			}
			// Check to see if skill is valid
			else if((ItemToUse->ItemType > ItemTypeThrowing) && (ItemToUse->ItemType != ItemTypeHand2Hand) && (ItemToUse->ItemType != ItemType2HPierce)) {
				//no weapon
				ItemToUse = NULL;
			}
		}

		//if we have no weapon..
		if (ItemToUse == NULL) {
			//above checks ensure ranged weapons do not fall into here
			// Work out if we're a monk
			if ((GetClass() == MONK) || (GetClass() == BEASTLORD)) {
				//we are a monk, use special delay
				int speed = (int)(GetMonkHandToHandDelay()*(100.0f+attack_speed)*PermaHaste);
				// 1200 seemed too much, with delay 10 weapons available
				if(speed < 800)	//lower bound
					speed = 800;
				TimerToUse->SetAtTrigger(speed, true);	// Hand to hand, delay based on level or epic
			} else {
				//not a monk... using fist, regular delay
				int speed = (int)(36*(100.0f+attack_speed)*PermaHaste);
				//if(speed < 800 && IsClient())	//lower bound
				//	speed = 800;
				TimerToUse->SetAtTrigger(speed, true); 	// Hand to hand, non-monk 2/36
			}
		} else {
			//we have a weapon, use its delay
			// Convert weapon delay to timer resolution (milliseconds)
			//delay * 100
			int speed = (int)(ItemToUse->Delay*(100.0f+attack_speed)*PermaHaste);
			if(speed < 800)
				speed = 800;

			if(ItemToUse && (ItemToUse->ItemType == ItemTypeBow || ItemToUse->ItemType == ItemTypeThrowing))
			{
				/*if(IsClient())
				{
					float max_quiver = 0;
					for(int r = SLOT_PERSONAL_BEGIN; r <= SLOT_PERSONAL_END; r++) 
					{
						const ItemInst *pi = CastToClient()->GetInv().GetItem(r);
						if(!pi)
							continue;
						if(pi->IsType(ItemClassContainer) && pi->GetItem()->BagType == bagTypeQuiver)
						{
							float temp_wr = (pi->GetItem()->BagWR / 3);
							if(temp_wr > max_quiver)
							{
								max_quiver = temp_wr;
							}
						}
					}
					if(max_quiver > 0)
					{
						float quiver_haste = 1 / (1 + max_quiver / 100);
						speed *= quiver_haste;
					}
				}*/
			}
			TimerToUse->SetAtTrigger(speed, true);
		}

		if(i == SLOT_PRIMARY)
			PrimaryWeapon = ItemToUse;
	}	
}

sint32 Bot::GetActSpellDamage(int16 spell_id, sint32 value) {
	sint32 modifier = 100;
	int8 casterClass = GetClass();
	int8 casterLevel = GetLevel();

	//Dunno if this makes sense:
	if (spells[spell_id].resisttype > 0)
		modifier += 5;


	int tt = spells[spell_id].targettype;
	if (tt == ST_UndeadAE || tt == ST_Undead || tt == ST_Summoned) {
		//undead/summoned spells
		modifier += 10;
	} else {
		//damage spells.
		modifier += 5;
	}

	//these spell IDs could be wrong
	if (spell_id == SPELL_LEECH_TOUCH) {	//leech touch
		value -= GetAA(aaConsumptionoftheSoul) * 200; //Consumption of the Soul
	}
	if (spell_id == SPELL_IMP_HARM_TOUCH) {	//harm touch
		modifier += 450*GetAA(aaUnholyTouch);
	}

	//spell crits, dont make sense if cast on self.
	if(tt != ST_Self) {
		int chance = RuleI(Spells, BaseCritChance) + 1;
		sint32 ratio = RuleI(Spells, BaseCritRatio);

		//here's an idea instead of bloating code with unused cases there's this thing called:
		//case 'default'
		switch(casterClass)
		{
		case WIZARD:
			{
				if (casterLevel >= RuleI(Spells, WizCritLevel)) {
					chance += RuleI(Spells, WizCritChance);
					ratio += RuleI(Spells, WizCritRatio);
				}
				break;
			}

		default: 
			break;
		}

		if((casterClass == MONK) || (casterClass == ROGUE) || (casterClass == WARRIOR) || (casterClass == BERSERKER)) {
			ratio += (25 + 25*GetAA(aaIngenuity));
			chance += GetAA(aaIngenuity);
		}

		if((casterClass != WARRIOR) && (casterClass != ROGUE) && (casterClass != MONK) && (casterClass != BERSERKER)) {

			switch (GetAA(aaFuryofMagic)) {
				case 1:
					chance += 2;
					break;
				case 2:
					chance += 4;
					break;
				case 3:
					chance += 7;
					break;
				case 4:    
					chance += 10;
					break;
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
					chance += 10 + (GetAA(aaFuryofMagic) - 4) * 2;
					break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
					chance += 20 + (GetAA(aaFuryofMagic) - 9);
					break;
			}
		}

		if (spell_id == SPELL_IMP_HARM_TOUCH) {
			if ( (GetAA(aaSpellCastingFury) > 0) && (GetAA(aaUnholyTouch) > 0) )
				chance = 100;
		} 

		if(tt == ST_Tap) {

			if(spells[spell_id].classes[SHADOWKNIGHT-1] >= 254 && spell_id != SPELL_LEECH_TOUCH) {
				if(ratio < 100)	//chance increase and ratio are made up, not confirmed
					ratio = 100;

				if(casterClass == SHADOWKNIGHT) {
					modifier += 100*GetAA(aaSoulAbrasion);
				}
			}
		}

		//crit damage modifiers
		if (casterClass == WIZARD) { //wizards get an additional bonus
			ratio += 8*GetAA(aaDestructiveFury);
		}
		else {
			ratio += 5*GetAA(aaDestructiveFury);
		}

		if (chance > 0) {
			mlog(SPELLS__CRITS, "Attempting spell crit. Spell: %s (%d), Value: %d, Modifier: %d, Chance: %d, Ratio: %d", spells[spell_id].name, spell_id, value, modifier, chance, ratio);
			if(MakeRandomInt(1,100) <= chance) {
				modifier += modifier*ratio/100;
				mlog(SPELLS__CRITS, "Spell crit successful. Final damage modifier: %d, Final Damage: %d", modifier, (value * modifier) / 100);
				entity_list.MessageClose(this, false, 100, MT_SpellCrits, "%s delivers a critical blast! (%d)", GetName(), ((-value * modifier) / 100));	
			} else 
				mlog(SPELLS__CRITS, "Spell crit failed. Final Damage Modifier: %d, Final Damage: %d", modifier, (value * modifier) / 100);
		}
	}

	return (value * modifier) / 100;
}

sint32 Bot::GetActSpellHealing(int16 spell_id, sint32 value) {
	sint32 modifier = 100;

	modifier += GetBotFocusEffect(BotfocusImprovedHeal, spell_id);

	if(spells[spell_id].buffduration < 1) {
		uint8 botlevel = GetLevel();
		int8 botclass = GetClass();
		int chance = 0;

		if((botclass == BEASTLORD)||(botclass == CLERIC)||(botclass == DRUID)||(botclass == PALADIN)||(botclass == RANGER)||(botclass == SHAMAN)) {
			switch (GetAA(aaHealingAdept)) {
				case 1:
					modifier += 2;
					break;
				case 2:
					modifier += 5;
					break;
				case 3:
					modifier += 10;
					break;
				case 4:    // Advanced Healing Adept AA
					modifier += 13;
					break;
				case 5:
					modifier += 16;
					break;
				case 6:
					modifier += 19;
					break;
			}

			switch (GetAA(aaHealingGift)) {
				case 1:
					chance += 3;
					break;
				case 2:
					chance += 6;
					break;
				case 3:
					chance += 10;
					break;
				case 4:    // Advanced Healing Gift AA
					chance += 12;
					break;
				case 5:
					chance += 14;
					break;
				case 6:
					chance += 16;
					break;
			}
		}

		if((botclass == NECROMANCER)||(botclass == SHADOWKNIGHT)) {
			if(spells[spell_id].targettype == ST_Tap) {

				switch (GetAA(aaTheftofLife)) {
					case 1:
						chance += 2;
						break;
					case 2:
						chance += 5;
						break;
					case 3:
						chance += 10;
						break;
					case 4:    // Advanced Theft of Life AA
						chance += 13;
						break;
					case 5:
						chance += 16;
						break;
					case 6:    // Soul Thief AA
						chance += 18;
						break;
					case 7:
						chance += 20;
						break;
					case 8:
						chance += 22;
						break;
				}
			}
		}

		if(MakeRandomInt(1,100) < chance) {
			entity_list.MessageClose(this, false, 100, MT_SpellCrits, "%s performs an exceptional heal! (%d)", GetName(), ((value * modifier) / 50));		
			return (value * modifier) / 50;
		}
		else{
			return (value * modifier) / 100;
		}		
	}

	return (value * modifier) / 100;
}

sint32 Bot::GetActSpellCasttime(int16 spell_id, sint32 casttime) {
	sint32 cast_reducer = 0;
	cast_reducer += GetBotFocusEffect(BotfocusSpellHaste, spell_id);

	uint8 botlevel = GetLevel();
	int8 botclass = GetClass();

	if (botlevel >= 51 && casttime >= 3000 && !BeneficialSpell(spell_id) 
		&& (botclass == SHADOWKNIGHT || botclass == RANGER || botclass == PALADIN || botclass == BEASTLORD ))
		cast_reducer += (GetLevel()-50)*3;

	if((casttime >= 4000) && BeneficialSpell(spell_id) && (CalcBuffDuration(this, this, spell_id) > 0)) {
		switch (GetAA(aaSpellCastingDeftness)) {
			case 1:
				cast_reducer += 5;
				break;
			case 2:
				cast_reducer += 10;
				break;
			case 3:
				cast_reducer += 25;
				break;
		}

		switch (GetAA(aaQuickBuff)) {
			case 1:
				cast_reducer += 10;
				break;
			case 2:
				cast_reducer += 25;
				break;
			case 3:
				cast_reducer += 50;
				break;
		}
	}

	if(IsSummonSpell(spell_id)) {
		switch (GetAA(aaQuickSummoning)) {
			case 1:
				cast_reducer += 10;
				break;
			case 2:
				cast_reducer += 25;
				break;
			case 3:
				cast_reducer += 50;
				break;
		}
	}

	if(IsEvacSpell(spell_id)) {
		switch (GetAA(aaQuickEvacuation)) {
			case 1:
				cast_reducer += 10;
				break;
			case 2:
				cast_reducer += 25;
				break;
			case 3:
				cast_reducer += 50;
				break;
		}
	}

	if(IsDamageSpell(spell_id) && spells[spell_id].cast_time >= 4000) {
		switch (GetAA(aaQuickDamage)) {
			case 1:
				cast_reducer += 2;
				break;
			case 2:
				cast_reducer += 5;
				break;
			case 3:
				cast_reducer += 10;
				break;
		}
	}
	if (cast_reducer > 50)
		cast_reducer = 50;	//is this just an arbitrary limit?

	casttime = (casttime*(100 - cast_reducer)/100);

	return casttime;
}

sint32 Bot::GetActSpellCost(int16 spell_id, sint32 cost) {
	sint32 Result = 0;

	if(GetClass() == WIZARD || GetClass() == ENCHANTER || GetClass() == MAGICIAN || GetClass() == NECROMANCER || GetClass() == DRUID || GetClass() == SHAMAN || GetClass() == CLERIC || GetClass() == BARD) {
		// This formula was derived from the following resource:
		// http://www.eqsummoners.com/eq1/specialization-library.html
		// WildcardX
		float PercentManaReduction = 0;
		float PercentOfMaxSpecializeSkill = 0;
		float MaxSpecilizationSkillAllowed = GetSkill(spells[spell_id].skill);
		float SpecializeSkill = GetSpecializeSkillValue(spell_id);
		int SuccessChance = MakeRandomInt(1, 100);

		if(MaxSpecilizationSkillAllowed > 0)
			PercentOfMaxSpecializeSkill = SpecializeSkill / MaxSpecilizationSkillAllowed;

		if(SuccessChance <= (PercentOfMaxSpecializeSkill * 100))
			PercentManaReduction = (SpecializeSkill * .053) - 5.65;

		switch (GetAA(aaSpellCastingMastery)) {
			case 1:
				PercentManaReduction += 5;
				break;
			case 2:
				PercentManaReduction += 15;
				break;
			case 3:
				PercentManaReduction += 30;
				break;
		}

		PercentManaReduction += GetBotFocusEffect(BotfocusManaCost, spell_id);
		cost -= (cost * (PercentManaReduction / 100));
	}

	if(cost < 0)
		cost = 0;

	Result = cost;

	return Result;
}

float Bot::GetActSpellRange(int16 spell_id, float range) {
	float extrange = 100;
	extrange += GetBotFocusEffect(BotfocusRange, spell_id);
	return (range * extrange) / 100;
}

sint32 Bot::GetActSpellDuration(int16 spell_id, sint32 duration) {
	int increase = 100;
	increase += GetBotFocusEffect(BotfocusSpellDuration, spell_id);

	switch (GetAA(aaSpellCastingReinforcement)) {
		case 1:
			increase += 5;
			break;
		case 2:
			increase += 15;
			break;
		case 3:
			increase += 30;
			break;
	}

	if (GetAA(aaSpellCastingReinforcementMastery)) 
		increase += 20;

	return (duration * increase) / 100;
}

float Bot::GetAOERange(uint16 spell_id) {
	float range = 0;
	
	range = Mob::GetAOERange(spell_id);

	if(spell_id > 0) {
		float mod = 0;
		if(IsBardSong(spell_id)) {

			switch (GetAA(aaExtendedNotes)) {
				case 1:
					mod += range * 0.10;
					break;
				case 2:
					mod += range * 0.15;
					break;
				case 3:
					mod += range * 0.25;
					break;
			}

			switch (GetAA(aaSionachiesCrescendo)) {
				case 1:
					mod += range * 0.05;
					break;
				case 2:
					mod += range * 0.10;
					break;
				case 3:
					mod += range * 0.15;
					break;
			}

			range += mod;
		}

		range = GetActSpellRange(spell_id, range);
	}

	return range;
}

bool Bot::SpellEffect(Mob* caster, int16 spell_id, float partial) {
	bool Result = false;
	
	Result = Mob::SpellEffect(caster, spell_id, partial);

    // Franck-add: If healed/doted, a bot must show its new HP to its leader
	if(IsGrouped()) {
		Group *g = GetGroup();
		if(g) {
			EQApplicationPacket hp_app;
			CreateHPPacket(&hp_app);
			for(int i=0; i<MAX_GROUP_MEMBERS; i++) {
				if(g->members[i] && g->members[i]->IsClient()) {
					g->members[i]->CastToClient()->QueuePacket(&hp_app);
				}
			}
		}
	}

	return Result;
}

void Bot::DoBuffTic(int16 spell_id, int32 ticsremaining, int8 caster_level, Mob* caster) {
	Mob::DoBuffTic(spell_id, ticsremaining, caster_level, caster);
}

bool Bot::CastSpell(int16 spell_id, int16 target_id, int16 slot, sint32 cast_time, sint32 mana_cost, int32* oSpellWillFinish, int32 item_slot, sint16 *resist_adjust) {
	bool Result = false;

	if(zone && !zone->IsSpellBlocked(spell_id, GetX(), GetY(), GetZ())) {
		_ZP(Bot_CastSpell);

		mlog(SPELLS__CASTING, "CastSpell called for spell %s (%d) on entity %d, slot %d, time %d, mana %d, from item slot %d",
			spells[spell_id].name, spell_id, target_id, slot, cast_time, mana_cost, (item_slot==0xFFFFFFFF)?999:item_slot);

		if(casting_spell_id == spell_id)
			ZeroCastingVars();

		if(GetClass() != BARD) {
			if(!IsValidSpell(spell_id) || casting_spell_id || delaytimer || spellend_timer.Enabled() || IsStunned() || IsFeared() || IsMezzed() || (IsSilenced() && !IsDiscipline(spell_id)) || (IsAmnesiad() && IsDiscipline(spell_id))) {
				mlog(SPELLS__CASTING_ERR, "Spell casting canceled: not able to cast now. Valid? %d, casting %d, waiting? %d, spellend? %d, stunned? %d, feared? %d, mezed? %d, silenced? %d",
					IsValidSpell(spell_id), casting_spell_id, delaytimer, spellend_timer.Enabled(), IsStunned(), IsFeared(), IsMezzed(), IsSilenced() );
				if(IsSilenced() && !IsDiscipline(spell_id))
					Message_StringID(13, SILENCED_STRING);
				if(IsAmnesiad() && IsDiscipline(spell_id))
					Message_StringID(13, MELEE_SILENCE);
				if(casting_spell_id)
					AI_Event_SpellCastFinished(false, casting_spell_slot);
				return(false);
			}
		}

		if(IsDetrimentalSpell(spell_id) && !zone->CanDoCombat()){
			Message_StringID(13, SPELL_WOULDNT_HOLD);
			if(casting_spell_id)
				AI_Event_SpellCastFinished(false, casting_spell_slot);
			return(false);
		}

		//cannot cast under deivne aura
		if(DivineAura()) {
			mlog(SPELLS__CASTING_ERR, "Spell casting canceled: cannot cast while Divine Aura is in effect.");
			InterruptSpell(173, 0x121, false);
			return(false);
		}

		// check for fizzle
		// note that CheckFizzle itself doesn't let NPCs fizzle,
		// but this code allows for it.
		if(slot < MAX_PP_MEMSPELL && !CheckFizzle(spell_id))
		{
			int fizzle_msg = IsBardSong(spell_id) ? MISS_NOTE : SPELL_FIZZLE;
			InterruptSpell(fizzle_msg, 0x121, spell_id);

			uint32 use_mana = ((spells[spell_id].mana) / 4);
			mlog(SPELLS__CASTING_ERR, "Spell casting canceled: fizzled. %d mana has been consumed", use_mana);

			// fizzle 1/4 the mana away
			SetMana(GetMana() - use_mana);
			return(false);
		}

		if (HasActiveSong()) {
			mlog(SPELLS__BARDS, "Casting a new spell/song while singing a song. Killing old song %d.", bardsong);
			//Note: this does NOT tell the client
			//_StopSong();
			bardsong = 0;
			bardsong_target_id = 0;
			bardsong_slot = 0;
			bardsong_timer.Disable();
		}

		Result = DoCastSpell(spell_id, target_id, slot, cast_time, mana_cost, oSpellWillFinish, item_slot);
	}

	return Result;
}

bool Bot::SpellOnTarget(int16 spell_id, Mob* spelltar) {
	bool Result = false;

	if(!IsValidSpell(spell_id))
		return false;

	if(spelltar) {
		if(spelltar->IsBot() && (spells[spell_id].targettype == ST_GroupTeleport)) {
			// So I made this check because teleporting a group of bots tended to crash the zone
			// It seems several group spells also show up as ST_GroupTeleport for some
			// reason so I now have to check by spell id. These appear to be Group v1 spells and
			// Heal over Time spells.
			switch(spell_id) {
				// Paladin
			case 3577: // Wave of Life
			case 4065: // Blessing of Austerity
			case 1455: // Wave of Healing
			case 2589: // Healing Wave of Prexus
			case 3427: // Wave of Marr
			case 3683: // Ethereal Cleansing
			case 1283: // Celestial Cleansing
			case 3485: // Supernal Cleansing
			case 5293: // Pious Cleansing
			case 4893: // Wave of Trushar
			case 5295: // Jeron's Mark
			case 5296: // Wave of Piety
				// Bard
			case 4085: // Forpar's Aria of Affliction
			case 4083: // Rizlona's Embers
			case 4086: // Forpar's Psalm of Pain
			case 4084: // Rizlona's Fire
			case 6734: // Song of the Storm
			case 3651: // Wind of Marr
			case 4087: // Forpar's Verse of Venom
			case 3362: // Rizlona's Call of Flame
			case 4112: // Call of the Muse
			case 4872: // Echo of the Trusik
			case 4873: // Dark Echo
			case 5377: // Cantata of Life
			case 5380: // Yelhun's Mystic Call
			case 5382: // Eriki's Psalm of Power
			case 6666: // Storm Blade
			case 5388: // Ancient Call of Power
				// Cleric
			case 134: // Word of Health
			case 136: // Word of Healing
			case 1520: // Word of Vigor
			case 1521: // Word of Restoration
			case 1523: // Word of Redemption
			case 3471: // Word of Replenishment
			case 5270: // Word of Vivification
			case 2502: // Celestial Remedy
			case 2175: // Celestial Health
			case 1444: // Celestial Healing
			case 1522: // Celestial Elixir
			case 2180: // Etherial Elixir
			case 3047: // Kazad's Mark
			case 3475: // Supernal Elixir
			case 4053: // Blessing of Temperance
			case 4108: // Aura of Reverence
			case 4882: // Holy Elixir
			case 5259: // Pious Elixir
			case 5272: // Aura of Devotion
			case 5277: // Balikor's Mark
				// Enchanter
			case 5517: // Circle of Alendar
			case 6671: // Rune of Rikkukin
			case 6739: // Rune of the Scale
				// Shaman
			case 2521: // Talisman of the Beast
			case 4055: // Pack Shrew
			case 3842: // Blood of Nadox
			case 5417: // Champion
				// Druid
			case 4058: // Feral Pack
			case 2520: // Natures Recovery
				break;
			default:
				return false;
			}
		}

		//Franck-add: can't detrimental spell on bots and bots can't detriment on you or the others bots
		if((IsDetrimentalSpell(spell_id) && spelltar->IsBot()) || (IsDetrimentalSpell(spell_id) && spelltar->IsClient()))
			return false;

		if(spelltar->IsPet()) {
			for(int i=0; i<EFFECT_COUNT; ++i)
			{
				if(spells[spell_id].effectid[i] == SE_Illusion)
					return false;
			}
		}

		Result = Mob::SpellOnTarget(spell_id, spelltar);
	}

	return Result;
}

bool Bot::IsImmuneToSpell(int16 spell_id, Mob *caster) {
	bool Result = false;

	if(!caster)
		return false;

	// TODO: Remove hard coded zone id's
	if(!IsSacrificeSpell(spell_id) && !(zone->GetZoneID() == 202) && !(this == caster)) {
		Result = Mob::IsImmuneToSpell(spell_id, caster);

		if(!Result) {
			if(caster->IsBot()) {
				if(spells[spell_id].targettype == ST_Undead) {
					if((GetBodyType() != BT_SummonedUndead) && (GetBodyType() != BT_Undead) && (GetBodyType() != BT_Vampire)) {
							mlog(SPELLS__RESISTS, "Bot's target is not an undead.");
							return true;
					}
				}
				if(spells[spell_id].targettype == ST_Summoned) {
					if((GetBodyType() != BT_SummonedUndead)
						&& (GetBodyType() != BT_Summoned)
						&& (GetBodyType() != BT_Summoned2)
						&& (GetBodyType() != BT_Summoned3)
						) {
							mlog(SPELLS__RESISTS, "Bot's target is not a summoned creature.");
							return true;
					}
				}
			}

			mlog(SPELLS__RESISTS, "No bot immunities to spell %d found.", spell_id);
		}
	}

	return Result;
}

bool Bot::DetermineSpellTargets(uint16 spell_id, Mob *&spell_target, Mob *&ae_center, CastAction_type &CastAction) {
	bool Result = false;

	SpellTargetType targetType = spells[spell_id].targettype;


	// This is so PoK NPC Necro/Shd can create essence emeralds for pc's from perl scripts
	if(targetType == ST_GroupClientAndPet) {
		if(((spell_id == 1768) && (zone->GetZoneID() == 202)) || (!IsDetrimentalSpell(spell_id))) {
			CastAction = SingleTarget;
			return true;
		}
	}

	Result = Mob::DetermineSpellTargets(spell_id, spell_target, ae_center, CastAction);

	return Result;
}

bool Bot::DoCastSpell(int16 spell_id, int16 target_id, int16 slot, sint32 cast_time, sint32 mana_cost, int32* oSpellWillFinish, int32 item_slot) {
	bool Result = false;
	
	if(GetClass() == BARD) { 
		// Bard bots casting time is interrupting thier melee
		cast_time = 0;
	}

	Result = Mob::DoCastSpell(spell_id, target_id, slot, cast_time, mana_cost, oSpellWillFinish, item_slot);

	if(oSpellWillFinish) {
		const SPDat_Spell_Struct &spell = spells[spell_id];
		*oSpellWillFinish = Timer::GetCurrentTime() + ((spell.recast_time > 20000) ? 10000 : spell.recast_time);
	}

	return Result;
}

sint32 Bot::GenerateBaseManaPoints()
{
	// Now, we need to calc the base mana.
	sint32 bot_mana = 0;
	sint32 WisInt = 0;
	sint32 MindLesserFactor, MindFactor;
	int wisint_mana = 0;
	int base_mana = 0;
	int ConvertedWisInt = 0;

	switch(GetCasterClass())
	{
		case 'I':
			WisInt = INT;
			if(GetOwner() && GetOwner()->CastToClient() && GetOwner()->CastToClient()->GetClientVersion() >= EQClientSoD && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
				if(WisInt > 100) {
					ConvertedWisInt = (((WisInt - 100) * 5 / 2) + 100);
					if(WisInt > 201) {
						ConvertedWisInt -= ((WisInt - 201) * 5 / 4);
					}
				}
				else {
					ConvertedWisInt = WisInt;
				}
				if(GetLevel() < 41) { 
					wisint_mana = (GetLevel() * 75 * ConvertedWisInt / 1000);
					base_mana = (GetLevel() * 15);
				}
				else if(GetLevel() < 81) {
					wisint_mana = ((3 * ConvertedWisInt) + ((GetLevel() - 40) * 15 * ConvertedWisInt / 100));
					base_mana = (600 + ((GetLevel() - 40) * 30));
				}
				else {
					wisint_mana = (9 * ConvertedWisInt);
					base_mana = (1800 + ((GetLevel() - 80) * 18));
				}
				bot_mana = base_mana + wisint_mana;
			}
			else {
				if((( WisInt - 199 ) / 2) > 0) {
					MindLesserFactor = ( WisInt - 199 ) / 2;
				}
				else {
					MindLesserFactor = 0;
				}
				MindFactor = WisInt - MindLesserFactor;
				if(WisInt > 100) {
					bot_mana = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				}
				else {
					bot_mana = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
				}
			}
			break;

		case 'W':
			WisInt = WIS;
			if(GetOwner() && GetOwner()->CastToClient() && GetOwner()->CastToClient()->GetClientVersion() >= EQClientSoD && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
				if(WisInt > 100) {
					ConvertedWisInt = (((WisInt - 100) * 5 / 2) + 100);
					if(WisInt > 201) {
						ConvertedWisInt -= ((WisInt - 201) * 5 / 4);
					}
				}
				else {
					ConvertedWisInt = WisInt;
				}
				if(GetLevel() < 41) { 
					wisint_mana = (GetLevel() * 75 * ConvertedWisInt / 1000);
					base_mana = (GetLevel() * 15);
				}
				else if(GetLevel() < 81) {
					wisint_mana = ((3 * ConvertedWisInt) + ((GetLevel() - 40) * 15 * ConvertedWisInt / 100));
					base_mana = (600 + ((GetLevel() - 40) * 30));
				}
				else {
					wisint_mana = (9 * ConvertedWisInt);
					base_mana = (1800 + ((GetLevel() - 80) * 18));
				}
				bot_mana = base_mana + wisint_mana;
			}
			else {
				if((( WisInt - 199 ) / 2) > 0) {
					MindLesserFactor = ( WisInt - 199 ) / 2;
				}
				else {
					MindLesserFactor = 0;
				}
				MindFactor = WisInt - MindLesserFactor;
				if(WisInt > 100) {
					bot_mana = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				}
				else {
					bot_mana = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
				}
			}
			break;

		default:
			bot_mana = 0;
			break;
	}

	max_mana = bot_mana;

	return bot_mana;
}

void Bot::GenerateSpecialAttacks()
{
	// Special Attacks
	if(((GetClass() == MONK) || (GetClass() == WARRIOR) || (GetClass() == RANGER) || (GetClass() == BERSERKER))	&& (GetLevel() >= 60)) {
		SpecAttacks[SPECATK_TRIPLE] = true;
	}
}

bool Bot::DoFinishedSpellAETarget(int16 spell_id, Mob* spellTarget, int16 slot, bool& stopLogic) {
	if(GetClass() == BARD) {
		if(!ApplyNextBardPulse(bardsong, this, bardsong_slot)) {
			InterruptSpell(SONG_ENDS_ABRUPTLY, 0x121, bardsong);
		}
		stopLogic = true;
	}

	return true;
}

bool Bot::DoFinishedSpellSingleTarget(int16 spell_id, Mob* spellTarget, int16 slot, bool& stopLogic) {
	if(spellTarget) {
		if(IsGrouped() && (spellTarget->IsBot() || spellTarget->IsClient()) && RuleB(Bots, BotGroupBuffing)) {
			//NPC *bot = this->CastToNPC();
			bool noGroupSpell = false;
			int16 thespell = spell_id;

			for(int i=0; i < AIspells.size(); i++) {
				int j = BotGetSpells(i);
				int spelltype = BotGetSpellType(i);
				bool spellequal = (j == thespell);
				bool spelltypeequal = ((spelltype == 2) || (spelltype == 16) || (spelltype == 32));
				bool spelltypetargetequal = ((spelltype == 8) && (spells[thespell].targettype == ST_Self));
				bool spelltypeclassequal = ((spelltype == 1024) && (GetClass() == SHAMAN));
				bool slotequal = (slot == USE_ITEM_SPELL_SLOT);

				// if it's a targeted heal or escape spell or pet spell or it's self only buff or self buff weapon proc, we only want to cast it once
				if(spellequal || slotequal) {
					if((spelltypeequal || spelltypetargetequal) || spelltypeclassequal || slotequal) {
						// Don't let the Shaman canni themselves to death
						if(((spells[thespell].effectid[0] == 0) && (spells[thespell].base[0] < 0)) && 
							(spellTarget->GetHP() < ((spells[thespell].base[0] * (-1)) + 100))) {
							return false;
						}

						SpellOnTarget(thespell, spellTarget);
						noGroupSpell = true;
						stopLogic = true;
					}
				}
			}

			if(!noGroupSpell) {
				Group *g = GetGroup();
				if(g) {
					for(int i=0; i<MAX_GROUP_MEMBERS;i++) {
						if(g->members[i]) {
							if((g->members[i]->GetClass() == NECROMANCER) &&
								(IsEffectInSpell(thespell, SE_AbsorbMagicAtt) || IsEffectInSpell(thespell, SE_Rune))) {
								// don't cast this on necro's, their health to mana
								// spell eats up the rune spell and it just keeps
								// getting recast over and over
							}
							else
							{
								SpellOnTarget(thespell, g->members[i]);
							}
							if(g->members[i] && g->members[i]->GetPetID()) {
								SpellOnTarget(thespell, g->members[i]->GetPet());
							}
						}
					}
					SetMana(GetMana() - (GetActSpellCost(thespell, spells[thespell].mana) * (g->GroupCount() - 1)));
				}
			}

			stopLogic = true;
		}
	}

	return true;
}

bool Bot::DoFinishedSpellGroupTarget(int16 spell_id, Mob* spellTarget, int16 slot, bool& stopLogic) {
	bool isMainGroupMGB = false;

	//if(GetBotRaidID() > 0) {
	//	BotRaids *br = entity_list.GetBotRaidByMob(this);
	//	if(br) {
	//		for(int n=0; n<MAX_GROUP_MEMBERS; ++n) {
	//			if(br->BotRaidGroups[0] && (br->BotRaidGroups[0]->members[n] == this)) {
	//				if(GetLevel() >= 59) // MGB AA
	//					isMainGroupMGB = true;
	//				break;
	//			}
	//		}
	//	}
	//}

	if(isMainGroupMGB && (GetClass() != BARD)) {
		Say("MGB %s", spells[spell_id].name);
		SpellOnTarget(spell_id, this);
		entity_list.AESpell(this, this, spell_id, true);
	}
	else {
		Group *g = GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; ++i) {
				if(g->members[i]) {
					SpellOnTarget(spell_id, g->members[i]);
					if(g->members[i] && g->members[i]->GetPetID()) {
						SpellOnTarget(spell_id, g->members[i]->GetPet());
					}
				}
			}
		}
	}

	stopLogic = true;

	return true;
}

void Bot::CalcBonuses() {
	GenerateBaseStats();
	CalcItemBonuses();
	CalcSpellBonuses(&spellbonuses);
	GenerateAABonuses();
	
	CalcATK();
	CalcSTR();
	CalcSTA();
	CalcDEX();
	CalcAGI();
	CalcINT();
	CalcWIS();
	CalcCHA();

	CalcMR();
	CalcFR();
	CalcDR();
	CalcPR();
	CalcCR();
	CalcCorrup();

	GenerateArmorClass();

	CalcMaxHP();
	CalcMaxMana();
	hp_regen = CalcHPRegen();
	mana_regen = CalcManaRegen();
}

sint32 Bot::CalcHPRegenCap(){ 
	int level = GetLevel();
	sint32 hpregen_cap = 0;
	hpregen_cap = RuleI(Character, ItemHealthRegenCap) + itembonuses.HeroicSTA/25; 	

#if EQDEBUG >= 11
	LogFile->write(EQEMuLog::Debug, "Bot::CalcHPRegenCap() called for %s - returning %d", GetName(), hpregen_cap);
#endif
	return hpregen_cap;
}

sint32 Bot::CalcManaRegenCap(){
	int level = GetLevel();
	sint32 manaregen_cap = 0;
	switch(GetCasterClass())
	{
		case 'I': 
			manaregen_cap = RuleI(Character, ItemManaRegenCap) + (itembonuses.HeroicINT / 25);
			break;
		case 'W': 
			manaregen_cap = RuleI(Character, ItemManaRegenCap) + (itembonuses.HeroicWIS / 25);
			break;
	}
	manaregen_cap += aabonuses.ItemManaRegenCap;
#if EQDEBUG >= 11
	LogFile->write(EQEMuLog::Debug, "Bot::CalcManaRegenCap() called for %s - returning %d", GetName(), manaregen_cap);
#endif
	return manaregen_cap;
}

// Return max stat value for level
sint16 Bot::GetMaxStat() {
	int level = GetLevel();
	sint16 base = 0;
	
	if (level < 61) {
		base = 255;
	}
	else if (GetOwner() && GetOwner()->CastToClient() && GetOwner()->CastToClient()->GetClientVersion() >= EQClientSoF) {
		base = 255 + 5 * (level - 60);
	}
	else if (level < 71) {
		base = 255 + 5 * (level - 60);
	}
	else {
		base = 330;
	}
	
	return(base);
}

sint16 Bot::GetMaxResist() {
	int level = GetLevel();

	sint16 base = 500;
	
	if(level > 60)
		base += ((level - 60) * 5);

	return base;
}

sint16 Bot::GetMaxSTR() {
	return GetMaxStat()
		+ itembonuses.STRCapMod
		+ spellbonuses.STRCapMod
		+ aabonuses.STRCapMod;
}
sint16 Bot::GetMaxSTA() {
	return GetMaxStat()
		+ itembonuses.STACapMod
		+ spellbonuses.STACapMod
		+ aabonuses.STACapMod;
}
sint16 Bot::GetMaxDEX() {
	return GetMaxStat()
		+ itembonuses.DEXCapMod
		+ spellbonuses.DEXCapMod
		+ aabonuses.DEXCapMod;
}
sint16 Bot::GetMaxAGI() {
	return GetMaxStat()
		+ itembonuses.AGICapMod
		+ spellbonuses.AGICapMod
		+ aabonuses.AGICapMod;
}
sint16 Bot::GetMaxINT() {
	return GetMaxStat()
		+ itembonuses.INTCapMod
		+ spellbonuses.INTCapMod
		+ aabonuses.INTCapMod;
}
sint16 Bot::GetMaxWIS() {
	return GetMaxStat()
		+ itembonuses.WISCapMod
		+ spellbonuses.WISCapMod
		+ aabonuses.WISCapMod;
}
sint16 Bot::GetMaxCHA() {
	return GetMaxStat()
		+ itembonuses.CHACapMod
		+ spellbonuses.CHACapMod
		+ aabonuses.CHACapMod;
}
sint16 Bot::GetMaxMR() {
	return GetMaxResist()
		+ itembonuses.MRCapMod
		+ spellbonuses.MRCapMod
		+ aabonuses.MRCapMod;
}
sint16 Bot::GetMaxPR() {
	return GetMaxResist()
		+ itembonuses.PRCapMod
		+ spellbonuses.PRCapMod
		+ aabonuses.PRCapMod;
}
sint16 Bot::GetMaxDR() {
	return GetMaxResist()
		+ itembonuses.DRCapMod
		+ spellbonuses.DRCapMod
		+ aabonuses.DRCapMod;
}
sint16 Bot::GetMaxCR() {
	return GetMaxResist()
		+ itembonuses.CRCapMod
		+ spellbonuses.CRCapMod
		+ aabonuses.CRCapMod;
}
sint16 Bot::GetMaxFR() {
	return GetMaxResist()
		+ itembonuses.FRCapMod
		+ spellbonuses.FRCapMod
		+ aabonuses.FRCapMod;
}
sint16 Bot::GetMaxCorrup() {
	return GetMaxResist()
		+ itembonuses.CorrupCapMod
		+ spellbonuses.CorrupCapMod
		+ aabonuses.CorrupCapMod;
}

sint16 Bot::CalcSTR() {
	sint16 val = STR + itembonuses.STR + spellbonuses.STR;

	sint16 mod = aabonuses.STR;

	if(val>255 && GetLevel() <= 60)
		val = 255;
	STR = val + mod;

	if(STR < 1)
		STR = 1;
	
	int m = GetMaxSTR();
	if(STR > m)
		STR = m;
	
	return(STR);
}

sint16 Bot::CalcSTA() {
	sint16 val = STA + itembonuses.STA + spellbonuses.STA;
	
	sint16 mod = aabonuses.STA;
	
	if(val>255 && GetLevel() <= 60)
		val = 255;
	STA = val + mod;
	
	if(STA < 1)
		STA = 1;

	int m = GetMaxSTA();
	if(STA > m)
		STA = m;
	
	return(STA);
}

sint16 Bot::CalcAGI() {
	sint16 val = AGI + itembonuses.AGI + spellbonuses.AGI;
	sint16 mod = aabonuses.AGI;

	if(val>255 && GetLevel() <= 60)
		val = 255;

	AGI = val + mod;

	if(AGI < 1)
		AGI = 1;

	int m = GetMaxAGI();
	if(AGI > m)
		AGI = m;
	
	return(AGI);
}

sint16 Bot::CalcDEX() {
	sint16 val = DEX + itembonuses.DEX + spellbonuses.DEX;
	
	sint16 mod = aabonuses.DEX;
	
	if(val>255 && GetLevel() <= 60)
		val = 255;
	DEX = val + mod;
	
	if(DEX < 1)
		DEX = 1;

	int m = GetMaxDEX();
	if(DEX > m)
		DEX = m;
	
	return(DEX);
}

sint16 Bot::CalcINT() {
	sint16 val = INT + itembonuses.INT + spellbonuses.INT;
	
	sint16 mod = aabonuses.INT;
	
	if(val>255 && GetLevel() <= 60)
		val = 255;
	INT = val + mod;
	
	if(INT < 1)
		INT = 1;
	int m = GetMaxINT();
	if(INT > m)
		INT = m;
	
	return(INT);
}

sint16 Bot::CalcWIS() {
	sint16 val = WIS + itembonuses.WIS + spellbonuses.WIS;
	
	sint16 mod = aabonuses.WIS;
	
	if(val>255 && GetLevel() <= 60)
		val = 255;
	WIS = val + mod;
	
	if(WIS < 1)
		WIS = 1;

	int m = GetMaxWIS();
	if(WIS > m)
		WIS = m;
	
	return(WIS);
}

sint16 Bot::CalcCHA() {
	sint16 val = CHA + itembonuses.CHA + spellbonuses.CHA;
	
	sint16 mod = aabonuses.CHA;
	
	if(val>255 && GetLevel() <= 60)
		val = 255;
	CHA = val + mod;
	
	if(CHA < 1)
		CHA = 1;

	int m = GetMaxCHA();
	if(CHA > m)
		CHA = m;
	
	return(CHA);
}

//The AA multipliers are set to be 5, but were 2 on WR
//The resistant discipline which I think should be here is implemented
//in Mob::ResistSpell
sint16	Bot::CalcMR()
{
	MR += itembonuses.MR + spellbonuses.MR;
	MR += (GetAA(aaInnateMagicProtection) + GetAA(aaMarrsProtection))*2;
	
	if(MR < 1)
		MR = 1;

	if(MR > GetMaxMR())
		MR = GetMaxMR();

	return(MR);
}

sint16	Bot::CalcFR()
{
	FR += itembonuses.FR + spellbonuses.FR;
	FR += (GetAA(aaInnateFireProtection) + GetAA(aaWardingofSolusek))*2;
	
	if(FR < 1)
		FR = 1;
	
	if(FR > GetMaxFR())
		FR = GetMaxFR();

	return(FR); 
}

sint16	Bot::CalcDR()
{
	DR += itembonuses.DR + spellbonuses.DR;
	DR += (GetAA(aaInnateDiseaseProtection) + GetAA(aaBertoxxulousGift))*2;
	
	if(DR < 1)
		DR = 1;

	if(DR > GetMaxDR())
		DR = GetMaxDR();

	return(DR);
}

sint16	Bot::CalcPR()
{
	PR += itembonuses.PR + spellbonuses.PR;
	PR += (GetAA(aaInnatePoisonProtection) + GetAA(aaShroudofTheFaceless))*2;
	
	if(PR < 1)
		PR = 1;

	if(PR > GetMaxPR())
		PR = GetMaxPR();

	return(PR);
}

sint16	Bot::CalcCR()
{
	CR += itembonuses.CR + spellbonuses.CR;
	CR += (GetAA(aaInnateColdProtection) + GetAA(aaBlessingofEci))*2;
	
	if(CR < 1)
		CR = 1;

	if(CR > GetMaxCR())
		CR = GetMaxCR();

	return(CR);
}

sint16	Bot::CalcCorrup()
{
	Corrup = Corrup + itembonuses.Corrup + spellbonuses.Corrup + aabonuses.Corrup;
	
	if(Corrup > GetMaxCorrup())
		Corrup = GetMaxCorrup();

	return(Corrup);
}

sint16 Bot::CalcATK() {
	ATK = itembonuses.ATK + spellbonuses.ATK + aabonuses.ATK + GroupLeadershipAAOffenseEnhancement();
	return(ATK);
}

void Bot::CalcRestState() {

	// This method calculates rest state HP and mana regeneration.
	// The bot must have been out of combat for RuleI(Character, RestRegenTimeToActivate) seconds,
	// must be sitting down, and must not have any detrimental spells affecting them.
	//
	if(!RuleI(Character, RestRegenPercent))
		return;

	RestRegenHP = RestRegenMana = 0;

	if(IsEngaged() || !IsSitting())
		return;

	if(!rest_timer.Check(false))
		return;

	uint32 buff_count = GetMaxTotalSlots();
	for (unsigned int j = 0; j < buff_count; j++) {
		if(buffs[j].spellid != SPELL_UNKNOWN) {
			if(IsDetrimentalSpell(buffs[j].spellid) && (buffs[j].ticsremaining > 0))
				if(!DetrimentalSpellAllowsRest(buffs[j].spellid))
					return;
		}
	}

	RestRegenHP = (GetMaxHP() * RuleI(Character, RestRegenPercent) / 100);

	RestRegenMana = (GetMaxMana() * RuleI(Character, RestRegenPercent) / 100);
}

sint32 Bot::LevelRegen()
{
	int level = GetLevel();
	bool bonus = GetRaceBitmask(_baseRace) & RuleI(Character, BaseHPRegenBonusRaces);
	uint8 multiplier1 = bonus ? 2 : 1;
	sint32 hp = 0;

	//these calculations should match up with the info from Monkly Business, which was last updated ~05/2008: http://www.monkly-business.net/index.php?pageid=abilities
	if (level < 51) {
		if (IsSitting()) {
			if (level < 20)
				hp += 2 * multiplier1;
			else if (level < 50)
				hp += 3 * multiplier1;
			else	//level == 50
				hp += 4 * multiplier1;
		}
		else	//feigned or standing
			hp += 1 * multiplier1;
	}
	//there may be an easier way to calculate this next part, but I don't know what it is
	else {	//level >= 51
		sint32 tmp = 0;
		float multiplier2 = 1;
		if (level < 56) {
			tmp = 2;
			if (bonus)
				multiplier2 = 3;
		}
		else if (level < 60) {
		tmp = 3;
			if (bonus)
				multiplier2 = 3.34;
		}
		else if (level < 61) {
			tmp = 4;
			if (bonus)
				multiplier2 = 3;
		}
		else if (level < 63) {
			tmp = 5;
			if (bonus)
				multiplier2 = 2.8;
		}
		else if (level < 65) {
			tmp = 6;
			if (bonus)
				multiplier2 = 2.67;
		}
		else {	//level >= 65
			tmp = 7;
			if (bonus)
				multiplier2 = 2.58;
		}

		hp += sint32(float(tmp) * multiplier2);
	}

	return hp;
}

sint32 Bot::CalcHPRegen() {
	sint32 regen = LevelRegen() + itembonuses.HPRegen + spellbonuses.HPRegen;
	regen += aabonuses.HPRegen + GroupLeadershipAAHealthRegeneration();

	regen = (regen * RuleI(Character, HPRegenMultiplier)) / 100;
	return regen;
}

sint32 Bot::CalcManaRegen() 
{
	uint8 level = GetLevel();
	uint8 botclass = GetClass();
	sint32 regen = 0;
	//this should be changed so we dont med while camping, etc...
	if (IsSitting()) 
	{
		BuffFadeBySitModifier();
		if(botclass != WARRIOR && botclass != MONK && botclass != ROGUE && botclass != BERSERKER) {
			regen = (((GetSkill(MEDITATE) / 10) + (level - (level / 4))) / 4) + 4;
			regen += spellbonuses.ManaRegen + itembonuses.ManaRegen;
		}
		else
			regen = 2 + spellbonuses.ManaRegen + itembonuses.ManaRegen;
	}
	else {
		regen = 2 + spellbonuses.ManaRegen + itembonuses.ManaRegen;
	}

	if(GetCasterClass() == 'I')
		regen += (itembonuses.HeroicINT / 25);
	else if(GetCasterClass() == 'W')
		regen += (itembonuses.HeroicWIS / 25);
	else
		regen = 0;

	regen += aabonuses.ManaRegen;

	regen = (regen * RuleI(Character, ManaRegenMultiplier)) / 100;

	float mana_regen_rate = RuleR(Bots, BotManaRegen);
	if(mana_regen_rate < 0.0f)
		mana_regen_rate = 0.0f;

	regen = regen * mana_regen_rate; // 90% of people wouldnt guess that manaregen would decrease the larger the number they input, this makes more sense

	return regen;
}

// This is for calculating Base HPs + STA bonus for SoD or later clients.
int32 Bot::GetClassHPFactor() {

	int factor;

	// Note: Base HP factor under level 41 is equal to factor / 12, and from level 41 to 80 is factor / 6.
	// Base HP over level 80 is factor / 10
	// HP per STA point per level is factor / 30 for level 80+
	// HP per STA under level 40 is the level 80 HP Per STA / 120, and for over 40 it is / 60.
	
	switch(GetClass())
	{
		case DRUID:
		case ENCHANTER:
		case NECROMANCER:
		case MAGICIAN:
		case WIZARD:
			factor = 240;
			break;
		case BEASTLORD:
		case BERSERKER:
		case MONK:
		case ROGUE:
		case SHAMAN:
			factor = 255;
			break;
		case BARD:
		case CLERIC:
			factor = 264;
			break;
		case SHADOWKNIGHT:
		case PALADIN:
			factor = 288;
			break;
		case RANGER:
			factor = 276;
			break;
		case WARRIOR:
			factor = 300;
			break;
		default:
			factor = 240;
			break;
	}
	return factor;
}

sint32 Bot::CalcMaxHP() {
	sint32 bot_hp = 0;
	int32 nd = 10000;
	
	bot_hp += GenerateBaseHitPoints() + itembonuses.HP;

	nd += aabonuses.MaxHP;	//Natural Durability, Physical Enhancement, Planar Durability

	bot_hp = bot_hp * nd / 10000;

	bot_hp += spellbonuses.HP + aabonuses.HP;

	bot_hp += GroupLeadershipAAHealthEnhancement();

	bot_hp += bot_hp * (spellbonuses.MaxHPChange + itembonuses.MaxHPChange) / 10000;
	max_hp = bot_hp;

	return max_hp;
}

void Bot::Camp(bool databaseSave) {
	Sit();

	if(IsGrouped()) {
		RemoveBotFromGroup(this, GetGroup());
	}

	if(databaseSave)
		Save();

	Depop();
}

void Bot::Zone() {
	if(HasGroup()) {
		GetGroup()->MemberZoned(this);
	}

	Save();
	Depop();
}

bool Bot::IsArcheryRange(Mob *target) {
	bool result = false;

	if(target) {
		float range = GetBotArcheryRange() + 5.0; //Fudge it a little, client will let you hit something at 0 0 0 when you are at 205 0 0

		range *= range;

		float targetDistance = DistNoRootNoZ(*target);
		
		float minRuleDistance = RuleI(Combat, MinRangedAttackDist) * RuleI(Combat, MinRangedAttackDist);

		if((targetDistance > range) || (targetDistance < minRuleDistance))		
			result = false;
		else
			result = true;
	}

	return result;
}

bool Bot::IsBotCasterCombatRange(Mob *target) {
	bool result = false;

	if(target) {
		float range = BotAISpellRange;

		range *= range;

		// half the max so the bot doesn't always stop at max range to allow combat movement
		range *= .5;

		float targetDistance = DistNoRootNoZ(*target);

		if(targetDistance > range)		
			result = false;
		else
			result = true;
	}

	return result;
}

bool Bot::CalculateNewPosition2(float x, float y, float z, float speed, bool checkZ) {
	// 2.5625 is the inverse of 0.3902439. The only difference is in implementation.
	// NOTE: You can not change just one of the constants below. They are the same number, just expressed inversly of each other.
	// const float clientOverServerRatio = 2.5625f;
	const float serverOverClientRatio = 0.3902439f;

	// Use this block if using 2.5625 as the ratio.
	// const int clientAnimationMovementRateTypeMultiple = 8;

	//	WildcardX: These are valid rates and observations based on painstaking testing of the client response to these values
	//	
	// 
	//	0 * 8 = 0 : No Movement
	//	1 * 8 = 8 : Death Walk
	//	2 * 8 = 16 : Slow Walk
	//	3 * 8 = 24 : Normal Walk
	//	4 * 8 = 32 : Jog
	//	5 * 8 = 40 : Normal Run
	//	6 * 8 = 48 : Faster Run
	//	7 * 8 = 56 : Even Faster Run
	//	8 * 8 = 64 : Fastest Yet Run (Bard Song Speed?)
	//	9 * 8 = 72 : Faster Fastest Yet Run
	//	10 * 8 = 80 : .... you get the idea, this is pretty fast
	//	11 * 8 = 88 : .... warp speed anyone?
	//	12 * 8 = 96 : .... transwarp drive was invented by gnomes in Norrath
	//	13 * 8 = 104 : ... who needs warp drives when you can just displace through time and space?
	//
	//
	//	You get the idea here with these... These seem to be "benchmark values" of animation movement and how fast
	//	the client thinks the Mob is moving so it can make it all look seemless between updates from the server.
	//	This chart is scalable by the client so you can pass an animation rate of 50 and get a "faster run" but not quite a "even faster run"

	// Convert the Bot movement rate to a value the client understands based on the chart above
	// Use this block if using 2.5625 as the ratio.
	// speed *= clientMovementRateTypeMultiple;


	// This sets the movement animation rate with the client
	// Use this block if using 2.5625 as the ratio.
	// pRunAnimSpeed = speed;
	pRunAnimSpeed = ((serverOverClientRatio * 10.0f) * speed) * 10.0f;

	// Now convert our "speed" from the value necessary for the client to animate the correct movement type rate to the server side speed
	// Use this block if using 2.5625 as the ratio.
	// speed *= serverOverClientRatio;
	speed = pRunAnimSpeed / serverOverClientRatio;

	return MakeNewPositionAndSendUpdate(x, y, z, speed, checkZ);
}

// Orders all bots in the specified group to follow their group leader.
void Bot::BotGroupOrderFollow(Group* group, Client* client) {
	if(group && client) {
		Mob* groupLeader = group->GetLeader();

		if(groupLeader) {
			for(int i = 0; i< MAX_GROUP_MEMBERS; i++) {
				if(group->members[i] && group->members[i]->IsBot()) {
					Bot* botGroupMember = group->members[i]->CastToBot();

					if(botGroupMember && botGroupMember->GetBotOwnerCharacterID() == client->CharacterID()) {
						if(group->IsLeader(botGroupMember) && botGroupMember->GetBotOwner()) {
							botGroupMember->SetFollowID(botGroupMember->GetBotOwner()->GetID());
							if(botGroupMember->GetBotOwner())
								botGroupMember->Say("Following %s.", botGroupMember->GetBotOwner()->GetName());
						}
						else {
							botGroupMember->SetFollowID(groupLeader->GetID());
							botGroupMember->Say("Following %s.", groupLeader->GetCleanName());
						}

						botGroupMember->WipeHateList();

						if(botGroupMember->HasPet() && botGroupMember->GetPet()) {
							botGroupMember->GetPet()->WipeHateList();
						}
					}
				}
			}
		}
	}
}

// Orders all bots in the specified group to guard their current location.
void Bot::BotGroupOrderGuard(Group* group, Client* client) {
	if(group && client) {
		for(int i = 0; i< MAX_GROUP_MEMBERS; i++) {
			if(group->members[i] && group->members[i]->IsBot()) {
				Bot* botGroupMember = group->members[i]->CastToBot();

				if(botGroupMember && botGroupMember->GetBotOwnerCharacterID() == client->CharacterID()) {
					botGroupMember->SetFollowID(0);
					botGroupMember->Say("Guarding here.");

					botGroupMember->WipeHateList();

					if(botGroupMember->HasPet() && botGroupMember->GetPet()) {
						botGroupMember->GetPet()->WipeHateList();
					}
				}
			}
		}
	}
}

// Orders all bots in the specified group to attack their group leader's target.
void Bot::BotGroupOrderAttack(Group* group, Mob* target, Client* client) {
	if(group && target) {
		Mob* groupLeader = group->GetLeader();

		if(groupLeader) {
			for(int i=0; i < MAX_GROUP_MEMBERS; i++) {
				if(group->members[i] && group->members[i]->IsBot()) {
					Bot* botGroupMember = group->members[i]->CastToBot();

					if(botGroupMember->GetBotOwnerCharacterID() == client->CharacterID()) {
						botGroupMember->WipeHateList();
						botGroupMember->AddToHateList(target, 1);

						if(botGroupMember->HasPet() && botGroupMember->GetPet()) {
							botGroupMember->GetPet()->WipeHateList();
							botGroupMember->GetPet()->AddToHateList(target, 1);
						}
					}
				}
			}
		}
	}
}

// Summons all bot group members to ther owners location.
void Bot::BotGroupSummon(Group* group, Client* client) {
	if(group) {
		for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if(group->members[i] && group->members[i]->IsBot()) {
				Bot* botMember = group->members[i]->CastToBot();

				if(botMember->GetBotOwnerCharacterID() == client->CharacterID()) {
					botMember->SetTarget(botMember->GetBotOwner());
					botMember->WipeHateList();
					botMember->Warp(botMember->GetBotOwner()->GetX(), botMember->GetBotOwner()->GetY(), botMember->GetBotOwner()->GetZ());

					if(botMember->HasPet() && botMember->GetPet()) {
						botMember->GetPet()->SetTarget(botMember);
						botMember->GetPet()->WipeHateList();
						botMember->GetPet()->Warp(botMember->GetBotOwner()->GetX(), botMember->GetBotOwner()->GetY(), botMember->GetBotOwner()->GetZ());
					}
				}
			}
		}
	}
}

// Finds a bot in the entitity list by bot owner character id and the bot first name
Bot* Bot::GetBotByBotClientOwnerAndBotName(Client* c, std::string botName) {
	Bot* Result = 0;

	if(c) {
		std::list<Bot*> BotList = entity_list.GetBotsByBotOwnerCharacterID(c->CharacterID());

		if(!BotList.empty()) {
			for(std::list<Bot*>::iterator botListItr = BotList.begin(); botListItr != BotList.end(); botListItr++) {
				if(std::string((*botListItr)->GetCleanName()) == botName) {
					Result = (*botListItr);
					break;
				}
			}
		}
	}

	return Result;
}

// Processes a group invite from a Client for a Bot character.
void Bot::ProcessBotGroupInvite(Client* c, std::string botName) {
	if(c) {
		Bot* invitedBot = GetBotByBotClientOwnerAndBotName(c, botName);

		if(invitedBot && !invitedBot->HasGroup()) {
			if(!c->IsGrouped()) {
				Group *g = new Group(c);
				if(AddBotToGroup(invitedBot, g)) {
					entity_list.AddGroup(g);
					database.SetGroupLeaderName(g->GetID(), c->GetName());
					g->SaveGroupLeaderAA();
					database.SetGroupID(c->GetName(), g->GetID(), c->CharacterID());
					database.SetGroupID(invitedBot->GetCleanName(), g->GetID(), invitedBot->GetBotID());
				}
			}
			else {
				AddBotToGroup(invitedBot, c->GetGroup());
				database.SetGroupID(invitedBot->GetCleanName(), c->GetGroup()->GetID(), invitedBot->GetBotID());
			}

			/*if(c->GetBotRaidID() > 0)
				invitedBot->SetBotRaidID(c->GetBotRaidID());*/
		}
		// TODO: if there is a bot but the bot is already in a group, do we send an group invitation cancel message back to the client?
	}
}

// Processes a group disband request from a Client for a Bot.
void Bot::ProcessBotGroupDisband(Client* c, std::string botName) {
	if(c) {
		Bot* tempBot = 0;

		if(botName.empty())
			tempBot = GetFirstBotInGroup(c->GetGroup());
		else
			tempBot = GetBotByBotClientOwnerAndBotName(c, botName);

		RemoveBotFromGroup(tempBot, c->GetGroup());
	}
}

// Handles all client zone change event
void Bot::ProcessClientZoneChange(Client* botOwner) {
	if(botOwner) {
		std::list<Bot*> BotList = entity_list.GetBotsByBotOwnerCharacterID(botOwner->CharacterID());

		for(list<Bot*>::iterator itr = BotList.begin(); itr != BotList.end(); itr++) {
			Bot* tempBot = *itr;

			if(tempBot) {
				if(tempBot->HasGroup()) {
					Group* g = tempBot->GetGroup();
					if(g && g->GetLeader()) {
						Mob* tempGroupLeader = tempBot->GetGroup()->GetLeader();
						if(tempGroupLeader && tempGroupLeader->IsClient()) {
							if(tempBot->GetBotOwnerCharacterID() == tempGroupLeader->CastToClient()->CharacterID())
								tempBot->Zone();
							else
								tempBot->Camp();
						}
					}
					else
						tempBot->Camp();
				}
				else
					tempBot->Camp();
			}
		}
	}
}

// Finds and returns the first Bot object found in specified group
Bot* Bot::GetFirstBotInGroup(Group* group) {
	Bot* Result = 0;

	if(group) {
		for(int Counter = 0; Counter < MAX_GROUP_MEMBERS; Counter++) {
			if (group->members[Counter] == NULL) {
				continue;
			}

			if(group->members[Counter]->IsBot()) {
				Result = group->members[Counter]->CastToBot();
				break;
			}
		}
	}

	return Result;
}

// Processes a client request to inspect a bot's equipment.
void Bot::ProcessBotInspectionRequest(Bot* inspectedBot, Client* client) {
	if(inspectedBot && client) {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_InspectAnswer, sizeof(InspectResponse_Struct));
		InspectResponse_Struct* insr = (InspectResponse_Struct*) outapp->pBuffer;
		insr->TargetID = inspectedBot->GetNPCTypeID();
		insr->playerid = inspectedBot->GetID();

		const Item_Struct* item = 0;
		const ItemInst* inst = 0;
		for(sint16 L=0; L<=21; ++L) {
			inst = inspectedBot->GetBotItem(L);
			if(inst) {
				if(item = inst->GetItem()) {
					strcpy(insr->itemnames[L], item->Name);
					insr->itemicons[L] = item->Icon;
				}
				else
					insr->itemicons[L] = 0xFFFFFFFF;
			}
		}

		client->QueuePacket(outapp); // Send answer to requester
	}
}

void Bot::CalcItemBonuses()
{
	memset(&itembonuses, 0, sizeof(StatBonuses));
	const Item_Struct* itemtmp = 0;

	for(int i=0; i<=21; ++i) {
		const ItemInst* item = GetBotItem(i);
		if(item) {
			for(int j=0; j<=4; ++j) {
				const ItemInst* aug = item->GetAugment(j);
				if(aug) {
					itemtmp = aug->GetItem();
					if(itemtmp->AC != 0)
						itembonuses.AC += itemtmp->AC;
					if(itemtmp->HP != 0)
						itembonuses.HP += itemtmp->HP;
					if(itemtmp->Mana != 0)
						itembonuses.Mana += itemtmp->Mana;
					if(itemtmp->Endur != 0)
						itembonuses.Endurance += itemtmp->Endur;
					if(itemtmp->AStr != 0)
						itembonuses.STR += itemtmp->AStr;
					if(itemtmp->ASta != 0)
						itembonuses.STA += itemtmp->ASta;
					if(itemtmp->ADex != 0)
						itembonuses.DEX += itemtmp->ADex;
					if(itemtmp->AAgi != 0)
						itembonuses.AGI += itemtmp->AAgi;
					if(itemtmp->AInt != 0)
						itembonuses.INT += itemtmp->AInt;
					if(itemtmp->AWis != 0)
						itembonuses.WIS += itemtmp->AWis;
					if(itemtmp->ACha != 0)
						itembonuses.CHA += itemtmp->ACha;
					if(itemtmp->MR != 0)
						itembonuses.MR += itemtmp->MR;
					if(itemtmp->FR != 0)
						itembonuses.FR += itemtmp->FR;
					if(itemtmp->CR != 0)
						itembonuses.CR += itemtmp->CR;
					if(itemtmp->PR != 0)
						itembonuses.PR += itemtmp->PR;
					if(itemtmp->DR != 0)
						itembonuses.DR += itemtmp->DR;
					if(itemtmp->SVCorruption != 0)
						itembonuses.Corrup += itemtmp->SVCorruption;
					if(itemtmp->Regen != 0)
						itembonuses.HPRegen += itemtmp->Regen;
					if(itemtmp->ManaRegen != 0)
						itembonuses.ManaRegen += itemtmp->ManaRegen;
					if(itemtmp->Attack != 0)
						itembonuses.ATK += itemtmp->Attack;
					if(itemtmp->DamageShield != 0)
						itembonuses.DamageShield += itemtmp->DamageShield;
					if(itemtmp->SpellShield != 0)
						itembonuses.SpellDamageShield += itemtmp->SpellShield;
					if(itemtmp->Shielding != 0)
						itembonuses.MeleeMitigation += itemtmp->Shielding;
					if(itemtmp->StunResist != 0)
						itembonuses.StunResist += itemtmp->StunResist;
					if(itemtmp->StrikeThrough != 0)
						itembonuses.StrikeThrough += itemtmp->StrikeThrough;
					if(itemtmp->Avoidance != 0)
						itembonuses.AvoidMeleeChance += itemtmp->Avoidance;
					if(itemtmp->Accuracy != 0)
						itembonuses.HitChance += itemtmp->Accuracy;
					if(itemtmp->CombatEffects != 0)
						itembonuses.ProcChance += itemtmp->CombatEffects;
					if(itemtmp->Haste != 0)
						if(itembonuses.haste < itemtmp->Haste)
							itembonuses.haste = itemtmp->Haste;
					if ((itemtmp->Worn.Effect != 0) && (itemtmp->Worn.Type == ET_WornEffect)) { // latent effects
						ApplySpellsBonuses(itemtmp->Worn.Effect, itemtmp->Worn.Level, &itembonuses);
					}
				}
			}
			itemtmp = item->GetItem();
			if(itemtmp->AC != 0)
				itembonuses.AC += itemtmp->AC;
			if(itemtmp->HP != 0)
				itembonuses.HP += itemtmp->HP;
			if(itemtmp->Mana != 0)
				itembonuses.Mana += itemtmp->Mana;
			if(itemtmp->Endur != 0)
				itembonuses.Endurance += itemtmp->Endur;
			if(itemtmp->AStr != 0)
				itembonuses.STR += itemtmp->AStr;
			if(itemtmp->ASta != 0)
				itembonuses.STA += itemtmp->ASta;
			if(itemtmp->ADex != 0)
				itembonuses.DEX += itemtmp->ADex;
			if(itemtmp->AAgi != 0)
				itembonuses.AGI += itemtmp->AAgi;
			if(itemtmp->AInt != 0)
				itembonuses.INT += itemtmp->AInt;
			if(itemtmp->AWis != 0)
				itembonuses.WIS += itemtmp->AWis;
			if(itemtmp->ACha != 0)
				itembonuses.CHA += itemtmp->ACha;
			if(itemtmp->MR != 0)
				itembonuses.MR += itemtmp->MR;
			if(itemtmp->FR != 0)
				itembonuses.FR += itemtmp->FR;
			if(itemtmp->CR != 0)
				itembonuses.CR += itemtmp->CR;
			if(itemtmp->PR != 0)
				itembonuses.PR += itemtmp->PR;
			if(itemtmp->DR != 0)
				itembonuses.DR += itemtmp->DR;
			if(itemtmp->SVCorruption != 0)
				itembonuses.Corrup += itemtmp->SVCorruption;
			if(itemtmp->Regen != 0)
				itembonuses.HPRegen += itemtmp->Regen;
			if(itemtmp->ManaRegen != 0)
				itembonuses.ManaRegen += itemtmp->ManaRegen;
			if(itemtmp->Attack != 0)
				itembonuses.ATK += itemtmp->Attack;
			if(itemtmp->DamageShield != 0)
				itembonuses.DamageShield += itemtmp->DamageShield;
			if(itemtmp->SpellShield != 0)
				itembonuses.SpellDamageShield += itemtmp->SpellShield;
			if(itemtmp->Shielding != 0)
				itembonuses.MeleeMitigation += itemtmp->Shielding;
			if(itemtmp->StunResist != 0)
				itembonuses.StunResist += itemtmp->StunResist;
			if(itemtmp->StrikeThrough != 0)
				itembonuses.StrikeThrough += itemtmp->StrikeThrough;
			if(itemtmp->Avoidance != 0)
				itembonuses.AvoidMeleeChance += itemtmp->Avoidance;
			if(itemtmp->Accuracy != 0)
				itembonuses.HitChance += itemtmp->Accuracy;
			if(itemtmp->CombatEffects != 0)
				itembonuses.ProcChance += itemtmp->CombatEffects;
			if(itemtmp->Haste != 0)
				if(itembonuses.haste < itemtmp->Haste)
					itembonuses.haste = itemtmp->Haste;
			if ((itemtmp->Worn.Effect != 0) && (itemtmp->Worn.Type == ET_WornEffect)) { // latent effects
				ApplySpellsBonuses(itemtmp->Worn.Effect, itemtmp->Worn.Level, &itembonuses);
			}
		}
	}

	if(itembonuses.HPRegen > CalcHPRegenCap())
		itembonuses.HPRegen = CalcHPRegenCap();

	if(itembonuses.ManaRegen > CalcManaRegenCap())
		itembonuses.ManaRegen = CalcManaRegenCap();
}

// This method is intended to call all necessary methods to do all bot stat calculations, including spell buffs, equipment, AA bonsues, etc.
void Bot::CalcBotStats(bool showtext) {
	if(!GetBotOwner())
		return;

	if(showtext) {
		GetBotOwner()->Message(15, "Bot updating...");
	}
	
	if(!IsValidRaceClassCombo()) {
		GetBotOwner()->Message(15, "A %s - %s bot was detected. Is this Race/Class combination allowed?.", GetRaceName(GetRace()), GetEQClassName(GetClass(), GetLevel()));
		GetBotOwner()->Message(15, "Previous Bots Code releases did not check Race/Class combinations during create.");
		GetBotOwner()->Message(15, "Unless you are experiencing heavy lag, you should delete and remake this bot.");
	}
	
	if(GetBotOwner()->GetLevel() != GetLevel())
		SetLevel(GetBotOwner()->GetLevel());

	GenerateSpecialAttacks();

	if(showtext) {
		GetBotOwner()->Message(15, "Base stats:");
		GetBotOwner()->Message(15, "Level: %i HP: %i AC: %i Mana: %i STR: %i STA: %i DEX: %i AGI: %i INT: %i WIS: %i CHA: %i", GetLevel(), base_hp, AC, max_mana, STR, STA, DEX, AGI, INT, WIS, CHA);
		GetBotOwner()->Message(15, "Resists-- Magic: %i, Poison: %i, Fire: %i, Cold: %i, Disease: %i, Corruption: %i.",MR,PR,FR,CR,DR,Corrup);
	}

	/*if(this->Save())
		this->GetBotOwner()->CastToClient()->Message(0, "%s saved.", this->GetCleanName());
	else
		this->GetBotOwner()->CastToClient()->Message(13, "%s save failed!", this->GetCleanName());*/
	
	CalcBonuses();

	AI_AddNPCSpells(this->GetBotSpellID());

	if(showtext) {
		GetBotOwner()->Message(15, "I'm updated.");
		GetBotOwner()->Message(15, "Level: %i HP: %i AC: %i Mana: %i STR: %i STA: %i DEX: %i AGI: %i INT: %i WIS: %i CHA: %i", GetLevel(), max_hp, GetAC(), max_mana, GetSTR(), GetSTA(), GetDEX(), GetAGI(), GetINT(), GetWIS(), GetCHA());
		GetBotOwner()->Message(15, "Resists-- Magic: %i, Poison: %i, Fire: %i, Cold: %i, Disease: %i, Corruption: %i.",GetMR(),GetPR(),GetFR(),GetCR(),GetDR(),GetCorrup());
	}
}

bool Bot::CheckLoreConflict(const Item_Struct* item) {
	if (!item)
		return false;
	if (!(item->LoreFlag))
		return false;

	if (item->LoreGroup == -1)	// Standard lore items; look everywhere except the shared bank, return the result
		return (m_inv.HasItem(item->ID, 0, invWhereWorn) != SLOT_INVALID);	

	//If the item has a lore group, we check for other items with the same group and return the result
	return (m_inv.HasItemByLoreGroup(item->LoreGroup, invWhereWorn) != SLOT_INVALID);
}

bool Bot::GroupHasClass(Group* group, uint8 classId) {
	bool result = false;

	if(group) {
		for(int counter = 0; counter < MAX_GROUP_MEMBERS; counter++) {
			if(group->members[counter]->GetClass() & classId) {
				result = true;
				break;
			}
		}
	}

	return result;
}

void Bot::ProcessBotCommands(Client *c, const Seperator *sep) {
	// All bot command processing occurs here now instead of in command.cpp

	// TODO: Log any possible error messages as most of these will be MySQL error messages.
	std::string TempErrorMessage;

	if(sep->arg[1][0] == '\0') {
		c->Message(13, "Bad argument, type #bot help");
		return;
	}
	if(!strcasecmp( sep->arg[1], "help") && !strcasecmp( sep->arg[2], "\0")){
		c->Message(0, "List of commands availables for bots :");
		c->Message(0, "#bot help - show this");
		c->Message(0, "#bot create [name] [class (id)] [race (id)] [model (male/female)] - create a permanent bot. See #bot help create.");
		c->Message(0, "#bot help create - show all the race/class id. (make it easier to create bots)");
		c->Message(0, "#bot delete - completely destroy forever the targeted bot and all its items.");
		c->Message(0, "#bot list [all/class(1-16)] - list your bots all or by class. Classes: 1(Warrior), 2(Cleric), 3(Paladin), 4(Ranger), 5(Sk), 6(Druid), 7(Monk), 8(Bard), 9(Rogue), 10(Shaman), 11(Necro), 12(Wiz), 13(Mag), 14(Ench), 15(Beast), 16(Bersek)");
		c->Message(0, "#bot spawn [bot name] - spawn a bot from it's name (use list to see all the bots). ");
		c->Message(0, "#bot inventory list - show the inventory (and the slots IDs) of the targetted bot.");
		c->Message(0, "#bot inventory remove [slotid] - remove the item at the given slot in the inventory of the targetted bot.");
		c->Message(0, "#bot update - you must type that command once you gain a level.");
		c->Message(0, "#bot summon - It will summon your targeted bot to you.");
		c->Message(0, "#bot ai mez - If you're grouped with an enchanter, he will mez your target.");
		c->Message(0, "#bot picklock - You must have a targeted rogue bot in your group and be right on the door.");
		c->Message(0, "#bot cure [poison|disease|curse|blindness] Cleric has most options");
		c->Message(0, "#bot bindme - You must have a Cleric in your group to get Bind Affinity cast on you.");
		c->Message(0, "#bot track - look at mobs in the zone (ranger has options)");
		c->Message(0, "#bot target calm - attempts to pacify your target mob.");
		c->Message(0, "#bot evac - transports your pc group to safe location in the current zone. bots are lost");
		c->Message(0, "#bot resurrectme - Your bot Cleric will rez you.");
		c->Message(0, "#bot corpse summon - Necromancers summon corpse.");
		c->Message(0, "#bot lore - cast Identify on the item on your mouse pointer.");
		c->Message(0, "#bot sow - Bot sow on you (Druid has options)");
		c->Message(0, "#bot invis - Bot invisiblity (must have proper class in group)");
		c->Message(0, "#bot levitate - Bot levitation (must have proper class in group)");
		c->Message(0, "#bot resist - Bot resist buffs (must have proper class in group)");
		c->Message(0, "#bot runeme - Enchanter Bot cast Rune spell on you");
		c->Message(0, "#bot shrink - Shaman or Beastlord will shrink target");
		c->Message(0, "#bot endureb - Bot enduring breath (must have proper class in group)");
		c->Message(0, "#bot charm - (must have proper class in group)");
		c->Message(0, "#bot dire charm - (must have proper class in group)");
		c->Message(0, "#bot pet remove - (remove pet before charm)");
		c->Message(0, "#bot gate - you need a Druid or Wizard in your group)");
		c->Message(0, "#bot archery - Toggle Archery Skilled bots between using a Bow or using Melee weapons.");
		c->Message(0, "#bot magepet [earth|water|air|fire|monster] - Select the pet type you want your Mage bot to use.");
		c->Message(0, "#bot giveitem - Gives your targetted bot the item you have on your cursor.");
		c->Message(0, "#bot augmentitem - Allows you to augment items for other classes. You must have the Augmentation Sealer window filled.");
		c->Message(0, "#bot camp - Tells your bot to camp out of the game.");
		c->Message(0, "#bot group help - Displays the commands available to manage any BOTs in your group.");
		c->Message(0, "#bot botgroup help - Displays the commands available to manage BOT ONLY groups.");
		c->Message(0, "#bot mana [<bot name or target> | all] - Displays a mana report for all your spawned bots.");
		c->Message(0, "#bot setfollowdistance ### - sets target bots follow distance to ### (ie 30 or 250).");
		c->Message(0, "#bot [hair|haircolor|beard|beardcolor|face|eyes|heritage|tattoo|details <value>] - Change your BOTs appearance.");
		c->Message(0, "#bot armorcolor <slot> <red> <green> <blue> - #bot help armorcolor for info");
		// TODO:
		// c->Message(0, "#bot illusion <bot/client name or target> - Enchanter Bot cast an illusion buff spell on you or your target.");
		c->Message(0, "#bot pull [<bot name>] [target] - Bot Pulling Target NPC's");
		return;
	}
	
	// pull
	if(!strcasecmp(sep->arg[1], "pull")) {
		Mob *target = c->GetTarget();
		if(target == NULL || target == c || target->IsBot() || (target->IsPet() && target->GetOwner()->IsBot()))
		{
			c->Message(15, "You must select a monster");
			return;
		}
		
		if(c->IsGrouped())
		{
			bool haspuller = false;
			Group *g = c->GetGroup();
			for(int i=0; i<MAX_GROUP_MEMBERS; i++)
			{
				if(g && g->members[i] && g->members[i]->IsBot() && !strcasecmp(g->members[i]->GetName() , sep->arg[2]))
				{
					haspuller = true;
					Mob *puller = g->members[i];
					if (puller->CastToBot()->IsArcheryRange(target))
					{
						puller->Say("Trying to Pull %s \n", target->GetCleanName());
						puller->CastToBot()->BotRangedAttack(target);
					}
					else {
						puller->Say("Out of Range %s \n", target->GetCleanName());
					}
				}
			}
			if(!haspuller) {
				c->Message(15, "You must have an Puller in your group.");
			}
		}
		return;
	}

	// added Bot follow distance - SetFollowDistance
	if(!strcasecmp(sep->arg[1], "setfollowdistance")) {
		if((c->GetTarget() == NULL) || (c->GetTarget() == c) || (!c->GetTarget()->IsBot()) || (c->GetTarget()->CastToBot()->GetBotOwner() != c)) {
			c->Message(15, "You must target a bot you own!");
		}
		else {
			int32 BotFollowDistance = atoi(sep->arg[2]);
			c->GetTarget()->SetFollowDistance(BotFollowDistance);

		}

		return;
	}

	//bot armor colors
	if(!strcasecmp(sep->arg[1], "armorcolor")) {
		if(c->GetTarget() && c->GetTarget()->IsBot() && (c->GetTarget()->CastToBot()->GetBotOwner() == c)) {

			if(sep->arg[2][0] == '\0' || sep->arg[3][0] == '\0' || sep->arg[4][0] == '\0' || sep->arg[5][0] == '\0') {
				c->Message(0, "Usage: #bot armorcolor [slot] [red] [green] [blue] - use #bot help armorcolor for info");
				return;
			}

			uint32 botid = c->GetTarget()->CastToBot()->GetBotID();
			std::string errorMessage;
			char* Query = 0;

			int setslot = atoi(sep->arg[2]);
			uint8 red = atoi(sep->arg[3]);
			uint8 green = atoi(sep->arg[4]);
			uint8 blue = atoi(sep->arg[5]);
			uint32 setcolor = (red << 16) | (green << 8) | blue;

			if(database.RunQuery(Query, MakeAnyLenString(&Query, "UPDATE botinventory SET color = %u WHERE slotID = %i AND botID = %u",setcolor, setslot, botid))){
				int slotmaterial = Inventory::CalcMaterialFromSlot(setslot);
				c->GetTarget()->CastToBot()->SendWearChange(slotmaterial);
			}
		}
		else {
			c->Message(15, "You must target a bot you own to do this.");
		}
		return;
	}
	// Help for coloring bot armor
		if(!strcasecmp(sep->arg[1], "help") && !strcasecmp(sep->arg[2], "armorcolor") ){
		//read from db
		char* Query = 0;
		MYSQL_RES* DatasetResult;
		MYSQL_ROW DataRow;
		
		c->Message(0, "-----------------#bot armorcolor help-----------------------------");
		c->Message(0, "Armor:  17(Chest/Robe), 7(Arms), 9(Bracer), 12(Hands), 18(Legs), 19(Boots), 2(Helm)");
		c->Message(0, "------------------------------------------------------------------");
		c->Message(0, "Color: [red] [green] [blue]  (enter a number from 0-255 for each");
		c->Message(0, "------------------------------------------------------------------");
		c->Message(0, "Example: #bot armorcolor 17 0 255 0 - this would make the chest bright green");
		return;
	}

	if(!strcasecmp(sep->arg[1], "augmentitem")) {
		AugmentItem_Struct* in_augment = new AugmentItem_Struct[sizeof(AugmentItem_Struct)];
		in_augment->container_slot = 1000;
		in_augment->unknown02[0] = 0;
		in_augment->unknown02[1] = 0;
		in_augment->augment_slot = -1;
		Object::HandleAugmentation(c, in_augment, c->GetTradeskillObject());
		return;
	}

	if(!strcasecmp(sep->arg[1], "giveitem")) {
		if(c->GetTarget() && c->GetTarget()->IsBot() && (c->GetTarget()->CastToBot()->GetBotOwner() == c)) {
			// Its a bot targetted and this client is the bots owner
			Bot* targetedBot = c->GetTarget()->CastToBot();
				if(targetedBot)
					targetedBot->FinishTrade(c, BotTradeClientNoDropNoTrade);
		}
		else {
			c->Message(15, "You must target a bot you own to do this.");
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "camp")) {
		if(!strcasecmp(sep->arg[2], "all")) {
			// Camp out all bots owned by this bot owner
			BotOrderCampAll(c);
		}
		else {
			// Camp only the targetted bot
			if(c->GetTarget() && c->GetTarget()->IsBot() && (c->GetTarget()->CastToBot()->GetBotOwner() == c)) {
				Bot* targetedBot = c->GetTarget()->CastToBot();
				if(targetedBot)
					targetedBot->Camp();
			}
			else
				c->Message(15, "You must target a bot you own to do this.");
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "create")) {
		if(sep->arg[2][0] == '\0' || sep->arg[3][0] == '\0' || sep->arg[4][0] == '\0' || sep->arg[5][0] == '\0' || sep->arg[6][0] != '\0') {
			c->Message(0, "Usage: #bot create [name] [class(id)] [race(id)] [gender (male/female)]");
			return;
		}
		else if(strcasecmp(sep->arg[3],"1") && strcasecmp(sep->arg[3],"2") && strcasecmp(sep->arg[3],"3") && strcasecmp(sep->arg[3],"4") && strcasecmp(sep->arg[3],"5") && strcasecmp(sep->arg[3],"6") && strcasecmp(sep->arg[3],"7") && strcasecmp(sep->arg[3],"8") && strcasecmp(sep->arg[3],"9") && strcasecmp(sep->arg[3],"10") && strcasecmp(sep->arg[3],"11") && strcasecmp(sep->arg[3],"12") && strcasecmp(sep->arg[3],"13") && strcasecmp(sep->arg[3],"14") && strcasecmp(sep->arg[3],"15") && strcasecmp(sep->arg[3],"16")) {
			c->Message(0, "Usage: #bot create [name] [class(id)] [race(id)] [gender (male/female)]");
			return;
		}		
		else if(strcasecmp(sep->arg[4],"1") && strcasecmp(sep->arg[4],"2") && strcasecmp(sep->arg[4],"3") && strcasecmp(sep->arg[4],"4") && strcasecmp(sep->arg[4],"5") && strcasecmp(sep->arg[4],"6") && strcasecmp(sep->arg[4],"7") && strcasecmp(sep->arg[4],"8") && strcasecmp(sep->arg[4],"9") && strcasecmp(sep->arg[4],"10") && strcasecmp(sep->arg[4],"11") && strcasecmp(sep->arg[4],"12") && strcasecmp(sep->arg[4],"330") && strcasecmp(sep->arg[4],"128") && strcasecmp(sep->arg[4],"130") && strcasecmp(sep->arg[4],"522")) {
			c->Message(0, "Usage: #bot create [name] [class(1-16)] [race(1-12,128,130,330,522)] [gender (male/female)]");
			return;
		}
		else if(strcasecmp(sep->arg[5],"male") && strcasecmp(sep->arg[5],"female")) {
			c->Message(0, "Usage: #bot create [name] [class(1-16)] [race(1-12,128,130,330,522)] [gender (male/female)]");
			return;
		}

		int32 MaxBotCreate = RuleI(Bots, CreateBotCount);
		if(SpawnedBotCount(c->CharacterID(), &TempErrorMessage) >= MaxBotCreate) {
			c->Message(0, "You cannot create more than %i bots.", MaxBotCreate);
			return;
		}

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			return;
		}

		int gender = 0;
		if(!strcasecmp(sep->arg[5], "female"))
			gender = 1;

		NPCType DefaultNPCTypeStruct = CreateDefaultNPCTypeStructForBot(std::string(sep->arg[2]), std::string(), c->GetLevel(), atoi(sep->arg[4]), atoi(sep->arg[3]), gender);
		Bot* NewBot = new Bot(DefaultNPCTypeStruct, c);

		if(NewBot) {
			if(!NewBot->IsValidRaceClassCombo()) {
				c->Message(0, "That Race/Class combination cannot be created.");
				return;
			}

			if(!NewBot->IsValidName()) {
				c->Message(0, "%s has invalid characters. You can use only the A-Z, a-z and _ characters in a bot name.", NewBot->GetCleanName());
				return;
			}

			if(!NewBot->IsBotNameAvailable(&TempErrorMessage)) {
				c->Message(0, "The name %s is already being used. Please choose a different name.", NewBot->GetCleanName());
				return;
			}

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				return;
			}

			// Now that all validation is complete, we can save our newly created bot
			if(!NewBot->Save())
				c->Message(0, "Unable to save %s as a bot.", NewBot->GetCleanName());
			else
				c->Message(0, "%s saved as bot %u.", NewBot->GetCleanName(), NewBot->GetBotID());
		}
		else {
			// TODO: Log error message here
		}

		// Bot creation is complete
		return;
	}

	if(!strcasecmp(sep->arg[1], "help") && !strcasecmp(sep->arg[2], "create") ){
		c->Message(0, "Classes:  1(Warrior), 2(Cleric), 3(Paladin), 4(Ranger), 5(Sk), 6(Druid), 7(Monk), 8(Bard), 9(Rogue), 10(Shaman), 11(Necro), 12(Wiz), 13(Mag), 14(Ench), 15(Beast), 16(Bersek)");
		c->Message(0, "------------------------------------------------------------------");
		c->Message(0, "Races: 1(Human), 2(Barb), 3(Erudit), 4(Wood elf), 5(High elf), 6(Dark elf), 7(Half elf), 8(Dwarf), 9(Troll), 10(Ogre), 11(Halfling), 12(Gnome), 128(Iksar), 130(Vah shir), 330(Froglok), 522(Drakkin)");
		c->Message(0, "------------------------------------------------------------------");
		c->Message(0, "Usage: #bot create [name] [class(1-16)] [race(1-12,128,130,330,522)] [gender(male/female)]");
		c->Message(0, "Example: #bot create Sneaky 9 6 male");
		return;
	}

	if(!strcasecmp(sep->arg[1], "delete") ) {
		if((c->GetTarget() == NULL) || !c->GetTarget()->IsBot())
		{
			c->Message(15, "You must target a bot!");
			return;
		}
		else if(c->GetTarget()->CastToBot()->GetBotOwnerCharacterID() != c->CharacterID())
		{
			c->Message(15, "You can't delete a bot that you don't own.");
			return;
		}

		if(c->GetTarget()->IsBot()) {
			Bot* BotTargeted = c->GetTarget()->CastToBot();

			if(BotTargeted) {
				BotTargeted->DeleteBot(&TempErrorMessage);

				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					return;
				}

				BotTargeted->Camp(false);
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "list")) {
		bool listAll = true;
		int iClass = atoi(sep->arg[2]);

		if(iClass > 0 && iClass < 17)
			listAll = false;

		std::list<BotsAvailableList> AvailableBots = GetBotList(c->CharacterID(), &TempErrorMessage);

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			return;
		}

		if(!AvailableBots.empty()) {
			for(std::list<BotsAvailableList>::iterator TempAvailableBotsList = AvailableBots.begin(); TempAvailableBotsList != AvailableBots.end(); TempAvailableBotsList++) {				
				if(!listAll && TempAvailableBotsList->BotClass != iClass)
					continue;

				c->Message(0, "Name: %s -- Class: %s -- Level: %u -- Race: %s", TempAvailableBotsList->BotName, ClassIdToString(TempAvailableBotsList->BotClass).c_str(), TempAvailableBotsList->BotLevel, RaceIdToString(TempAvailableBotsList->BotRace).c_str());
			}
		}
		else {
			c->Message(0, "You have no bots created. Use the #bot create command to create a bot.");
		}
	}

	if(!strcasecmp(sep->arg[1], "mana")) {
		bool listAll = false;
		Bot* bot = 0;

		if(sep->argnum == 2) {
			if(std::string(sep->arg[2]).compare("all") == 0)
				listAll = true;
			else {
				string botName = std::string(sep->arg[2]);
				
				Bot* tempBot = entity_list.GetBotByBotName(botName);
				
				if(tempBot && tempBot->GetBotOwner() == c) {
					bot = tempBot;
				}
			}
		}
		else {
			if(c->GetTarget() && c->GetTarget()->IsBot())
				bot = c->GetTarget()->CastToBot();
		}

		if(bot && !listAll) {
			// Specific bot only
			if(bot->GetClass() !=  WARRIOR && bot->GetClass() !=  MONK && bot->GetClass() !=  BARD && bot->GetClass() !=  BERSERKER && bot->GetClass() !=  ROGUE)
				c->Message(0, "Name: %s -- Class: %s -- Mana: %3.1f%%", bot->GetCleanName(), ClassIdToString(bot->GetClass()).c_str(), bot->GetManaRatio());
		}
		else {
			// List all
			std::list<Bot*> spawnedBots = entity_list.GetBotsByBotOwnerCharacterID(c->CharacterID());

			if(!spawnedBots.empty()) {
				for(std::list<Bot*>::iterator botsListItr = spawnedBots.begin(); botsListItr != spawnedBots.end(); botsListItr++) {
					Bot* tempBot = *botsListItr;
					if(tempBot) {
						if(tempBot->GetClass() !=  WARRIOR && tempBot->GetClass() !=  MONK && tempBot->GetClass() !=  BARD && tempBot->GetClass() !=  BERSERKER && tempBot->GetClass() !=  ROGUE)
							c->Message(0, "Name: %s -- Class: %s -- Mana: %3.1f%%", tempBot->GetCleanName(), ClassIdToString(tempBot->GetClass()).c_str(), tempBot->GetManaRatio());
					}
				}
			}
			else {
				c->Message(0, "You have no spawned bots in this zone.");
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "spawn") ) {
		uint32 botId = GetBotIDByBotName(std::string(sep->arg[2]));

		if(GetBotOwnerCharacterID(botId, &TempErrorMessage) != c->CharacterID()) {
			c->Message(0, "You can't spawn a bot that you don't own.");
			return;
		}

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			return;
		}

		if(c->GetFeigned()) {
			c->Message(0, "You can't summon bots while you are feigned.");
			return;
		}

		/*if(c->GetBotRaidID() > 0) {
			BotRaids *br = entity_list.GetBotRaidByMob(c->CastToMob());
			if(br) {
				if(br->GetBotRaidAggro()) {
					c->Message(15, "You can't summon bots while you are engaged.");
					return;
				}
			}
		}*/

		if(c->IsGrouped()) {
			Group *g = entity_list.GetGroupByClient(c);
			for (int i=0; i<MAX_GROUP_MEMBERS; i++) {
				if(g && g->members[i] && !g->members[i]->qglobal && (g->members[i]->GetAppearance() != eaDead) && g->members[i]->IsEngaged()) {
					c->Message(0, "You can't summon bots while you are engaged.");
					return;
				}
				if(g && g->members[i] && g->members[i]->qglobal) {
					return;
				}
			}
		}

		Mob* TempBotMob = entity_list.GetMobByBotID(botId);

		if(TempBotMob) {
			c->Message(0, "This bot is already in the zone.");
			return;
		}

		int spawnedBotCount = SpawnedBotCount(c->CharacterID(), &TempErrorMessage);

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			return;
		}

		if(RuleB(Bots, BotQuest) && !c->GetGM()) {
			const int allowedBots = AllowedBotSpawns(c->CharacterID(), &TempErrorMessage);

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				return;
			}

			if(allowedBots == 0) {
				c->Message(0, "You cannot spawn any bots.");
				return;
			}

			if(spawnedBotCount >= allowedBots) {
				c->Message(0, "You cannot spawn more than %i bots.", spawnedBotCount);
				return;
			}

		}

		if(spawnedBotCount >= RuleI(Bots, SpawnBotCount) && !c->GetGM()) {
			c->Message(0, "You cannot spawn more than %i bots.", spawnedBotCount);
			return;
		}

		Bot* TempBot = LoadBot(botId, &TempErrorMessage);

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			safe_delete(TempBot);
			return;
		}

		if(TempBot) {
			// We have a bot loaded from the database
			TempBot->Spawn(c, &TempErrorMessage);

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				safe_delete(TempBot);
				return;
			}
			
			TempBot->CastToMob()->Say("I am ready for battle.");
		}
		else {
			// We did not find a bot for the specified bot id from the database
			c->Message(0, "BotID: %i not found", atoi(sep->arg[2]));
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "archery")) {
		if((c->GetTarget() == NULL) || (c->GetTarget() == c) || !c->GetTarget()->IsBot()) {
			c->Message(15, "You must target a bot!");
			return;
		}

		Bot* archerBot = c->GetTarget()->CastToBot();

		if(archerBot) {
			if(archerBot->IsBotArcher())
				archerBot->SetBotArcher(false);
			else
				archerBot->SetBotArcher(true);

			archerBot->ChangeBotArcherWeapons(archerBot->IsBotArcher());

			if(archerBot->GetClass() == RANGER && archerBot->GetLevel() >= 61)
				archerBot->SetRangerAutoWeaponSelect(archerBot->IsBotArcher());
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "picklock")) {
		if((c->GetTarget() == NULL) || (c->GetTarget() == c) || !c->GetTarget()->IsBot() || (c->GetTarget()->GetClass() != ROGUE)) {
			c->Message(15, "You must target a rogue bot!");
		}
		else {
			entity_list.BotPickLock(c->GetTarget()->CastToBot());
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "summon")) {
		if((c->GetTarget() == NULL) || (c->GetTarget() == c) || !c->GetTarget()->IsBot() || c->GetTarget()->IsPet())
		{
			c->Message(15, "You must target a bot!");
		}
		else if(c->GetTarget()->IsMob() && !c->GetTarget()->IsPet())
		{
			Mob *b = c->GetTarget();
			if(b) 
			{
				// Is our target "botable" ?
				if(!b->IsBot()){
					c->Message(15, "You must target a bot!");
				}
				else if((b->CastToBot()->GetBotOwnerCharacterID() != c->CharacterID()))
				{
					b->Say("You can only summon your own bots.");
				}
				else 
				{
					b->SetTarget(c->CastToMob());
					b->Warp(c->GetX(), c->GetY(), c->GetZ());
				}
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "inventory") && !strcasecmp(sep->arg[2], "list")) {
		if(c->GetTarget() != NULL) {
			if(c->GetTarget()->IsBot() && c->GetTarget()->CastToBot()->GetBotOwnerCharacterID() == c->CharacterID()) {
				Mob* b = c->GetTarget();	
				int x = c->GetTarget()->CastToBot()->GetBotItemsCount(&TempErrorMessage);

				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					return;
				}

				const char* equipped[22] = {"Charm", "Left Ear", "Head", "Face", "Right Ear", "Neck", "Shoulders", "Arms", "Back",
					"Left Wrist", "Right Wrist", "Range", "Hands", "Primary Hand", "Secondary Hand",
					"Left Finger", "Right Finger", "Chest", "Legs", "Feet", "Waist", "Ammo" };
				const ItemInst* item1 = NULL;
				const Item_Struct* item2 = NULL;
				bool is2Hweapon = false;
				for(int i=0; i<22; ++i)
				{
					if((i == 14) && is2Hweapon) {
						continue;
					}

					item1 = b->CastToBot()->GetBotItem(i);
					if(item1)
						item2 = item1->GetItem();
					else
						item2 = NULL;

					if(!TempErrorMessage.empty()) {
						c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
						return;
					}
					if(item2 == 0) {
						c->Message(15, "I need something for my %s (Item %i)", equipped[i], i);
						continue;
					}
					if((i == 13) && ((item2->ItemType == ItemType2HS) || (item2->ItemType == ItemType2HB) || (item2->ItemType == ItemType2HPierce))) {
						is2Hweapon = true;
					}

					char* itemLink = 0;
					if((i == 0) || (i == 11) || (i == 13) || (i == 14) || (i == 21)) {
						if (c->GetClientVersion() >= EQClientSoF)
						{
							MakeAnyLenString(&itemLink, "%1X" "%05X" "%05X" "%05X" "%05X" "%05X" "%05X" "%1X" "%04X" "%1X" "%05X" "%08X", 
								0,
								item2->ID, 
								item1->GetAugmentItemID(0), 
								item1->GetAugmentItemID(1), 
								item1->GetAugmentItemID(2), 
								item1->GetAugmentItemID(3), 
								item1->GetAugmentItemID(4), 
								0, 
								0, 
								0,
								0,
								0
								);
							c->Message(15, "Using %c%s%s%c in my %s (Item %i)", 0x12, itemLink, item2->Name, 0x12, equipped[i], i);
						}
						else
						{
							MakeAnyLenString(&itemLink, "%1X" "%05X" "%05X" "%05X" "%05X" "%05X" "%05X" "%1X" "%04X" "%1X" "%08X",
								0,
								item2->ID, 
								item1->GetAugmentItemID(0), 
								item1->GetAugmentItemID(1), 
								item1->GetAugmentItemID(2), 
								item1->GetAugmentItemID(3), 
								item1->GetAugmentItemID(4), 
								0, 
								0, 
								0, 
								0);
							c->Message(15, "Using %c%s%s%c in my %s (Item %i)", 0x12, itemLink, item2->Name, 0x12, equipped[i], i);
						}
					}
					else {
						if (c->GetClientVersion() >= EQClientSoF)
						{
							MakeAnyLenString(&itemLink, "%1X" "%05X" "%05X" "%05X" "%05X" "%05X" "%05X" "%1X" "%04X" "%1X" "%05X" "%08X", 
								0,
								item2->ID, 
								item1->GetAugmentItemID(0), 
								item1->GetAugmentItemID(1), 
								item1->GetAugmentItemID(2), 
								item1->GetAugmentItemID(3), 
								item1->GetAugmentItemID(4), 
								0, 
								0, 
								0,
								0,
								0
								);
							c->Message(15, "Using %c%s%s%c in my %s (Item %i)", 0x12, itemLink, item2->Name, 0x12, equipped[i], i);
						}
						else
						{
							MakeAnyLenString(&itemLink, "%1X" "%05X" "%05X" "%05X" "%05X" "%05X" "%05X" "%1X" "%04X" "%1X" "%08X",
								0,
								item2->ID, 
								item1->GetAugmentItemID(0), 
								item1->GetAugmentItemID(1), 
								item1->GetAugmentItemID(2), 
								item1->GetAugmentItemID(3), 
								item1->GetAugmentItemID(4), 
								0, 
								0, 
								0, 
								0);
							c->Message(15, "Using %c%s%s%c in my %s (Item %i)", 0x12, itemLink, item2->Name, 0x12, equipped[i], i);
						}
					}
				}
			}
			else {
				c->Message(15, "You must group your bot first.");
			}
		}
		else {
			c->Message(15, "You must target a bot first.");
		}
		return;
	}

	if(!strcasecmp(sep->arg[1], "inventory") && !strcasecmp(sep->arg[2], "remove")) {
		if((c->GetTarget() == NULL) || (sep->arg[3] == '\0') || !c->GetTarget()->IsBot())
		{
			c->Message(15, "Usage: #bot inventory remove [slotid] (You must have a bot targetted) ");
			return;
		}		
		else if(c->GetTarget()->IsBot() && c->GetTarget()->CastToBot()->GetBotOwnerCharacterID() == c->CharacterID())
		{
			if(c->GetTradeskillObject() || (c->trade->state == Trading))
				return;

			int slotId = atoi(sep->arg[3]);
			if(slotId > 21 || slotId < 0) {
				c->Message(15, "A bot has 21 slots in its inventory, please choose a slot between 0 and 21.");
				return;
			}
			const char* equipped[22] = {"Charm", "Left Ear", "Head", "Face", "Right Ear", "Neck", "Shoulders", "Arms", "Back",
										"Left Wrist", "Right Wrist", "Range", "Hands", "Primary Hand", "Secondary Hand",
										"Left Finger", "Right Finger", "Chest", "Legs", "Feet", "Waist", "Ammo" };

			const Item_Struct* itm = NULL;
			const ItemInst* itminst = c->GetTarget()->CastToBot()->GetBotItem(slotId);
			if(itminst)
				itm = itminst->GetItem();

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				return;
			}

			// Don't allow the player to remove a lore item they already possess and cause a crash
			bool failedLoreCheck = false;
			if(itminst) {
				for(int m=0; m<MAX_AUGMENT_SLOTS; ++m) {
					ItemInst *itma = itminst->GetAugment(m);
					if(itma)
					{
						if(c->CheckLoreConflict(itma->GetItem())) {
							failedLoreCheck = true;
						}
					}
				}
				if(c->CheckLoreConflict(itm)) {
					failedLoreCheck = true;
				}
			}
			if(!failedLoreCheck) {
				if(itm) {
					c->PushItemOnCursor(*itminst, true);
					Bot *gearbot = c->GetTarget()->CastToBot();
					if((slotId == SLOT_RANGE)||(slotId == SLOT_AMMO)||(slotId == SLOT_PRIMARY)||(slotId == SLOT_SECONDARY)) {
						gearbot->SetBotArcher(false);
					}
					gearbot->RemoveBotItemBySlot(slotId, &TempErrorMessage);

					if(!TempErrorMessage.empty()) {
						c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
						return;
					}

					gearbot->BotRemoveEquipItem(slotId);
					gearbot->CalcBotStats();
					switch(slotId) {
						case 0:
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 8:
						case 9:
						case 10:
						case 11:
						case 13:
						case 14:
						case 15:
						case 16:
						case 17:
						case 20:
						case 21:
							gearbot->Say("My %s is now unequipped.", equipped[slotId]);
							break;
						case 6:
						case 7:
						case 12:
						case 18:
						case 19:
							gearbot->Say("My %s are now unequipped.", equipped[slotId]);
							break;
						default:
							break;
					}
				}
				else {
					switch(slotId) {
						case 0:
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 8:
						case 9:
						case 10:
						case 11:
						case 13:
						case 14:
						case 15:
						case 16:
						case 17:
						case 20:
						case 21:
							c->GetTarget()->Say("My %s is already unequipped.", equipped[slotId]);
							break;
						case 6:
						case 7:
						case 12:
						case 18:
						case 19:
							c->GetTarget()->Say("My %s are already unequipped.", equipped[slotId]);
							break;
						default:
							break;
					}
				}
			}
			else {
				c->Message_StringID(0, PICK_LORE);
			}
		}
		return;
	}

	if(!strcasecmp(sep->arg[1], "update")) {
		// Congdar: add IsEngaged check for exploit to keep bots alive by repeatedly using #bot update.
		if((c->GetTarget() != NULL) && c->GetTarget()->IsBot()) {
			if(c->GetLevel() <= c->GetTarget()->GetLevel()) {
				c->Message(15, "This bot has already been updated.");
				return;
			}

			if(c->IsGrouped())
			{
				Group *g = entity_list.GetGroupByClient(c);
				for (int i=0; i<MAX_GROUP_MEMBERS; i++)
				{
					if(g && g->members[i] && g->members[i]->IsEngaged())
					{
						c->Message(15, "You can't update bots while you are engaged.");
						return;
					}
				}
			}

			if((c->GetTarget()->CastToBot()->GetBotOwner() == c->CastToMob()) && !c->GetFeigned()) {
				Bot* bot = c->GetTarget()->CastToBot();
				//bot->SetLevel(c->GetLevel());
				bot->SetPetChooser(false);
				bot->CalcBotStats();
			}
			else {
				if(c->GetFeigned()) {
					c->Message(15, "You cannot update bots while feigned.");
				}
				else {
					c->Message(15, "You must target your bot first");
				}
			}
		}
		else {
			c->Message(15, "You must target a bot first");
		}

		return;
	}

	//Bind
	if(!strcasecmp(sep->arg[1], "bindme")) {
		Mob *binder = NULL;
		bool hasbinder = false;
		if(c->IsGrouped())
		{
			Group *g = c->GetGroup();
			if(g) {
				for(int i=0; i<MAX_GROUP_MEMBERS; i++)
				{
					if(g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == CLERIC))
					{
						hasbinder = true;
						binder = g->members[i];
					}
				}
				if(!hasbinder) {
					c->Message(15, "You must have a Cleric in your group.");
				}
			}
		}
		if(hasbinder) {
			binder->Say("Attempting to bind you %s.", c->GetName());
			binder->CastToNPC()->CastSpell(35, c->GetID(), 1, -1, -1);
		}
		return;
	}

	// Rune
	if(!strcasecmp(sep->arg[1], "runeme")) {
		Mob *runeer = NULL;
		bool hasruneer = false;
		if(c->IsGrouped())
		{
			Group *g = c->GetGroup();
			if(g) {
				for(int i=0; i<MAX_GROUP_MEMBERS; i++)
				{
					if(g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == ENCHANTER))
					{
						hasruneer = true;
						runeer = g->members[i];
					}
				}
				if(!hasruneer) {
					c->Message(15, "You must have an Enchanter in your group.");
				}
			}
		}
		if(hasruneer) {
			if      (c->GetLevel() <= 12) {
				runeer->Say("I need to be level 13 or higher for this...");
			}
			else if ((c->GetLevel() >= 13) && (c->GetLevel() <= 21)) {
				runeer->Say("Casting Rune I...");
				runeer->CastSpell(481, c->GetID(), 1, -1, -1);
			}
			else if ((c->GetLevel() >= 22) && (c->GetLevel() <= 32)) {
				runeer->Say("Casting Rune II...");
				runeer->CastSpell(482, c->GetID(), 1, -1, -1);
			}
			else if ((c->GetLevel() >= 33) && (c->GetLevel() <= 39)) { 
				runeer->Say("Casting Rune III...");
				runeer->CastSpell(483, c->GetID(), 1, -1, -1);
			}
			else if ((c->GetLevel() >= 40) && (c->GetLevel() <= 51)) { 
				runeer->Say("Casting Rune IV...");
				runeer->CastSpell(484, c->GetID(), 1, -1, -1);
			}
			else if ((c->GetLevel() >= 52) && (c->GetLevel() <= 60)) { 
				runeer->Say("Casting Rune V...");
				runeer->CastSpell(1689, c->GetID(), 1, -1, -1);
			}
			else if (c->GetLevel() >= 61){ 
				runeer->Say("Casting Rune of Zebuxoruk...");
				runeer->CastSpell(3343, c->GetID(), 1, -1, -1);
			}
		}
		return;
	}

	//Tracking
	if(!strcasecmp(sep->arg[1], "track") && c->IsGrouped()) {
		Mob *Tracker;
		int32 TrackerClass = 0;

		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++) {
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case RANGER:
							Tracker = g->members[i];
							TrackerClass = RANGER;
							break;
						case DRUID:
							// Unless we have a ranger, druid is next best.
							if(TrackerClass != RANGER) {
								Tracker = g->members[i];
								TrackerClass = DRUID;
							}
							break;
						case BARD:
							// If we haven't found a tracker yet, use bard.
							if(TrackerClass == 0) {
								Tracker = g->members[i];
								TrackerClass = BARD;
							}
							break;
						default:
							break;
					}
				}
			}

			int Level = (c->GetLevel());
			int RangeR = (Level*80); //Ranger
			int RangeD = (Level*30); //Druid
			int RangeB = (Level*20); //Bard
			switch(TrackerClass) {
				case RANGER:
					if(!strcasecmp(sep->arg[2], "all")) {
						Tracker->Say("Tracking everything", c->GetName());
						entity_list.ShowSpawnWindow(c, RangeR, false);
					}
					else if(!strcasecmp(sep->arg[2], "rare")) { 
						Tracker->Say("Selective tracking", c->GetName());
						entity_list.ShowSpawnWindow(c, RangeR, true);
					}
					else if(!strcasecmp(sep->arg[2], "near")) { 
						Tracker->Say("Tracking mobs nearby", c->GetName());
						entity_list.ShowSpawnWindow(c, RangeD, false);
					}
					else 
						Tracker->Say("You want to [track all], [track near], or [track rare]?", c->GetName());

					break;

				case BARD:

					if(TrackerClass != RANGER)
						Tracker->Say("Tracking up", c->GetName());
					entity_list.ShowSpawnWindow(c, RangeB, false);
					break;

				case DRUID:

					if(TrackerClass = BARD)
						Tracker->Say("Tracking up", c->GetName());
					entity_list.ShowSpawnWindow(c, RangeD, false);
					break;

				default:
					c->Message(15, "You must have a Ranger, Druid, or Bard in your group.");
					break;
			}
		}
	}

	//Cure
	if ((!strcasecmp(sep->arg[1], "cure")) && (c->IsGrouped())) {
		Mob *Curer;
		int32 CurerClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case CLERIC:
							Curer = g->members[i];
							CurerClass = CLERIC;
							break;
						case SHAMAN:
							if(CurerClass != CLERIC){
								Curer = g->members[i];
								CurerClass = SHAMAN;
							}
						case DRUID:
							if (CurerClass == 0){
								Curer = g->members[i];
								CurerClass = DRUID;
							}
							break;
							break;
						default:
							break;
					}
				}
			}
			switch(CurerClass) {
				case CLERIC:
					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 1))  {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(1, Curer->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 4)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(2, Curer->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "curse") && (c->GetLevel() >= 8)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(3, Curer->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "blindness") && (c->GetLevel() >= 3)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(4, Curer->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "curse") && (c->GetLevel() <= 8)
						|| !strcasecmp(sep->arg[2], "blindness") && (c->GetLevel() <= 3) 
						|| !strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 4)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 1)) {
							Curer->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Curer->Say("Do you want [cure poison], [cure disease], [cure curse], or [cure blindness]?", c->GetName());

					break;

				case SHAMAN:
					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 2))  {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(1, Curer->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 1)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(2, Curer->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "curse")) {
						Curer->Say("I don't have that spell", sep->arg[2]);
					}
					else if(!strcasecmp(sep->arg[2], "blindness") && (c->GetLevel() >= 7)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(4, Curer->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "blindness") && (c->GetLevel() <= 7) 
						|| !strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 1)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 2)) {
							Curer->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else 
						Curer->Say("Do you want [cure poison], [cure disease], or [cure blindness]?", c->GetName());

					break;

				case DRUID:

					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 5)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(1, Curer->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 4)) {
						Curer->Say("Trying to cure us of %s.", sep->arg[2]);
						Curer->CastToBot()->Bot_Command_Cure(2, Curer->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "curse")) { // Fire level 1
						Curer->Say("I don't have that spell", sep->arg[2]);
					}
					else if(!strcasecmp(sep->arg[2], "blindness") && (c->GetLevel() >= 13)) {
						Curer->Say("I don't have that spell", sep->arg[2]);
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 4)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 5)) {
							Curer->Say("I don't have the needed level yet", sep->arg[2]) ;
					}
					else 
						Curer->Say("Do you want [cure poison], or [cure disease]?", c->GetName());

					break;

				default:
					c->Message(15, "You must have a Cleric, Shaman, or Druid in your group.");
					break;
			}
		}
	}

	//Mez
	if(!strcasecmp(sep->arg[1], "ai") && !strcasecmp(sep->arg[2], "mez"))
	{
		Mob *target = c->GetTarget();
		if(target == NULL || target == c || target->IsBot() || (target->IsPet() && target->GetOwner()->IsBot()))
		{
			c->Message(15, "You must select a monster");
			return;
		}

		if(c->IsGrouped())
		{
			bool hasmezzer = false;
			Group *g = c->GetGroup();
			for(int i=0; i<MAX_GROUP_MEMBERS; i++)
			{
				if(g && g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == ENCHANTER))
				{
					hasmezzer = true;
					Mob *mezzer = g->members[i];
					mezzer->Say("Trying to mez %s \n", target->GetCleanName());
					mezzer->CastToBot()->MesmerizeTarget(target);
				}
			}
			if(!hasmezzer) {
				c->Message(15, "You must have an Enchanter in your group.");
			}
		}
		return;
	}

	//Lore (Identify item)
	if(!strcasecmp(sep->arg[1], "lore")) {
		if(c->IsGrouped())
		{
			bool hascaster = false;
			Group *g = c->GetGroup();
			for(int i=0; i<MAX_GROUP_MEMBERS; i++)
			{
				if(g && g->members[i] && g->members[i]->IsBot()) {
					uint8 casterlevel = g->members[i]->GetLevel();
					switch(g->members[i]->GetClass()) {
						case ENCHANTER:
							if(casterlevel >= 15) {
								hascaster = true;
							}
							break;
						case WIZARD:
							if(casterlevel >= 14) {
								hascaster = true;
							}
							break;
						case NECROMANCER:
							if(casterlevel >= 17) {
								hascaster = true;
							}
							break;
						case MAGICIAN:
							if(casterlevel >= 13) {
								hascaster = true;
							}
							break;
						default:
							break;
					}
					if(hascaster) {
						g->members[i]->Say("Trying to Identify your item...");
						g->members[i]->CastSpell(305, c->GetID(), 1, -1, -1);
						break;
					}
				}
			}
			if(!hascaster) {
				c->Message(15, "You don't see anyone in your group that can cast Identify.");
			}
		}
		else {
			c->Message(15, "You don't see anyone in your group that can cast Identify.");
		}
		return;
	}

	//Resurrect
	if(!strcasecmp(sep->arg[1], "resurrectme"))
	{
		Mob *target = c->GetTarget();
		if(target == NULL || !target->IsCorpse())
		{
			c->Message(15, "You must select a corpse");
			return;
		}

		if(c->IsGrouped())
		{
			bool hasrezzer = false;
			Group *g = c->GetGroup();
			for(int i=0; i<MAX_GROUP_MEMBERS; i++)
			{
				if(g && g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == CLERIC))
				{
					hasrezzer = true;
					Mob *rezzer = g->members[i];
					rezzer->Say("Trying to rez %s", target->GetCleanName());
					rezzer->CastToBot()->Bot_Command_RezzTarget(target);
					break;
				}
			}
			if(!hasrezzer) {
				c->Message(15, "You must have a Cleric in your group.");
			}
		}
		else {
			c->Message(15, "You must have a Cleric in your group.");
		}
		return;
	}

	if(!strcasecmp(sep->arg[1], "magepet"))
	{
		if(c->GetTarget() && c->GetTarget()->IsBot() && (c->GetTarget()->GetClass() == MAGICIAN))
		{
			if(c->GetTarget()->CastToBot()->GetBotOwnerCharacterID() == c->CharacterID())
			{
				int botlevel = c->GetTarget()->GetLevel();
				c->GetTarget()->CastToBot()->SetPetChooser(true);
				if(botlevel == 1)
				{
					c->GetTarget()->Say("I don't have any pets yet.");
					return;
				}
				if(!strcasecmp(sep->arg[2], "water"))
				{
					c->GetTarget()->CastToBot()->SetPetChooserID(0);
				}
				else if(!strcasecmp(sep->arg[2], "fire"))
				{
					if(botlevel < 3)
					{
						c->GetTarget()->Say("I don't have that pet yet.");
						return;
					}
					else
					{
						c->GetTarget()->CastToBot()->SetPetChooserID(1);
					}
				}
				else if(!strcasecmp(sep->arg[2], "air"))
				{
					if(botlevel < 4)
					{
						c->GetTarget()->Say("I don't have that pet yet.");
						return;
					}
					else
					{
						c->GetTarget()->CastToBot()->SetPetChooserID(2);
					}
				}
				else if(!strcasecmp(sep->arg[2], "earth"))
				{
					if(botlevel < 5)
					{
						c->GetTarget()->Say("I don't have that pet yet.");
						return;
					}
					else
					{
						c->GetTarget()->CastToBot()->SetPetChooserID(3);
					}
				}
				else if(!strcasecmp(sep->arg[2], "monster"))
				{
					if(botlevel < 30)
					{
						c->GetTarget()->Say("I don't have that pet yet.");
						return;
					}
					else
					{
						c->GetTarget()->CastToBot()->SetPetChooserID(4);
					}
				}
				if(c->GetTarget()->GetPet())
				{
					// cast reclaim energy
					int16 id = c->GetTarget()->GetPetID();
					c->GetTarget()->SetPetID(0);
					c->GetTarget()->CastSpell(331, id);
				}
			}
		}
		else
		{
			c->Message(15, "You must target your Magician bot.");
		}
		return;
	}

	//Summon Corpse
	if(!strcasecmp(sep->arg[1], "corpse") && !strcasecmp(sep->arg[2], "summon")) {
		if(c->GetTarget() == NULL) {
			c->Message(15, "You must select player with his corpse in the zone.");
			return;
		}
		if(c->IsGrouped()) {
			bool hassummoner = false;
			Mob *t = c->GetTarget();
			Group *g = c->GetGroup();
			int summonerlevel = 0;
			for(int i=0; i<MAX_GROUP_MEMBERS; i++) {
				if(g && g->members[i] && g->members[i]->IsBot() && ((g->members[i]->GetClass() == NECROMANCER)||(g->members[i]->GetClass() == SHADOWKNIGHT))) {
					hassummoner = true;
					summonerlevel = g->members[i]->GetLevel();
					g->members[i]->InterruptSpell();
					if(!t->IsClient()) {
						g->members[i]->Say("You have to target a player with a corpse in the zone");
						return;
					}
					else if(summonerlevel < 12) {
						g->members[i]->Say("I don't have that spell yet.");
					}
					else if((summonerlevel > 11) && (summonerlevel < 35)) {
						g->members[i]->Say("Attempting to summon %s\'s corpse.", t->GetCleanName());
						g->members[i]->CastSpell(2213, t->GetID(), 1, -1, -1);
						return;
					}
					else if((summonerlevel > 34) && (summonerlevel < 71)) {
						g->members[i]->Say("Attempting to summon %s\'s corpse.", t->GetCleanName());
						g->members[i]->CastSpell(3, t->GetID(), 1, -1, -1);
						return;
					}
					else if(summonerlevel > 70) {
						g->members[i]->Say("Attempting to summon %s\'s corpse.", t->GetCleanName());
						g->members[i]->CastSpell(10042, t->GetID(), 1, -1, -1);
						return;
					}
				}
			}
			if (!hassummoner) {
				c->Message(15, "You must have a Necromancer or Shadowknight in your group.");
			}
			return;
		}
	}

	//Pacify
	if(!strcasecmp(sep->arg[1], "target") && !strcasecmp(sep->arg[2], "calm"))
	{
		Mob *target = c->GetTarget();

		if(target == NULL || target->IsClient() || target->IsBot() || target->IsPet() && target->GetOwner()->IsBot())
			c->Message(15, "You must select a monster");
		else {
			if(c->IsGrouped()) {
				Group *g = c->GetGroup();

				for(int i=0; i<MAX_GROUP_MEMBERS; i++) {
					// seperated cleric and chanter so chanter is primary
					if(g && g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == ENCHANTER)) {
						Bot *pacer = g->members[i]->CastToBot();
						pacer->Say("Trying to pacify %s \n", target->GetCleanName());

						if(pacer->Bot_Command_CalmTarget(target)) {
							if(target->FindType(SE_Lull) || target->FindType(SE_Harmony) || target->FindType(SE_Calm))
							//if(pacer->IsPacified(target))
								c->Message(0, "I have successfully pacified %s.", target->GetCleanName());
								return;
							/*else
								c->Message(0, "I failed to pacify %s.", target->GetCleanName());*/
						}
						else
							c->Message(0, "I failed to pacify %s.", target->GetCleanName());
					}
					// seperated cleric and chanter so chanter is primary
					if(g && g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == CLERIC) && (GroupHasEnchanterClass(g) == false)) {
						Bot *pacer = g->members[i]->CastToBot();
						pacer->Say("Trying to pacify %s \n", target->GetCleanName());

						if(pacer->Bot_Command_CalmTarget(target)) {
							if(target->FindType(SE_Lull) || target->FindType(SE_Harmony) || target->FindType(SE_Calm))
							//if(pacer->IsPacified(target))
								c->Message(0, "I have successfully pacified %s.", target->GetCleanName());
								return;
							/*else
								c->Message(0, "I failed to pacify %s.", target->GetCleanName());*/
						}
						else
							c->Message(0, "I failed to pacify %s.", target->GetCleanName());
					}
					/*else
						c->Message(15, "You must have an Enchanter or Cleric in your group.");*/
				}
			}
		}

		return;
	}

	//Charm
	if(!strcasecmp(sep->arg[1], "charm"))
	{
		Mob *target = c->GetTarget();
		if(target == NULL || target->IsClient() || target->IsBot() || (target->IsPet() && target->GetOwner()->IsBot()))
		{
			c->Message(15, "You must select a monster");
			return;
		}
		int32 DBtype = c->GetTarget()->GetBodyType();
		Mob *Charmer;
		int32 CharmerClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case ENCHANTER:
							Charmer = g->members[i];
							CharmerClass = ENCHANTER;
							break;
						case NECROMANCER:
							if(CharmerClass != ENCHANTER){
								Charmer = g->members[i];
								CharmerClass = NECROMANCER;
							}
						case DRUID:
							if (CharmerClass == 0){
								Charmer = g->members[i];
								CharmerClass = DRUID;
							}
							break;
							break;
						default:
							break;
					}
				}
			}
			switch(CharmerClass) {
				case ENCHANTER:
					if	(c->GetLevel() >= 11) {
						Charmer->Say("Trying to charm %s \n", target->GetCleanName(), sep->arg[2]);
						Charmer->CastToBot()->Bot_Command_CharmTarget (1,target);
					}
					else if (c->GetLevel() <= 10){
						Charmer->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Charmer->Say("Mob level is too high or can't be charmed", c->GetName());
					break;

				case NECROMANCER:
					if	((c->GetLevel() >= 18) && (DBtype == 3)) {
						Charmer->Say("Trying to Charm %s \n", target->GetCleanName(), sep->arg[2]);
						Charmer->CastToBot()->Bot_Command_CharmTarget (2,target);
					}
					else if (c->GetLevel() <= 17){
						Charmer->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Charmer->Say("Mob Is not undead...", c->GetName());
					break;

				case DRUID:
					if	((c->GetLevel() >= 13) && (DBtype == 21)) {
						Charmer->Say("Trying to charm %s \n", target->GetCleanName(), sep->arg[2]);
						Charmer->CastToBot()->Bot_Command_CharmTarget (3,target);
					}
					else if (c->GetLevel() <= 12){
						Charmer->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Charmer->Say("Mob is not an animal...", c->GetName());
					break;

				default:
					c->Message(15, "You must have an Enchanter, Necromancer or Druid in your group.");
					break;
			}
		}
	}

	// Remove Bot's Pet
	if(!strcasecmp(sep->arg[1], "pet") && !strcasecmp(sep->arg[2], "remove")) {
		if(c->GetTarget() != NULL) {
			if (c->IsGrouped() && c->GetTarget()->IsBot() && (c->GetTarget()->CastToBot()->GetBotOwner() == c) &&
				((c->GetTarget()->GetClass() == NECROMANCER) || (c->GetTarget()->GetClass() == ENCHANTER) || (c->GetTarget()->GetClass() == DRUID))) {
					if(c->GetTarget()->CastToBot()->IsBotCharmer()) {
						c->GetTarget()->CastToBot()->SetBotCharmer(false);
						c->GetTarget()->Say("Using a summoned pet.");
					}
					else {
						if(c->GetTarget()->GetPet())
						{
							c->GetTarget()->GetPet()->Say_StringID(PET_GETLOST_STRING);
							// c->GetTarget()->GetPet()->Kill();
							c->GetTarget()->GetPet()->Depop(false);
							c->GetTarget()->SetPetID(0);
						}
						c->GetTarget()->CastToBot()->SetBotCharmer(true);
						c->GetTarget()->Say("Available for Dire Charm command.");
					}
			}
			else {
				c->Message(15, "You must target your Enchanter, Necromancer, or Druid bot.");
			}
		}
		else {
			c->Message(15, "You must target an Enchanter, Necromancer, or Druid bot.");
		}
		return;
	}

	//Dire Charm
	if(!strcasecmp(sep->arg[1], "Dire") && !strcasecmp(sep->arg[2], "Charm"))
	{
		Mob *target = c->GetTarget();
		if(target == NULL || target->IsClient() || target->IsBot() || (target->IsPet() && target->GetOwner()->IsBot()))
		{
			c->Message(15, "You must select a monster");
			return;
		}
		int32 DBtype = c->GetTarget()->GetBodyType();
		Mob *Direr;
		int32 DirerClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case ENCHANTER:
							Direr = g->members[i];
							DirerClass = ENCHANTER;
							break;
						case NECROMANCER:
							if(DirerClass != ENCHANTER){
								Direr = g->members[i];
								DirerClass = NECROMANCER;
							}
						case DRUID:
							if (DirerClass == 0){
								Direr = g->members[i];
								DirerClass = DRUID;
							}
							break;
							break;
						default:
							break;
					}
				}
			}
			switch(DirerClass) {
				case ENCHANTER:
					if	(c->GetLevel() >= 55) {
						Direr->Say("Trying to dire charm %s \n", target->GetCleanName(), sep->arg[2]);
						Direr->CastToBot()->Bot_Command_DireTarget (1,target);
					}
					else if (c->GetLevel() <= 55){
						Direr->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Direr->Say("Mob level is too high or can't be charmed", c->GetName());
					break;

				case NECROMANCER:
					if	((c->GetLevel() >= 55) && (DBtype == 3)) {
						Direr->Say("Trying to dire charm %s \n", target->GetCleanName(), sep->arg[2]);
						Direr->CastToBot()->Bot_Command_DireTarget (2,target);
					}
					else if (c->GetLevel() <= 55){
						Direr->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Direr->Say("Mob Is not undead...", c->GetName());
					break;

				case DRUID:
					if	((c->GetLevel() >= 55) && (DBtype == 21)) {
						Direr->Say("Trying to dire charm %s \n", target->GetCleanName(), sep->arg[2]);
						Direr->CastToBot()->Bot_Command_DireTarget (3,target);
					}
					else if (c->GetLevel() <= 55){
						Direr->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else
						Direr->Say("Mob is not an animal...", c->GetName());
					break;

				default:
					c->Message(15, "You must have an Enchanter, Necromancer or Druid in your group.");
					break;
			}
		}
	}

	// Evacuate
	if(!strcasecmp(sep->arg[1], "evac")) {
		Mob *evac = NULL;
		bool hasevac = false;
		if(c->IsGrouped())
		{
			Group *g = c->GetGroup();
			if(g) {
				for(int i=0; i<MAX_GROUP_MEMBERS; i++)
				{
					if((g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == DRUID)) 
						|| (g->members[i] && g->members[i]->IsBot() && (g->members[i]->GetClass() == WIZARD)))
					{
						hasevac = true;
						evac = g->members[i];
					}
				}
				if(!hasevac) {
					c->Message(15, "You must have a Druid in your group.");
				}
			}
		}
		if((hasevac)  && (c->GetLevel() >= 18)) {
			evac->Say("Attempting to Evac you %s.", c->GetName());
			evac->CastToClient()->CastSpell(2183, c->GetID(), 1, -1, -1);
		}
		else if((hasevac)  && (c->GetLevel() <= 17)) {
			evac->Say("I'm not level 18 yet.", c->GetName());
		}
		return;
	}

	// Sow
	if ((!strcasecmp(sep->arg[1], "sow")) && (c->IsGrouped())) {
		Mob *Sower;
		int32 SowerClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case DRUID:
							Sower = g->members[i];
							SowerClass = DRUID;
							break;
						case SHAMAN:
							if (SowerClass != DRUID){
								Sower = g->members[i];
								SowerClass = SHAMAN;
							}
							break;
						case RANGER:
							if (SowerClass == 0){
								Sower = g->members[i];
								SowerClass = RANGER;
							}
							break;
						case BEASTLORD:
							if (SowerClass == 0){
								Sower = g->members[i];
								SowerClass = BEASTLORD;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(SowerClass) {
				case DRUID:
					if      ((!strcasecmp(sep->arg[2], "regular")) && (zone->CanCastOutdoor())  && (c->GetLevel() >= 10) ) {
						Sower->Say("Casting sow...");
						Sower->CastSpell(278, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "regular")) && (zone->CanCastOutdoor())  && (c->GetLevel() <= 10) ) {
						Sower->Say("I'm not level 10 yet.");
					}
					else if ((!strcasecmp(sep->arg[2], "wolf")) && zone->CanCastOutdoor() && (c->GetLevel() >= 20)) {
						Sower->Say("Casting group wolf...");
						Sower->CastSpell(428, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wolf")) && (c->GetLevel() <= 20)) {
						Sower->Say("I'm not level 20 yet.");
					}
					else if ((!strcasecmp(sep->arg[2], "feral")) && (c->GetLevel() >= 50)) { 
						Sower->Say("Casting Feral Pack...");
						Sower->CastSpell(4058, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "feral")) && (c->GetLevel() <= 50)) {
						Sower->Say("I'm not level 50 yet.");
					}
					else if ((!strcasecmp(sep->arg[2], "shrew")) && (c->GetLevel() >= 35)) { 
						Sower->Say("Casting Pack Shrew...");
						Sower->CastSpell(4055, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wolf")) && (c->GetLevel() <= 35)) {
						Sower->Say("I'm not level 35 yet.");
					}
					else if ((!zone->CanCastOutdoor()) && (!strcasecmp(sep->arg[2], "regular")) ||
						(!zone->CanCastOutdoor()) && (!strcasecmp(sep->arg[2], "wolf"))) {
							Sower->Say("I can't cast this spell indoors, try [sow shrew] if you're 35 or higher, or [sow feral] if you're 50 or higher,", c->GetName());
					}
					else if (!zone->CanCastOutdoor()) {
						Sower->Say("I can't cast this spell indoors, try [sow shrew] if you're 35 or higher, or [sow feral] if you're 50 or higher,", c->GetName());
					}
					else if (zone->CanCastOutdoor()) {
						Sower->Say("Do you want [sow regular] or [sow wolf]?", c->GetName());
					}
					else if (!zone->CanCastOutdoor()) {
						Sower->Say("I can't cast this spell indoors, try [sow shrew] if you're 35 or higher, or [sow feral] if you're 50 or higher,", c->GetName());
					}
					break;

				case SHAMAN:

					if ((zone->CanCastOutdoor()) && (c->GetLevel() >= 9)) { 
						Sower->Say("Casting SoW...");
						Sower->CastToClient()->CastSpell(278, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Sower->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 9) {
						Sower->Say("I'm not level 9 yet.");
					}
					break;

				case RANGER:

					if ((zone->CanCastOutdoor()) && (c->GetLevel() >= 28)){
						Sower->Say("Casting SoW...");
						Sower->CastToClient()->CastSpell(278, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Sower->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 28) {
						Sower->Say("I'm not level 28 yet.");
					}
					break;

				case BEASTLORD:

					if((zone->CanCastOutdoor()) && (c->GetLevel() >= 24)) {
						Sower->Say("Casting SoW...");
						Sower->CastToClient()->CastSpell(278, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Sower->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 24) {
						Sower->Say("I'm not level 24 yet.");
					}
					break;


				default:
					c->Message(15, "You must have a Druid, Shaman, Ranger,  or Beastlord in your group.");
					break;
			}
		}
	}

	// Shrink
	if ((!strcasecmp(sep->arg[1], "shrink")) && (c->IsGrouped())) {
		Mob *Shrinker;
		int32 ShrinkerClass = 0;
		Group *g = c->GetGroup();
		Mob *target = c->GetTarget();

		if(target == NULL || (!target->IsClient() && (c->GetTarget()->CastToBot()->GetBotOwner() != c)))
			c->Message(15, "You must select a player or bot you own");

		else if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case SHAMAN:
							Shrinker = g->members[i];
							ShrinkerClass = SHAMAN;
							break;
						case BEASTLORD:
							if (ShrinkerClass != SHAMAN){
								Shrinker = g->members[i];
								ShrinkerClass = BEASTLORD;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(ShrinkerClass) {
				case SHAMAN:

					if (c->GetLevel() >= 15) { 
						Shrinker->Say("Casting Shrink...");
						Shrinker->CastToBot()->SpellOnTarget(345, target);
					}
					else if (c->GetLevel() <= 14) {
						Shrinker->Say("I'm not level 15 yet.");
					}
					break;

				case BEASTLORD:

					if (c->GetLevel() >= 23) {
						Shrinker->Say("Casting Shrink...");
						Shrinker->CastToBot()->SpellOnTarget(345, target);
					}
					else if (c->GetLevel() <= 22) {
						Shrinker->Say("I'm not level 23 yet.");
					}
					break;

				default:
					c->Message(15, "You must have a Shaman or Beastlord in your group.");
					break;
			}
		}
	}

	// Gate
	if ((!strcasecmp(sep->arg[1], "gate")) && (c->IsGrouped())) {
		Mob *Gater;
		int32 GaterClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case DRUID:
							Gater = g->members[i];
							GaterClass = DRUID;
							break;
						case WIZARD:
							if (GaterClass == 0){
								Gater = g->members[i];
								GaterClass = WIZARD;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(GaterClass) {
				case DRUID:
					if      ((!strcasecmp(sep->arg[2], "karana")) && (c->GetLevel() >= 25) ) {
						Gater->Say("Casting Circle of Karana...");
						Gater->CastSpell(550, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "commons")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Circle of Commons...");
						Gater->CastSpell(551, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "tox")) && (c->GetLevel() >= 25)) { 
						Gater->Say("Casting Circle of Toxxulia...");
						Gater->CastSpell(552, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "butcher")) && (c->GetLevel() >= 25)) { 
						Gater->Say("Casting Circle of Butcherblock...");
						Gater->CastSpell(553, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "lava")) && (c->GetLevel() >= 30)) { 
						Gater->Say("Casting Circle of Lavastorm...");
						Gater->CastSpell(554, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "ro")) && (c->GetLevel() >= 32)) { 
						Gater->Say("Casting Circle of Ro...");
						Gater->CastSpell(555, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "feerrott")) && (c->GetLevel() >= 32)) { 
						Gater->Say("Casting Circle of feerrott...");
						Gater->CastSpell(556, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "steamfont")) && (c->GetLevel() >= 31)) { 
						Gater->Say("Casting Circle of Steamfont...");
						Gater->CastSpell(557, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "misty")) && (c->GetLevel() >= 36)) { 
						Gater->Say("Casting Circle of Misty...");
						Gater->CastSpell(558, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wakening")) && (c->GetLevel() >= 40)) { 
						Gater->Say("Casting Circle of Wakening Lands...");
						Gater->CastSpell(1398, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "iceclad")) && (c->GetLevel() >= 32)) { 
						Gater->Say("Casting Circle of Iceclad Ocean...");
						Gater->CastSpell(1434, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "divide")) && (c->GetLevel() >= 36)) { 
						Gater->Say("Casting Circle of The Great Divide...");
						Gater->CastSpell(1438, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "cobalt")) && (c->GetLevel() >= 42)) { 
						Gater->Say("Casting Circle of Cobalt Scar...");
						Gater->CastSpell(1440, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "combines")) && (c->GetLevel() >= 33)) { 
						Gater->Say("Casting Circle of The Combines...");
						Gater->CastSpell(1517, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "surefall")) && (c->GetLevel() >= 26)) { 
						Gater->Say("Casting Circle of Surefall Glade...");
						Gater->CastSpell(2020, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "grimling")) && (c->GetLevel() >= 29)) { 
						Gater->Say("Casting Circle of Grimling Forest...");
						Gater->CastSpell(2419, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "twilight")) && (c->GetLevel() >= 33)) { 
						Gater->Say("Casting Circle of Twilight...");
						Gater->CastSpell(2424, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "dawnshroud")) && (c->GetLevel() >= 37)) { 
						Gater->Say("Casting Circle of Dawnshroud...");
						Gater->CastSpell(2429, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "nexus")) && (c->GetLevel() >= 26)) { 
						Gater->Say("Casting Circle of The Nexus...");
						Gater->CastSpell(2432, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "pok")) && (c->GetLevel() >= 38)) { 
						Gater->Say("Casting Circle of Knowledge...");
						Gater->CastSpell(3184, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "stonebrunt")) && (c->GetLevel() >= 28)) { 
						Gater->Say("Casting Circle of Stonebrunt Mountains...");
						Gater->CastSpell(3792, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "bloodfields")) && (c->GetLevel() >= 55)) { 
						Gater->Say("Casting Circle of Bloodfields...");
						Gater->CastSpell(6184, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "emerald")) && (c->GetLevel() >= 39)) { 
						Gater->Say("Casting Wind of the South...");
						Gater->CastSpell(1737, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "skyfire")) && (c->GetLevel() >= 44)) { 
						Gater->Say("Casting Wind of the North...");
						Gater->CastSpell(1736, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "slaughter")) && (c->GetLevel() >= 64)) { 
						Gater->Say("Casting Circle of Slaughter...");
						Gater->CastSpell(6179, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "karana") 
						|| !strcasecmp(sep->arg[2], "tox") 
						|| !strcasecmp(sep->arg[2], "butcher") && (c->GetLevel() <= 25))
						|| !strcasecmp(sep->arg[2], "commons") && (c->GetLevel() <= 27)
						|| (!strcasecmp(sep->arg[2], "ro") 
						|| !strcasecmp(sep->arg[2], "feerrott") && (c->GetLevel() <= 32))
						|| !strcasecmp(sep->arg[2], "steamfont") && (c->GetLevel() <= 31)
						|| !strcasecmp(sep->arg[2], "misty") && (c->GetLevel() <= 36)
						|| !strcasecmp(sep->arg[2], "lava") && (c->GetLevel() <= 30)
						|| !strcasecmp(sep->arg[2], "wakening") && (c->GetLevel() <= 40)
						|| !strcasecmp(sep->arg[2], "iceclad") && (c->GetLevel() <= 32)
						|| !strcasecmp(sep->arg[2], "divide") && (c->GetLevel() <= 38)
						|| !strcasecmp(sep->arg[2], "cobalt") && (c->GetLevel() <= 42)
						|| !strcasecmp(sep->arg[2], "combines") && (c->GetLevel() <= 33)
						|| !strcasecmp(sep->arg[2], "surefall") && (c->GetLevel() <= 26)
						|| !strcasecmp(sep->arg[2], "grimling") && (c->GetLevel() <= 29)
						|| !strcasecmp(sep->arg[2], "twilight") && (c->GetLevel() <= 33)
						|| !strcasecmp(sep->arg[2], "dawnshroud") && (c->GetLevel() <= 37)
						|| !strcasecmp(sep->arg[2], "nexus") && (c->GetLevel() <= 26)
						|| !strcasecmp(sep->arg[2], "pok") && (c->GetLevel() <= 38)
						|| !strcasecmp(sep->arg[2], "stonebrunt") && (c->GetLevel() <= 28)
						|| !strcasecmp(sep->arg[2], "bloodfields") && (c->GetLevel() <= 55)
						|| !strcasecmp(sep->arg[2], "emerald") && (c->GetLevel() <= 38)
						|| !strcasecmp(sep->arg[2], "skyfire") && (c->GetLevel() <= 43)
						|| !strcasecmp(sep->arg[2], "wos") && (c->GetLevel() <= 64)) {
							Gater->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else {
						Gater->Say("With the proper level I can [gate] to [karana],[commons],[tox],[butcher],[lava],[ro],[feerrott],[steamfont],[misty],[wakening],[iceclad],[divide],[cobalt],[combines],[surefall],[grimling],[twilight],[dawnshroud],[nexus],[pok],[stonebrunt],[bloodfields],[emerald],[skyfire] or [wos].", c->GetName());
					}
					break;

				case WIZARD:

					if      ((!strcasecmp(sep->arg[2], "commons")) && (c->GetLevel() >= 35) ) {
						Gater->Say("Casting Common Portal...");
						Gater->CastSpell(566, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "fay")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Fay Portal...");
						Gater->CastSpell(563, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "ro")) && (c->GetLevel() >= 37)) {
						Gater->Say("Casting Ro Portal...");
						Gater->CastSpell(567, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "tox")) && (c->GetLevel() >= 25)) {
						Gater->Say("Casting Toxxula Portal...");
						Gater->CastSpell(561, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "nk")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting North Karana Portal...");
						Gater->CastSpell(562, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "nek")) && (c->GetLevel() >= 32)) {
						Gater->Say("Casting Nektulos Portal...");
						Gater->CastSpell(564, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wakening")) && (c->GetLevel() >= 43)) {
						Gater->Say("Casting Wakening Lands Portal...");
						Gater->CastSpell(1399, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "iceclad")) && (c->GetLevel() >= 33)) {
						Gater->Say("Casting Iceclad Ocean Portal...");
						Gater->CastSpell(1418, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "divide")) && (c->GetLevel() >= 36)) {
						Gater->Say("Casting Great Divide Portal...");
						Gater->CastSpell(1423, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "cobalt")) && (c->GetLevel() >= 43)) {
						Gater->Say("Casting Cobalt Scar Portal...");
						Gater->CastSpell(1425, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "combines")) && (c->GetLevel() >= 34)) {
						Gater->Say("Casting Combines Portal...");
						Gater->CastSpell(1516, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wk")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting West Karana Portal...");
						Gater->CastSpell(568, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "twilight")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Twilight Portal...");
						Gater->CastSpell(2425, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "dawnshroud")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Dawnshroud Portal...");
						Gater->CastSpell(2430, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "nexus")) && (c->GetLevel() >= 29)) {
						Gater->Say("Casting Nexus Portal...");
						Gater->CastSpell(2944, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "pok")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Plane of Knowledge Portal...");
						Gater->CastSpell(3180, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "wos")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Wall of Slaughter Portal...");
						Gater->CastSpell(6178, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "grimling")) && (c->GetLevel() >= 29)) {
						Gater->Say("Casting Fay Portal...");
						Gater->CastSpell(2420, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "emerald")) && (c->GetLevel() >= 37)) {
						Gater->Say("Porting to Emerald Jungle...");
						Gater->CastSpell(1739, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "hateplane")) && (c->GetLevel() >= 39)) {
						Gater->Say("Porting to Hate Plane...");
						Gater->CastSpell(666, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "airplane")) && (c->GetLevel() >= 39)) {
						Gater->Say("Porting to airplane...");
						Gater->CastSpell(674, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "skyfire")) && (c->GetLevel() >= 36)) {
						Gater->Say("Porting to Skyfire...");
						Gater->CastSpell(1738, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "bloodfields")) && (c->GetLevel() >= 55)) {
						Gater->Say("Casting Bloodfields Portal...");
						Gater->CastSpell(6183, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "stonebrunt")) && (c->GetLevel() >= 27)) {
						Gater->Say("Casting Stonebrunt Portal...");
						Gater->CastSpell(3793, c->GetID(), 1, -1, -1);
					}
					else if ((!strcasecmp(sep->arg[2], "commons") && (c->GetLevel() <= 35))
						|| !strcasecmp(sep->arg[2], "fay") && (c->GetLevel() <= 27)
						|| (!strcasecmp(sep->arg[2], "ro") && (c->GetLevel() <= 37))
						|| !strcasecmp(sep->arg[2], "tox") && (c->GetLevel() <= 25)
						|| !strcasecmp(sep->arg[2], "nk") && (c->GetLevel() <= 25)
						|| !strcasecmp(sep->arg[2], "nek") && (c->GetLevel() <= 32)
						|| !strcasecmp(sep->arg[2], "wakening") && (c->GetLevel() <= 43)
						|| !strcasecmp(sep->arg[2], "iceclad") && (c->GetLevel() <= 33)
						|| !strcasecmp(sep->arg[2], "divide") && (c->GetLevel() <= 36)
						|| !strcasecmp(sep->arg[2], "cobalt") && (c->GetLevel() <= 43)
						|| !strcasecmp(sep->arg[2], "combines") && (c->GetLevel() <= 34)
						|| !strcasecmp(sep->arg[2], "wk") && (c->GetLevel() <= 37)
						|| !strcasecmp(sep->arg[2], "twilight") && (c->GetLevel() <= 33)
						|| !strcasecmp(sep->arg[2], "dawnshroud") && (c->GetLevel() <= 39)
						|| !strcasecmp(sep->arg[2], "nexus") && (c->GetLevel() <= 29)
						|| (!strcasecmp(sep->arg[2], "pok")
						|| !strcasecmp(sep->arg[2], "hateplane")
						|| !strcasecmp(sep->arg[2], "airplane") && (c->GetLevel() <= 38))
						|| !strcasecmp(sep->arg[2], "grimling") && (c->GetLevel() <= 29)
						|| !strcasecmp(sep->arg[2], "bloodfields") && (c->GetLevel() <= 55)
						|| !strcasecmp(sep->arg[2], "stonebrunt") && (c->GetLevel() <= 27)
						|| !strcasecmp(sep->arg[2], "emerald") && (c->GetLevel() <= 36)
						|| !strcasecmp(sep->arg[2], "skyfire") && (c->GetLevel() <= 36)
						|| !strcasecmp(sep->arg[2], "wos") && (c->GetLevel() <= 64)) {
							Gater->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else {
						Gater->Say("With the proper level I can [gate] to [commons],[fay],[ro],[tox],[nk],[wakening],[iceclad],[divide],[cobalt],[combines],[wk],[grimling],[twilight],[dawnshroud],[nexus],[pok],[stonebrunt],[bloodfields],[emerald],[skyfire],[hateplane],[airplane] or [wos].", c->GetName());
					}
					break;
				default:
					c->Message(15, "You must have a Druid or Wizard in your group.");
					break;
			}
		}
	}

	//Endure Breath
	if ((!strcasecmp(sep->arg[1], "endureb")) && (c->IsGrouped())) {
		Mob *Endurer;
		int32 EndurerClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case DRUID:
							Endurer = g->members[i];
							EndurerClass = DRUID;
							break;
						case SHAMAN:
							if (EndurerClass != DRUID){
								Endurer = g->members[i];
								EndurerClass = SHAMAN;
							}
							break;
						case ENCHANTER:
							if(EndurerClass == 0){
								Endurer = g->members[i];
								EndurerClass = ENCHANTER;
							}
							break;
						case RANGER:
							if(EndurerClass == 0) {
								Endurer = g->members[i];
								EndurerClass = RANGER;
							}
							break;
						case BEASTLORD:
							if(EndurerClass == 0) {
								Endurer = g->members[i];
								EndurerClass = BEASTLORD;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(EndurerClass) {
				case DRUID:
					if  (c->GetLevel() < 6) {
						Endurer->Say("I'm not level 6 yet.");
					}
					else {
						Endurer->Say("Casting Enduring Breath...");
						Endurer->CastSpell(86, c->GetID(), 1, -1, -1);
						break;
					}
					break;
				case SHAMAN:
					if  (c->GetLevel() < 12) {
						Endurer->Say("I'm not level 12 yet.");
					}
					else {
						Endurer->Say("Casting Enduring Breath...");
						Endurer->CastSpell(86, c->GetID(), 1, -1, -1);
					}
					break;
				case RANGER:
					if  (c->GetLevel() < 20) {
						Endurer->Say("I'm not level 20 yet.");
					}
					else {
						Endurer->Say("Casting Enduring Breath...");
						Endurer->CastSpell(86, c->GetID(), 1, -1, -1);
					}
					break;
				case ENCHANTER:
					if  (c->GetLevel() < 12) {
						Endurer->Say("I'm not level 12 yet.");
					}
					else {
						Endurer->Say("Casting Enduring Breath...");
						Endurer->CastSpell(86, c->GetID(), 1, -1, -1);
					}
					break;
				case BEASTLORD:
					if  (c->GetLevel() < 25) {
						Endurer->Say("I'm not level 25 yet.");
 					}
					else {
						Endurer->Say("Casting Enduring Breath...");
						Endurer->CastSpell(86, c->GetID(), 1, -1, -1);
					}
 					break;
				default:
					c->Message(15, "You must have a Druid, Shaman, Ranger, Enchanter, or Beastlord in your group.");
					break;
			}
		}
	}

	//Invisible
	if ((!strcasecmp(sep->arg[1], "invis")) && (c->IsGrouped())) {
		Mob *Inviser;
		int32 InviserClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case ENCHANTER:
							Inviser = g->members[i];
							InviserClass = ENCHANTER;
							break;
						case MAGICIAN:
							if (InviserClass != ENCHANTER){
								Inviser = g->members[i];
								InviserClass = MAGICIAN;
							}
							break;
						case WIZARD:
							if((InviserClass != ENCHANTER) && (InviserClass != MAGICIAN)){
								Inviser = g->members[i];
								InviserClass = WIZARD;
							}
							break;
						case NECROMANCER:
							if(InviserClass == 0){
								Inviser = g->members[i];
								InviserClass = NECROMANCER;
							}
							break;
						case DRUID:
							if((InviserClass != ENCHANTER) && (InviserClass != WIZARD)
								|| (InviserClass != MAGICIAN)){
									Inviser = g->members[i];
									InviserClass = DRUID;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(InviserClass) {
				case ENCHANTER:
					if  ((c->GetLevel() <= 14) && (!strcasecmp(sep->arg[2], "undead"))) {
						Inviser->Say("I'm not level 14 yet.");
					}
					else if ((!c->IsInvisible(c)) && (!c->invisible_undead) && (c->GetLevel() >= 14) && (!strcasecmp(sep->arg[2], "undead"))) {
						Inviser->Say("Casting invis undead...");
						Inviser->CastSpell(235, c->GetID(), 1, -1, -1);
					}
					else if  ((c->GetLevel() <= 4) && (!strcasecmp(sep->arg[2], "live"))) {
						Inviser->Say("I'm not level 4 yet.");
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 4) && (!strcasecmp(sep->arg[2], "live"))) { 
						Inviser->Say("Casting invisibilty...");
						Inviser->CastSpell(42, c->GetID(), 1, -1, -1);
					}
					else if  ((c->GetLevel() <= 6) && (!strcasecmp(sep->arg[2], "see"))) {
						Inviser->Say("I'm not level 6 yet.");
					}
					else if ((c->GetLevel() >= 6) && (!strcasecmp(sep->arg[2], "see"))) { 
						Inviser->Say("Casting see invisible...");
						Inviser->CastSpell(80, c->GetID(), 1, -1, -1);
					}
					else if ((c->IsInvisible(c)) || (c->invisible_undead)) { 
						Inviser->Say("I can't cast this if you're already invis-buffed...");
					}
					else {
						Inviser->Say("Do you want [invis undead], [invis live] or [invis see] ?", c->GetName());
					}
					break;
				case MAGICIAN:
					if  (!strcasecmp(sep->arg[2], "undead")) {
						Inviser->Say("I don't have that spell.");
					}
					else if  ((c->GetLevel() <= 8) && (!strcasecmp(sep->arg[2], "live"))) {
						Inviser->Say("I'm not level 8 yet.");
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 8) && (!strcasecmp(sep->arg[2], "live"))) { 
						Inviser->Say("Casting invisibilty...");
						Inviser->CastSpell(42, c->GetID(), 1, -1, -1);
					}
					else if  ((c->GetLevel() <= 16) && (!strcasecmp(sep->arg[2], "see"))) {
						Inviser->Say("I'm not level 16 yet.");
					}
					else if ((c->GetLevel() >= 16) && (!strcasecmp(sep->arg[2], "see"))) { 
						Inviser->Say("Casting see invisible...");
						Inviser->CastSpell(80, c->GetID(), 1, -1, -1);
					}
					else if ((c->IsInvisible(c)) || (c->invisible_undead)) { 
						Inviser->Say("I can't cast this if you're already invis-buffed...");
					}
					else {
						Inviser->Say("Do you want [invis live] or [invis see] ?", c->GetName());
					}
					break;
				case WIZARD:
					if  ((c->GetLevel() <= 39) && (!strcasecmp(sep->arg[2], "undead"))) {
						Inviser->Say("I'm not level 39 yet.");
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 39) && (!strcasecmp(sep->arg[2], "undead"))) {
						Inviser->Say("Casting invis undead...");
						Inviser->CastSpell(235, c->GetID(), 1, -1, -1);
					}
					else if  ((c->GetLevel() <= 16) && (!strcasecmp(sep->arg[2], "live"))) {
						Inviser->Say("I'm not level 16 yet.");
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 16) && (!strcasecmp(sep->arg[2], "live"))) { 
						Inviser->Say("Casting invisibilty...");
						Inviser->CastSpell(42, c->GetID(), 1, -1, -1);
					}
					else if  ((c->GetLevel() <= 4) && (!strcasecmp(sep->arg[2], "see"))) {
						Inviser->Say("I'm not level 6 yet.");
					}
					else if ((c->GetLevel() >= 4) && (!strcasecmp(sep->arg[2], "see"))) { 
						Inviser->Say("Casting see invisible...");
						Inviser->CastSpell(80, c->GetID(), 1, -1, -1);
					}
					else if ((c->IsInvisible(c)) || (c->invisible_undead)) { 
						Inviser->Say("I can't cast this if you're already invis-buffed...");
					}
					else {
						Inviser->Say("Do you want [invis undead], [invis live] or [invis see] ?", c->GetName());
					}
					break;
				case NECROMANCER:
					if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (!strcasecmp(sep->arg[2], "undead"))) {
						Inviser->Say("Casting invis undead...");
						Inviser->CastSpell(235, c->GetID(), 1, -1, -1);
					}
					else if (!strcasecmp(sep->arg[2], "see")) { 
						Inviser->Say("I don't have that spell...");
					}
					else if (!strcasecmp(sep->arg[2], "live")) { 
						Inviser->Say("I don't have that spell...");
					}
					else if ((c->IsInvisible(c))|| (c->invisible_undead)) { 
						Inviser->Say("I can't cast this if you're already invis-buffed...");
					}
					else {
						Inviser->Say("I only have [invis undead]", c->GetName());
					}
					break;
				case DRUID:
					if  (!strcasecmp(sep->arg[2], "undead")) {
						Inviser->Say("I don't have that spell...");
					}
					else if  ((c->GetLevel() <= 4) && (!strcasecmp(sep->arg[2], "live"))) {
						Inviser->Say("I'm not level 4 yet.");
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 18) && (!strcasecmp(sep->arg[2], "live"))) { 
						Inviser->Say("Casting Superior Camouflage...");
						Inviser->CastSpell(34, c->GetID(), 1, -1, -1);
					}
					else if ((!c->IsInvisible(c))&& (!c->invisible_undead) && (c->GetLevel() >= 4) && (!strcasecmp(sep->arg[2], "live")) && (zone->CanCastOutdoor())) { 
						Inviser->Say("Casting Camouflage...");
						Inviser->CastSpell(247, c->GetID(), 1, -1, -1);
					}
					else if ((c->GetLevel() >= 4) && (!strcasecmp(sep->arg[2], "live")) && (!zone->CanCastOutdoor())) { 
						Inviser->Say("I can't cast this spell indoors...");
					}
					else if  ((c->GetLevel() <= 13) && (!strcasecmp(sep->arg[2], "see"))) {
						Inviser->Say("I'm not level 13 yet.");
					}
					else if ((c->GetLevel() >= 13) && (!strcasecmp(sep->arg[2], "see"))) { 
						Inviser->Say("Casting see invisible...");
						Inviser->CastSpell(80, c->GetID(), 1, -1, -1);
					}
					else if ((c->IsInvisible(c)) || (c->invisible_undead)) { 
						Inviser->Say("I can't cast this if you're already invis-buffed...");
					}
					else {
						Inviser->Say("Do you want [invis live] or [invis see] ?", c->GetName());
					}
					break;
				default:
					c->Message(15, "You must have a Enchanter, Magician, Wizard, Druid, or Necromancer in your group.");
					break;
			}
		}
	}

	//Levitate
	if ((!strcasecmp(sep->arg[1], "levitate")) && (c->IsGrouped())) {
		Mob *Lever;
		int32 LeverClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case DRUID:
							Lever = g->members[i];
							LeverClass = DRUID;
							break;
						case SHAMAN:
							if (LeverClass != DRUID){
								Lever = g->members[i];
								LeverClass = SHAMAN;
							}
							break;
						case WIZARD:
							if(LeverClass == 0){
								Lever = g->members[i];
								LeverClass = WIZARD;
							}
							break;
						case ENCHANTER:
							if (LeverClass == 0) {
								Lever = g->members[i];
								LeverClass = ENCHANTER;
							}
							break;
						default:
							break;
					}
				}
			}
			switch(LeverClass) {
				case DRUID:
					if  (c->GetLevel() <= 14) {
						Lever->Say("I'm not level 14 yet.");
					}
					else if (zone->CanCastOutdoor()) {
						Lever->Say("Casting Levitate...");
						Lever->CastSpell(261, c->GetID(), 1, -1, -1);
						break;
					}
					else if (!zone->CanCastOutdoor()) {
						Lever->Say("I can't cast this spell indoors", c->GetName());
					}
					break;

				case SHAMAN:

					if ((zone->CanCastOutdoor()) && (c->GetLevel() >= 10)) { 
						Lever->Say("Casting Levitate...");
						Lever->CastToClient()->CastSpell(261, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Lever->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 10) {
						Lever->Say("I'm not level 10 yet.");
					}
					break;

				case WIZARD:

					if((zone->CanCastOutdoor()) && (c->GetLevel() >= 22)){
						Lever->Say("Casting Levitate...");
						Lever->CastToClient()->CastSpell(261, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Lever->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 22) {
						Lever->Say("I'm not level 22 yet.");
					}
					break;

				case ENCHANTER:

					if((zone->CanCastOutdoor()) && (c->GetLevel() >= 15)) {
						Lever->Say("Casting Levitate...");
						Lever->CastToClient()->CastSpell(261, c->GetID(), 1, -1, -1);
					}
					else if (!zone->CanCastOutdoor()) {
						Lever->Say("I can't cast this spell indoors", c->GetName());
					}
					else if (c->GetLevel() <= 15) {
						Lever->Say("I'm not level 15 yet.");
					}
					break;


				default:
					c->Message(15, "You must have a Druid, Shaman, Wizard, or Enchanter in your group.");
					break;
			}
		}
	}

	//Resists
	if ((!strcasecmp(sep->arg[1], "resist")) && (c->IsGrouped())) {
		Mob *Resister;
		int32 ResisterClass = 0;
		Group *g = c->GetGroup();
		if(g) {
			for(int i=0; i<MAX_GROUP_MEMBERS; i++){
				if(g->members[i] && g->members[i]->IsBot()) {
					switch(g->members[i]->GetClass()) {
						case CLERIC:
							Resister = g->members[i];
							ResisterClass = CLERIC;
							break;
						case SHAMAN:
							if(ResisterClass != CLERIC){
								Resister = g->members[i];
								ResisterClass = SHAMAN;
							}
						case DRUID:
							if (ResisterClass == 0){
								Resister = g->members[i];
								ResisterClass = DRUID;
							}
							break;
							break;
						default:
							break;
					}
				}
			}
			switch(ResisterClass) {
				case CLERIC:
					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 6))  {
						Resister->Say("Casting poison protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(1, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 11)) {
						Resister->Say("Casting disease protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(2, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "fire") && (c->GetLevel() >= 8)) {
						Resister->Say("Casting fire protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(3, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "cold") && (c->GetLevel() >= 13)) {
						Resister->Say("Casting cold protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(4, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() >= 16)) {
						Resister->Say("Casting magic protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(5, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() <= 16)
						|| !strcasecmp(sep->arg[2], "cold") && (c->GetLevel() <= 13)
						|| !strcasecmp(sep->arg[2], "fire") && (c->GetLevel() <= 8) 
						|| !strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 11)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 6)) {
							Resister->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else 
						Resister->Say("Do you want [resist poison], [resist disease], [resist fire], [resist cold], or [resist magic]?", c->GetName());

					break;

				case SHAMAN:
					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 20))  {
						Resister->Say("Casting poison protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(12, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 8)) {
						Resister->Say("Casting disease protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(13, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "fire") && (c->GetLevel() >= 5)) {
						Resister->Say("Casting fire protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(14, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "cold") && (c->GetLevel() >= 1)) {
						Resister->Say("Casting cold protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(15, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() >= 19)) {
						Resister->Say("Casting magic protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(16, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() <= 19)
						|| !strcasecmp(sep->arg[2], "cold") && (c->GetLevel() <= 1)
						|| !strcasecmp(sep->arg[2], "fire") && (c->GetLevel() <= 5) 
						|| !strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 8)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 20)) {
							Resister->Say("I don't have the needed level yet", sep->arg[2]);
					}
					else 
						Resister->Say("Do you want [resist poison], [resist disease], [resist fire], [resist cold], or [resist magic]?", c->GetName());

					break;

				case DRUID:

					if	(!strcasecmp(sep->arg[2], "poison") && (c->GetLevel() >= 19)) {
						Resister->Say("Casting poison protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(7, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "disease") && (c->GetLevel() >= 19)) {
						Resister->Say("Casting disease protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(8, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "fire")) { // Fire level 1
						Resister->Say("Casting fire protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(9, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "cold") && (c->GetLevel() >= 13)) {
						Resister->Say("Casting cold protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(10, Resister->GetLevel());
					}
					else if(!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() >= 16)) {
						Resister->Say("Casting magic protection...", sep->arg[2]);
						Resister->CastToBot()->Bot_Command_Resist(11, Resister->GetLevel());
					}
					else if (!strcasecmp(sep->arg[2], "magic") && (c->GetLevel() <= 16)
						|| !strcasecmp(sep->arg[2], "cold") && (c->GetLevel() <= 9)
						|| !strcasecmp(sep->arg[2], "disease") && (c->GetLevel() <= 19)
						|| !strcasecmp(sep->arg[2], "poison") && (c->GetLevel() <= 19)) {
							Resister->Say("I don't have the needed level yet", sep->arg[2]) ;
					}
					else 
						Resister->Say("Do you want [resist poison], [resist disease], [resist fire], [resist cold], or [resist magic]?", c->GetName());

					break;

				default:
					c->Message(15, "You must have a Cleric, Shaman, or Druid in your group.");
					break;
			}
		}
	}

	// debug commands
	if(!strcasecmp(sep->arg[1], "debug") && !strcasecmp(sep->arg[2], "inventory")) {
		Mob *target = c->GetTarget();

		if(target && target->IsBot()) {
			for(int i=0; i<9; i++) {
				c->Message(15,"Equiped slot: %i , item: %i \n", i, target->CastToBot()->GetEquipment(i));
			}
			if(target->CastToBot()->GetEquipment(8) > 0)
				c->Message(15,"This bot has an item in off-hand.");
		}
		return;
	}

	if(!strcasecmp(sep->arg[1], "debug") && !strcasecmp(sep->arg[2], "botcaracs"))
	{
		Mob *target = c->GetTarget();
		if(target && target->IsBot())
		{
			if(target->CanThisClassDualWield())
				c->Message(15, "This class can dual wield.");
			if(target->CanThisClassDoubleAttack())
				c->Message(15, "This class can double attack.");
		}
		if(target->GetPetID())
			c->Message(15, "I've a pet and its name is %s", target->GetPet()->GetCleanName() );
		return;
	}

	if(!strcasecmp(sep->arg[1], "debug") && !strcasecmp(sep->arg[2], "spells"))
	{
		Mob *target = c->GetTarget();
		if(target && target->IsBot())
		{
			for(int i=0; i<target->CastToBot()->AIspells.size(); i++)
			{
				if(target->CastToBot()->BotGetSpells(i) != 0)
				{
					SPDat_Spell_Struct botspell = spells[target->CastToBot()->BotGetSpells(i)];
					c->Message(15, "(DEBUG) %s , Slot(%i), Spell (%s) Priority (%i) \n", target->GetCleanName(), i, botspell.name, target->CastToBot()->BotGetSpellPriority(i));
				}
			}
		}
		return;
	}

	// #bot group ...
	if(!strcasecmp(sep->arg[1], "group") && !strcasecmp(sep->arg[2], "help")) {
		c->Message(0, "#bot group help - will show this help.");
		c->Message(0, "#bot group summon <bot group leader name or target>. Summons the bot group to your location.");
		c->Message(0, "#bot group follow <bot group leader name or target>");
		c->Message(0, "#bot group guard <bot group leader name or target>");
		c->Message(0, "#bot group attack <bot group leader name> <mob name to attack or target>");

		return;
	}

	if(!strcasecmp(sep->arg[1], "group")) {
		if(!strcasecmp(sep->arg[2], "follow")) {
			if(c->IsGrouped())
				BotGroupOrderFollow(c->GetGroup(), c);
		}
		else if(!strcasecmp(sep->arg[2], "guard")) {
			if(c->IsGrouped())
				BotGroupOrderGuard(c->GetGroup(), c);
		}
		else if(!strcasecmp(sep->arg[2], "attack")) {
			if(c->IsGrouped() && (c->GetTarget() != NULL) && c->IsAttackAllowed(c->GetTarget())) {
				BotGroupOrderAttack(c->GetGroup(), c->GetTarget(), c);
			}
			else
				c->Message(15, "You must target a monster.");
		}
		else if(!strcasecmp(sep->arg[2], "summon")) {
			if(c->IsGrouped())
				BotGroupSummon(c->GetGroup(), c);
		}

		return;
	}

	// #bot botgroup ...
	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "help")) {
		c->Message(0, "#bot botgroup help - will show this help.");
		c->Message(0, "#bot botgroup create <bot group leader name or target>. This will designate a bot to be a bot group leader.");
		c->Message(0, "#bot botgroup add <bot group member name to add> <bot group leader name or target>");
		c->Message(0, "#bot botgroup remove <bot group member name to remove or target>");
		c->Message(0, "#bot botgroup disband <bot group leader name or target>. Disbands the designated bot group leader's bot group.");
		c->Message(0, "#bot botgroup summon <bot group leader name or target>. Summons the bot group to your location.");
		c->Message(0, "#bot botgroup follow <bot group leader name or target>");
		c->Message(0, "#bot botgroup guard <bot group leader name or target>");
		c->Message(0, "#bot botgroup attack <bot group leader name> <mob name to attack or target>");
		c->Message(0, "#bot botgroup list");
		c->Message(0, "#bot botgroup load <bot group name>");
		c->Message(0, "#bot botgroup save <bot group name> <bot group leader name or target>");
		c->Message(0, "#bot botgroup delete <bot group name>");

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "create")) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupLeader = 0;
		
		if(!targetName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupLeader = targetMob->CastToBot();
		}

		if(botGroupLeader) {
			if(Bot::BotGroupCreate(botGroupLeader))
				botGroupLeader->Say("I am prepared to lead.");
			else
				botGroupLeader->Say("I can not lead.");
		}
		else
			c->Message(13, "You must target a spawned bot first.");

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "add")) {
		int argCount = 0;

		argCount = sep->argnum;

		std::string botGroupLeaderName;
		std::string botGroupMemberName;

		if(argCount >= 3)
			botGroupMemberName = std::string(sep->arg[3]);

		Bot* botGroupMember = entity_list.GetBotByBotName(botGroupMemberName);

		if(!botGroupMember) {
			if(botGroupMemberName.empty())
				c->Message(13, "You must target a bot in this zone. Please try again.");
			else
				c->Message(13, "%s is not a bot in this zone. Please try again.", botGroupMemberName.c_str());

			return;
		}

		Bot* botGroupLeader = 0;

		if(argCount == 4) {
			botGroupLeaderName = std::string(sep->arg[4]);

			botGroupLeader = entity_list.GetBotByBotName(botGroupLeaderName);
		}
		else if(c->GetTarget() && c->GetTarget()->IsBot())
			botGroupLeader = c->GetTarget()->CastToBot();

		if(!botGroupLeader) {
			if(botGroupLeaderName.empty())
				c->Message(13, "You must target a bot in this zone. Please try again.");
			else
				c->Message(13, "%s is not a bot in this zone. Please try again.", botGroupLeaderName.c_str());

			return;
		}

		if(botGroupLeader->HasGroup()) {
			Group* g = botGroupLeader->GetGroup();

			if(g) {
				if(g->IsLeader(botGroupLeader)) {
					if(g->GroupCount() < MAX_GROUP_MEMBERS) {
						if(!botGroupMemberName.empty() && botGroupMember) {
							botGroupMember = entity_list.GetBotByBotName(botGroupMemberName);
						}

						if(botGroupMember) {
							if(!botGroupMember->HasGroup()) {
								// invite
								if(Bot::AddBotToGroup(botGroupMember, g)) {
									database.SetGroupID(botGroupMember->GetName(), g->GetID(), botGroupMember->GetBotID());
									botGroupMember->Say("I have joined %s\'s group.", botGroupLeader->GetName());
								}
								else {
									botGroupMember->Say("I can not join %s\'s group.", botGroupLeader->GetName());
								}
							}
							else {
								// "I am already in a group."
								Group* tempGroup = botGroupMember->GetGroup();
								if(tempGroup)
									botGroupMember->Say("I can not join %s\'s group. I am already a member in %s\'s group.", botGroupLeader->GetName(), tempGroup->GetLeaderName());
							}
						}
						else {
							// must target a bot message
							c->Message(13, "You must target a spawned bot first.");
						}
					}
					else {
						// "My group is full."
						botGroupLeader->Say("I have no more openings in my group, %s.", c->GetName());
					}
				}
				else {
					// "I am not a group leader."
					Group* tempGroup = botGroupLeader->GetGroup();
					if(tempGroup)
						botGroupLeader->Say("I can not lead anyone because I am a member in %s\'s group.", tempGroup->GetLeaderName());
				}
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "remove")) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupMember = 0;
		
		if(!targetName.empty()) {
			botGroupMember = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupMember = targetMob->CastToBot();
		}

		if(botGroupMember) {
			if(botGroupMember->HasGroup()) {
				Group* g = botGroupMember->GetGroup();

				if(Bot::RemoveBotFromGroup(botGroupMember, g))
					botGroupMember->Say("I am no longer in a group.");
				else
					botGroupMember->Say("I can not leave %s\'s group.", g->GetLeaderName());
			}
			else
				botGroupMember->Say("I am not in a group.");
		}
		else
			c->Message(13, "You must target a spawned bot first.");

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "disband")) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupLeader = 0;
		
		if(!targetName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupLeader = targetMob->CastToBot();
		}

		if(botGroupLeader) {
			if(botGroupLeader->HasGroup()) {
				Group* g = botGroupLeader->GetGroup();
				
				if(g->IsLeader(botGroupLeader)) {
					if(Bot::RemoveBotFromGroup(botGroupLeader, g))
						botGroupLeader->Say("I have disbanded my group, %s.", c->GetName());
					else
						botGroupLeader->Say("I was not able to disband my group, %s.", c->GetName());
				}
				else {
					botGroupLeader->Say("I can not disband my group, %s, because I am not the leader. %s is the leader of my group.", c->GetName(), g->GetLeaderName());
				}
			}
			else
				botGroupLeader->Say("I am not a group leader, %s.", c->GetName());
		}
		else
			c->Message(13, "You must target a spawned bot group leader first.");

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "summon") ) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupLeader = 0;
		
		if(!targetName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupLeader = targetMob->CastToBot();
		}

		if(botGroupLeader) {
			if(botGroupLeader->HasGroup()) {
				Group* g = botGroupLeader->GetGroup();
				
				if(g->IsLeader(botGroupLeader))
					BotGroupSummon(g, c);
			}
		}
		else if(c->HasGroup())
			BotGroupSummon(c->GetGroup(), c);

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "follow") ) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupLeader = 0;
		
		if(!targetName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupLeader = targetMob->CastToBot();
		}

		if(botGroupLeader) {
			if(botGroupLeader->HasGroup()) {
				Group* g = botGroupLeader->GetGroup();
				
				if(g->IsLeader(botGroupLeader))
					BotGroupOrderFollow(g, c);
			}
		}
		else if(c->HasGroup())
			BotGroupOrderFollow(c->GetGroup(), c);

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "guard") ) {
		Mob* targetMob = c->GetTarget();
		std::string targetName = std::string(sep->arg[3]);
		Bot* botGroupLeader = 0;
		
		if(!targetName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(targetName);
		}
		else if(targetMob) {
			if(targetMob->IsBot())
				botGroupLeader = targetMob->CastToBot();
		}

		if(botGroupLeader) {
			if(botGroupLeader->HasGroup()) {
				Group* g = botGroupLeader->GetGroup();
				
				if(g->IsLeader(botGroupLeader))
					BotGroupOrderGuard(g, c);
			}
		}
		else if(c->HasGroup())
			BotGroupOrderGuard(c->GetGroup(), c);

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "attack") ) {
		Mob* targetMob = c->GetTarget();
		Bot* botGroupLeader = 0;
		std::string botGroupLeaderName = std::string(sep->arg[3]);
		std::string targetName = std::string(sep->arg[4]);
		
		if(!botGroupLeaderName.empty()) {
			botGroupLeader = entity_list.GetBotByBotName(botGroupLeaderName);

			if(botGroupLeader) {
				if(!targetName.empty()) {
					targetMob = entity_list.GetMob(targetName.c_str());
				}
				
				if(targetMob) {
					if(c->IsAttackAllowed(targetMob)) {
						if(botGroupLeader->HasGroup()) {
							Group* g = botGroupLeader->GetGroup();

							if(g) {
								if(g->IsLeader(botGroupLeader))
									BotGroupOrderAttack(g, targetMob, c);
							}
						}
						else if(c->HasGroup())
							BotGroupOrderAttack(c->GetGroup(), targetMob, c);
					}
					else
						c->Message(13, "You must target a monster.");
				}
				else
					c->Message(13, "You must target a monster.");
			}
			else
				c->Message(13, "You must target a spawned bot group leader first.");
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "list")) {
		std::list<BotGroupList> botGroupList = GetBotGroupListByBotOwnerCharacterId(c->CharacterID(), &TempErrorMessage);

		if(!TempErrorMessage.empty()) {
			c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
			return;
		}

		if(!botGroupList.empty()) {
			for(std::list<BotGroupList>::iterator botGroupListItr = botGroupList.begin(); botGroupListItr != botGroupList.end(); botGroupListItr++) {				
				c->Message(0, "Bot Group Name: %s -- Bot Group Leader: %s", botGroupListItr->BotGroupName.c_str(), botGroupListItr->BotGroupLeaderName.c_str());
			}
		}
		else {
			c->Message(0, "You have no bot groups created. Use the #bot botgroup save command to save bot groups.");
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "load")) {
		std::string botGroupName = std::string(sep->arg[3]);

		if(!botGroupName.empty()) {
			uint32 botGroupId = CanLoadBotGroup(c->CharacterID(), botGroupName, &TempErrorMessage);

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				return;
			}

			if(botGroupId > 0) {
				std::list<BotGroup> botGroup = LoadBotGroup(botGroupName, &TempErrorMessage);

				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					return;
				}

				uint32 botGroupLeaderBotId = GetBotGroupLeaderIdByBotGroupName(botGroupName);

				Bot* groupLeader = LoadBot(botGroupLeaderBotId, &TempErrorMessage);

				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					safe_delete(groupLeader);
					return;
				}

				groupLeader->Spawn(c, &TempErrorMessage);
				
				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					safe_delete(groupLeader);
					return;
				}

				if(BotGroupCreate(groupLeader)) {
					Group* g = groupLeader->GetGroup();

					for(std::list<BotGroup>::iterator botGroupItr = botGroup.begin(); botGroupItr != botGroup.end(); botGroupItr++) {
						if(botGroupItr->BotID == groupLeader->GetBotID())
							continue;

						Bot* botGroupMember = LoadBot(botGroupItr->BotID, &TempErrorMessage);

						if(!TempErrorMessage.empty()) {
							c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
							safe_delete(botGroupMember);
							return;
						}

						if(botGroupMember) {
							botGroupMember->Spawn(c, &TempErrorMessage);

							if(!TempErrorMessage.empty()) {
								c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
								safe_delete(botGroupMember);
								return;
							}

							AddBotToGroup(botGroupMember, g);
						}
					}
				}
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "delete")) {
		std::string botGroupName = std::string(sep->arg[3]);

		if(!botGroupName.empty()) {
			uint32 botGroupId = CanLoadBotGroup(c->CharacterID(), botGroupName, &TempErrorMessage);

			if(!TempErrorMessage.empty()) {
				c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
				return;
			}

			if(botGroupId > 0) {
				DeleteBotGroup(botGroupName, &TempErrorMessage);

				if(!TempErrorMessage.empty()) {
					c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
					return;
				}
			}
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "botgroup") && !strcasecmp(sep->arg[2], "save")) {
		std::string botGroupName = std::string(sep->arg[3]);

		if(!botGroupName.empty()) {
			if(!DoesBotGroupNameExist(botGroupName)) {
				Bot* groupLeader = 0;

				if(c->GetTarget() && c->GetTarget()->IsBot())
					groupLeader = c->GetTarget()->CastToBot();
				else
					groupLeader = entity_list.GetBotByBotName(std::string(sep->arg[4]));

				if(groupLeader) {
					if(groupLeader->HasGroup() && groupLeader->GetGroup()->IsLeader(groupLeader)) {
						SaveBotGroup(groupLeader->GetGroup(), botGroupName, &TempErrorMessage);

						if(!TempErrorMessage.empty()) {
							c->Message(13, "Database Error: %s", TempErrorMessage.c_str());
						}
						else
							c->Message(0, "%s's bot group has been saved as %s.", groupLeader->GetName(), botGroupName.c_str());
					}
					else
						c->Message(0, "You must target a bot group leader only.");
				}
				else
					c->Message(0, "You must target a bot that is in the same zone as you.");
			}
			else 
				c->Message(0, "The bot group name already exists. Please choose another name to save your bot group as.");
		}

		return;
	}

	if(!strcasecmp(sep->arg[1], "haircolor") || !strcasecmp(sep->arg[1], "hair") || !strcasecmp(sep->arg[1], "beard") || !strcasecmp(sep->arg[1], "beardcolor") || !strcasecmp(sep->arg[1], "face")
		|| !strcasecmp(sep->arg[1], "eyes") || !strcasecmp(sep->arg[1], "heritage") || !strcasecmp(sep->arg[1], "tattoo") || !strcasecmp(sep->arg[1], "details")) {
		if(c->GetTarget() && c->GetTarget()->IsBot()) {
			if  (sep->IsNumber(2)) {
				if (c->GetTarget()->CastToBot()->GetBotOwnerCharacterID() == c->CharacterID()) {
					Bot *target = c->GetTarget()->CastToBot();
					int16 Race = target->GetRace();
					int8 Gender = target->GetGender();
					int8 Texture = 0xFF;
					int8 HelmTexture = 0xFF;
					int8 HairStyle = target->GetHairStyle();
					int8 HairColor = target->GetHairColor();
					int8 BeardColor = target->GetBeardColor();
					int8 EyeColor1 = target->GetEyeColor1();
					int8 EyeColor2 = target->GetEyeColor2();
						
					int8 LuclinFace = target->GetLuclinFace();
					int8 Beard = target->GetBeard();
					int32 DrakkinHeritage = target->GetDrakkinHeritage();
					int32 DrakkinTattoo = target->GetDrakkinTattoo();
					int32 DrakkinDetails = target->GetDrakkinDetails();
					float Size = target->GetSize();

					if (!strcasecmp(sep->arg[1], "hair"))
						HairStyle = atoi(sep->arg[2]);
					if (!strcasecmp(sep->arg[1], "haircolor"))
						HairColor = atoi(sep->arg[2]);
					if (!strcasecmp(sep->arg[1], "beard") || !strcasecmp(sep->arg[1], "beardcolor")) {
						if (!Gender || Race == 8) {
							if (!strcasecmp(sep->arg[1], "beard"))
								Beard = atoi(sep->arg[2]);
							if (!strcasecmp(sep->arg[1], "beardcolor"))
								BeardColor = atoi(sep->arg[2]);
						} else {
							c->Message(0, "Must be a male bot, or dwarf.");
							return;
						}
					}
					if (!strcasecmp(sep->arg[1], "face")) 
						LuclinFace = atoi(sep->arg[2]);

					if (!strcasecmp(sep->arg[1], "eyes")) {
						EyeColor1 = EyeColor2 = atoi(sep->arg[2]);
						c->Message(0, "Eye Values = 0 - 11");
					}
					if(!strcasecmp(sep->arg[1], "heritage") || !strcasecmp(sep->arg[1], "tattoo") || !strcasecmp(sep->arg[1], "details")) {
						if(Race == 522) {
							if(!strcasecmp(sep->arg[1], "heritage")) {
								DrakkinHeritage = atoi(sep->arg[2]);
								c->Message(0, "Heritage Values = 0 - 6");
							}
							if(!strcasecmp(sep->arg[1], "tattoo")) {
								DrakkinTattoo = atoi(sep->arg[2]);
								c->Message(0, "Tattoo Values = 0 - 7");
							}
							if(!strcasecmp(sep->arg[1], "details")) {
								DrakkinDetails = atoi(sep->arg[2]);
								c->Message(0, "Details Values = 0 - 7");
							}
						}
						else {
							c->Message(0, "Drakkin only.");
							return;
						}
					}
						
					target->SendIllusionPacket(Race, Gender, Texture, HelmTexture, HairColor, BeardColor,
												EyeColor1, EyeColor2, HairStyle, LuclinFace, Beard, 0xFF,
												DrakkinHeritage, DrakkinTattoo, DrakkinDetails, Size);

					if(target->CastToBot()->Save())
						c->Message(0, "%s saved.", target->GetCleanName());
					else
						c->Message(13, "%s save failed!", target->GetCleanName());

					c->Message(0,"Feature changed.");
				} else {
					c->Message(0, "You must own the bot to make changes.");
				}
			} else {
				c->Message(0, "Requires a value.");
			}
		} else {
			c->Message(0,"A bot needs to be targetted.");
		}
		return;
	}
}

// franck: EQoffline
// This function has been reworked for the caster bots, when engaged.
// Healers bots must heal thoses who loose HP.
bool EntityList::Bot_AICheckCloseBeneficialSpells(Bot* caster, int8 iChance, float iRange, int16 iSpellTypes) {
	_ZP(EntityList_Bot_AICheckCloseBeneficialSpells);

	if((iSpellTypes&SpellTypes_Detrimental) != 0) {
		//according to live, you can buff and heal through walls...
		//now with PCs, this only applies if you can TARGET the target, but
		// according to Rogean, Live NPCs will just cast through walls/floors, no problem..
		//
		// This check was put in to address an idle-mob CPU issue
		_log(AI__ERROR, "Error: detrimental spells requested from AICheckCloseBeneficialSpells!!");
		return(false);
	}
	
	if(!caster)
		return false;

	if(!caster->AI_HasSpells())
		return false;

	if (iChance < 100) {
		int8 tmp = MakeRandomInt(1, 100);
		if (tmp > iChance)
			return false;
	}

	int8 botCasterClass = caster->GetClass();

	// Changed so heal based on health percentage is different for hybrids
	if( botCasterClass == CLERIC || botCasterClass == DRUID || botCasterClass == SHAMAN) {
		//If AI_EngagedCastCheck() said to the healer that he had to heal
		if( iSpellTypes == SpellType_Heal )	{
			// check in group
			if(caster->HasGroup()) {
				Group *g = caster->GetGroup();
				
				if(g) {
					for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
						if(g->members[i] && !g->members[i]->qglobal) {
							if(g->members[i]->IsClient() && g->members[i]->GetHPRatio() < 90) {
								if(caster->AICastSpell(g->members[i], iChance, SpellType_Heal))
									return true;
							}
							else if((g->members[i]->GetClass() ==  WARRIOR || g->members[i]->GetClass() == PALADIN || g->members[i]->GetClass() == SHADOWKNIGHT) &&
								g->members[i]->GetHPRatio() < 95) 
							{
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
							else if(g->members[i]->GetClass() ==  ENCHANTER && g->members[i]->GetHPRatio() < 80) {
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
							else if(g->members[i]->GetHPRatio() < 70) {
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
						}

						if(g->members[i] && !g->members[i]->qglobal && g->members[i]->HasPet() && g->members[i]->GetPet()->GetHPRatio() < 50) {
							if(g->members[i]->GetPet()->GetOwner() != caster && caster->IsEngaged() && g->members[i]->IsCasting() && g->members[i]->GetClass() != ENCHANTER )
								continue;

							if(caster->AICastSpell(g->members[i]->GetPet(), 100, SpellType_Heal))
								return true;
						}
					}
				}
			}

			// TODO: raid heals
		}
	}

		// Changed so heal based on health percentage is different for hybrids
		if( botCasterClass == PALADIN || botCasterClass == BEASTLORD || botCasterClass == RANGER) {
		//If AI_EngagedCastCheck() said to the healer that he had to heal
		if( iSpellTypes == SpellType_Heal )	{
			// check in group
			if(caster->HasGroup()) {
				Group *g = caster->GetGroup();
				
				if(g) {
					for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
						if(g->members[i] && !g->members[i]->qglobal) {
							if(g->members[i]->IsClient() && g->members[i]->GetHPRatio() < 20) {
								if(caster->AICastSpell(g->members[i], iChance, SpellType_Heal))
									return true;
							}
							else if((g->members[i]->GetClass() ==  WARRIOR || g->members[i]->GetClass() == PALADIN || g->members[i]->GetClass() == SHADOWKNIGHT) &&
								g->members[i]->GetHPRatio() < 20) 
							{
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
							else if(g->members[i]->GetClass() ==  ENCHANTER && g->members[i]->GetHPRatio() < 20) {
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
							else if(g->members[i]->GetHPRatio() < 20) {
								if(caster->AICastSpell(g->members[i], 100, SpellType_Heal))
									return true;
							}
						}

						if(g->members[i] && !g->members[i]->qglobal && g->members[i]->HasPet() && g->members[i]->GetPet()->GetHPRatio() < 20) {
							if(g->members[i]->GetPet()->GetOwner() != caster && caster->IsEngaged() && g->members[i]->IsCasting() && g->members[i]->GetClass() != ENCHANTER )
								continue;

							if(caster->AICastSpell(g->members[i]->GetPet(), 100, SpellType_Heal))
								return true;
						}
					}
				}
			}

			// TODO: raid heals
		}
	}
	
	//Ok for the buffs..
	if( iSpellTypes == SpellType_Buff) {
		// Let's try to make Bard working...
		if(botCasterClass == BARD) {
			if(caster->AICastSpell(caster, 100, SpellType_Buff))
				return true;
			else
				return false;
		}
		
		if(caster->HasGroup()) {
			Group *g = caster->GetGroup();
			
			if(g) {
				for( int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if(g->members[i]) {
						if(caster->AICastSpell(g->members[i], 100, SpellType_Buff))
							return true;

						if(caster->AICastSpell(g->members[i]->GetPet(), 100, SpellType_Buff))
							return true;
					}
				}
			}
		}

		// TODO: raid buffs
	}

	return false;
}


Mob* EntityList::GetMobByBotID(uint32 botID) {
	Mob* Result = 0;

	if(botID > 0) {
		LinkedListIterator<Mob*> iterator(mob_list);
	
		iterator.Reset();

		while(iterator.MoreElements()) {
			if(iterator.GetData()->IsBot() && iterator.GetData()->CastToBot()->GetBotID() == botID) {
				Result = iterator.GetData();
				break;
			}

			iterator.Advance();
		}
	}

	return Result;
}

Bot* EntityList::GetBotByBotID(uint32 botID) {
	Bot* Result = 0;

	if(botID > 0) {
		for(list<Bot*>::iterator botListItr = bot_list.begin(); botListItr != bot_list.end(); botListItr++) {
			Bot* tempBot = *botListItr;

			if(tempBot && tempBot->GetBotID() == botID) {
				Result = tempBot;
				break;
			}
		}
	}

	return Result;
}

Bot* EntityList::GetBotByBotName(std::string botName) {
	Bot* Result = 0;

	if(!botName.empty()) {
		for(list<Bot*>::iterator botListItr = bot_list.begin(); botListItr != bot_list.end(); botListItr++) {
			Bot* tempBot = *botListItr;

			if(tempBot && std::string(tempBot->GetName()) == botName) {
				Result = tempBot;
				break;
			}
		}
	}

	return Result;
}

void EntityList::AddBot(Bot *newBot, bool SendSpawnPacket, bool dontqueue) {
	if(newBot) {
		newBot->SetID(GetFreeID());

		if(SendSpawnPacket) {
			if(dontqueue) {
				// Send immediately
				EQApplicationPacket* outapp = new EQApplicationPacket();
				newBot->CreateSpawnPacket(outapp);
				outapp->priority = 6;
				QueueClients(newBot, outapp, true);
				safe_delete(outapp);
			}
			else {
				// Queue the packet
				NewSpawn_Struct* ns = new NewSpawn_Struct;
				memset(ns, 0, sizeof(NewSpawn_Struct));
				newBot->FillSpawnStruct(ns, newBot);
				AddToSpawnQueue(newBot->GetID(), &ns);
				safe_delete(ns);
			}

            parse->EventNPC(EVENT_SPAWN, newBot, NULL, "", 0);
		}

		bot_list.push_back(newBot);

		mob_list.Insert(newBot);
	}
}

list<Bot*> EntityList::GetBotsByBotOwnerCharacterID(uint32 botOwnerCharacterID) {
	list<Bot*> Result;

	if(botOwnerCharacterID > 0) {
		for(list<Bot*>::iterator botListItr = bot_list.begin(); botListItr != bot_list.end(); botListItr++) {
			Bot* tempBot = *botListItr;

			if(tempBot && tempBot->GetBotOwnerCharacterID() == botOwnerCharacterID)
				Result.push_back(tempBot);
		}
	}

	return Result;
}

void EntityList::BotPickLock(Bot* rogue)
{
	LinkedListIterator<Doors*> iterator(door_list);
	iterator.Reset();
	while(iterator.MoreElements()) {
		Doors *cdoor = iterator.GetData();
		if(cdoor && !cdoor->IsDoorOpen()) {
			float zdiff = rogue->GetZ() - cdoor->GetZ();
			if(zdiff < 0)
				zdiff = 0 - zdiff;
			float curdist = 0;
			float tmp = rogue->GetX() - cdoor->GetX();
			curdist += (tmp * tmp);
			tmp = rogue->GetY() - cdoor->GetY();
			curdist += (tmp * tmp);
			if((zdiff < 10) && (curdist <= 130)) {
				// All rogue items with lock pick bonuses are hands or primary
				const ItemInst* item1 = rogue->GetBotItem(SLOT_HANDS);
				const ItemInst* item2 = rogue->GetBotItem(SLOT_PRIMARY);

				float bonus1 = 0.0f;
				float bonus2 = 0.0f;
				float skill = rogue->GetSkill(PICK_LOCK);

				if(item1) { // Hand slot item
					if(item1->GetItem()->SkillModType == PICK_LOCK) {
						bonus1 = skill * (((float)item1->GetItem()->SkillModValue) / 100.0f);
					}
				}

				if(item2) { // Primary slot item
					if(item2->GetItem()->SkillModType == PICK_LOCK) {
						bonus2 = skill * (((float)item2->GetItem()->SkillModValue) / 100.0f);
					}
				}

				if((skill+bonus1+bonus2) >= cdoor->GetLockpick()) {
					cdoor->ForceOpen(rogue);
				}
				else {
					rogue->Say("I am not skilled enough for this lock.");
				}
			}
		}
		iterator.Advance();
	}
}

bool EntityList::RemoveBot(int16 entityID) {
	bool Result = false;

	if(entityID > 0) {
		for(list<Bot*>::iterator botListItr = bot_list.begin(); botListItr != bot_list.end(); botListItr++)
		{
			Bot* tempBot = *botListItr;

			if(tempBot && tempBot->GetID() == entityID) {
				bot_list.erase(botListItr);
				Result = true;
				break;
			}
		}
	}

	return Result;
}

void EntityList::ShowSpawnWindow(Client* client, int Distance, bool NamedOnly) {

	const char *WindowTitle = "Bot Tracking Window";

	string WindowText;
	int LastCon = -1;
	int CurrentCon = 0;
	
	int32 array_counter = 0;
	
	LinkedListIterator<Mob*> iterator(mob_list);
	iterator.Reset();

	while(iterator.MoreElements())
	{
		if (iterator.GetData() && (iterator.GetData()->DistNoZ(*client)<=Distance))
		{
			if(iterator.GetData()->IsTrackable()) {
				Mob* cur_entity = iterator.GetData();
				int  Extras = (cur_entity->IsBot() || cur_entity->IsPet() || cur_entity->IsFamiliar() || cur_entity->IsClient());
				const char *const MyArray[] = {
					"a_","an_","Innkeep_","Barkeep_",
					"Guard_","Merchant_","Lieutenant_",
					"Banker_","Centaur_","Aviak_","Baker_",
					"Sir_","Armorer_","Deathfist_","Deputy_",
					"Sentry_","Sentinel_","Leatherfoot_",
					"Corporal_","goblin_","Bouncer_","Captain_",
					"orc_","fire_","inferno_","young_","cinder_",
					"flame_","gnomish_","CWG_","sonic_","greater_",
					"ice_","dry_","Priest_","dark-boned_",
					"Tentacle_","Basher_","Dar_","Greenblood_",
					"clockwork_","guide_","rogue_","minotaur_",
					"brownie_","Teir'","dark_","tormented_",
					"mortuary_","lesser_","giant_","infected_",
					"wharf_","Apprentice_","Scout_","Recruit_",
					"Spiritist_","Pit_","Royal_","scalebone_",
					"carrion_","Crusader_","Trooper_","hunter_",
					"decaying_","iksar_","klok_","templar_","lord_",
					"froglok_","war_","large_","charbone_","icebone_",
					"Vicar_","Cavalier_","Heretic_","Reaver_","venomous_",
					"Sheildbearer_","pond_","mountain_","plaguebone_","Brother_",
					"great_","strathbone_","briarweb_","strathbone_","skeletal_",
					"minion_","spectral_","myconid_","spurbone_","sabretooth_",
					"Tin_","Iron_","Erollisi_","Petrifier_","Burynai_",
					"undead_","decayed_","You_","smoldering_","gyrating_",
					"lumpy_","Marshal_","Sheriff_","Chief_","Risen_",
					"lascar_","tribal_","fungi_","Xi_","Legionnaire_",
					"Centurion_","Zun_","Diabo_","Scribe_","Defender_","Capt_",
					"blazing_","Solusek_","imp_","hexbone_","elementalbone_",
					"stone_","lava_","_",""
				};
				unsigned int MyArraySize;
				 for ( MyArraySize = 0; true; MyArraySize++) {   //Find empty string & get size
				   if (!(*(MyArray[MyArraySize]))) break;   //Checks for null char in 1st pos
				};
				if (NamedOnly) {
				   bool ContinueFlag = false;
				   const char *CurEntityName = cur_entity->GetName();  //Call function once
				   for (int Index = 0; Index < MyArraySize; Index++) {
				      if (!strncasecmp(CurEntityName, MyArray[Index], strlen(MyArray[Index])) || (Extras)) {
				         iterator.Advance();
				         ContinueFlag = true;
				         break;   //From Index for
				       };
				   };
				  if (ContinueFlag) continue; //Moved here or would apply to Index for
				};

				CurrentCon = client->GetLevelCon(cur_entity->GetLevel());
				if(CurrentCon != LastCon) {

					if(LastCon != -1)
						WindowText += "</c>";

					LastCon = CurrentCon;

					switch(CurrentCon) {

						case CON_GREEN: {
							WindowText += "<c \"#00FF00\">";
							break;
						}

						case CON_LIGHTBLUE: {
							WindowText += "<c \"#8080FF\">";
							break;
						}
						case CON_BLUE: {
							WindowText += "<c \"#2020FF\">";
							break;
						}

						case CON_YELLOW: {
							WindowText += "<c \"#FFFF00\">";
							break;
						}
						case CON_RED: {
							WindowText += "<c \"#FF0000\">";
							break;
						}
						default: {
							WindowText += "<c \"#FFFFFF\">";
							break;
						}
					}
				}

				WindowText += cur_entity->GetCleanName();
				WindowText += "<br>";

				if(strlen(WindowText.c_str()) > 4000) {
					// Popup window is limited to 4096 characters.
					WindowText += "</c><br><br>List truncated ... too many mobs to display";
					break;
				}
			}
		}

		iterator.Advance();
	}
	WindowText += "</c>";

	client->SendPopupToClient(WindowTitle, WindowText.c_str());

	return; 
}

int8 Bot::GetNumberNeedingHealedInGroup(int8 hpr, bool includePets) {
	int8 needHealed = 0;
	Group *g;
	
	if(this->HasGroup()) {
		g = this->GetGroup();
			
		if(g) {
			for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
				if(g->members[i] && !g->members[i]->qglobal) {

					if(g->members[i]->GetHPRatio() <= hpr) 
						needHealed++;

					if(includePets) {
						if(g->members[i]->GetPet() && g->members[i]->GetPet()->GetHPRatio() <= hpr) {
							needHealed++;
						}
					}
				}
			}	
		}
	}

	return needHealed;
}

uint32 Bot::GetEquipmentColor(int8 material_slot) const
{
	//Bot tints
	int32 slotid = 0;
	uint32 returncolor = 0;
	uint32 botid = this->GetBotID();
	
	//Translate code slot # to DB slot #
	slotid = Inventory::CalcSlotFromMaterial(material_slot);

	//read from db
	char* Query = 0;
	MYSQL_RES* DatasetResult;
	MYSQL_ROW DataRow;
	
	if(database.RunQuery(Query, MakeAnyLenString(&Query, "SELECT color FROM botinventory WHERE BotID = %u AND SlotID = %u", botid, slotid), 0, &DatasetResult)) {
		if(mysql_num_rows(DatasetResult) == 1) {
			DataRow = mysql_fetch_row(DatasetResult);
			if(DataRow)
				returncolor = atoi(DataRow[0]);
		}
		mysql_free_result(DatasetResult);
		safe_delete_array(Query);
	}
	return returncolor;
}

int Bot::GetRawACNoShield(int &shield_ac)
{
	int ac = itembonuses.AC + spellbonuses.AC;
	shield_ac = 0;
	ItemInst* inst = GetBotItem(SLOT_SECONDARY);
	if(inst)
	{
		if(inst->GetItem()->ItemType == ItemTypeShield)
		{
			ac -= inst->GetItem()->AC;
			shield_ac = inst->GetItem()->AC;
			for(uint8 i = 0; i < MAX_AUGMENT_SLOTS; i++)
			{
				if(inst->GetAugment(i))
				{
					ac -= inst->GetAugment(i)->GetItem()->AC;
					shield_ac += inst->GetAugment(i)->GetItem()->AC;
				}
			}
		}
	}
	return ac;
}

uint32 Bot::CalcCurrentWeight() {

	const Item_Struct* TempItem = 0;
	ItemInst* inst;
	uint32 Total = 0;
	
	for(int i=0; i<=21; ++i) {
		inst = GetBotItem(i);
		if(inst) {
			TempItem = inst->GetItem();
		if (TempItem)
			Total += TempItem->Weight;
		}
	}

	float Packrat = (float)spellbonuses.Packrat + (float)aabonuses.Packrat;

	if (Packrat > 0)
		Total = (uint32)((float)Total * (1.0f - ((Packrat * 1.0f) / 100.0f)));	//AndMetal: 1% per level, up to 5% (calculated from Titanium client). verified thru client that it reduces coin weight by the same %
																				//without casting to float & back to uint32, this didn't work right

	return Total;
}

int Bot::GroupLeadershipAAHealthEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < 3))
		return 0;

	switch(g->GetLeadershipAA(groupAAHealthEnhancement))
	{
		case 0:
			return 0;
		case 1:
			return 30;
		case 2:
			return 60;
		case 3:
			return 100;
	}

	return 0;
}

int Bot::GroupLeadershipAAManaEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < 3))
		return 0;

	switch(g->GetLeadershipAA(groupAAManaEnhancement))
	{
		case 0:
			return 0;
		case 1:
			return 30;
		case 2:
			return 60;
		case 3:
			return 100;
	}

	return 0;
}

int Bot::GroupLeadershipAAHealthRegeneration()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < 3))
		return 0;

	switch(g->GetLeadershipAA(groupAAHealthRegeneration))
	{
		case 0:
			return 0;
		case 1:
			return 4;
		case 2:
			return 6;
		case 3:
			return 8;
	}

	return 0;
}

int Bot::GroupLeadershipAAOffenseEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < 3))
		return 0;

	switch(g->GetLeadershipAA(groupAAOffenseEnhancement))
	{
		case 0:
			return 0;
		case 1:
			return 10;
		case 2:
			return 19;
		case 3:
			return 28;
		case 4:
			return 34;
		case 5:
			return 40;
	}
	return 0;
}


#endif
