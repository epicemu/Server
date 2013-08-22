/*  EQEMu:  Everquest Server Emulator
Copyright (C) 2001-2002  EQEMu Development Team (http://eqemulator.net)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	
	  You should have received a copy of the GNU General Public License
	  along with this program; if not, write to the Free Software
	  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#if EQDEBUG >= 5
//#define ATTACK_DEBUG 20
#endif

#include "../common/debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
using namespace std;
#include <assert.h>

#include "masterentity.h"
#include "NpcAI.h"
#include "../common/packet_dump.h"
#include "../common/eq_packet_structs.h"
#include "../common/eq_constants.h"
#include "../common/skills.h"
#include "spdat.h"
#include "zone.h"
#include "StringIDs.h"
#include "../common/MiscFunctions.h"
#include "../common/rulesys.h"
#include "QuestParserCollection.h"
#include "watermap.h"


#ifdef WIN32
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp  _stricmp
#endif

extern EntityList entity_list;
#if !defined(NEW_LoadSPDat) && !defined(DB_LoadSPDat)
	extern SPDat_Spell_Struct spells[SPDAT_RECORDS];
#endif

extern Zone* zone;

bool Mob::AttackAnimation(SkillType &skillinuse, int Hand, const ItemInst* weapon)
{
	// Determine animation
	int type = 0;
	if (weapon && weapon->IsType(ItemClassCommon)) {
		const Item_Struct* item = weapon->GetItem();
#if EQDEBUG >= 11
			LogFile->write(EQEMuLog::Debug, "Weapon skill:%i", item->ItemType);
#endif		
		switch (item->ItemType)
		{
			case ItemType1HS: // 1H Slashing
			{
				skillinuse = _1H_SLASHING;
				type = anim1HWeapon;
				break;
			}
			case ItemType2HS: // 2H Slashing
			{
				skillinuse = _2H_SLASHING;
				type = anim2HSlashing;
				break;
			}
			case ItemTypePierce: // Piercing
			{
				skillinuse = PIERCING;
				type = animPiercing;
				break;
			}
			case ItemType1HB: // 1H Blunt
			{
				skillinuse = _1H_BLUNT;
				type = anim1HWeapon;
				break;
			}
			case ItemType2HB: // 2H Blunt
			{
				skillinuse = _2H_BLUNT;
				type = anim2HWeapon;
				break;
			}
			case ItemType2HPierce: // 2H Piercing
			{
				skillinuse = PIERCING;
				type = anim2HWeapon;
				break;
			}
			case ItemTypeHand2Hand:
			{
				skillinuse = HAND_TO_HAND;
				type = animHand2Hand;
				break;
			}
			default:
			{
				skillinuse = HAND_TO_HAND;
				type = animHand2Hand;
				break;
			}
		}// switch
	}
	else if(IsNPC()) {

		switch (skillinuse)
		{
			case _1H_SLASHING: // 1H Slashing
			{
				type = anim1HWeapon;
				break;
			}
			case _2H_SLASHING: // 2H Slashing
			{
				type = anim2HSlashing;
				break;
			}
			case PIERCING: // Piercing
			{
				type = animPiercing;
				break;
			}
			case _1H_BLUNT: // 1H Blunt
			{
				type = anim1HWeapon;
				break;
			}
			case _2H_BLUNT: // 2H Blunt
			{
				type = anim2HWeapon;
				break;
			}
			case 99: // 2H Piercing
			{
				type = anim2HWeapon;
				break;
			}
			case HAND_TO_HAND:
			{
				type = animHand2Hand;
				break;
			}
			default:
			{
				type = animHand2Hand;
				break;
			}
		}// switch
	}
	else {
		skillinuse = HAND_TO_HAND;
		type = animHand2Hand;
	}
	
	// If we're attacking with the secondary hand, play the dual wield anim
	if (Hand == 14)	// DW anim
		type = animDualWield;

	DoAnim(type);
    return true;
}

// called when a mob is attacked, does the checks to see if it's a hit
// and does other mitigation checks.  'this' is the mob being attacked.
bool Mob::CheckHitChance(Mob* other, SkillType skillinuse, int Hand)
{
/*
		Reworked a lot of this code to achieve better balance at higher levels.
		The old code basically meant that any in high level (50+) combat,
		both parties always had 95% chance to hit the other one.
*/
	Mob *attacker=other;
	Mob *defender=this;
	float chancetohit = RuleR(Combat, BaseHitChance);

	if(attacker->IsNPC() && !attacker->IsPet())
		chancetohit += RuleR(Combat, NPCBonusHitChance);

#if ATTACK_DEBUG>=11
		LogFile->write(EQEMuLog::Debug, "CheckHitChance(%s) attacked by %s", defender->GetName(), attacker->GetName());
#endif
	
	bool pvpmode = false;
	if(IsClient() && other->IsClient())
		pvpmode = true;
	
	float bonus;
	
	////////////////////////////////////////////////////////
	// To hit calcs go here
	////////////////////////////////////////////////////////

	int8 attacker_level = attacker->GetLevel() ? attacker->GetLevel() : 1;
	int8 defender_level = defender->GetLevel() ? defender->GetLevel() : 1;

	//Calculate the level difference

	mlog(COMBAT__TOHIT, "Chance to hit before level diff calc %.2f", chancetohit);
	double level_difference = attacker_level - defender_level;
	double range = defender->GetLevel();
	range = ((range / 4) + 3);

	if(level_difference < 0)
	{
		if(level_difference >= -range)
		{
			chancetohit += (level_difference / range) * RuleR(Combat,HitFalloffMinor); //5
		}
		else if (level_difference >= -(range+3.0))
		{
			chancetohit -= RuleR(Combat,HitFalloffMinor);
			chancetohit += ((level_difference+range) / (3.0)) * RuleR(Combat,HitFalloffModerate); //7
		}
		else
		{
			chancetohit -= (RuleR(Combat,HitFalloffMinor) + RuleR(Combat,HitFalloffModerate));
			chancetohit += ((level_difference+range+3.0)/12.0) * RuleR(Combat,HitFalloffMajor); //50
		}
	}
	else
	{
		chancetohit += (RuleR(Combat,HitBonusPerLevel) * level_difference); 
	}

	mlog(COMBAT__TOHIT, "Chance to hit after level diff calc %.2f", chancetohit);

	chancetohit -= ((float)defender->GetAGI() * RuleR(Combat, AgiHitFactor));

	mlog(COMBAT__TOHIT, "Chance to hit after agil calc %.2f", chancetohit);

	if(attacker->IsClient())
	{
		chancetohit -= (RuleR(Combat,WeaponSkillFalloff) * (attacker->CastToClient()->MaxSkill(skillinuse) - attacker->GetSkill(skillinuse)));
		mlog(COMBAT__TOHIT, "Chance to hit after weapon falloff calc (attack) %.2f", chancetohit);
	}

	if(defender->IsClient())
	{
		chancetohit += (RuleR(Combat,WeaponSkillFalloff) * (defender->CastToClient()->MaxSkill(DEFENSE) - defender->GetSkill(DEFENSE)));
		mlog(COMBAT__TOHIT, "Chance to hit after weapon falloff calc (defense) %.2f", chancetohit);
	}

	//I dont think this is 100% correct, but at least it does something...
	if(attacker->spellbonuses.MeleeSkillCheckSkill == skillinuse || attacker->spellbonuses.MeleeSkillCheckSkill == 255) {
		chancetohit += attacker->spellbonuses.MeleeSkillCheck;
		mlog(COMBAT__TOHIT, "Applied spell melee skill bonus %d, yeilding %.2f", attacker->spellbonuses.MeleeSkillCheck, chancetohit);
	}
	if(attacker->itembonuses.MeleeSkillCheckSkill == skillinuse || attacker->itembonuses.MeleeSkillCheckSkill == 255) {
		chancetohit += attacker->itembonuses.MeleeSkillCheck;
		mlog(COMBAT__TOHIT, "Applied item melee skill bonus %d, yeilding %.2f", attacker->spellbonuses.MeleeSkillCheck, chancetohit);
	}
	
	//subtract off avoidance by the defender
	bonus = defender->spellbonuses.AvoidMeleeChance + defender->itembonuses.AvoidMeleeChance;
	if(bonus > 0) {
		chancetohit -= ((bonus * chancetohit) / 1000);
		mlog(COMBAT__TOHIT, "Applied avoidance chance %.2f/10, yeilding %.2f", bonus, chancetohit);
	}

	if(attacker->IsNPC())
		chancetohit += (chancetohit * attacker->CastToNPC()->GetAccuracyRating() / 1000);

	mlog(COMBAT__TOHIT, "Chance to hit after accuracy rating calc %.2f", chancetohit);

	uint16 AA_mod = 0;
	switch(GetAA(aaCombatAgility))
	{
	case 1:
		AA_mod = 2;
		break;
	case 2:
		AA_mod = 5;
		break;
	case 3:
		AA_mod = 10;
		break;
	}
	AA_mod += 3*GetAA(aaPhysicalEnhancement);
	AA_mod += 2*GetAA(aaLightningReflexes);
	AA_mod += GetAA(aaReflexiveMastery);
	chancetohit -= chancetohit * AA_mod / 100;

	mlog(COMBAT__TOHIT, "Chance to hit after AA calc %.2f", chancetohit);

	//add in our hit chance bonuses if we are using the right skill
	//does the hit chance cap apply to spell bonuses from disciplines?
	float hitBonus = 0;
	if(attacker->spellbonuses.HitChanceSkill == 255 || attacker->spellbonuses.HitChanceSkill == skillinuse)
	{
		hitBonus = (attacker->spellbonuses.HitChance / 15.0f > attacker->spellbonuses.Accuracy) ? attacker->spellbonuses.HitChance / 15.0f : attacker->spellbonuses.Accuracy;
		chancetohit += chancetohit * hitBonus / 100;
		mlog(COMBAT__TOHIT, "Applied spell melee hit chance %.2f, yeilding %.2f", hitBonus, chancetohit);
	}
	else if(attacker->spellbonuses.Accuracy) {
		hitBonus = attacker->spellbonuses.Accuracy;
		chancetohit += chancetohit * hitBonus / 100;
		mlog(COMBAT__TOHIT, "Applied spell melee accuracy chance %.2f, yeilding %.2f", hitBonus, chancetohit);
	}

	hitBonus = 0;
	if(attacker->itembonuses.HitChanceSkill == 255 || attacker->itembonuses.HitChanceSkill == skillinuse)
	{
		hitBonus = (attacker->itembonuses.HitChance / 15.0f > attacker->itembonuses.Accuracy) ? attacker->itembonuses.HitChance / 15.0f : attacker->itembonuses.Accuracy;
		chancetohit += chancetohit * hitBonus / 100;
		mlog(COMBAT__TOHIT, "Applied item melee hit chance %.2f, yeilding %.2f", hitBonus, chancetohit);
	} 
	else if(attacker->itembonuses.Accuracy)
	{
		hitBonus = attacker->itembonuses.Accuracy;
		chancetohit += chancetohit * hitBonus / 100;
		mlog(COMBAT__TOHIT, "Applied item melee accuracy chance %.2f, yeilding %.2f", hitBonus, chancetohit);
	}

	if (attacker->IsClient()) {
		int modAA = 100;
		switch (attacker->CastToClient()->GetAA(aaPrecisionofthePathfinder)) {
			case 1:
				modAA += 2;
				break;
			case 2:
				modAA += 4;
				break;
			case 3:
				modAA += 6;
				break;
		}
		chancetohit = ((chancetohit * modAA) / 100);
	}
	//Wolftousen - Add Berserker Dead Aim AA accuracy bonus for throwing
	if(skillinuse == THROWING)
	{
		switch(GetAA(aaDeadAim))
		{
			case 1:
				chancetohit = chancetohit * 105/100;
				break;
			case 2:
				chancetohit = chancetohit * 110/100;
				break;
			case 3:
				chancetohit = chancetohit * 115/100;
				break;
		}
	}

	if(skillinuse == ARCHERY)
		chancetohit -= (chancetohit * RuleR(Combat, ArcheryHitPenalty)) / 100.0f;

	// Chance to hit;   Max 95%, Min 30%
	if(chancetohit > 1000) {
		//if chance to hit is crazy high, that means a discipline is in use, and let it stay there
	} 
	else if(chancetohit > 95) {
		chancetohit = 95;
	} 
	else if(chancetohit < 5) {
		chancetohit = 5;
	}
	
	//I dont know the best way to handle a garunteed hit discipline being used
	//agains a garunteed riposte (for example) discipline... for now, garunteed hit wins
	
		
	#if EQDEBUG>=11
		LogFile->write(EQEMuLog::Debug, "3 FINAL calculated chance to hit is: %5.2f", chancetohit);
	#endif

	//
	// Did we hit?
	//

	float tohit_roll = MakeRandomFloat(0, 100);

	mlog(COMBAT__TOHIT, "Final hit chance: %.2f%%. Hit roll %.2f", chancetohit, tohit_roll);
	return(tohit_roll <= chancetohit);
}

bool Mob::AvoidDamage(Mob* other, sint32 &damage)
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
	float skill;
	float bonus;
	float RollTable[4] = {0,0,0,0};
	float roll;
	Mob *attacker=other;
	Mob *defender=this;

	//garunteed hit
	bool ghit = false;
	if((attacker->spellbonuses.MeleeSkillCheck + attacker->itembonuses.MeleeSkillCheck) > 500)
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
	float riposte_chance = 0.0f;
	if (damage > 0 && CanThisClassRiposte() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
		riposte_chance = (100.0f + (float)defender->spellbonuses.RiposteChance + (float)defender->itembonuses.RiposteChance) / 100.0f;
        skill = GetSkill(RIPOSTE);
		if (IsClient()) {
			CastToClient()->CheckIncreaseSkill(RIPOSTE, other, -10);
		}
		
		if (!ghit) {	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetDEX()/200);
			bonus *= riposte_chance;
			RollTable[0] = bonus + (itembonuses.HeroicDEX / 25); // 25 heroic = 1%, applies to ripo, parry, block
		}
	}
	
	///////////////////////////////////////////////////////	
	// block
	///////////////////////////////////////////////////////

	bool bBlockFromRear = false;

	if (this->IsClient()) {
		float aaChance = 0;

		// a successful roll on this does not mean a successful block is forthcoming. only that a chance to block
		// from a direction other than the rear is granted.
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

		if (aaChance > MakeRandomInt(1, 100))
			bBlockFromRear = true;
	}

	float block_chance = 0.0f;
	if (damage > 0 && CanThisClassBlock() && (!other->BehindMob(this, other->GetX(), other->GetY()) || bBlockFromRear)) {
		block_chance = (100.0f + (float)spellbonuses.IncreaseBlockChance + (float)itembonuses.IncreaseBlockChance) / 100.0f;
		skill = CastToClient()->GetSkill(BLOCKSKILL);
		if (IsClient()) {
			CastToClient()->CheckIncreaseSkill(BLOCKSKILL, other, -10);
		}
		
		if (!ghit) {	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/35.0 + (GetDEX()/200);
			RollTable[1] = RollTable[0] + (bonus * block_chance) - riposte_chance;
			block_chance *= bonus; // set this so we can remove it from the parry calcs
		}
	}
	else{
		RollTable[1] = RollTable[0];
	}

	if(damage > 0 && GetAA(aaShieldBlock) && (!other->BehindMob(this, other->GetX(), other->GetY()))) {
		bool equiped = CastToClient()->m_inv.GetItem(14);
		if(equiped) {
			uint8 shield = CastToClient()->m_inv.GetItem(14)->GetItem()->ItemType;

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
	float parry_chance = 0.0f;
	if (damage > 0 && CanThisClassParry() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
		parry_chance = (100.0f + (float)defender->spellbonuses.ParryChance + (float)defender->itembonuses.ParryChance) / 100.0f;
		skill = CastToClient()->GetSkill(PARRY);
		if (IsClient()) {
			CastToClient()->CheckIncreaseSkill(PARRY, other, -10); 
		}
		
		if (!ghit) {	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetDEX()/200);
			bonus *= parry_chance;
			RollTable[2] = RollTable[1] + bonus - block_chance;
		}
	}
	else{
		RollTable[2] = RollTable[1] - block_chance;
	}
	
	////////////////////////////////////////////////////////
	// dodge
	////////////////////////////////////////////////////////
	float dodge_chance = 0.0f;
	if (damage > 0 && CanThisClassDodge() && !other->BehindMob(this, other->GetX(), other->GetY()))
	{
		dodge_chance = (100.0f + (float)defender->spellbonuses.DodgeChance + (float)defender->itembonuses.DodgeChance) / 100.0f;
        skill = CastToClient()->GetSkill(DODGE);
		if (IsClient()) {
			CastToClient()->CheckIncreaseSkill(DODGE, other, -10);
		}
		
		if (!ghit) {	//if they are not using a garunteed hit discipline
			bonus = 2.0 + skill/60.0 + (GetAGI()/200);
			bonus *= dodge_chance;
			RollTable[3] = RollTable[2] + bonus - (itembonuses.HeroicDEX / 25) + (itembonuses.HeroicAGI / 25) - parry_chance; // Remove the dex as it doesnt count for dodge
		}
	}
	else{
		RollTable[3] = RollTable[2] - (itembonuses.HeroicDEX / 25) + (itembonuses.HeroicAGI / 25) - parry_chance;
	}

	if(damage > 0){
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
//Original Location of MeleeMitigaion
void Mob::MeleeMitigation(Mob *attacker, sint32 &damage, sint32 minhit)
{
	if(damage <= 0)
		return;

	Mob* defender = this;
	float aa_mit = 0;

	switch(GetAA(aaCombatStability)){
		case 1:
			aa_mit += 0.02;
			break;
		case 2:
			aa_mit += 0.05;
			break;
		case 3:
			aa_mit += 0.10;
			break;
	}

	aa_mit += GetAA(aaPhysicalEnhancement) * 0.02;
	aa_mit += GetAA(aaInnateDefense) * 0.03;
	aa_mit += GetAA(aaDefensiveInstincts)*0.02;

	if(RuleB(Combat, UseIntervalAC))
	{
		float softcap = 0.0;
		float mitigation_rating = 0.0;
		float attack_rating = 0.0;
		int shield_ac = 0;
		int armor;
		float weight = 0.0;
		if(IsClient())
		{
			armor = CastToClient()->GetRawACNoShield(shield_ac);
			weight = (CastToClient()->CalcCurrentWeight() / 10.0);
		}
		else if(IsNPC())
		{
			armor = spellbonuses.AC + itembonuses.AC + (CastToNPC()->GetRawAC() / RuleR(Combat, NPCACFactor)) + 1;
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
			attack_rating = (attacker->CastToClient()->CalcATK() + ((attacker->GetSTR()-66) * 0.9) + (attacker->GetSkill(OFFENSE)*1.345));
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
	damage -= (minhit * defender->itembonuses.MeleeMitigation / 100);
	damage -= (damage * defender->spellbonuses.MeleeMitigation / 100);

	if(damage < 0)
		damage = 0;
}
//Returns the weapon damage against the input mob
//if we cannot hit the mob with the current weapon we will get a value less than or equal to zero
//Else we know we can hit.
//GetWeaponDamage(mob*, const Item_Struct*) is intended to be used for mobs or any other situation where we do not have a client inventory item
//GetWeaponDamage(mob*, const ItemInst*) is intended to be used for situations where we have a client inventory item
int Mob::GetWeaponDamage(Mob *against, const Item_Struct *weapon_item) {
	_ZP(Mob_GetWeaponDamageA);
	int dmg = 0;
	int banedmg = 0;

	//can't hit invulnerable stuff with weapons.
	if(against->GetInvul() || against->SpecAttacks[IMMUNE_MELEE]){
		return 0;
	}
	
	//check to see if our weapons or fists are magical.
	if(against->SpecAttacks[IMMUNE_MELEE_NONMAGICAL]){
		if(weapon_item){
			if(weapon_item->Magic){
				dmg = weapon_item->Damage;

				//this is more for non weapon items, ex: boots for kick
				//they don't have a dmg but we should be able to hit magical
				dmg = dmg <= 0 ? 1 : dmg;
			}
			else
				return 0;
		}
		else{
			if((GetClass() == MONK || GetClass() == BEASTLORD) && GetLevel() >= 30){
				dmg = GetMonkHandToHandDamage();
			}
			else if(GetOwner() && GetLevel() >= RuleI(Combat, PetAttackMagicLevel)){
				//pets wouldn't actually use this but...
				//it gives us an idea if we can hit due to the dual nature of this function
				dmg = 1;						   
			}
			else if(SpecAttacks[SPECATK_MAGICAL])
			{
				dmg = 1;
			}
			else
				return 0;
		}
	}
	else{
		if(weapon_item){
			dmg = weapon_item->Damage;

			dmg = dmg <= 0 ? 1 : dmg;
		}
		else{
			if(GetClass() == MONK || GetClass() == BEASTLORD){
				dmg = GetMonkHandToHandDamage();
			}
			else{
				dmg = 1;
			}
		}
	}
	
	int eledmg = 0;
	if(!against->SpecAttacks[IMMUNE_MAGIC]){
		if(weapon_item && weapon_item->ElemDmgAmt){
			//we don't check resist for npcs here
			eledmg = weapon_item->ElemDmgAmt;
			dmg += eledmg;
		}
	}

	if(against->SpecAttacks[IMMUNE_MELEE_EXCEPT_BANE]){
		if(weapon_item){
			if(weapon_item->BaneDmgBody == against->GetBodyType()){
				banedmg += weapon_item->BaneDmgAmt;
			}

			if(weapon_item->BaneDmgRace == against->GetRace()){
				banedmg += weapon_item->BaneDmgRaceAmt;
			}
		}

		if(!eledmg && !banedmg){
			if(!SpecAttacks[SPECATK_BANE])
				return 0;
			else
				return 1;
		}
		else
			dmg += banedmg;
	}
	else{
		if(weapon_item){
			if(weapon_item->BaneDmgBody == against->GetBodyType()){
				banedmg += weapon_item->BaneDmgAmt;
			}

			if(weapon_item->BaneDmgRace == against->GetRace()){
				banedmg += weapon_item->BaneDmgRaceAmt;
			}
		}

		dmg += (banedmg + eledmg);
	}

	if(dmg <= 0){
		return 0;
	}
	else
		return dmg;
}

int Mob::GetWeaponDamage(Mob *against, const ItemInst *weapon_item, int32 *hate)
{
	_ZP(Mob_GetWeaponDamageB);
	int dmg = 0;
	int banedmg = 0;

	if(against->GetInvul() || against->SpecAttacks[IMMUNE_MELEE]){
		return 0;
	}

	//check for items being illegally attained
	if(weapon_item){
		const Item_Struct *mWeaponItem = weapon_item->GetItem();
		if(mWeaponItem){
			if(mWeaponItem->ReqLevel > GetLevel()){
				return 0;
			}

			if(!weapon_item->IsEquipable(GetBaseRace(), GetClass())){
				return 0;
			}
		}
		else{
			return 0;
		}
	}

	if(against->SpecAttacks[IMMUNE_MELEE_NONMAGICAL]){
		if(weapon_item){
			// check to see if the weapon is magic
			bool MagicWeapon = false;
			if(weapon_item->GetItem() && weapon_item->GetItem()->Magic) 
				MagicWeapon = true;
			else {					
				if(spellbonuses.MagicWeapon || itembonuses.MagicWeapon)
					MagicWeapon = true;
			}
			
			if(MagicWeapon) {

				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->Damage);
				}
				else{
					dmg = weapon_item->GetItem()->Damage;
				}

				for(int x = 0; x < 5; x++){
					if(weapon_item->GetAugment(x) && weapon_item->GetAugment(x)->GetItem()){
						dmg += weapon_item->GetAugment(x)->GetItem()->Damage;
						if (hate) *hate += weapon_item->GetAugment(x)->GetItem()->Damage + weapon_item->GetAugment(x)->GetItem()->ElemDmgAmt;
					}
				}
				dmg = dmg <= 0 ? 1 : dmg;
			}
			else
				return 0;
		}
		else{
			if((GetClass() == MONK || GetClass() == BEASTLORD) && GetLevel() >= 30){
				dmg = GetMonkHandToHandDamage();
				if (hate) *hate += dmg;
			}
			else if(GetOwner() && GetLevel() >= RuleI(Combat, PetAttackMagicLevel)){ //pets wouldn't actually use this but...
				dmg = 1;						   //it gives us an idea if we can hit
			}
			else if(SpecAttacks[SPECATK_MAGICAL]){
				dmg = 1;
			}
			else
				return 0;
		}
	}
	else{
		if(weapon_item){
			if(weapon_item->GetItem()){

				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->Damage);
				}
				else{
					dmg = weapon_item->GetItem()->Damage;
				}

				for(int x = 0; x < 5; x++){
					if(weapon_item->GetAugment(x) && weapon_item->GetAugment(x)->GetItem()){
						dmg += weapon_item->GetAugment(x)->GetItem()->Damage;
						if (hate) *hate += weapon_item->GetAugment(x)->GetItem()->Damage + weapon_item->GetAugment(x)->GetItem()->ElemDmgAmt;
					}
				}
				dmg = dmg <= 0 ? 1 : dmg;
			}
		}
		else{
			if(GetClass() == MONK || GetClass() == BEASTLORD){
				dmg = GetMonkHandToHandDamage();
				if (hate) *hate += dmg;
			}
			else{
				dmg = 1;
			}
		}
	}

	int eledmg = 0;
	if(!against->SpecAttacks[IMMUNE_MAGIC]){
		if(weapon_item && weapon_item->GetItem() && weapon_item->GetItem()->ElemDmgAmt){
			if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
				eledmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->ElemDmgAmt);
			}
			else{
				eledmg = weapon_item->GetItem()->ElemDmgAmt;
			}

			if(eledmg)
			{
				eledmg = (eledmg * against->ResistSpell(weapon_item->GetItem()->ElemDmgType, 0, this) / 100);
			}
		}

		if(weapon_item){
			for(int x = 0; x < 5; x++){
				if(weapon_item->GetAugment(x) && weapon_item->GetAugment(x)->GetItem()){
					if(weapon_item->GetAugment(x)->GetItem()->ElemDmgAmt)
						eledmg += (weapon_item->GetAugment(x)->GetItem()->ElemDmgAmt * against->ResistSpell(weapon_item->GetAugment(x)->GetItem()->ElemDmgType, 0, this) / 100);
				}
			}
		}
	}

	if(against->SpecAttacks[IMMUNE_MELEE_EXCEPT_BANE]){
		if(weapon_item && weapon_item->GetItem()){
			if(weapon_item->GetItem()->BaneDmgBody == against->GetBodyType()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}

			if(weapon_item->GetItem()->BaneDmgRace == against->GetRace()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgRaceAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgRaceAmt;
				}
			}

			for(int x = 0; x < 5; x++){
				if(weapon_item->GetAugment(x) && weapon_item->GetAugment(x)->GetItem()){
					if(weapon_item->GetAugment(x)->GetItem()->BaneDmgBody == against->GetBodyType()){
						banedmg += weapon_item->GetAugment(x)->GetItem()->BaneDmgAmt;
					}

					if(weapon_item->GetAugment(x)->GetItem()->BaneDmgRace == against->GetRace()){
						banedmg += weapon_item->GetAugment(x)->GetItem()->BaneDmgRaceAmt;
					}
				}
			}
		}

		if(!eledmg && !banedmg)
		{
			if(!SpecAttacks[SPECATK_BANE])
				return 0;
			else
				return 1;
		}
		else {
			dmg += (banedmg + eledmg);
			if (hate) *hate += banedmg;
		}
	}
	else{
		if(weapon_item && weapon_item->GetItem()){
			if(weapon_item->GetItem()->BaneDmgBody == against->GetBodyType()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}

			if(weapon_item->GetItem()->BaneDmgRace == against->GetRace()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgRaceAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgRaceAmt;
				}
			}

			for(int x = 0; x < 5; x++){
				if(weapon_item->GetAugment(x) && weapon_item->GetAugment(x)->GetItem()){
					if(weapon_item->GetAugment(x)->GetItem()->BaneDmgBody == against->GetBodyType()){
						banedmg += weapon_item->GetAugment(x)->GetItem()->BaneDmgAmt;
					}

					if(weapon_item->GetAugment(x)->GetItem()->BaneDmgRace == against->GetRace()){
						banedmg += weapon_item->GetAugment(x)->GetItem()->BaneDmgRaceAmt;
					}
				}
			}
		}
		dmg += (banedmg + eledmg);
		if (hate) *hate += banedmg;
	}

	if(dmg <= 0){
		return 0;
	}
	else
		return dmg;
}


//Jeremy's Cone Script
bool Mob::IsInAttackCone(Mob *target)
 {
		float my_heading = GetHeading();
		float ideal_heading = CalculateHeadingToTarget(target->GetX(), target->GetY());
		float range = 39.8f; // 55.969 degrees in either direction
 
		if(ideal_heading - range < 0.0f) { // wrap around
			float bottomend = ideal_heading - range + 256.0f;
			return (my_heading >= 0.0f && my_heading <= ideal_heading + range) || (my_heading >= bottomend && my_heading < 256.0f);
		}
		else if(ideal_heading + range >= 256.0f) { // wrap around the other way
			float topend = ideal_heading + range - 256.0f;
			return (my_heading < 256.0f && my_heading >= ideal_heading - range) || (my_heading >= 0.0f && my_heading <= topend);
		}
		else // no wrap around
		return my_heading <= ideal_heading + range && my_heading >= ideal_heading - range;
 }


//note: throughout this method, setting `damage` to a negative is a way to
//stop the attack calculations
bool Client::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough)
{

	_ZP(Client_Attack);

	if (!other) {
		SetTarget(NULL);
		LogFile->write(EQEMuLog::Error, "A null Mob object was passed to Client::Attack() for evaluation!");
		return false;
	}
	
	if(!GetTarget())
		SetTarget(other);
	
	mlog(COMBAT__ATTACKS, "Attacking %s with hand %d %s", other?other->GetName():"(NULL)", Hand, bRiposte?"(this is a riposte)":"");
	
	//SetAttackTimer();
	if (
		   (IsCasting() && GetClass() != BARD)
		|| other == NULL
		|| ((IsClient() && CastToClient()->dead) || (other->IsClient() && other->CastToClient()->dead))
		|| (GetHP() < 0)
		|| (!IsAttackAllowed(other))
		) {
		mlog(COMBAT__ATTACKS, "Attack canceled, invalid circumstances.");
		return false; // Only bards can attack while casting
	}
	if(DivineAura() && !GetGM()) {//cant attack while invulnerable unless your a gm
		mlog(COMBAT__ATTACKS, "Attack canceled, Divine Aura is in effect.");
		Message_StringID(MT_DefaultText, DIVINE_AURA_NO_ATK);	//You can't attack while invulnerable!
		return false;
	}

	if (GetFeigned())
		return false; // Rogean: How can you attack while feigned? Moved up from Aggro Code.
		
	if(!IsInAttackCone(other)) {
		//mlog(COMBAT__ATTACKS, "Attack canceled, target not within attack cone.");
		return false;
	}
	
	
	ItemInst* weapon;
	if (Hand == 14)	// Kaiyodo - Pick weapon from the attacking hand
		weapon = GetInv().GetItem(SLOT_SECONDARY);
	else
		weapon = GetInv().GetItem(SLOT_PRIMARY);

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
	int8 mylevel = GetLevel() ? GetLevel() : 1;
	int32 hate = 0;
	if (weapon) hate = weapon->GetItem()->Damage + weapon->GetItem()->ElemDmgAmt;
	int weapon_damage = GetWeaponDamage(other, weapon, &hate);
	if (hate == 0 && weapon_damage > 1) hate = weapon_damage;
	
	//if weapon damage > 0 then we know we can hit the target with this weapon
	//otherwise we cannot and we set the damage to -5 later on
	if(weapon_damage > 0){
		
		//Berserker Berserk damage bonus
		if(berserk && GetClass() == BERSERKER){
			int bonus = 3 + GetLevel()/10;		//unverified
			weapon_damage = weapon_damage * (100+bonus) / 100;
			mlog(COMBAT__DAMAGE, "Berserker damage bonus increases DMG to %d", weapon_damage);
		}

		//try a finishing blow.. if successful end the attack
		if(TryFinishingBlow(other, skillinuse))
			return (true);
		
		int min_hit = 1;
		int max_hit = (2*weapon_damage*GetDamageTable(skillinuse)) / 100;

		if(GetLevel() < 10 && max_hit > RuleI(Combat, HitCapPre10))
			max_hit = (RuleI(Combat, HitCapPre10));
		else if(GetLevel() < 20 && max_hit > RuleI(Combat, HitCapPre20))
			max_hit = (RuleI(Combat, HitCapPre20));
		else if(GetLevel() < 30 && max_hit > RuleI(Combat, HitCapPre30)) // Add pre 30 dmg cap, rule in ruletypes.h - Kegz @ EpicEmu.com
			max_hit = (RuleI(Combat, HitCapPre30));

		CheckIncreaseSkill(skillinuse, other, -15);
		CheckIncreaseSkill(OFFENSE, other, -15);


		// ***************************************************************
		// *** Calculate the damage bonus, if applicable, for this hit ***
		// ***************************************************************

#ifndef EQEMU_NO_WEAPON_DAMAGE_BONUS

		// If you include the preprocessor directive "#define EQEMU_NO_WEAPON_DAMAGE_BONUS", that indicates that you do not
		// want damage bonuses added to weapon damage at all. This feature was requested by ChaosSlayer on the EQEmu Forums.
		//
		// This is not recommended for normal usage, as the damage bonus represents a non-trivial component of the DPS output
		// of weapons wielded by higher-level melee characters (especially for two-handed weapons).

		if( Hand == 13 && GetLevel() >= 28 && IsWarriorClass() )
		{
			// Damage bonuses apply only to hits from the main hand (Hand == 13) by characters level 28 and above
			// who belong to a melee class. If we're here, then all of these conditions apply.

			int ucDamageBonus = GetWeaponDamageBonus( weapon ? weapon->GetItem() : (const Item_Struct*) NULL );

			min_hit += (int) ucDamageBonus;
			max_hit += (int) ucDamageBonus;
			hate += ucDamageBonus;
		}
#endif

		min_hit = min_hit * (100 + itembonuses.MinDamageModifier + spellbonuses.MinDamageModifier) / 100;

		if (Hand==14) {
			if(GetAA(aaSinisterStrikes)) {
				int sinisterBonus = MakeRandomInt(5, 10);
				min_hit += (min_hit * sinisterBonus / 100);
				max_hit += (max_hit * sinisterBonus / 100);
				hate += (hate * sinisterBonus / 100);
			}
		}

		if(max_hit < min_hit)
			max_hit = min_hit;

		if(RuleB(Combat, UseIntervalAC))
			damage = max_hit;
		else
			damage = MakeRandomInt(min_hit, max_hit);

		mlog(COMBAT__DAMAGE, "Damage calculated to %d (min %d, max %d, str %d, skill %d, DMG %d, lv %d)",
			damage, min_hit, max_hit, GetSTR(), GetSkill(skillinuse), weapon_damage, mylevel);

		//check to see if we hit..
		if(!other->CheckHitChance(this, skillinuse, Hand)) {
			mlog(COMBAT__ATTACKS, "Attack missed. Damage set to 0.");
			damage = 0;
		} else {	//we hit, try to avoid it
			other->AvoidDamage(this, damage);
			other->MeleeMitigation(this, damage, min_hit);
			if(damage > 0) {
				ApplyMeleeDamageBonus(skillinuse, damage);
				damage += (itembonuses.HeroicSTR / 10) + (damage * other->GetSkillDmgTaken(skillinuse) / 100) + GetSkillDmgAmt(skillinuse);
				TryCriticalHit(other, skillinuse, damage);
			}
			mlog(COMBAT__DAMAGE, "Final damage after all reductions: %d", damage);
		}

		//riposte
		bool slippery_attack = false; // Part of hack to allow riposte to become a miss, but still allow a Strikethrough chance (like on Live)
		if (damage == -3)  {
			if (bRiposte) return false;
			else {
				if (Hand == 14 && GetAA(aaSlipperyAttacks) > 0) {// Do we even have it & was attack with mainhand? If not, don't bother with other calculations
					if (MakeRandomInt(0, 100) < (GetAA(aaSlipperyAttacks) * 20)) {
						damage = 0; // Counts as a miss
						slippery_attack = true;
					} else 
						DoRiposte(other);
				}
				else 
					DoRiposte(other);
			}
		}

		//strikethrough..
		int aaStrikethroughBonus = 0;
		switch (GetAA(aaStrikethrough))
		{
		case 1:
			aaStrikethroughBonus = 2;
			break;
		case 2:
			aaStrikethroughBonus = 4;
			break;
		case 3:
			aaStrikethroughBonus = 6;
			break;
		}
		switch (GetAA(aaTacticalMastery))
		{
		case 1:
			aaStrikethroughBonus = 2;
			break;
		case 2:
			aaStrikethroughBonus = 4;
			break;
		case 3:
			aaStrikethroughBonus = 6;
			break;
		}
		if (((damage < 0) || slippery_attack) && !bRiposte && !IsStrikethrough) { // Hack to still allow Strikethrough chance w/ Slippery Attacks AA
			if(MakeRandomInt(0, 100) < (itembonuses.StrikeThrough + spellbonuses.StrikeThrough + aaStrikethroughBonus)) {
				Message_StringID(MT_StrikeThrough, STRIKETHROUGH_STRING); // You strike through your opponents defenses!
				Attack(other, Hand, false, true); // Strikethrough only gives another attempted hit
				return false;
			}
		}
	}
	else{
		damage = -5;
	}


	// Hate Generation is on a per swing basis, regardless of a hit, miss, or block, its always the same.
	// If we are this far, this means we are atleast making a swing.
	if (!bRiposte) // Ripostes never generate any aggro.
		other->AddToHateList(this, hate);
	
	///////////////////////////////////////////////////////////
	//////    Send Attack Damage
	///////////////////////////////////////////////////////////
	other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);
	
	other->Push(this, skillinuse);//Melee/Spell Push
	

	if(damage > 0 && (spellbonuses.MeleeLifetap || itembonuses.MeleeLifetap))
	{
		int lifetap_amt = spellbonuses.MeleeLifetap + itembonuses.MeleeLifetap;
		if(lifetap_amt > 100)
			lifetap_amt = 100;

		lifetap_amt = damage * lifetap_amt / 100;

		mlog(COMBAT__DAMAGE, "Melee lifetap healing for %d damage.", damage);
		//heal self for damage done..
		HealDamage(lifetap_amt);
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
	
	if (damage > 0)
	{
		// Give the opportunity to throw back a defensive proc, if we are successful in affecting damage on our target
		other->TriggerDefensiveProcs(this);	
        return true;
	}
	else
		return false;
}

//used by complete heal and #heal
void Mob::Heal()
{
	SetMaxHP();
	SendHPUpdate();
}

void Client::Damage(Mob* other, sint32 damage, int16 spell_id, SkillType attack_skill, bool avoidable, sint8 buffslot, bool iBuffTic)
{
	if(dead || IsCorpse())
		return;
	
	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	/*if(other && other->IsMob() && flee_timer.Check(false)) {
		FaceTarget(GetTarget()); 
	}*/ //NPC's will face target's more often during combat - Kegz @ EpicEmu.com
	
	
	// cut all PVP spell damage to 2/3 -solar
	// EverHood - Blasting ourselfs is considered PvP 
	//Don't do PvP mitigation if the caster is damaging himself
	if(other && other->IsClient() && (other != this) && damage > 0) {
		int PvPMitigation = 100;
		if(attack_skill == ARCHERY)
			PvPMitigation = 80;
		else
			PvPMitigation = 67;
		damage = (damage * PvPMitigation) / 100;
	}
			
	if(!ClientFinishedLoading())
		damage = -5;

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
	
	

	if (damage > 0) {

		if (spell_id == SPELL_UNKNOWN)
			CheckIncreaseSkill(DEFENSE, other, -15);
	}
}

void Client::Death(Mob* killerMob, sint32 damage, int16 spell, SkillType attack_skill)
{
	if(!ClientFinishedLoading())
		return;

	if(dead)
		return;	//cant die more than once...

	int exploss;

	mlog(COMBAT__HITS, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob ? killerMob->GetName() : "Unknown", damage, spell, attack_skill);
	
	//
	// #1: Send death packet to everyone
	//

	if(!spell) spell = SPELL_UNKNOWN;
	
	SendLogoutPackets();
	
	//make our become corpse packet, and queue to ourself before OP_Death.
	EQApplicationPacket app2(OP_BecomeCorpse, sizeof(BecomeCorpse_Struct));
	BecomeCorpse_Struct* bc = (BecomeCorpse_Struct*)app2.pBuffer;
	bc->spawn_id = GetID();
	bc->x = GetX();
	bc->y = GetY();
	bc->z = GetZ();
	QueuePacket(&app2);
	
	// make death packet
	EQApplicationPacket app(OP_Death, sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)app.pBuffer;
	d->spawn_id = GetID();
	d->killer_id = killerMob ? killerMob->GetID() : 0;
	d->corpseid=GetID();
	d->bindzoneid = m_pp.binds[0].zoneId;
	d->spell_id = spell == SPELL_UNKNOWN ? 0xffffffff : spell;
	d->attack_skill = spell != SPELL_UNKNOWN ? 0xe7 : attack_skill;
	d->damage = damage;
	app.priority = 6;
	entity_list.QueueClients(this, &app);

	//
	// #2: figure out things that affect the player dying and mark them dead
	//


	InterruptSpell();
	SetPet(0);
	SetHorseId(0);
	dead = true;


	if (killerMob != NULL)
	{
		if (killerMob->IsNPC()) {
            parse->EventNPC(EVENT_SLAY, killerMob->CastToNPC(), this, "", 0);
			killerMob->TrySpellOnKill();
		}
		
		if(killerMob->IsClient() && (IsDueling() || killerMob->CastToClient()->IsDueling())) {
			SetDueling(false);
			SetDuelTarget(0);
			if (killerMob->IsClient() && killerMob->CastToClient()->IsDueling() && killerMob->CastToClient()->GetDuelTarget() == GetID())
			{
				//if duel opponent killed us...
				killerMob->CastToClient()->SetDueling(false);
				killerMob->CastToClient()->SetDuelTarget(0);
				entity_list.DuelMessage(killerMob,this,false);
			} else {
				//otherwise, we just died, end the duel.
				Mob* who = entity_list.GetMob(GetDuelTarget());
				if(who && who->IsClient()) {
					who->CastToClient()->SetDueling(false);
					who->CastToClient()->SetDuelTarget(0);
				}
			}
		}
	}

	entity_list.RemoveFromTargets(this);
	hate_list.RemoveEnt(this);
	
	
	//remove ourself from all proximities
	ClearAllProximities();

	//
	// #3: exp loss and corpse generation
	//

	// figure out if they should lose exp
	if(RuleB(Character, UseDeathExpLossMult)){
		float GetNum [] = {0.005f,0.015f,0.025f,0.035f,0.045f,0.055f,0.065f,0.075f,0.085f,0.095f,0.110f };
		int Num = RuleI(Character, DeathExpLossMultiplier);
		if((Num < 0) || (Num > 10))
			Num = 3;
		float loss = GetNum[Num];
		exploss=(int)((float)GetEXP() * (loss)); //loose % of total XP pending rule (choose 0-10)
	}

	if(!RuleB(Character, UseDeathExpLossMult)){
		exploss = (int)(GetLevel() * (GetLevel() / 18.0) * 12000);
	}

	if( (GetLevel() < RuleI(Character, DeathExpLossLevel)) || (GetLevel() > RuleI(Character, DeathExpLossMaxLevel)) || IsBecomeNPC() )
	{
		exploss = 0;
	}
	else if( killerMob )
	{
		if( killerMob->IsClient() )
		{
			exploss = 0;
		}
		else if( killerMob->GetOwner() && killerMob->GetOwner()->IsClient() )
		{
			exploss = 0;
		}
	}

	if(spell != SPELL_UNKNOWN)
	{
		uint32 buff_count = GetMaxTotalSlots();
		for(uint16 buffIt = 0; buffIt < buff_count; buffIt++)
		{
			if(buffs[buffIt].spellid == spell && buffs[buffIt].client)
			{
				exploss = 0;	// no exp loss for pvp dot
				break;
			}
		}
	}

	bool LeftCorpse = false;

	// now we apply the exp loss, unmem their spells, and make a corpse
	// unless they're a GM (or less than lvl 10
	if(!GetGM())
	{
		if(exploss > 0) {
			sint32 newexp = GetEXP();
			if(exploss > newexp) {
				//lost more than we have... wtf..
				newexp = 1;
			} else {
				newexp -= exploss;
			}
			SetEXP(newexp, GetAAXP());
			//m_epp.perAA = 0;	//reset to no AA exp on death.
		}

		//this generates a lot of 'updates' to the client that the client does not need
		BuffFadeAll();
		if((GetClientVersionBit() & BIT_SoFAndLater) && RuleB(Character, RespawnFromHover))
			UnmemSpellAll(true);
		else
			UnmemSpellAll(false);
		
		if(RuleB(Character, LeaveCorpses) && GetLevel() >= RuleI(Character, DeathItemLossLevel) || RuleB(Character, LeaveNakedCorpses))
		{
			// creating the corpse takes the cash/items off the player too
			Corpse *new_corpse = new Corpse(this, exploss);

			char tmp[20];
			database.GetVariable("ServerType", tmp, 9);
			if(atoi(tmp)==1 && killerMob != NULL && killerMob->IsClient()){
				char tmp2[10] = {0};
				database.GetVariable("PvPreward", tmp, 9);
				int reward = atoi(tmp);
				if(reward==3){
					database.GetVariable("PvPitem", tmp2, 9);
					int pvpitem = atoi(tmp2);
					if(pvpitem>0 && pvpitem<200000)
						new_corpse->SetPKItem(pvpitem);
				}
				else if(reward==2)
					new_corpse->SetPKItem(-1);
				else if(reward==1)
					new_corpse->SetPKItem(1);
				else
					new_corpse->SetPKItem(0);
				if(killerMob->CastToClient()->isgrouped) {
					Group* group = entity_list.GetGroupByClient(killerMob->CastToClient());
					if(group != 0) 
					{
						for(int i=0;i<6;i++) 
						{
							if(group->members[i] != NULL) 
							{
								new_corpse->AllowMobLoot(group->members[i],i);
							}
						}
					}
				}
			}
			
			entity_list.AddCorpse(new_corpse, GetID());
			SetID(0);
			
			//send the become corpse packet to everybody else in the zone.
			entity_list.QueueClients(this, &app2, true);

			LeftCorpse = true;
		}

//		if(!IsLD())//Todo: make it so an LDed client leaves corpse if its enabled
//			MakeCorpse(exploss);
	} else {
		BuffFadeDetrimental();
	}



#if 0	// solar: commenting this out for now TODO reimplement becomenpc stuff
	if (IsBecomeNPC() == true)
	{
		if (killerMob != NULL && killerMob->IsClient()) {
			if (killerMob->CastToClient()->isgrouped && entity_list.GetGroupByMob(killerMob) != 0)
				entity_list.GetGroupByMob(killerMob->CastToClient())->SplitExp((uint32)(level*level*75*3.5f), this);

			else
				killerMob->CastToClient()->AddEXP((uint32)(level*level*75*3.5f)); // Pyro: Comment this if NPC death crashes zone
			//hate_list.DoFactionHits(GetNPCFactionID());
		}

		Corpse* corpse = new Corpse(this->CastToClient(), 0);
		entity_list.AddCorpse(corpse, this->GetID());
		this->SetID(0);
		if(killerMob->GetOwner() != 0 && killerMob->GetOwner()->IsClient())
			killerMob = killerMob->GetOwner();
		if(killerMob != 0 && killerMob->IsClient()) {
			corpse->AllowMobLoot(killerMob, 0);
			if(killerMob->CastToClient()->isgrouped) {
				Group* group = entity_list.GetGroupByClient(killerMob->CastToClient());
				if(group != 0) {
					for(int i=0; i < MAX_GROUP_MEMBERS; i++) { // Doesnt work right, needs work
						if(group->members[i] != NULL) {
							corpse->AllowMobLoot(group->members[i],i);
						}
					}
				}
			}
		}
	}
#endif


	//
	// Finally, send em home
	//

	// we change the mob variables, not pp directly, because Save() will copy
	// from these and overwrite what we set in pp anyway
	//

	if(LeftCorpse && (GetClientVersionBit() & BIT_SoFAndLater) && RuleB(Character, RespawnFromHover))
	{
		ClearDraggedCorpses();

		RespawnFromHoverTimer.Start(RuleI(Character, RespawnFromHoverTimer) * 1000);

		SendRespawnBinds();
	}
	else
	{
		if(isgrouped)
		{
			Group *g = GetGroup();
			if(g)
				g->MemberZoned(this);
		}
	
		Raid* r = entity_list.GetRaidByClient(this);

		if(r)
			r->MemberZoned(this);

		dead_timer.Start(5000, true);

		m_pp.zone_id = m_pp.binds[0].zoneId;
		m_pp.zoneInstance = 0;
		database.MoveCharacterToZone(this->CharacterID(), database.GetZoneName(m_pp.zone_id));
		
		Save();
	
		GoToDeath();
	}
}


bool NPC::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough) // Kaiyodo - base function has changed prototype, need to update overloaded version
{
	_ZP(NPC_Attack);
	int damage = 0;
	
	if (!other) {
		SetTarget(NULL);
		LogFile->write(EQEMuLog::Error, "A null Mob object was passed to NPC::Attack() for evaluation!");
		return false;
	}

	if(DivineAura())
		return(false);
	
	if(!GetTarget())
		SetTarget(other);

	//Check that we can attack before we calc heading and face our target	
	if (!IsAttackAllowed(other)) {
		if (this->GetOwnerID())
			entity_list.MessageClose(this, 1, 200, 10, "%s says, 'That is not a legal target master.'", this->GetCleanName());
		if(other) {
			RemoveFromHateList(other);
			mlog(COMBAT__ATTACKS, "I am not allowed to attack %s", other->GetName());
		}
		return false;
	}

	FaceTarget(GetTarget());

	SkillType skillinuse = HAND_TO_HAND;
	if (Hand == 13) {
		skillinuse = static_cast<SkillType>(GetPrimSkill());
	}
	if (Hand == 14) {
		skillinuse = static_cast<SkillType>(GetSecSkill());
	}

	//figure out what weapon they are using, if any
	const Item_Struct* weapon = NULL;
	if (Hand == 13 && equipment[SLOT_PRIMARY] > 0)
	    weapon = database.GetItem(equipment[SLOT_PRIMARY]);
	else if (equipment[SLOT_SECONDARY])
	    weapon = database.GetItem(equipment[SLOT_SECONDARY]);
	
	//We dont factor much from the weapon into the attack.
	//Just the skill type so it doesn't look silly using punching animations and stuff while wielding weapons
	if(weapon) {
		mlog(COMBAT__ATTACKS, "Attacking with weapon: %s (%d) (too bad im not using it for much)", weapon->Name, weapon->ID);
		
		if(Hand == 14 && weapon->ItemType == ItemTypeShield){
			mlog(COMBAT__ATTACKS, "Attack with shield canceled.");
			return false;
		}

		switch(weapon->ItemType){
			case ItemType1HS:
				skillinuse = _1H_SLASHING;
				break;
			case ItemType2HS:
				skillinuse = _2H_SLASHING;
				break;
			case ItemTypePierce:
			case ItemType2HPierce:
				skillinuse = PIERCING;
				break;
			case ItemType1HB:
				skillinuse = _1H_BLUNT;
				break;
			case ItemType2HB:
				skillinuse = _2H_BLUNT;
				break;
			case ItemTypeBow:
				skillinuse = ARCHERY;
				break;
			case ItemTypeThrowing:
			case ItemTypeThrowingv2:
				skillinuse = THROWING;
				break;
			default:
				skillinuse = HAND_TO_HAND;
				break;
		}
	}
	
	int weapon_damage = GetWeaponDamage(other, weapon);
	
	//do attack animation regardless of whether or not we can hit below
	sint16 charges = 0;
	ItemInst weapon_inst(weapon, charges);
	AttackAnimation(skillinuse, Hand, &weapon_inst);

	//Work-around for there being no 2HP skill - We use 99 for the 2HB animation and 36 for pierce messages
	if(skillinuse == 99)
		skillinuse = static_cast<SkillType>(36);

	//basically "if not immune" then do the attack
	if((weapon_damage) > 0) {

		//ele and bane dmg too
		//NPCs add this differently than PCs
		//if NPCs can't inheriently hit the target we don't add bane/magic dmg which isn't exactly the same as PCs
		int16 eleBane = 0;
		if(weapon){
			if(weapon->BaneDmgBody == other->GetBodyType()){
				eleBane += weapon->BaneDmgAmt;
			}

			if(weapon->BaneDmgRace == other->GetRace()){
				eleBane += weapon->BaneDmgRaceAmt;
			}

			if(weapon->ElemDmgAmt){
				eleBane += (weapon->ElemDmgAmt * other->ResistSpell(weapon->ElemDmgType, 0, this) / 100);
			}
		}
		
		if(!RuleB(NPC, UseItemBonusesForNonPets)){
			if(!GetOwner()){
				eleBane = 0;
			}
		}
		
		int8 otherlevel = other->GetLevel();
		int8 mylevel = this->GetLevel();
		int otherac = other->GetAC();
		float acdiv = (RuleR(Combat, ACDR));

		//otherlevel = otherlevel ? otherlevel : 1;
		//mylevel = mylevel ? mylevel : 1;
		
		//instead of calcing damage in floats lets just go straight to ints
		
		if(RuleB(Combat, UseIntervalAC)) {			
			//int8 leveldif = ((other->GetLevel()) - (this->GetLevel())); // //AC Damage Reduction Level Diff - Kegz @ EpicEmu.com
			damage = ((max_dmg+eleBane) - (otherac * acdiv/100.0f)); //AC Damage Reduction - Kegz @ EpicEmu.com
		}
		else {
			//float acdiv2 = 0.25;
			//int otherac = other->GetAC();
			//damage = ((max_dmg+eleBane) - (otherac * acdiv2/100.0f));
			damage = ((max_dmg+eleBane) - (otherac * acdiv/100.0f));
		}		

		//check if we're hitting above our max or below it.
		if((min_dmg+eleBane) != 0 && damage < (min_dmg+eleBane)) {
			mlog(COMBAT__DAMAGE, "Damage (%d) is below min (%d). Setting to min.", damage, (min_dmg+eleBane));
		    //damage = (min_dmg+eleBane);
			damage = ((min_dmg+eleBane) - (otherac * acdiv/100.0f));
		}
		if((max_dmg+eleBane) != 0 && damage > (max_dmg+eleBane)) {
			mlog(COMBAT__DAMAGE, "Damage (%d) is above max (%d). Setting to max.", damage, (max_dmg+eleBane));
		    //damage = (max_dmg+eleBane);
			damage = ((max_dmg+eleBane) - (otherac * acdiv/100.0f));
		}
		
		
		sint32 hate = damage;
		if(IsPet())
		{
			hate = hate * 100 / GetDamageTable(skillinuse);
		}
		//THIS IS WHERE WE CHECK TO SEE IF WE HIT:
		if(other->IsClient() && other->CastToClient()->IsSitting()) {
			mlog(COMBAT__DAMAGE, "Client %s is sitting. Hitting for max damage (%d).", other->GetName(), (max_dmg+eleBane));
			damage = (max_dmg+eleBane);
			damage += (itembonuses.HeroicSTR / 10) + (damage * other->GetSkillDmgTaken(skillinuse) / 100) + GetSkillDmgAmt(skillinuse);

			mlog(COMBAT__HITS, "Generating hate %d towards %s", hate, GetName());
			// now add done damage to the hate list
			other->AddToHateList(this, hate);
		} else {
			if(!other->CheckHitChance(this, skillinuse, Hand)) {
				damage = 0;	//miss
			} else {	//hit, check for damage avoidance
				other->AvoidDamage(this, damage);
				other->MeleeMitigation(this, damage, min_dmg+eleBane);
				if(damage > 0) {
					ApplyMeleeDamageBonus(skillinuse, damage);
					damage += (itembonuses.HeroicSTR / 10) + (damage * other->GetSkillDmgTaken(skillinuse) / 100) + GetSkillDmgAmt(skillinuse);
					TryCriticalHit(other, skillinuse, damage);
				}
				mlog(COMBAT__HITS, "Generating hate %d towards %s", hate, GetName());
				// now add done damage to the hate list
				if(damage > 0)
				{
					other->AddToHateList(this, hate);
				}
				else
					other->AddToHateList(this, 0);
			}
		}
		
		mlog(COMBAT__DAMAGE, "Final damage against %s: %d", other->GetName(), damage);
		
		if(other->IsClient() && IsPet() && GetOwner()->IsClient()) {
			//pets do half damage to clients in pvp
			damage=damage/2;
		}
	}
	else
		damage = -5;
		
	//cant riposte a riposte
	if (bRiposte && damage == -3) {
		mlog(COMBAT__DAMAGE, "Riposte of riposte canceled.");
		return false;
	}
		
	if(GetHP() > 0 && other->GetHP() >= -11) {
		other->Damage(this, damage, SPELL_UNKNOWN, skillinuse, false); // Not avoidable client already had thier chance to Avoid
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

	if(hidden || improved_hidden)
	{
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
		SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
		sa_out->spawn_id = GetID();
		sa_out->type = 0x03;
		sa_out->parameter = 0;
		entity_list.QueueClients(this, outapp, true);
		safe_delete(outapp);
	}


	hidden = false;
	improved_hidden = false;
	
	//I doubt this works...
	if (!GetTarget())
		return true; //We killed them
	
	if( !bRiposte && other->GetHP() > 0 ) {
		TryWeaponProc(weapon, other, Hand);	//no weapon
	}
	
	// now check ripostes
	if (damage == -3) { // riposting
		DoRiposte(other);
	}
	
	if (damage > 0)
	{
		other->TriggerDefensiveProcs(this);
        return true;
	}
	else
        return false;
}


void NPC::Damage(Mob* other, sint32 damage, int16 spell_id, SkillType attack_skill, bool avoidable, sint8 buffslot, bool iBuffTic) {
	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;
	
	//handle EVENT_ATTACK. Resets after we have not been attacked for 12 seconds
	if(attacked_timer.Check()) 
	{
		mlog(COMBAT__HITS, "Triggering EVENT_ATTACK due to attack by %s", other->GetName());
        parse->EventNPC(EVENT_ATTACK, this, other, "", 0);
	}
	attacked_timer.Start(CombatEventTimer_expire);
    
	if (!IsEngaged())
		zone->AddAggroMob();
	
	if(GetClass() == LDON_TREASURE) 
	{
		if(IsLDoNLocked() && GetLDoNLockedSkill() != LDoNTypeMechanical) 
		{
			damage = -5;
		}
		else
		{
			if(IsLDoNTrapped())
			{
				Message_StringID(13, LDON_ACCIDENT_SETOFF2);
				SpellFinished(GetLDoNTrapSpellID(), other, 10, 0, -1, spells[GetLDoNTrapSpellID()].ResistDiff);
				SetLDoNTrapSpellID(0);
				SetLDoNTrapped(false);
				SetLDoNTrapDetected(false);
			}
		}
	}

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
	
	if(damage > 0) {
		//see if we are gunna start fleeing
		if(!IsPet()) CheckFlee();
	}
}



void NPC::Death(Mob* killerMob, sint32 damage, int16 spell, SkillType attack_skill) {
	_ZP(NPC_Death);
	mlog(COMBAT__HITS, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob->GetName(), damage, spell, attack_skill);
	
	if (this->IsEngaged())
	{
		zone->DelAggroMob();
#if EQDEBUG >= 11
		LogFile->write(EQEMuLog::Debug,"NPC::Death() Mobs currently Aggro %i", zone->MobsAggroCount());
#endif
	}
	SetHP(0);
	SetPet(0);
	Mob* killer = GetHateDamageTop(this);
	
	entity_list.RemoveFromTargets(this);

	if(p_depop == true)
		return;

	BuffFadeAll();
	
	EQApplicationPacket* app= new EQApplicationPacket(OP_Death,sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)app->pBuffer;
	d->spawn_id = GetID();
	d->killer_id = killerMob ? killerMob->GetID() : 0;
//	d->unknown12 = 1;
	d->bindzoneid = 0;
	d->spell_id = spell == SPELL_UNKNOWN ? 0xffffffff : spell;
	d->attack_skill = SkillDamageTypes[attack_skill];
	d->damage = damage;
	app->priority = 6;
	entity_list.QueueClients(killerMob, app, false);
	
	if(respawn2) {
		respawn2->DeathReset();
	}

	if (killerMob) {
		if(GetClass() != LDON_TREASURE)
			hate_list.Add(killerMob, damage);
	}

	safe_delete(app);
	
	Mob *give_exp = hate_list.GetDamageTop(this);

	if(!give_exp)
      SetDSKill();

	if(give_exp == NULL && !GetDSKill())
      give_exp = killer;

	//Guard Exp exploit fix - players will no longer get exp from an npc's kill - fixed
	if(killerMob && killerMob->IsNPC() && !killerMob->IsPet()) //PET EXP EXPLOIT if you let pets get the kill the owner jacks exp completely... nice bug bro
		give_exp = killerMob;

	if(give_exp && give_exp->HasOwner()) 
		give_exp = give_exp->GetUltimateOwner();

	Client *give_exp_client = NULL;
	if(give_exp && give_exp->IsClient())
		give_exp_client = give_exp->CastToClient();

	bool IsLdonTreasure = (this->GetClass() == LDON_TREASURE);
	if (give_exp_client && !IsCorpse() && MerchantType == 0)
	{
		Group *kg = entity_list.GetGroupByClient(give_exp_client);
		Raid *kr = entity_list.GetRaidByClient(give_exp_client);

		if(kr)
		{
			if(!IsLdonTreasure) {
				kr->SplitExp((EXP_FORMULA), this);
				if(killerMob && (kr->IsRaidMember(killerMob->GetName()) || kr->IsRaidMember(killerMob->GetUltimateOwner()->GetName())))
					killerMob->TrySpellOnKill();
			}
			/* Send the EVENT_KILLED_MERIT event for all raid members */
			for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
				if (kr->members[i].member != NULL) { // If Group Member is Client
                    parse->EventNPC(EVENT_KILLED_MERIT, this, kr->members[i].member, "killed", 0);
					if(RuleB(TaskSystem, EnableTaskSystem))
						kr->members[i].member->UpdateTasksOnKill(GetNPCTypeID());
				}
			}
		}
		else if (give_exp_client->IsGrouped() && kg != NULL)
		{
			if(!IsLdonTreasure) {
				kg->SplitExp((EXP_FORMULA), this);
				if(killerMob && (kg->IsGroupMember(killerMob->GetName()) || kg->IsGroupMember(killerMob->GetUltimateOwner()->GetName())))
					killerMob->TrySpellOnKill();
			}
			/* Send the EVENT_KILLED_MERIT event and update kill tasks
			 * for all group members */
			for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
				if (kg->members[i] != NULL && kg->members[i]->IsClient()) { // If Group Member is Client
					Client *c = kg->members[i]->CastToClient();
                    parse->EventNPC(EVENT_KILLED_MERIT, this, c, "killed", 0);
					if(RuleB(TaskSystem, EnableTaskSystem))
						c->UpdateTasksOnKill(GetNPCTypeID());
				}
			}
		}
		else
		{
			if(!IsLdonTreasure) {
				int conlevel = give_exp->GetLevelCon(GetLevel());
				if (conlevel != CON_GREEN)
				{
					if(GetOwner() && GetOwner()->IsClient()){
					}
					else {
						give_exp_client->AddEXP((EXP_FORMULA), conlevel); // Pyro: Comment this if NPC death crashes zone
						if(killerMob && (killerMob->GetID() == give_exp_client->GetID() || killerMob->GetUltimateOwner()->GetID() == give_exp_client->GetID()))
							killerMob->TrySpellOnKill();
					}
				}
			}
			 /* Send the EVENT_KILLED_MERIT event */
            parse->EventNPC(EVENT_KILLED_MERIT, this, give_exp_client, "killed", 0);
			if(RuleB(TaskSystem, EnableTaskSystem))
				give_exp_client->UpdateTasksOnKill(GetNPCTypeID());
		}
	}
	
	//do faction hits even if we are a merchant, so long as a player killed us
	if(give_exp_client)
		hate_list.DoFactionHits(GetNPCFactionID());
	
	if (!HasOwner() && class_ != MERCHANT && class_ != ADVENTUREMERCHANT && !GetSwarmInfo()
		&& MerchantType == 0 && killer && (killer->IsClient() || (killer->HasOwner() && killer->GetUltimateOwner()->IsClient()) ||
		(killer->IsNPC() && killer->CastToNPC()->GetSwarmInfo() && killer->CastToNPC()->GetSwarmInfo()->GetOwner() && killer->CastToNPC()->GetSwarmInfo()->GetOwner()->IsClient()))) {
		Corpse* corpse = new Corpse(this, &itemlist, GetNPCTypeID(), &NPCTypedata,level>54?RuleI(NPC,MajorNPCCorpseDecayTimeMS):RuleI(NPC,MinorNPCCorpseDecayTimeMS));
		entity_list.LimitRemoveNPC(this);
		entity_list.AddCorpse(corpse, this->GetID());

		entity_list.UnMarkNPC(GetID());
		entity_list.RemoveNPC(GetID());
		this->SetID(0);
		if(killer->GetOwner() != 0 && killer->GetOwner()->IsClient())
			killer = killer->GetOwner();
		if(killer != 0 && killer->IsClient()) {
			corpse->AllowMobLoot(killer, 0);
			if(killer->IsGrouped()) {
				Group* group = entity_list.GetGroupByClient(killer->CastToClient());
				if(group != 0) {
					for(int i=0;i<6;i++) { // Doesnt work right, needs work
						if(group->members[i] != NULL) {
							corpse->AllowMobLoot(group->members[i],i);
						}
					}
				}
			}
			else if(killer->IsRaidGrouped()){
				Raid* r = entity_list.GetRaidByClient(killer->CastToClient());
				if(r){
					int i = 0;
					for(int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						switch(r->GetLootType())
						{
						case 0:
						case 1:
							if(r->members[x].member && r->members[x].IsRaidLeader){
								corpse->AllowMobLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 2:
							if(r->members[x].member && r->members[x].IsRaidLeader){
								corpse->AllowMobLoot(r->members[x].member, i);
								i++;
							}
							else if(r->members[x].member && r->members[x].IsGroupLeader){
								corpse->AllowMobLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 3:
							if(r->members[x].member && r->members[x].IsLooter){
								corpse->AllowMobLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 4:
							if(r->members[x].member)
							{
								corpse->AllowMobLoot(r->members[x].member, i);
								i++;
							}
							break;
						}
					}
				}
			}
		}

		if(zone && zone->adv_data)
		{
			ServerZoneAdventureDataReply_Struct *sr = (ServerZoneAdventureDataReply_Struct*)zone->adv_data;
			if(sr->type == Adventure_Kill)
			{
				zone->DoAdventureCountIncrease();
			}
			else if(sr->type == Adventure_Assassinate)
			{
				if(sr->data_id == GetNPCTypeID())
				{
					zone->DoAdventureCountIncrease();
				}
				else
				{
					zone->DoAdventureAssassinationCountIncrease();
				}
			}
		}
	}
	
	// Parse quests even if we're killed by an NPC
	if(killerMob) {
		Mob *oos = killerMob->GetOwnerOrSelf();
        parse->EventNPC(EVENT_DEATH, this, oos, "", 0);
		if(oos->IsNPC())
		{
            parse->EventNPC(EVENT_NPC_SLAY, oos->CastToNPC(), this, "", 0);
			killerMob->TrySpellOnKill();
		}
	}
	
	this->WipeHateList();
	p_depop = true;
	if(killerMob && killerMob->GetTarget() == this) //we can kill things without having them targeted
		killerMob->SetTarget(NULL); //via AE effects and such..

	entity_list.UpdateFindableNPCState(this, true);
}

void Mob::AddToHateList(Mob* other, sint32 hate, sint32 damage, bool iYellForHelp, bool bFrenzy, bool iBuffTic, bool fromDS) {
    assert(other != NULL);
    if (other == this)
        return;

    if(damage < 0){
        hate = 1;
    }

	bool wasengaged = IsEngaged();
	Mob* owner = other->GetOwner();
	Mob* mypet = this->GetPet();
	Mob* myowner = this->GetOwner();
	
	if(other){
		AddRampage(other);
		int hatemod = 100 + other->spellbonuses.hatemod + other->itembonuses.hatemod + other->aabonuses.hatemod;
		if(hatemod < 1)
			hatemod = 1;
		hate = ((hate * (hatemod))/100);
	}
	
	if(IsPet() && GetOwner() && GetOwner()->GetAA(aaPetDiscipline) && IsHeld()){
		return; 
	}	

	if(IsClient() && !IsAIControlled())
		return;

	if(IsFamiliar() || SpecAttacks[IMMUNE_AGGRO])
		return;	

	if (other == myowner)
		return;

	if(other->SpecAttacks[IMMUNE_AGGRO_ON])
		return;

    if(SpecAttacks[NPC_TUNNELVISION]) {
        Mob *top = GetTarget();
        if(top && top != other) {
            hate *= RuleR(Aggro, TunnelVisionAggroMod);
        }
    }

    if(IsNPC() && CastToNPC()->IsUnderwaterOnly() && zone->HasWaterMap()) {
        if(!zone->watermap->InLiquid(other->GetX(), other->GetY(), other->GetZ())) {
            return;
        }
    }
	// first add self
	
	// The damage on the hate list is used to award XP to the killer. This check is to prevent Killstealing.
	// e.g. Mob has 5000 hit points, Player A melees it down to 500 hp, Player B executes a headshot (10000 damage).
	// If we add 10000 damage, Player B would get the kill credit, so we only award damage credit to player B of the
	// amount of HP the mob had left.
	//
	if(damage > GetHP())
		damage = GetHP();

	hate_list.Add(other, hate, damage, bFrenzy, !iBuffTic, fromDS);

#ifdef BOTS
	// if other is a bot, add the bots client to the hate list
	if(other->IsBot()) {
		if(other->CastToBot()->GetBotOwner() && other->CastToBot()->GetBotOwner()->CastToClient()->GetFeigned()) {
			AddFeignMemory(other->CastToBot()->GetBotOwner()->CastToClient());
		}
		else {
			if(!hate_list.IsOnHateList(other->CastToBot()->GetBotOwner()))
				hate_list.Add(other->CastToBot()->GetBotOwner(), 0, 0, false, true, fromDS);
		}
	}
#endif //BOTS


	// then add pet owner if there's one
	if (owner) { // Other is a pet, add him and it
		// EverHood 6/12/06
		// Can't add a feigned owner to hate list
		
		if(owner->IsClient() && owner->CastToClient()->GetFeigned()) {
			//they avoid hate due to feign death...
		} else {
			// cb:2007-08-17
			// owner must get on list, but he's not actually gained any hate yet
			if(!owner->SpecAttacks[IMMUNE_AGGRO])
				hate_list.Add(owner, 0, 0, false, !iBuffTic, fromDS);
		}
	}	
	
	if (myowner) { // NPC Pets should agro their owners when attacked.... EpicEmu
		if(myowner->IsAIControlled()) {
			if(!myowner->SpecAttacks[IMMUNE_AGGRO])
				myowner->hate_list.Add(other, 0, 0, false, !iBuffTic, fromDS);
		}
	}
	
	if (mypet && (!(GetAA(aaPetDiscipline) && mypet->IsHeld()))) { // I have a pet, add other to it
		if(!mypet->IsFamiliar() && !mypet->SpecAttacks[IMMUNE_AGGRO])
			mypet->hate_list.Add(other, 0, 0, bFrenzy, fromDS);
	} else if (myowner) { // I am a pet, add other to owner if it's NPC/LD
		if (myowner->IsAIControlled() && !myowner->SpecAttacks[IMMUNE_AGGRO])
			myowner->hate_list.Add(other, 0, 0, bFrenzy, fromDS);
	}
	if (!wasengaged) { 
		if(IsNPC() && other->IsClient() && other->CastToClient())
            parse->EventNPC(EVENT_AGGRO, this->CastToNPC(), other, "", 0);
		AI_Event_Engaged(other, iYellForHelp); 
		adverrorinfo = 8293;
	}
}

// solar: this is called from Damage() when 'this' is attacked by 'other.  
// 'this' is the one being attacked
// 'other' is the attacker
// a damage shield causes damage (or healing) to whoever attacks the wearer
// a reverse ds causes damage to the wearer whenever it attack someone
// given this, a reverse ds must be checked each time the wearer is attacking
// and not when they're attacked
//a damage shield on a spell is a negative value but on an item it's a positive value so add the spell value and subtract the item value to get the end ds value
void Mob::DamageShield(Mob* attacker, bool spell_ds) {

	if(!attacker || this == attacker) 
		return;
	
	int DS = 0;
	int rev_ds = 0;
	int16 spellid = 0;
	
	if(!spell_ds) 
	{
		DS = spellbonuses.DamageShield;
		rev_ds = attacker->spellbonuses.ReverseDamageShield;
		
		if(spellbonuses.DamageShieldSpellID != 0 && spellbonuses.DamageShieldSpellID != SPELL_UNKNOWN)
			spellid = spellbonuses.DamageShieldSpellID;
	}
	else {	
		DS = spellbonuses.SpellDamageShield;
		rev_ds = 0;
		// This ID returns "you are burned", seemed most appropriate for spell DS
		spellid = 2166;
	}
	
	if(DS == 0 && rev_ds == 0)
		return;
	
	mlog(COMBAT__HITS, "Applying Damage Shield of value %d to %s", DS, attacker->GetName());
		
	//invert DS... spells yield negative values for a true damage shield
	if(DS < 0) {
		if(!spell_ds)	{
			if(IsClient()) {
				int dsMod = 100;
				switch (CastToClient()->GetAA(aaCoatofThistles))
				{
				case 1:
					dsMod = 110;
					break;
				case 2:
					dsMod = 115;
					break;
				case 3:
					dsMod = 120;
					break;
				case 4:
					dsMod = 125;
					break;
				case 5:
					dsMod = 130;
					break;
				}
				DS = ((DS * dsMod) / 100);
			}
			DS -= itembonuses.DamageShield; //+Damage Shield should only work when you already have a DS spell
			DS += attacker->itembonuses.DSMitigation + attacker->spellbonuses.DSMitigation;
		}
		attacker->Damage(this, -DS, spellid, ABJURE/*hackish*/, false);
		//we can assume there is a spell now
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Damage, sizeof(CombatDamage_Struct));
		CombatDamage_Struct* cds = (CombatDamage_Struct*)outapp->pBuffer;
		cds->target = attacker->GetID();
		cds->source = GetID();
		cds->type = spellbonuses.DamageShieldType;
		cds->spellid = 0x0;
		cds->damage = DS;
		entity_list.QueueCloseClients(this, outapp);
		safe_delete(outapp);
	} else if (DS > 0 && !spell_ds) {
		//we are healing the attacker...
		attacker->HealDamage(DS);
		//TODO: send a packet???
	}

	//Reverse DS
	//this is basically a DS, but the spell is on the attacker, not the attackee
	//if we've gotten to this point, we know we know "attacker" hit "this" (us) for damage & we aren't invulnerable
	uint16 rev_ds_spell_id = SPELL_UNKNOWN;

	if(spellbonuses.ReverseDamageShieldSpellID != 0 && spellbonuses.ReverseDamageShieldSpellID != SPELL_UNKNOWN)
		rev_ds_spell_id = spellbonuses.ReverseDamageShieldSpellID;

	if(rev_ds < 0) {
		mlog(COMBAT__HITS, "Applying Reverse Damage Shield of value %d to %s", rev_ds, attacker->GetName());
		attacker->Damage(this, -rev_ds, rev_ds_spell_id, ABJURE/*hackish*/, false); //"this" (us) will get the hate, etc. not sure how this works on Live, but it'll works for now, and tanks will love us for this
		//do we need to send a damage packet here also?
		/*
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Damage, sizeof(CombatDamage_Struct));
		CombatDamage_Struct* cds = (CombatDamage_Struct*)outapp->pBuffer;
		cds->target = attacker->GetID();
		cds->source = GetID();
		cds->type = attacker->spellbonuses.ReverseDamageShieldType;
		cds->spellid = 0x0;
		cds->damage = rev_ds;
		entity_list.QueueCloseClients(this, outapp);
		safe_delete(outapp);
		*/
	}
}


int8 Mob::GetWeaponDamageBonus( const Item_Struct *Weapon )
{
	_ZP(Mob_GetWeaponDamageBonus);
	// This function calculates and returns the damage bonus for the weapon identified by the parameter "Weapon".
	// Modified 9/21/2008 by Cantus


	// Assert: This function should only be called for hits by the mainhand, as damage bonuses apply only to the
	//         weapon in the primary slot. Be sure to check that Hand == 13 before calling.

	// Assert: The caller should ensure that Weapon is actually a weapon before calling this function.
	//         The ItemInst::IsWeapon() method can be used to quickly determine this.

	// Assert: This function should not be called if the player's level is below 28, as damage bonuses do not begin
	//         to apply until level 28.

	// Assert: This function should not be called unless the player is a melee class, as casters do not receive a damage bonus.


	if( Weapon == NULL || Weapon->ItemType == ItemType1HS || Weapon->ItemType == ItemType1HB || Weapon->ItemType == ItemTypeHand2Hand || Weapon->ItemType == ItemTypePierce )
	{
		// The weapon in the player's main (primary) hand is a one-handed weapon, or there is no item equipped at all.
		//
		// According to player posts on Allakhazam, 1H damage bonuses apply to bare fists (nothing equipped in the mainhand,
		// as indicated by Weapon == NULL).
		//
		// The following formula returns the correct damage bonus for all 1H weapons:

		return (int8) ((GetLevel() - 25) / 3); //was 3
	}

	// If we've gotten to this point, the weapon in the mainhand is a two-handed weapon.
	// Calculating damage bonuses for 2H weapons is more complicated, as it's based on PC level AND the delay of the weapon.
	// The formula to calculate 2H bonuses is HIDEOUS. It's a huge conglomeration of ternary operators and multiple operations.
	//
	// The following is a hybrid approach. In cases where the Level and Delay merit a formula that does not use many operators,
	// the formula is used. In other cases, lookup tables are used for speed.
	// Though the following code may look bloated and ridiculous, it's actually a very efficient way of calculating these bonuses.

	// Player Level is used several times in the code below, so save it into a variable.
	// If GetLevel() were an ordinary function, this would DEFINITELY make sense, as it'd cut back on all of the function calling
	// overhead involved with multiple calls to GetLevel(). But in this case, GetLevel() is a simple, inline accessor method.
	// So it probably doesn't matter. If anyone knows for certain that there is no overhead involved with calling GetLevel(),
	// as I suspect, then please feel free to delete the following line, and replace all occurences of "ucPlayerLevel" with "GetLevel()".
	int8 ucPlayerLevel = (int8) GetLevel();


	// The following may look cleaner, and would certainly be easier to understand, if it was
	// a simple 53x150 cell matrix.
	//
	// However, that would occupy 7,950 Bytes of memory (7.76 KB), and would likely result
	// in "thrashing the cache" when performing lookups.
	//
	// Initially, I thought the best approach would be to reverse-engineer the formula used by
	// Sony/Verant to calculate these 2H weapon damage bonuses. But the more than Reno and I
	// worked on figuring out this formula, the more we're concluded that the formula itself ugly
	// (that is, it contains so many operations and conditionals that it's fairly CPU intensive).
	// Because of that, we're decided that, in most cases, a lookup table is the most efficient way
	// to calculate these damage bonuses.
	//
	// The code below is a hybrid between a pure formulaic approach and a pure, brute-force
	// lookup table. In cases where a formula is the best bet, I use a formula. In other places
	// where a formula would be ugly, I use a lookup table in the interests of speed. 


	if( Weapon->Delay <= 27 )
	{
		// Damage Bonuses for all 2H weapons with delays of 27 or less are identical.
		// They are the same as the damage bonus would be for a corresponding 1H weapon, plus one.
		// This formula applies to all levels 28-80, and will probably continue to apply if

		// the level cap on Live ever is increased beyond 80.

		return (ucPlayerLevel - 22) / 3; //was 3
	}


	if( ucPlayerLevel == 65 && Weapon->Delay <= 59 )
	{
		// Consider these two facts:
		//   * Level 65 is the maximum level on many EQ Emu servers.
		//   * If you listed the levels of all characters logged on to a server, odds are that the number you'll
		//     see most frequently is level 65. That is, there are more level 65 toons than any other single level.
		//
		// Therefore, if we can optimize this function for level 65 toons, we're speeding up the server!
		//
		// With that goal in mind, I create an array of Damage Bonuses for level 65 characters wielding 2H weapons with
		// delays between 28 and 59 (inclusive). I suspect that this one small lookup array will therefore handle
		// many of the calls to this function.

		static const int8 ucLevel65DamageBonusesForDelays28to59[] = {35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 42, 42, 42, 45, 45, 47, 48, 49, 49, 51, 51, 52, 53, 54, 54, 56, 56, 57, 58, 59};

		return ucLevel65DamageBonusesForDelays28to59[Weapon->Delay-28];
	}


	if( ucPlayerLevel > 65 )
	{
		if( ucPlayerLevel > 80 )
		{
			// As level 80 is currently the highest achievable level on Live, we only include
			// damage bonus information up to this level.
			//
			// If there is a custom EQEmu server that allows players to level beyond 80, the
			// damage bonus for their 2H weapons will simply not increase beyond their damage
			// bonus at level 80.

			ucPlayerLevel = 80;
		}

		// Lucy does not list a chart of damage bonuses for players levels 66+,
		// so my original version of this function just applied the level 65 damage
		// bonus for level 66+ toons. That sucked for higher level toons, as their
		// 2H weapons stopped ramping up in DPS as they leveled past 65.
		//
		// Thanks to the efforts of two guys, this is no longer the case:
		//
		// Janusd (Zetrakyl) ran a nifty query against the PEQ item database to list
		// the name of an example 2H weapon that represents each possible unique 2H delay.
		//
		// Romai then wrote an excellent script to automatically look up each of those
		// weapons, open the Lucy item page associated with it, and iterate through all
		// levels in the range 66 - 80. He saved the damage bonus for that weapon for
		// each level, and that forms the basis of the lookup tables below.

		if( Weapon->Delay <= 59 )
		{
			static const int8 ucDelay28to59Levels66to80[32][15]=
			{
			/*							Level:								*/
			/*	 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80	*/

				{36, 37, 38, 39, 41, 42, 43, 44, 45, 47, 49, 49, 49, 50, 53},	/* Delay = 28 */
				{36, 38, 38, 39, 42, 43, 43, 45, 46, 48, 49, 50, 51, 52, 54},	/* Delay = 29 */
				{37, 38, 39, 40, 43, 43, 44, 46, 47, 48, 50, 51, 52, 53, 55},	/* Delay = 30 */
				{37, 39, 40, 40, 43, 44, 45, 46, 47, 49, 51, 52, 52, 52, 54},	/* Delay = 31 */
				{38, 39, 40, 41, 44, 45, 45, 47, 48, 48, 50, 52, 53, 55, 57},	/* Delay = 32 */
				{38, 40, 41, 41, 44, 45, 46, 48, 49, 50, 52, 53, 54, 56, 58},	/* Delay = 33 */
				{39, 40, 41, 42, 45, 46, 47, 48, 49, 51, 53, 54, 55, 57, 58},	/* Delay = 34 */
				{39, 41, 42, 43, 46, 46, 47, 49, 50, 52, 54, 55, 56, 57, 59},	/* Delay = 35 */
				{40, 41, 42, 43, 46, 47, 48, 50, 51, 53, 55, 55, 56, 58, 60},	/* Delay = 36 */
				{40, 42, 43, 44, 47, 48, 49, 50, 51, 53, 55, 56, 57, 59, 61},	/* Delay = 37 */
				{41, 42, 43, 44, 47, 48, 49, 51, 52, 54, 56, 57, 58, 60, 62},	/* Delay = 38 */
				{41, 43, 44, 45, 48, 49, 50, 52, 53, 55, 57, 58, 59, 61, 63},	/* Delay = 39 */
				{43, 45, 46, 47, 50, 51, 52, 54, 55, 57, 59, 60, 61, 63, 65},	/* Delay = 40 */
				{43, 45, 46, 47, 50, 51, 52, 54, 55, 57, 59, 60, 61, 63, 65},	/* Delay = 41 */
				{44, 46, 47, 48, 51, 52, 53, 55, 56, 58, 60, 61, 62, 64, 66},	/* Delay = 42 */
				{46, 48, 49, 50, 53, 54, 55, 58, 59, 61, 63, 64, 65, 67, 69},	/* Delay = 43 */
				{47, 49, 50, 51, 54, 55, 56, 58, 59, 61, 64, 65, 66, 68, 70},	/* Delay = 44 */
				{48, 50, 51, 52, 56, 57, 58, 60, 61, 63, 65, 66, 68, 70, 72},	/* Delay = 45 */
				{50, 52, 53, 54, 57, 58, 59, 62, 63, 65, 67, 68, 69, 71, 74},	/* Delay = 46 */
				{50, 52, 53, 55, 58, 59, 60, 62, 63, 66, 68, 69, 70, 72, 74},	/* Delay = 47 */
				{51, 53, 54, 55, 58, 60, 61, 63, 64, 66, 69, 69, 71, 73, 75},	/* Delay = 48 */
				{52, 54, 55, 57, 60, 61, 62, 65, 66, 68, 70, 71, 73, 75, 77},	/* Delay = 49 */
				{53, 55, 56, 57, 61, 62, 63, 65, 67, 69, 71, 72, 74, 76, 78},	/* Delay = 50 */
				{53, 55, 57, 58, 61, 62, 64, 66, 67, 69, 72, 73, 74, 77, 79},	/* Delay = 51 */
				{55, 57, 58, 59, 63, 64, 65, 68, 69, 71, 74, 75, 76, 78, 81},	/* Delay = 52 */
				{57, 55, 59, 60, 63, 65, 66, 68, 70, 72, 74, 76, 77, 79, 82},	/* Delay = 53 */
				{56, 58, 59, 61, 64, 65, 67, 69, 70, 73, 75, 76, 78, 80, 82},	/* Delay = 54 */
				{57, 59, 61, 62, 66, 67, 68, 71, 72, 74, 77, 78, 80, 82, 84},	/* Delay = 55 */
				{58, 60, 61, 63, 66, 68, 69, 71, 73, 75, 78, 79, 80, 83, 85},	/* Delay = 56 */

				/* Important Note: Janusd's search for 2H weapons did not find	*/
				/* any 2H weapon with a delay of 57. Therefore the values below	*/
				/* are interpolated, not exact!									*/
				{59, 61, 62, 64, 67, 69, 70, 72, 74, 76, 77, 78, 81, 84, 86},	/* Delay = 57 INTERPOLATED */

				{60, 62, 63, 65, 68, 70, 71, 74, 75, 78, 80, 81, 83, 85, 88},	/* Delay = 58 */

				/* Important Note: Janusd's search for 2H weapons did not find	*/
				/* any 2H weapon with a delay of 59. Therefore the values below	*/
				/* are interpolated, not exact!									*/
				{60, 62, 64, 65, 69, 70, 72, 74, 76, 78, 81, 82, 84, 86, 89},	/* Delay = 59 INTERPOLATED */
			};

			return ucDelay28to59Levels66to80[Weapon->Delay-28][ucPlayerLevel-66];
		}
		else
		{
			// Delay is 60+ 

			const static int8 ucDelayOver59Levels66to80[6][15] =
			{
			/*							Level:								*/
			/*	 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80	*/

				{61, 63, 65, 66, 70, 71, 73, 75, 77, 79, 82, 83, 85, 87, 90},				/* Delay = 60 */
				{65, 68, 69, 71, 75, 76, 78, 80, 82, 85, 87, 89, 91, 93, 96},				/* Delay = 65 */

				/* Important Note: Currently, the only 2H weapon with a delay	*/
				/* of 66 is not player equippable (it's None/None). So I'm		*/
				/* leaving it commented out to keep this table smaller.			*/
				//{66, 68, 70, 71, 75, 77, 78, 81, 83, 85, 88, 90, 91, 94, 97},				/* Delay = 66 */

				{70, 72, 74, 76, 80, 81, 83, 86, 88, 88, 90, 95, 97, 99, 102},				/* Delay = 70 */
				{82, 85, 87, 89, 89, 94, 98, 101, 103, 106, 109, 111, 114, 117, 120},		/* Delay = 85 */
				{90, 93, 96, 98, 103, 105, 107, 111, 113, 116, 120, 122, 125, 128, 131},	/* Delay = 95 */

				/* Important Note: Currently, the only 2H weapons with delay	*/
				/* 100 are GM-only items purchased from vendors in Sunset Home	*/
				/* (cshome). Because they are highly unlikely to be used in		*/
				/* combat, I'm commenting it out to keep the table smaller.		*/
				//{95, 98, 101, 103, 108, 110, 113, 116, 119, 122, 126, 128, 131, 134, 138},/* Delay = 100 */

				{136, 140, 144, 148, 154, 157, 161, 166, 170, 174, 179, 183, 187, 191, 196}	/* Delay = 150 */
			};

			if( Weapon->Delay < 65 )
			{
				return ucDelayOver59Levels66to80[0][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 70 )
			{
				return ucDelayOver59Levels66to80[1][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 85 )
			{
				return ucDelayOver59Levels66to80[2][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 95 )
			{
				return ucDelayOver59Levels66to80[3][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 150 )
			{
				return ucDelayOver59Levels66to80[4][ucPlayerLevel-66];
			}
			else
			{
				return ucDelayOver59Levels66to80[5][ucPlayerLevel-66];
			}
		}
	}


	// If we've gotten to this point in the function without hitting a return statement,
	// we know that the character's level is between 28 and 65, and that the 2H weapon's
	// delay is 28 or higher.

	// The Damage Bonus values returned by this function (in the level 28-65 range) are
	// based on a table of 2H Weapon Damage Bonuses provided by Lucy at the following address:
	// http://lucy.allakhazam.com/dmgbonus.html

	if( Weapon->Delay <= 39 )
	{
		if( ucPlayerLevel <= 53)
		{
			// The Damage Bonus for all 2H weapons with delays between 28 and 39 (inclusive) is the same for players level 53 and below...
			static const int8 ucDelay28to39LevelUnder54[] = {1, 1, 2, 3, 3, 3, 4, 5, 5, 6, 6, 6, 8, 8, 8, 9, 9, 10, 11, 11, 11, 12, 13, 14, 16, 17};

			// As a note: The following formula accurately calculates damage bonuses for 2H weapons with delays in the range 28-39 (inclusive)
			// for characters levels 28-50 (inclusive):
			// return ( (ucPlayerLevel - 22) / 3 ) + ( (ucPlayerLevel - 25) / 5 );
			//
			// However, the small lookup array used above is actually much faster. So we'll just use it instead of the formula
			//
			// (Thanks to Reno for helping figure out the above formula!)

			return ucDelay28to39LevelUnder54[ucPlayerLevel-28];
		}
		else
		{
			// Use a matrix to look up the damage bonus for 2H weapons with delays between 28 and 39 wielded by characters level 54 and above.
			static const int8 ucDelay28to39Level54to64[12][11] =
			{
			/*						Level:					*/
			/*	 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64	*/

				{17, 21, 21, 23, 25, 26, 28, 30, 31, 31, 33},	/* Delay = 28 */
				{17, 21, 22, 23, 25, 26, 29, 30, 31, 32, 34},	/* Delay = 29 */
				{18, 21, 22, 23, 25, 27, 29, 31, 32, 32, 34},	/* Delay = 30 */
				{18, 21, 22, 23, 25, 27, 29, 31, 32, 33, 34},	/* Delay = 31 */
				{18, 21, 22, 24, 26, 27, 30, 32, 32, 33, 35},	/* Delay = 32 */
				{18, 21, 22, 24, 26, 27, 30, 32, 33, 34, 35},	/* Delay = 33 */
				{18, 22, 22, 24, 26, 28, 30, 32, 33, 34, 36},	/* Delay = 34 */
				{18, 22, 23, 24, 26, 28, 31, 33, 34, 34, 36},	/* Delay = 35 */
				{18, 22, 23, 25, 27, 28, 31, 33, 34, 35, 37},	/* Delay = 36 */
				{18, 22, 23, 25, 27, 29, 31, 33, 34, 35, 37},	/* Delay = 37 */
				{18, 22, 23, 25, 27, 29, 32, 34, 35, 36, 38},	/* Delay = 38 */
				{18, 22, 23, 25, 27, 29, 32, 34, 35, 36, 38}	/* Delay = 39 */
			};

			return ucDelay28to39Level54to64[Weapon->Delay-28][ucPlayerLevel-54];
		}
	}
	else if( Weapon->Delay <= 59 )
	{
		if( ucPlayerLevel <= 52 )
		{
			if( Weapon->Delay <= 45 )
			{
				static const int8 ucDelay40to45Levels28to52[6][25] =
				{
				/*												Level:														*/
				/*	 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52		*/

					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 40 */
					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 41 */
					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 42 */
					{4,  4,  5,  6,  6,  6,  7,  8,  8,  9,  9,  9,  11, 11, 11, 12, 12, 13, 14, 14, 14, 15, 16, 18, 20},	/* Delay = 43 */
					{4,  4,  5,  6,  6,  6,  7,  8,  8,  9,  9,  9,  11, 11, 11, 12, 12, 13, 14, 14, 14, 15, 16, 18, 20},	/* Delay = 44 */
					{5,  5,  6,  7,  7,  7,  8,  9,  9,  10, 10, 10, 12, 12, 12, 13, 13, 14, 15, 15, 15, 16, 17, 19, 21} 	/* Delay = 45 */
				};

				return ucDelay40to45Levels28to52[Weapon->Delay-40][ucPlayerLevel-28];
			}
			else
			{
				static const int8 ucDelay46Levels28to52[] = {6,  6,  7,  8,  8,  8,  9,  10, 10, 11, 11, 11, 13, 13, 13, 14, 14, 15, 16, 16, 16, 17, 18, 20, 22};

				return ucDelay46Levels28to52[ucPlayerLevel-28] + ((Weapon->Delay-46) / 3);
			}
		}
		else
		{
			// Player is in the level range 53 - 64

			// Calculating damage bonus for 2H weapons with a delay between 40 and 59 (inclusive) involves, unforunately, a brute-force matrix lookup.
			static const int8 ucDelay40to59Levels53to64[20][37] = 
			{
			/*						Level:							*/
			/*	 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64		*/

				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 40 */
				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 41 */
				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 42 */
				{21, 22, 26, 27, 29, 31, 33, 37, 39, 40, 41, 43},	/* Delay = 43 */
				{21, 22, 26, 27, 29, 32, 34, 37, 39, 40, 41, 43},	/* Delay = 44 */
				{22, 23, 27, 28, 31, 33, 35, 38, 40, 42, 43, 45},	/* Delay = 45 */
				{23, 24, 28, 30, 32, 34, 36, 40, 42, 43, 44, 46},	/* Delay = 46 */
				{23, 24, 29, 30, 32, 34, 37, 40, 42, 43, 44, 47},	/* Delay = 47 */
				{23, 24, 29, 30, 32, 35, 37, 40, 43, 44, 45, 47},	/* Delay = 48 */
				{24, 25, 30, 31, 34, 36, 38, 42, 44, 45, 46, 49},	/* Delay = 49 */
				{24, 26, 30, 31, 34, 36, 39, 42, 44, 46, 47, 49},	/* Delay = 50 */
				{24, 26, 30, 31, 34, 36, 39, 42, 45, 46, 47, 49},	/* Delay = 51 */
				{25, 27, 31, 33, 35, 38, 40, 44, 46, 47, 49, 51},	/* Delay = 52 */
				{25, 27, 31, 33, 35, 38, 40, 44, 46, 48, 49, 51},	/* Delay = 53 */
				{26, 27, 32, 33, 36, 38, 41, 44, 47, 48, 49, 52},	/* Delay = 54 */
				{27, 28, 33, 34, 37, 39, 42, 46, 48, 50, 51, 53},	/* Delay = 55 */
				{27, 28, 33, 34, 37, 40, 42, 46, 49, 50, 51, 54},	/* Delay = 56 */
				{27, 28, 33, 34, 37, 40, 43, 46, 49, 50, 52, 54},	/* Delay = 57 */
				{28, 29, 34, 36, 39, 41, 44, 48, 50, 52, 53, 56},	/* Delay = 58 */
				{28, 29, 34, 36, 39, 41, 44, 48, 51, 52, 54, 56}	/* Delay = 59 */
			};

			return ucDelay40to59Levels53to64[Weapon->Delay-40][ucPlayerLevel-53];
		}
	}
	else
	{
		// The following table allows us to look up Damage Bonuses for weapons with delays greater than or equal to 60.
		//
		// There aren't a lot of 2H weapons with a delay greater than 60. In fact, both a database and Lucy search run by janusd confirm
		// that the only unique 2H delays greater than 60 are: 65, 70, 85, 95, and 150.
		//
		// To be fair, there are also weapons with delays of 66 and 100. But they are either not equippable (None/None), or are
		// only available to GMs from merchants in Sunset Home (cshome). In order to keep this table "lean and mean", I will not
		// include the values for delays 66 and 100. If they ever are wielded, the 66 delay weapon will use the 65 delay bonuses,
		// and the 100 delay weapon will use the 95 delay bonuses. So it's not a big deal.
		//
		// Still, if someone in the future decides that they do want to include them, here are the tables for these two delays:
		//
		// {12, 12, 13, 14, 14, 14, 15, 16, 16, 17, 17, 17, 19, 19, 19, 20, 20, 21, 22, 22, 22, 23, 24, 26, 29, 30, 32, 37, 39, 42, 45, 48, 53, 55, 57, 59, 61, 64}		/* Delay = 66 */
		// {24, 24, 25, 26, 26, 26, 27, 28, 28, 29, 29, 29, 31, 31, 31, 32, 32, 33, 34, 34, 34, 35, 36, 39, 43, 45, 48, 55, 57, 62, 66, 71, 77, 80, 83, 85, 89, 92}		/* Delay = 100 */
		//
		// In case there are 2H weapons added in the future with delays other than those listed above (and until the damage bonuses
		// associated with that new delay are added to this function), this function is designed to do the following:
		//
		//		For weapons with delays in the range 60-64, use the Damage Bonus that would apply to a 2H weapon with delay 60.
		//		For weapons with delays in the range 65-69, use the Damage Bonus that would apply to a 2H weapon with delay 65
		//		For weapons with delays in the range 70-84, use the Damage Bonus that would apply to a 2H weapon with delay 70.
		//		For weapons with delays in the range 85-94, use the Damage Bonus that would apply to a 2H weapon with delay 85.
		//		For weapons with delays in the range 95-149, use the Damage Bonus that would apply to a 2H weapon with delay 95.
		//		For weapons with delays 150 or higher, use the Damage Bonus that would apply to a 2H weapon with delay 150.

		static const int8 ucDelayOver59Levels28to65[6][38] =
		{
		/*																	Level:																					*/
		/*	 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64. 65	*/

			{10, 10, 11, 12, 12, 12, 13, 14, 14, 15, 15, 15, 17, 17, 17, 18, 18, 19, 20, 20, 20, 21, 22, 24, 27, 28, 30, 35, 36, 39, 42, 45, 49, 51, 53, 54, 57, 59},		/* Delay = 60 */
			{12, 12, 13, 14, 14, 14, 15, 16, 16, 17, 17, 17, 19, 19, 19, 20, 20, 21, 22, 22, 22, 23, 24, 26, 29, 30, 32, 37, 39, 42, 45, 48, 52, 55, 57, 58, 61, 63},		/* Delay = 65 */
			{14, 14, 15, 16, 16, 16, 17, 18, 18, 19, 19, 19, 21, 21, 21, 22, 22, 23, 24, 24, 24, 25, 26, 28, 31, 33, 35, 40, 42, 45, 48, 52, 56, 59, 61, 62, 65, 68},		/* Delay = 70 */
			{19, 19, 20, 21, 21, 21, 22, 23, 23, 24, 24, 24, 26, 26, 26, 27, 27, 28, 29, 29, 29, 30, 31, 34, 37, 39, 41, 47, 49, 54, 57, 61, 66, 69, 72, 74, 77, 80},		/* Delay = 85 */
			{22, 22, 23, 24, 24, 24, 25, 26, 26, 27, 27, 27, 29, 29, 29, 30, 30, 31, 32, 32, 32, 33, 34, 37, 40, 43, 45, 52, 54, 59, 62, 67, 73, 76, 79, 81, 84, 88},		/* Delay = 95 */
			{40, 40, 41, 42, 42, 42, 43, 44, 44, 45, 45, 45, 47, 47, 47, 48, 48, 49, 50, 50, 50, 51, 52, 56, 61, 65, 69, 78, 82, 89, 94, 102, 110, 115, 119, 122, 127, 132}	/* Delay = 150 */
		};

		if( Weapon->Delay < 65 )
		{
			return ucDelayOver59Levels28to65[0][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 70 )
		{
			return ucDelayOver59Levels28to65[1][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 85 )
		{
			return ucDelayOver59Levels28to65[2][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 95 )
		{
			return ucDelayOver59Levels28to65[3][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 150 )
		{
			return ucDelayOver59Levels28to65[4][ucPlayerLevel-28];
		}
		else
		{
			return ucDelayOver59Levels28to65[5][ucPlayerLevel-28];
		}
	}
}

int Mob::GetMonkHandToHandDamage(void)
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

	if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652)
		return(9);
	else
	{
		int Level = GetLevel();
        if (Level > 65)
		    return(19);
        else
            return damage[Level];
	}
}

int Mob::GetMonkHandToHandDelay(void)
{
	// Kaiyodo - Determine a monk's fist delay. Table data from www.monkly-business.com
	// saved as static array - this should speed this function up considerably
	static int delayshuman[66] = {
	//  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
        99,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,
        36,36,36,36,36,35,35,35,35,35,34,34,34,34,34,33,33,33,33,33,
        32,32,32,32,32,31,31,31,31,31,30,30,30,29,29,29,28,28,28,27,
        26,24,22,20,20,20  };
	static int delaysiksar[66] = {
	//  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
        99,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,
        36,36,36,36,36,36,36,36,36,36,35,35,35,35,35,34,34,34,34,34,
        33,33,33,33,33,32,32,32,32,32,31,31,31,30,30,30,29,29,29,28,
        27,24,22,20,20,20 };
	
	// Have a look to see if we have epic fists on
	if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652)
		return(16);
	else
	{
		int Level = GetLevel();
		if (GetRace() == HUMAN)
		{
            if (Level > 65)
			    return(24);
            else
                return delayshuman[Level];
		}
		else	//heko: iksar table
		{
            if (Level > 65)
			    return(25);
            else
                return delaysiksar[Level];
		}
	}
}

sint32 Mob::ReduceDamage(sint32 damage)
{
	if(damage <= 0)
		return damage;

	int slot = GetBuffSlotFromType(SE_NegateAttacks);
	if(slot >= 0) {
		if(CheckHitsRemaining(slot, false, true))
			return -6;
	}

	slot = GetBuffSlotFromType(SE_MitigateMeleeDamage);
	if(slot >= 0)
	{
		int damage_to_reduce = damage * GetPartialMeleeRuneReduction(buffs[slot].spellid) / 100;
		if(damage_to_reduce > buffs[slot].melee_rune)
		{
			mlog(SPELLS__EFFECT_VALUES, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
				" damage remaining, fading buff.", damage_to_reduce, buffs[slot].melee_rune);
			damage -= damage_to_reduce;
			if(!TryFadeEffect(slot))
				BuffFadeBySlot(slot);
			UpdateRuneFlags();
		}
		else
		{
			mlog(SPELLS__EFFECT_VALUES, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
				" damage remaining.", damage_to_reduce, buffs[slot].melee_rune);
			buffs[slot].melee_rune = (buffs[slot].melee_rune - damage_to_reduce);
			damage -= damage_to_reduce;
		}
	}

	if(damage < 1)
	{
		return -6;
	}

	slot = GetBuffSlotFromType(SE_Rune);
	while(slot >= 0)
	{
		uint32 melee_rune_left = buffs[slot].melee_rune;
		if(melee_rune_left >= damage)
		{
			melee_rune_left -= damage;
			damage = -6;
			buffs[slot].melee_rune = melee_rune_left;
			break;
		}
		else
		{
			if(melee_rune_left > 0)
				damage -= melee_rune_left;
			if(!TryFadeEffect(slot))
				BuffFadeBySlot(slot);
			slot = GetBuffSlotFromType(SE_Rune);
			UpdateRuneFlags();
		}
	}
	
	if(damage < 1)
	{
		return -6;
	}
	
	slot = GetBuffSlotFromType(SE_ManaAbsorbPercentDamage);
	if(slot >= 0) {
		for (int i = 0; i < EFFECT_COUNT; i++) {
			if (spells[buffs[slot].spellid].effectid[i] == SE_ManaAbsorbPercentDamage) {
				if(GetMana() > damage * spells[buffs[slot].spellid].base[i] / 100) {
					damage -= (damage * spells[buffs[slot].spellid].base[i] / 100);
					SetMana(GetMana() - damage);
				}
			}
		}
	}

	return(damage);
}
	
sint32 Mob::AffectMagicalDamage(sint32 damage, int16 spell_id, const bool iBuffTic, Mob* attacker) 
{
	if(damage <= 0)
	{
		return damage;
	}

	// See if we block the spell outright first
	int slot = GetBuffSlotFromType(SE_NegateAttacks);
	if(slot >= 0) {
		if(CheckHitsRemaining(slot, false, true))
			return -6;
	}
	
	// If this is a DoT, use DoT Shielding...
	if(iBuffTic)
	{
		damage -= (damage * this->itembonuses.DoTShielding / 100);
	}
	// This must be a DD then so lets apply Spell Shielding and runes.
	else 
	{
		// Reduce damage by the Spell Shielding first so that the runes don't take the raw damage.
		damage -= (damage * this->itembonuses.SpellShield / 100);
	
		// Do runes now.
		slot = GetBuffSlotFromType(SE_MitigateSpellDamage);
		if(slot >= 0)
		{
			int damage_to_reduce = damage * GetPartialMagicRuneReduction(buffs[slot].spellid) / 100;
			if(damage_to_reduce > buffs[slot].magic_rune)
			{
				mlog(SPELLS__EFFECT_VALUES, "Mob::ReduceDamage SE_MitigateSpellDamage %d damage negated, %d"
					" damage remaining, fading buff.", damage_to_reduce, buffs[slot].magic_rune);
				damage -= damage_to_reduce;
				if(!TryFadeEffect(slot))
					BuffFadeBySlot(slot);
				UpdateRuneFlags();
			}
			else
			{
				mlog(SPELLS__EFFECT_VALUES, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
					" damage remaining.", damage_to_reduce, buffs[slot].magic_rune);
				buffs[slot].magic_rune = (buffs[slot].magic_rune - damage_to_reduce);
				damage -= damage_to_reduce;
			}
		}

		if(damage < 1)
		{
			return -6;
		}


		slot = GetBuffSlotFromType(SE_AbsorbMagicAtt);
		while(slot >= 0)
		{
            uint32 magic_rune_left = buffs[slot].magic_rune;
			if(magic_rune_left >= damage)
			{
				magic_rune_left -= damage;
				damage = 0;
				buffs[slot].magic_rune = magic_rune_left;
				break;
			}
			else
			{
				if(magic_rune_left > 0)
					damage -= magic_rune_left;
				if(!TryFadeEffect(slot))
					BuffFadeBySlot(slot);
				slot = GetBuffSlotFromType(SE_AbsorbMagicAtt);
				UpdateRuneFlags();
			}
		}
		
		slot = GetBuffSlotFromType(SE_ManaAbsorbPercentDamage);
		if(slot >= 0) {
			for (int k = 0; k < EFFECT_COUNT; k++) {
				if (spells[buffs[slot].spellid].effectid[k] == SE_ManaAbsorbPercentDamage) {
					if(GetMana() > damage * spells[buffs[slot].spellid].base[k] / 100) {
						damage -= (damage * spells[buffs[slot].spellid].base[k] / 100);
						SetMana(GetMana() - damage);
					}
				}
			}
		}
	}
	return damage;
}

bool Mob::HasProcs() const
{
    for (int i = 0; i < MAX_PROCS; i++)
        if (PermaProcs[i].spellID != SPELL_UNKNOWN || SpellProcs[i].spellID != SPELL_UNKNOWN)
            return true;
    return false;
}

bool Mob::HasDefensiveProcs() const
{
	for (int i = 0; i < MAX_PROCS; i++)
        if (DefensiveProcs[i].spellID != SPELL_UNKNOWN)
            return true;
    return false;
}

bool Mob::HasRangedProcs() const
{
	for (int i = 0; i < MAX_PROCS; i++)
        if (RangedProcs[i].spellID != SPELL_UNKNOWN)
            return true;
    return false;
}

bool Client::CheckDoubleAttack(bool tripleAttack) {

	// If you don't have the double attack skill, return
	if(!HasSkill(DOUBLE_ATTACK) && !(GetClass() == BARD || GetClass() == BEASTLORD))
		return false;
	
	// You start with no chance of double attacking
	int chance = 0;
	
	// Used for maxSkill and triple attack calcs
	int8 classtype = GetClass();
	
	// The current skill level
	uint16 skill = GetSkill(DOUBLE_ATTACK);

	// Discipline bonuses give you 100% chance to double attack
	sint16 buffs = spellbonuses.DoubleAttackChance + itembonuses.DoubleAttackChance;
	
	// The maximum value for the Class based on the server rule of MaxLevel
	int16 maxSkill = MaxSkill(DOUBLE_ATTACK, classtype, RuleI(Character, MaxLevel));

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

void Mob::CommonDamage(Mob* attacker, sint32 &damage, const int16 spell_id, const SkillType skill_used, bool &avoidable, const sint8 buffslot, const bool iBuffTic) {
	// This method is called with skill_used=ABJURE for Damage Shield damage. 
	bool FromDamageShield = (skill_used == ABJURE);

	mlog(COMBAT__HITS, "Applying damage %d done by %s with skill %d and spell %d, avoidable? %s, is %sa buff tic in slot %d",
		damage, attacker?attacker->GetName():"NOBODY", skill_used, spell_id, avoidable?"yes":"no", iBuffTic?"":"not ", buffslot);
	
	if (GetInvul() || DivineAura()) {
		mlog(COMBAT__DAMAGE, "Avoiding %d damage due to invulnerability.", damage);
		damage = -5;
	}
	
	if( spell_id != SPELL_UNKNOWN || attacker == NULL )
		avoidable = false;
	
    // only apply DS if physical damage (no spell damage)
    // damage shield calls this function with spell_id set, so its unavoidable
	if (attacker && damage > 0 && spell_id == SPELL_UNKNOWN && skill_used != ARCHERY && skill_used != THROWING) {
		this->DamageShield(attacker);
		uint32 buff_count = GetMaxTotalSlots();
		for(uint32 bs = 0; bs < buff_count; bs++){
			if((buffs[bs].spellid != SPELL_UNKNOWN) && IsEffectInSpell(buffs[bs].spellid, SE_DamageShield) && spells[buffs[bs].spellid].numhits > 0)
				CheckHitsRemaining(bs);
		}		
	}
	
	if(attacker){
		if(attacker->IsClient()){
			if(!attacker->CastToClient()->GetFeigned())
				AddToHateList(attacker, 0, damage, true, false, iBuffTic, FromDamageShield);
		}
		else
			AddToHateList(attacker, 0, damage, true, false, iBuffTic, FromDamageShield);
	}
    
	if(damage > 0) {
		//if there is some damage being done and theres an attacker involved
		if(attacker) {
			if(spell_id == SPELL_HARM_TOUCH2 && attacker->IsClient() && attacker->CastToClient()->CheckAAEffect(aaEffectLeechTouch)){
				int healed = damage;
				healed = attacker->GetActSpellHealing(spell_id, healed);
				attacker->HealDamage(healed);
				entity_list.MessageClose(this, true, 300, MT_Emote, "%s beams a smile at %s", attacker->GetCleanName(), this->GetCleanName() );
				attacker->CastToClient()->DisableAAEffect(aaEffectLeechTouch);
			}

			// if spell is lifetap add hp to the caster
			if (spell_id != SPELL_UNKNOWN && IsLifetapSpell( spell_id )) {
				int healed = damage;

				healed = attacker->GetActSpellHealing(spell_id, healed);				
				mlog(COMBAT__DAMAGE, "Applying lifetap heal of %d to %s", healed, attacker->GetName());
				attacker->HealDamage(healed);

				//we used to do a message to the client, but its gone now.
				// emote goes with every one ... even npcs
				entity_list.MessageClose(this, true, 300, MT_Emote, "%s beams a smile at %s", attacker->GetCleanName(), this->GetCleanName() );
			}
		}	//end `if there is some damage being done and theres anattacker person involved`

		Mob *pet = GetPet();
		if (pet && !pet->IsFamiliar() && !pet->SpecAttacks[IMMUNE_AGGRO] && !pet->IsEngaged() && attacker && attacker != this && !attacker->IsCorpse()) 
		{
			mlog(PETS__AGGRO, "Sending pet %s into battle due to attack.", pet->GetName());
			pet->AddToHateList(attacker, 1);
			pet->SetTarget(attacker);
			Message_StringID(10, PET_ATTACKING, pet->GetCleanName(), attacker->GetCleanName());
		}	
	
		//see if any runes want to reduce this damage
		if(spell_id == SPELL_UNKNOWN) {
			damage = ReduceDamage(damage);
			mlog(COMBAT__HITS, "Melee Damage reduced to %d", damage);
		} else {
			sint32 origdmg = damage;
			damage = AffectMagicalDamage(damage, spell_id, iBuffTic, attacker);
			mlog(COMBAT__HITS, "Melee Damage reduced to %d", damage);
			if (origdmg != damage && attacker && attacker->IsClient()) {
				if(attacker->CastToClient()->GetFilter(FILTER_DAMAGESHIELD) != FilterHide)
					attacker->Message(15, "The Spellshield absorbed %d of %d points of damage", origdmg - damage, origdmg);
			}
		}
		

	if(IsClient() && CastToClient()->sneaking){
		CastToClient()->sneaking = false;
		SendAppearancePacket(AT_Sneak, 0);
	}
	if(attacker && attacker->IsClient() && attacker->CastToClient()->sneaking){
		attacker->CastToClient()->sneaking = false;
		attacker->SendAppearancePacket(AT_Sneak, 0);
	}
		//final damage has been determined.
		
		/*
		//check for death conditions
		if(IsClient()) {
			if((GetHP()) <= -10) {
				Death(attacker, damage, spell_id, skill_used);
				return;
			}
		} else {
			if (damage >= GetHP()) {
				//killed...
				SetHP(-100);
				Death(attacker, damage, spell_id, skill_used);
				return;
			}
		}
		*/
		
		SetHP(GetHP() - damage);

		if(HasDied()) {
			bool IsSaved = false;

			if(HasDeathSaveChance()) {
				if(TryDeathSave()) {
					IsSaved = true;
				}
			}
						
			if(!IsSaved && !TrySpellOnDeath()) {
				SetHP(-500);

				if(attacker && attacker->IsClient() && (spell_id != SPELL_UNKNOWN) && damage>0) {
					char val1[20]={0};
					attacker->Message_StringID(4,OTHER_HIT_NONMELEE,GetCleanName(),ConvertArray(damage,val1));
				}

				Death(attacker, damage, spell_id, skill_used);
				return;
			}
		}

    	//fade mez if we are mezzed
		if (IsMezzed()) {
			mlog(COMBAT__HITS, "Breaking mez due to attack.");
			BuffFadeByEffect(SE_Mez);
		}
    	
    	//check stun chances if bashing or kicking and level 55+ - Kegz @ EpicEmu.com
		if (damage > 0 && ((skill_used == BASH || skill_used == KICK) && attacker))
		{
			// NPCs can stun with their bash/kick as soon as they recieve it.
			// Clients can stun mobs under level 56 with their bash/kick when they get level 55 or greater like on velious era live servers
			if((attacker->IsNPC() && attacker->GetLevel() >= 35 && attacker->GetLevel() <= 60) || (attacker->IsClient() && attacker->GetLevel() >= 55 && attacker->GetLevel() <= 60))
			{
				if (MakeRandomInt(0,99) < (RuleI(Character, NPCBashKickStunChance)) || attacker->IsClient())
				{
					int stun_resist = itembonuses.StunResist+spellbonuses.StunResist;
			
					if(this->IsClient())
						stun_resist += aabonuses.StunResist;
			
					if(this->GetBaseRace() == OGRE && this->IsClient() && !attacker->BehindMob(this, attacker->GetX(), attacker->GetY())) 
					{
						mlog(COMBAT__HITS, "Stun Resisted. Ogres are immune to frontal melee stuns.");
					}
					else 
					{
						if(stun_resist <= 0 || MakeRandomInt(0,99) >= stun_resist) 
						{
							mlog(COMBAT__HITS, "Stunned. We had %d percent resist chance.");
							Stun(1);
						} 
						else 
						{
							if(this->IsClient())
								Message_StringID(MT_Stun, SHAKE_OFF_STUN);
							
							mlog(COMBAT__HITS, "Stun Resisted. We had %dpercent resist chance.");
						}
					}
				}
			}
		}

		//check stun chances if Bashing and level 1-56 - Kegz @ EpicEmu.com
		if (damage > 0 && (skill_used == BASH && attacker))
		{
			// Clients can stun mobs under level 56 with their bash no matter what.
			if((attacker->IsNPC()) || (attacker->IsClient() && attacker->GetLevel() >= 1 && GetLevel() < 56))
			{
				if (MakeRandomInt(0,99) < (RuleI(Character, NPCBashKickStunChance)) || attacker->IsClient())
				{
					int stun_resist = itembonuses.StunResist+spellbonuses.StunResist;
			
					if(this->IsClient())
						stun_resist += aabonuses.StunResist;
			
					if(this->GetBaseRace() == OGRE && this->IsClient() && !attacker->BehindMob(this, attacker->GetX(), attacker->GetY())) 
					{
						mlog(COMBAT__HITS, "Stun Resisted. Ogres are immune to frontal melee stuns.");
					}
					else 
					{
						if(stun_resist <= 0 || MakeRandomInt(0,99) >= stun_resist) 
						{
							mlog(COMBAT__HITS, "Stunned. We had %d percent resist chance.");
							Stun(1);
						} 
						else 
						{
							if(this->IsClient())
								Message_StringID(MT_Stun, SHAKE_OFF_STUN);
							
							mlog(COMBAT__HITS, "Stun Resisted. We had %dpercent resist chance.");
						}
					}
				}
			}
		}

		if(spell_id != SPELL_UNKNOWN && !iBuffTic) {
			//see if root will break
			if (IsRooted() && !FromDamageShield) { // neotoyko: only spells cancel root
				if(GetAA(aaEnhancedRoot))
				{
					if (MakeRandomInt(0, 99) < 10) {
						mlog(COMBAT__HITS, "Spell broke root! 10percent chance");
						BuffFadeByEffect(SE_Root, buffslot); // buff slot is passed through so a root w/ dam doesnt cancel itself
					} else {
						mlog(COMBAT__HITS, "Spell did not break root. 10 percent chance");
					}
				}
				else
				{
					if (MakeRandomInt(0, 99) < 20) {
						mlog(COMBAT__HITS, "Spell broke root! 20percent chance");
						BuffFadeByEffect(SE_Root, buffslot); // buff slot is passed through so a root w/ dam doesnt cancel itself
					} else {
						mlog(COMBAT__HITS, "Spell did not break root. 20 percent chance");
					}
				}			
			}
		}
		else if(spell_id == SPELL_UNKNOWN)
		{
			//increment chances of interrupting
			if(IsCasting()) { //shouldnt interrupt on regular spell damage
				attacked_count++;
				mlog(COMBAT__HITS, "Melee attack while casting. Attack count %d", attacked_count);
			}
		}
		
		//send an HP update if we are hurt
		if(GetHP() < GetMaxHP())
			SendHPUpdate();
	}	//end `if damage was done`
	
    //send damage packet...
   	if(!iBuffTic) { //buff ticks do not send damage, instead they just call SendHPUpdate(), which is done below
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Damage, sizeof(CombatDamage_Struct));
		CombatDamage_Struct* a = (CombatDamage_Struct*)outapp->pBuffer;
		a->target = GetID();
		if (attacker == NULL)
			a->source = 0;
		else if (attacker->IsClient() && attacker->CastToClient()->GMHideMe())
			a->source = 0;
		else
			a->source = attacker->GetID();
		a->type = SkillDamageTypes[skill_used]; // was 0x1c
		a->damage = damage;
//		if (attack_skill != 231)
//			a->spellid = SPELL_UNKNOWN;
//		else
			a->spellid = spell_id;
		
		//Note: if players can become pets, they will not receive damage messages of their own
		//this was done to simplify the code here (since we can only effectively skip one mob on queue)
		eqFilterType filter;
		Mob *skip = attacker;
		if(attacker && attacker->GetOwnerID()) {
			//attacker is a pet, let pet owners see their pet's damage
			Mob* owner = attacker->GetOwner();
			if (owner && owner->IsClient()) {
				if (((spell_id != SPELL_UNKNOWN) || (FromDamageShield)) && damage>0) {
					//special crap for spell damage, looks hackish to me
					char val1[20]={0};
					owner->Message_StringID(MT_NonMelee,OTHER_HIT_NONMELEE,GetCleanName(),ConvertArray(damage,val1));
			    } else {
			    	if(damage > 0) {
						if(spell_id != SPELL_UNKNOWN)
							filter = iBuffTic ? FilterDOT : FilterSpellDamage;
						else
							filter = FILTER_MYPETHITS;
					} else if(damage == -5)
						filter = FilterNone;	//cant filter invulnerable
					else
						filter = FILTER_MYPETMISSES;

					if(!FromDamageShield)
						owner->CastToClient()->QueuePacket(outapp,true,CLIENT_CONNECTED,filter);
				}
			}
			skip = owner;
		} else {
			//attacker is not a pet, send to the attacker
			
			//if the attacker is a client, try them with the correct filter
			if(attacker && attacker->IsClient()) {
				if (((spell_id != SPELL_UNKNOWN)||(FromDamageShield)) && damage>0) {
					//special crap for spell damage, looks hackish to me
					char val1[20]={0};
					attacker->Message_StringID(MT_NonMelee,OTHER_HIT_NONMELEE,GetCleanName(),ConvertArray(damage,val1));
			    } else {
			    	if(damage > 0) {
						if(spell_id != SPELL_UNKNOWN)
							filter = iBuffTic ? FilterDOT : FilterSpellDamage;
						else
							filter = FilterNone;	//cant filter our own hits
					} else if(damage == -5)
						filter = FilterNone;	//cant filter invulnerable
					else
						filter = FILTER_MYMISSES;

					attacker->CastToClient()->QueuePacket(outapp, true, CLIENT_CONNECTED, filter);
				}
			}
			skip = attacker;
		}
		
		//send damage to all clients around except the specified skip mob (attacker or the attacker's owner) and ourself
		if(damage > 0) {
			if(spell_id != SPELL_UNKNOWN)
				filter = iBuffTic ? FilterDOT : FilterSpellDamage;
			else
				filter = FILTER_OTHERHITS;
		} else if(damage == -5)
			filter = FilterNone;	//cant filter invulnerable
		else
			filter = FILTER_OTHERMISSES;
		//make attacker (the attacker) send the packet so we can skip them and the owner
		//this call will send the packet to `this` as well (using the wrong filter) (will not happen until PC charm works)
//LogFile->write(EQEMuLog::Debug, "Queue damage to all except %s with filter %d (%d), type %d", skip->GetName(), filter, IsClient()?CastToClient()->GetFilter(filter):-1, a->type);
		//
		// If this is Damage Shield damage, the correct OP_Damage packets will be sent from Mob::DamageShield, so 
		// we don't send them here.
		if(!FromDamageShield) {
			entity_list.QueueCloseClients(this, outapp, true, 200, skip, true, filter);
			//send the damage to ourself if we are a client
			if(IsClient()) {
				//I dont think any filters apply to damage affecting us
				CastToClient()->QueuePacket(outapp);
			}
		}
		
		safe_delete(outapp);
	} else {
        //else, it is a buff tic...
		// Everhood - So we can see our dot dmg like live shows it.
		if(spell_id != SPELL_UNKNOWN && damage > 0 && attacker && attacker != this && attacker->IsClient()) {
			//might filter on (attack_skill>200 && attack_skill<250), but I dont think we need it
			if(attacker->CastToClient()->GetFilter(FilterDOT) != FilterHide) {
				attacker->Message_StringID(MT_DoTDamage, OTHER_HIT_DOT, GetCleanName(),itoa(damage),spells[spell_id].name);
			}
		}
	} //end packet sending
}


void Mob::HealDamage(uint32 amount, Mob* caster) {
	uint32 maxhp = GetMaxHP();
	uint32 curhp = GetHP();
	uint32 acthealed = 0;
	if(amount > (maxhp - curhp))
		acthealed = (maxhp - curhp);
	else
		acthealed = amount;

	char *TempString = NULL;

	MakeAnyLenString(&TempString, "%d", acthealed);
		
	if(acthealed > 100){
		if(caster){
			Message_StringID(MT_NonMelee, YOU_HEALED, caster->GetCleanName(), TempString);
			if(caster != this){
				caster->Message_StringID(MT_NonMelee, YOU_HEAL, GetCleanName(), TempString);
			}
		}
		else{
			Message(MT_NonMelee, "You have been healed for %d points of damage.", acthealed);
		}	
	}		
		
	if (curhp < maxhp) {
		if ((curhp+amount)>maxhp)
			curhp=maxhp;
		else
			curhp+=amount;
		SetHP(curhp);

		SendHPUpdate();
	}
	safe_delete_array(TempString);
}



//proc chance includes proc bonus
float Mob::GetProcChances(float &ProcBonus, float &ProcChance, int16 weapon_speed) {
	int mydex = GetDEX();
	float AABonus = 0;
	ProcBonus = 0;
	ProcChance = 0;
	if(IsClient()) {
		//increases based off 1 guys observed results.
		switch(CastToClient()->GetAA(aaWeaponAffinity)) {
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
	}

	float PermaHaste;
	if(GetHaste() > 0)
		PermaHaste = 1 / (1 + (float)GetHaste()/100);
	else if(GetHaste() < 0)
		PermaHaste = 1 * (1 - (float)GetHaste()/100);
	else
		PermaHaste = 1.0f;
		
	weapon_speed = ((int)(weapon_speed*(100.0f+attack_speed)*PermaHaste) / 100);
	if(weapon_speed < 10) // fast as a client can swing, so should be the floor of the proc chance
		weapon_speed = 10;

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

void Mob::TryDefensiveProc(Mob *on) {
	// this should have already been checked, but just in case...
	if (!this->HasDefensiveProcs())
		return;

	if (!on) {
		SetTarget(NULL);
		LogFile->write(EQEMuLog::Error, "A null Mob object was passed to Mob::TryDefensiveProc for evaluation!");
		return;
	}

	// iterate through our defensive procs and try each of them
	for (int i = 0; i < MAX_PROCS; i++) {
			if (MakeRandomInt(0, 100) < MakeRandomInt(0, 20)) {
				ExecWeaponProc(DefensiveProcs[i].spellID, on);
			}
	}

	return;
}

void Mob::TryWeaponProc(const ItemInst* weapon_g, Mob *on, int16 hand) {
	_ZP(Mob_TryWeaponProcA);
	if(!on) {
		SetTarget(NULL);
		LogFile->write(EQEMuLog::Error, "A null Mob object was passed  to Mob::TryWeaponProc for evaluation!");
		return;
	}
	if(!IsInAttackCone(on))
		return;

	if(!weapon_g) {
		TryWeaponProc((const Item_Struct*) NULL, on, hand);
		return;
	}

	if(!weapon_g->IsType(ItemClassCommon)) {
		TryWeaponProc((const Item_Struct*) NULL, on, hand);
		return;
	}
	
	//do main procs
	TryWeaponProc(weapon_g->GetItem(), on, hand);
	
	
	//we have to calculate these again, oh well
	int ourlevel = GetLevel();
	float ProcChance, ProcBonus;
	GetProcChances(ProcBonus, ProcChance, weapon_g->GetItem()->Delay);
	if(hand != 13)
	{
		ProcChance /= 2;
	}
	
	//do augment procs
	int r;
	for(r = 0; r < MAX_AUGMENT_SLOTS; r++) {
		const ItemInst* aug_i = weapon_g->GetAugment(r);
		if(!aug_i)
			continue;
		const Item_Struct* aug = aug_i->GetItem();
		if(!aug)
			continue;
		
		if (aug->Proc.Type == ET_CombatProc) {
				ProcChance = ProcChance*(100+aug->ProcRate)/100;
			if (MakeRandomFloat(0, 1) < ProcChance) {
				if(aug->Proc.Level > ourlevel) {
					Mob * own = GetOwner();
					if(own != NULL) {
						own->Message_StringID(13,PROC_PETTOOLOW);
					} else {
						Message_StringID(13,PROC_TOOLOW);
					}
				} else {
					ExecWeaponProc(aug->Proc.Effect, on);
				}
			}
		}
	}
}

void Mob::TryWeaponProc(const Item_Struct* weapon, Mob *on, int16 hand) {
	_ZP(Mob_TryWeaponProcB);
	int ourlevel = GetLevel();
	float ProcChance, ProcBonus;
	if(weapon!=NULL)
		GetProcChances(ProcBonus, ProcChance, weapon->Delay);
	else
		GetProcChances(ProcBonus, ProcChance);
		
	if(hand != 13)
		ProcChance /= 2;	
	
	//give weapon a chance to proc first.
	if(weapon != NULL) {
		if (weapon->Proc.Type == ET_CombatProc) {
			float WPC = ProcChance*(100.0f+(float)weapon->ProcRate)/100.0f;
			if (MakeRandomFloat(0, 1) <= WPC) {	// 255 dex = 0.084 chance of proc. No idea what this number should be really.
				if(weapon->Proc.Level > ourlevel) {
					mlog(COMBAT__PROCS, "Tried to proc (%s), but our level (%d) is lower than required (%d)", weapon->Name, ourlevel, weapon->Proc.Level);
					Mob * own = GetOwner();
					if(own != NULL) {
						own->Message_StringID(13,PROC_PETTOOLOW);
					} else {
						Message_StringID(13,PROC_TOOLOW);
					}
				} else {
					mlog(COMBAT__PROCS, "Attacking weapon (%s) successfully procing spell %d (%.2f percent chance)", weapon->Name, weapon->Proc.Effect, ProcChance*100);
					ExecWeaponProc(weapon->Proc.Effect, on);
				}
			} else {
				mlog(COMBAT__PROCS, "Attacking weapon (%s) did no proc (%.2f percent chance).", weapon->Name, ProcChance*100);
			}
		}
	}
	
	if(ProcBonus == -1) {
		LogFile->write(EQEMuLog::Error, "ProcBonus was -1 value!");
		return;
	}

	bool bRangedAttack = false;
	if (weapon != NULL) {
		if (weapon->ItemType == ItemTypeBow || weapon->ItemType == ItemTypeThrowing || weapon->ItemType == ItemTypeThrowingv2) {
			bRangedAttack = true;
		}
	}

	bool isRanged = false;
	if(weapon)
	{
		if(weapon->ItemType == ItemTypeArrow ||
			weapon->ItemType == ItemTypeThrowing ||
			weapon->ItemType == ItemTypeThrowingv2 ||
			weapon->ItemType == ItemTypeBow)
		{
			isRanged = true;
		}
	}

	uint32 i;
	for(i = 0; i < MAX_PROCS; i++) {
		if (PermaProcs[i].spellID != SPELL_UNKNOWN) {
			if(MakeRandomInt(0, 100) < PermaProcs[i].chance) {
				mlog(COMBAT__PROCS, "Permanent proc %d procing spell %d (%d percent chance)", i, PermaProcs[i].spellID, PermaProcs[i].chance);
				ExecWeaponProc(PermaProcs[i].spellID, on);
			} else {
				mlog(COMBAT__PROCS, "Permanent proc %d failed to proc %d (%d percent chance)", i, PermaProcs[i].spellID, PermaProcs[i].chance);
			}
		}
		if(!isRanged)
		{
				int chance = ProcChance * (SpellProcs[i].chance);
				if(MakeRandomInt(0, 100) < chance) {
					mlog(COMBAT__PROCS, "Spell proc %d procing spell %d (%d percent chance)", i, SpellProcs[i].spellID, chance);
					ExecWeaponProc(SpellProcs[i].spellID, on);
				} else {
					mlog(COMBAT__PROCS, "Spell proc %d failed to proc %d (%d percent chance)", i, SpellProcs[i].spellID, chance);
				}
		}
		if (bRangedAttack) {
			if(MakeRandomInt(0, 100) < MakeRandomInt(0, 25)) {
				mlog(COMBAT__PROCS, "Ranged proc %d procing spell %d", i, RangedProcs[i].spellID, RangedProcs[i].chance);
				ExecWeaponProc(RangedProcs[i].spellID, on);
			} else {
				mlog(COMBAT__PROCS, "Ranged proc %d failed to proc %d", i, RangedProcs[i].spellID, RangedProcs[i].chance);
			}
		}
	
	}
}

void Mob::TryPetCriticalHit(Mob *defender, int16 skill, sint32 &damage)
{
	if(damage < 1)
		return;
		
	Client *owner = NULL;
	int critChance = RuleI(Combat, MeleeBaseCritChance);
	uint16 critMod = 200;

	if (damage < 1) //We can't critical hit if we don't hit.
		return;

	if (!this->IsPet() || !this->GetOwner()->IsClient())
		return;

	owner = this->GetOwner()->CastToClient();

	sint16 aaClass = -1;
	switch (owner->GetClass()) {
		case NECROMANCER:
			aaClass = aaDeathsFury;
			break;
		case MAGICIAN:
			aaClass = aaElementalFury;
			break;
		case BEASTLORD:
			aaClass = aaWardersFury;
			break;
	}

	switch (owner->GetAA(aaClass)) {
	case 1:
		critChance += 4;
		break;
	case 2:
		critChance += 8;
		break;
	case 3:
		critChance += 12;
		break;
	case 4:
		critChance += 16;
		break;
	case 5:
		critChance += 20;
		break;
	}

	int CritBonus = GetCriticalChanceBonus(skill);
	if(CritBonus > 0) {
		if(critChance == 0) //If we have a bonus to crit in items or spells but no actual chance to crit
			critChance = (CritBonus / 100) + 1; //Give them a small one so skills and items appear to have some effect.
		else
			critChance += (critChance * CritBonus / 100); //crit chance is a % increase to your reg chance
	}
	if (critChance > 0) {
		if (MakeRandomInt(0, 99) < critChance) {
			critMod += GetCritDmgMob(skill) * 2; // To account for base crit mod being 200 not 100
			damage = (damage * critMod) / 100;
            entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, CRITICAL_HIT, GetCleanName(), itoa(damage));
		}
	}
}

void Mob::TryCriticalHit(Mob *defender, int16 skill, sint32 &damage)
{
	if(damage < 1)
		return;
		
	bool slayUndeadCrit = false;

	// decided to branch this into it's own function since it's going to be duplicating a lot of the
	// code in here, but could lead to some confusion otherwise
	if (this->IsPet() && this->GetOwner()->IsClient()) {
		this->TryPetCriticalHit(defender,skill,damage);
		return;
	}

	int critChance = RuleI(Combat, MeleeBaseCritChance);
	if(IsClient())
		critChance += RuleI(Combat, ClientBaseCritChance);
	
	// Bonus to crippling blow chance 
	bool crip_success = false;
	if(MakeRandomInt(0,99) < GetCrippBlowChance())
		crip_success = true;

	uint16 critMod = 200; 
	if(((GetClass() == WARRIOR || GetClass() == BERSERKER) && GetLevel() >= 12 && IsClient()) || crip_success) 
	{
		if(CastToClient()->berserk || crip_success)
		{
			critChance += RuleI(Combat, BerserkBaseCritChance);
			critMod = 400;
		}
		else
		{
			critChance += RuleI(Combat, WarBerBaseCritChance);
		}
	}

	if(skill == ARCHERY && GetClass() == RANGER && GetSkill(ARCHERY) >= 65){
		critChance += 6;
	}

	int CritBonus = GetCriticalChanceBonus(skill);
	if(IsClient())
		critChance += GetCriticalChanceBonus(skill, true); // These add straight on

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
			//Veteran's Wrath AA
			//first, find out of we have it (don't multiply by 0 :-\ )
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
			critMod += GetCritDmgMob(skill) * 2; // To account for base crit mod being 200 not 100
			damage = damage * critMod / 100;
			
			if(IsClient() && CastToClient()->berserk || crip_success)
			{
				entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, CRIPPLING_BLOW, GetCleanName(), itoa(damage));
				// Crippling blows also have a chance to stun
				if(MakeRandomInt(0,99) < 50) //improbable to stun every hit
					defender->Stun(0);
			}

			else
			{
				entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, CRITICAL_HIT, GetCleanName(), itoa(damage));
			}
		}
	}
}

bool Mob::TryFinishingBlow(Mob *defender, SkillType skillinuse)
{
	int8 aa_item = GetAA(aaFinishingBlow) + GetAA(aaCoupdeGrace) + GetAA(aaDeathblow);

	if(aa_item && !defender->IsClient() && defender->GetHPRatio() < 10){
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
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, FINISHING_BLOW, GetName());
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

void Mob::DoRiposte(Mob* defender) {
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
	}

	DoubleRipChance += 10*defender->GetAA(aaFlashofSteel);

	if(DoubleRipChance >= MakeRandomInt(0, 100)) {
		mlog(COMBAT__ATTACKS, "Preforming a double riposed (%d percent chance)", DoubleRipChance);

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
 
void Mob::ApplyMeleeDamageBonus(int16 skill, sint32 &damage){

	if(skill == THROWING) {
		switch(GetAA(aaThrowingMastery))
		{
			case 1:
				damage = damage * 115/100;
				break;
			case 2:
				damage = damage * 125/100;
				break;
			case 3:
				damage = damage * 150/100;
				break;
		}
	}

	if(!RuleB(Combat, UseIntervalAC)){
		if(IsNPC()){ //across the board NPC damage bonuses.
 			//only account for STR here, assume their base STR was factored into their DB damages
			int dmgbonusmod = 0;
			dmgbonusmod += (100*(itembonuses.STR + spellbonuses.STR))/3;
			dmgbonusmod += (100*(spellbonuses.ATK + itembonuses.ATK))/5;
			mlog(COMBAT__DAMAGE, "Damage bonus: %d percent from ATK and STR bonuses.", (dmgbonusmod/100));
			damage += (damage*dmgbonusmod/10000);
		}
	}
	
	damage += damage * GetMeleeDamageMod_SE(skill) / 100;
	
	//Rogue sneak attack disciplines make use of this, they are active for one hit
	uint32 buff_count = GetMaxTotalSlots();
	for(int bs = 0; bs < buff_count; bs++){
		if((buffs[bs].spellid != SPELL_UNKNOWN) && IsEffectInSpell(buffs[bs].spellid, SE_HitChance) && spells[buffs[bs].spellid].numhits > 0){
			if(skill == spells[buffs[bs].spellid].skill)
				CheckHitsRemaining(bs);
		}	
	}	
}

bool Mob::HasDied() {
	bool Result = false;
	sint16 hp_below = 0;

	if(IsClient())
		hp_below = (CastToClient()->GetDelayDeath() * -1);

	if((GetHP()) <= (hp_below))
		Result = true;

	return Result;
}

int16 Mob::GetDamageTable(SkillType skillinuse)
{
	if(GetLevel() <= 60)  //was 51
	{
		int16 ret_table = 0;
		int str_over_75 = 0;
		if(GetSTR() > 75)
			str_over_75 = GetSTR() - 75;
		if(str_over_75 > 255)
			ret_table = (GetSkill(skillinuse)+255)/2;
		else
			ret_table = (GetSkill(skillinuse)+str_over_75)/2;

		if(ret_table < 100)
			return 100;

		return ret_table;
	}
	else if(GetLevel() >= 90)
	{
		if(GetClass() == MONK)
			return 379;
		else
			return 345;
	}
	else
	{
		int16 dmg_table[] = { 
			275, 275, 275, 275, 275,
			280, 280, 280, 280,	285,
			285, 285, 290, 290, 295,
			295, 300, 300, 300, 305, 
			305, 305, 310, 310, 315,
			315, 320, 320, 320, 325,
			325, 325, 330, 330, 335, 
			335, 340, 340, 340,
		};
		if(GetClass() == MONK)
			return (dmg_table[GetLevel()-51]*(100+RuleI(Combat,MonkDamageTableBonus))/100);
		else
			return dmg_table[GetLevel()-51];
	}
}
