/*  EQEMu:  Everquest Server Emulator
Copyright (C) 2001-2004  EQEMu Development Team (http://eqemu.org)

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
#include "../common/debug.h"
#include "spdat.h"
#include "masterentity.h"
#include "../common/packet_dump.h"
#include "../common/moremath.h"
#include "../common/Item.h"
#include "worldserver.h"
#include "../common/skills.h"
#include "../common/bodytypes.h"
#include "../common/classes.h"
#include "../common/rulesys.h"
#include "QuestParserCollection.h"
#include <math.h>
#include <assert.h>
#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif

#include "StringIDs.h"

void Mob::CalcBonuses()
{	
	CalcSpellBonuses(&spellbonuses);
	CalcMaxHP();
	CalcMaxMana();
	SetAttackTimer();
	
	rooted = FindType(SE_Root);
}

void NPC::CalcBonuses()
{
	if(RuleB(NPC, UseItemBonusesForNonPets)){
		memset(&itembonuses, 0, sizeof(StatBonuses));
		CalcItemBonuses(&itembonuses);
	}
	else{
		if(GetOwner()){
			memset(&itembonuses, 0, sizeof(StatBonuses));
			CalcItemBonuses(&itembonuses);
		}
	}

	// This has to happen last, so we actually take the item bonuses into account.
	Mob::CalcBonuses();
}

void Client::CalcBonuses()
{
	_ZP(Client_CalcBonuses);
	memset(&itembonuses, 0, sizeof(StatBonuses));
	CalcItemBonuses(&itembonuses);
	CalcEdibleBonuses(&itembonuses);
	
	CalcSpellBonuses(&spellbonuses);

	_log(AA__BONUSES, "Calculating AA Bonuses for %s.", this->GetCleanName());
	CalcAABonuses(&aabonuses);	//we're not quite ready for this
	_log(AA__BONUSES, "Finished calculating AA Bonuses for %s.", this->GetCleanName());
	
	RecalcWeight();
	
	CalcAC();
	CalcATK();
	CalcHaste();
	
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
	
	CalcMaxHP();
	CalcMaxMana();
	CalcMaxEndurance();
	
	rooted = FindType(SE_Root);

	XPRate = 100 + spellbonuses.XPRateMod;
}

int Client::CalcRecommendedLevelBonus(int8 level, uint8 reclevel, int basestat)
{
	if( (reclevel > 0) && (level < reclevel) )
	{
		sint32 statmod = (level * 10000 / reclevel) * basestat;
	
		if( statmod < 0 )
		{
			statmod -= 5000;
			return (statmod/10000);
		}
		else
		{
			statmod += 5000;
			return (statmod/10000);
		}
	}

	return 0;
}

void Client::CalcItemBonuses(StatBonuses* newbon) {
	//memset assumed to be done by caller.
	
	// Clear item faction mods
	ClearItemFactionBonuses();

	unsigned int i;
	//should not include 21 (SLOT_AMMO)
	for (i=0; i<21; i++) {
		const ItemInst* inst = m_inv[i];
		if(inst == 0)
			continue;
		AddItemBonuses(inst, newbon);
	}
	
	//Power Source Slot
	if (GetClientVersion() >= EQClientSoF)
	{
		const ItemInst* inst = m_inv[9999];
		if(inst)
			AddItemBonuses(inst, newbon);
	}

	//tribute items
	for (i = 0; i < MAX_PLAYER_TRIBUTES; i++) {
		const ItemInst* inst = m_inv[TRIBUTE_SLOT_START + i];
		if(inst == 0)
			continue;
		AddItemBonuses(inst, newbon, false, true);
	}
	// Caps
	if(newbon->HPRegen > CalcHPRegenCap())
		newbon->HPRegen = CalcHPRegenCap();

	if(newbon->ManaRegen > CalcManaRegenCap())
		newbon->ManaRegen = CalcManaRegenCap();
	
	if(newbon->EnduranceRegen > CalcEnduranceRegenCap())
		newbon->EnduranceRegen = CalcEnduranceRegenCap();
			
	SetAttackTimer();
}
		
void Client::AddItemBonuses(const ItemInst *inst, StatBonuses* newbon, bool isAug, bool isTribute) {
	if(!inst || !inst->IsType(ItemClassCommon))
	{
		return;
	}

	if(inst->GetAugmentType()==0 && isAug == true)
	{
		return;
	}

	const Item_Struct *item = inst->GetItem();

	if(!isTribute && !inst->IsEquipable(GetBaseRace(),GetClass()))
	{
		if(item->ItemType != ItemTypeFood && item->ItemType != ItemTypeDrink)
			return;
	}

	if(GetLevel() < item->ReqLevel)
	{
		return;
	}

	if(GetLevel() >= item->RecLevel)
	{
		newbon->AC += item->AC;
		newbon->HP += item->HP;
		newbon->Mana += item->Mana;
		newbon->Endurance += item->Endur;
		newbon->STR += (item->AStr + item->HeroicStr);
		newbon->STA += (item->ASta + item->HeroicSta);
		newbon->DEX += (item->ADex + item->HeroicDex);
		newbon->AGI += (item->AAgi + item->HeroicAgi);
		newbon->INT += (item->AInt + item->HeroicInt);
		newbon->WIS += (item->AWis + item->HeroicWis);
		newbon->CHA += (item->ACha + item->HeroicCha);
		
		newbon->MR += (item->MR + item->HeroicMR);
		newbon->FR += (item->FR + item->HeroicFR);
		newbon->CR += (item->CR + item->HeroicCR);
		newbon->PR += (item->PR + item->HeroicPR);
		newbon->DR += (item->DR + item->HeroicDR);
		newbon->Corrup += (item->SVCorruption + item->HeroicSVCorrup);

		newbon->STRCapMod += item->HeroicStr;
		newbon->STACapMod += item->HeroicSta;
		newbon->DEXCapMod += item->HeroicDex;
		newbon->AGICapMod += item->HeroicAgi;
		newbon->INTCapMod += item->HeroicInt;
		newbon->WISCapMod += item->HeroicWis;
		newbon->CHACapMod += item->HeroicCha;
		newbon->MRCapMod += item->HeroicMR;
		newbon->CRCapMod += item->HeroicFR;
		newbon->FRCapMod += item->HeroicCR;
		newbon->PRCapMod += item->HeroicPR;
		newbon->DRCapMod += item->HeroicDR;
		newbon->CorrupCapMod += item->HeroicSVCorrup;

		newbon->HeroicSTR += item->HeroicStr;
		newbon->HeroicSTA += item->HeroicSta;
		newbon->HeroicDEX += item->HeroicDex;
		newbon->HeroicAGI += item->HeroicAgi;
		newbon->HeroicINT += item->HeroicInt;
		newbon->HeroicWIS += item->HeroicWis;
		newbon->HeroicCHA += item->HeroicCha;
		newbon->HeroicMR += item->HeroicMR;
		newbon->HeroicFR += item->HeroicFR;
		newbon->HeroicCR += item->HeroicCR;
		newbon->HeroicPR += item->HeroicPR;
		newbon->HeroicDR += item->HeroicDR;
		newbon->HeroicCorrup += item->HeroicSVCorrup;

	}
	else
	{
		int lvl = GetLevel();
		int reclvl = item->RecLevel;

		newbon->AC += CalcRecommendedLevelBonus( lvl, reclvl, item->AC );
		newbon->HP += CalcRecommendedLevelBonus( lvl, reclvl, item->HP );
		newbon->Mana += CalcRecommendedLevelBonus( lvl, reclvl, item->Mana );
		newbon->Endurance += CalcRecommendedLevelBonus( lvl, reclvl, item->Endur );
		newbon->STR += CalcRecommendedLevelBonus( lvl, reclvl, (item->AStr + item->HeroicStr) );
		newbon->STA += CalcRecommendedLevelBonus( lvl, reclvl, (item->ASta + item->HeroicSta) );
		newbon->DEX += CalcRecommendedLevelBonus( lvl, reclvl, (item->ADex + item->HeroicDex) );
		newbon->AGI += CalcRecommendedLevelBonus( lvl, reclvl, (item->AAgi + item->HeroicAgi) );
		newbon->INT += CalcRecommendedLevelBonus( lvl, reclvl, (item->AInt + item->HeroicInt) );
		newbon->WIS += CalcRecommendedLevelBonus( lvl, reclvl, (item->AWis + item->HeroicWis) );
		newbon->CHA += CalcRecommendedLevelBonus( lvl, reclvl, (item->ACha + item->HeroicCha) );

		newbon->MR += CalcRecommendedLevelBonus( lvl, reclvl, (item->MR + item->HeroicMR) );
		newbon->FR += CalcRecommendedLevelBonus( lvl, reclvl, (item->FR + item->HeroicFR) );
		newbon->CR += CalcRecommendedLevelBonus( lvl, reclvl, (item->CR + item->HeroicCR) );
		newbon->PR += CalcRecommendedLevelBonus( lvl, reclvl, (item->PR + item->HeroicPR) );
		newbon->DR += CalcRecommendedLevelBonus( lvl, reclvl, (item->DR + item->HeroicDR) );
		newbon->Corrup += CalcRecommendedLevelBonus( lvl, reclvl, (item->SVCorruption + item->HeroicSVCorrup) );

		newbon->STRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicStr );
		newbon->STACapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicSta );
		newbon->DEXCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicDex );
		newbon->AGICapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicAgi );
		newbon->INTCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicInt );
		newbon->WISCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicWis );
		newbon->CHACapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicCha );
		newbon->MRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicMR );
		newbon->CRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicFR );
		newbon->FRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicCR );
		newbon->PRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicPR );
		newbon->DRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicDR );
		newbon->CorrupCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicSVCorrup );

		newbon->HeroicSTR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicStr );
		newbon->HeroicSTA += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicSta );
		newbon->HeroicDEX += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicDex );
		newbon->HeroicAGI += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicAgi );
		newbon->HeroicINT += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicInt );
		newbon->HeroicWIS += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicWis );
		newbon->HeroicCHA += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicCha );
		newbon->HeroicMR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicMR );
		newbon->HeroicFR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicFR );
		newbon->HeroicCR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicCR );
		newbon->HeroicPR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicPR );
		newbon->HeroicDR += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicDR );
		newbon->HeroicCorrup += CalcRecommendedLevelBonus( lvl, reclvl, item->HeroicSVCorrup );
	}
	
	//FatherNitwit: New style haste, shields, and regens
	if(newbon->haste < (sint16)item->Haste) {
		newbon->haste = item->Haste;
	}
	if(item->Regen > 0)
		newbon->HPRegen += item->Regen;

	if(item->ManaRegen > 0)
		newbon->ManaRegen += item->ManaRegen;
	
	if(item->EnduranceRegen > 0)
		newbon->EnduranceRegen += item->EnduranceRegen;

	if(item->Attack > 0) {
		if((newbon->ATK + item->Attack) > RuleI(Character, ItemATKCap))
		{
			newbon->ATK = RuleI(Character, ItemATKCap);
		}
		else
		{
			newbon->ATK += item->Attack;
		}
	}
	if(item->DamageShield > 0) {
		if((newbon->DamageShield + item->DamageShield) > RuleI(Character, ItemDamageShieldCap))
			newbon->DamageShield = RuleI(Character, ItemDamageShieldCap);
		else
			newbon->DamageShield += item->DamageShield;
	}
	if(item->SpellShield > 0) {
		if((newbon->SpellShield + item->SpellShield) > RuleI(Character, ItemSpellShieldingCap))
			newbon->SpellShield = RuleI(Character, ItemSpellShieldingCap);
		else
			newbon->SpellShield += item->SpellShield;
	}
	if(item->Shielding > 0) {
		if((newbon->MeleeMitigation + item->Shielding) > RuleI(Character, ItemShieldingCap))
			newbon->MeleeMitigation = RuleI(Character, ItemShieldingCap);
		else
			newbon->MeleeMitigation += item->Shielding;
	}
	if(item->StunResist > 0) {
		if((newbon->StunResist + item->StunResist) > RuleI(Character, ItemStunResistCap))
			newbon->StunResist = RuleI(Character, ItemStunResistCap);
		else
			newbon->StunResist += item->StunResist;
	}
	if(item->StrikeThrough > 0) {
		if((newbon->StrikeThrough + item->StrikeThrough) > RuleI(Character, ItemStrikethroughCap))
			newbon->StrikeThrough = RuleI(Character, ItemStrikethroughCap);
		else
			newbon->StrikeThrough += item->StrikeThrough;
	}
	if(item->Avoidance > 0) {
		if((newbon->AvoidMeleeChance + item->Avoidance) > RuleI(Character, ItemAvoidanceCap))
			newbon->AvoidMeleeChance = RuleI(Character, ItemAvoidanceCap);
		else
			newbon->AvoidMeleeChance += item->Avoidance;
	}
	if(item->Accuracy > 0) {
		if((newbon->HitChance + item->Accuracy) > RuleI(Character, ItemAccuracyCap))
			newbon->HitChance = RuleI(Character, ItemAccuracyCap);
		else
			newbon->HitChance += item->Accuracy;
	}
	if(item->CombatEffects > 0) {
		if((newbon->ProcChance + item->CombatEffects) > RuleI(Character, ItemCombatEffectsCap))
			newbon->ProcChance = RuleI(Character, ItemCombatEffectsCap);
		else
			newbon->ProcChance += item->CombatEffects;
	}
	if(item->DotShielding > 0) {
		if((newbon->DoTShielding + item->DotShielding) > RuleI(Character, ItemDoTShieldingCap))
			newbon->DoTShielding = RuleI(Character, ItemDoTShieldingCap);
		else
			newbon->DoTShielding += item->DotShielding;
	}
	
	if(item->HealAmt > 0) {
		if((newbon->HealAmt + item->HealAmt) > RuleI(Character, ItemHealAmtCap))
			newbon->HealAmt = RuleI(Character, ItemHealAmtCap);
		else
			newbon->HealAmt += item->HealAmt;
	}
	if(item->SpellDmg > 0) {
		if((newbon->SpellDmg + item->SpellDmg) > RuleI(Character, ItemSpellDmgCap))
			newbon->SpellDmg = RuleI(Character, ItemSpellDmgCap);
		else
			newbon->SpellDmg += item->SpellDmg;
	}
	if(item->Clairvoyance > 0) {
		if((newbon->Clairvoyance + item->Clairvoyance) > RuleI(Character, ItemClairvoyanceCap))
			newbon->Clairvoyance = RuleI(Character, ItemClairvoyanceCap);
		else
			newbon->Clairvoyance += item->Clairvoyance;
	}
	
	if(item->DSMitigation > 0) {
		if((newbon->DSMitigation + item->DSMitigation) > RuleI(Character, ItemDSMitigationCap))
			newbon->DSMitigation = RuleI(Character, ItemDSMitigationCap);
		else
			newbon->DSMitigation += item->DSMitigation;
	}
	if (item->Worn.Effect>0 && (item->Worn.Type == ET_WornEffect)) { // latent effects
		ApplySpellsBonuses(item->Worn.Effect, item->Worn.Level, newbon, 0, true);
	}
	switch(item->BardType)
	{
	case 51: /* All (e.g. Singing Short Sword) */
		{
			if(item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			if(item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			if(item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			if(item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			if(item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		}
	case 50: /* Singing */
		{
			if(item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			break;
		}
	case 23: /* Wind */
		{
			if(item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		}
	case 24: /* stringed */
		{
			if(item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			break;
		}
	case 25: /* brass */
		{
			if(item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			break;
		}
	case 26: /* Percussion */
		{
			if(item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			break;
		}
	}
	
	if (item->SkillModValue != 0 && item->SkillModType <= HIGHEST_SKILL){
		if ((item->SkillModValue > 0 && newbon->skillmod[item->SkillModType] < item->SkillModValue) ||
			(item->SkillModValue < 0 && newbon->skillmod[item->SkillModType] > item->SkillModValue))
		{
			newbon->skillmod[item->SkillModType] = item->SkillModValue;
		}
	}

	// Add Item Faction Mods
	if (item->FactionMod1)
	{
		if (item->FactionAmt1 > 0 && item->FactionAmt1 > GetItemFactionBonus(item->FactionMod1))
		{
			AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
		}
		else if (item->FactionAmt1 < 0 && item->FactionAmt1 < GetItemFactionBonus(item->FactionMod1))
		{
			AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
		}
	}
	if (item->FactionMod2)
	{
		if (item->FactionAmt2 > 0 && item->FactionAmt2 > GetItemFactionBonus(item->FactionMod2))
		{
			AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
		}
		else if (item->FactionAmt2 < 0 && item->FactionAmt2 < GetItemFactionBonus(item->FactionMod2))
		{
			AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
		}
	}
	if (item->FactionMod3)
	{
		if (item->FactionAmt3 > 0 && item->FactionAmt3 > GetItemFactionBonus(item->FactionMod3))
		{
			AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
		}
		else if (item->FactionAmt3 < 0 && item->FactionAmt3 < GetItemFactionBonus(item->FactionMod3))
		{
			AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
		}
	}
	if (item->FactionMod4)
	{
		if (item->FactionAmt4 > 0 && item->FactionAmt4 > GetItemFactionBonus(item->FactionMod4))
		{
			AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
		}
		else if (item->FactionAmt4 < 0 && item->FactionAmt4 < GetItemFactionBonus(item->FactionMod4))
		{
			AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
		}
	}
	
	if (item->ExtraDmgSkill != 0 && item->ExtraDmgSkill <= HIGHEST_SKILL) {
		if((newbon->SkillDamageAmount[item->ExtraDmgSkill] + item->ExtraDmgAmt) > RuleI(Character, ItemExtraDmgCap))
			newbon->SkillDamageAmount[item->ExtraDmgSkill] = RuleI(Character, ItemExtraDmgCap);
		else
			newbon->SkillDamageAmount[item->ExtraDmgSkill] += item->ExtraDmgAmt;
	}

	if (!isAug)
	{
		int i;
		for(i = 0; i < MAX_AUGMENT_SLOTS; i++) {
			AddItemBonuses(inst->GetAugment(i),newbon,true);
		}
	}

}

void Client::CalcEdibleBonuses(StatBonuses* newbon) {
#if EQDEBUG >= 11
    cout<<"Client::CalcEdibleBonuses(StatBonuses* newbon)"<<endl;
#endif
  // Search player slots for skill=14(food) and skill=15(drink)
  	uint32 i;
  	
	bool food = false;
	bool drink = false;
	for (i = 22; i <= 29; i++)
	{
		if (food && drink)
			break;
		const ItemInst* inst = GetInv().GetItem(i);
		if (inst && inst->GetItem() && inst->IsType(ItemClassCommon)) {
			const Item_Struct *item=inst->GetItem();
			if (item->ItemType == ItemTypeFood && !food)
				food = true;
			else if (item->ItemType == ItemTypeDrink && !drink)
				drink = true;
			else
				continue;
			AddItemBonuses(inst, newbon);
		}
	}
	for (i = 251; i <= 330; i++)
	{
		if (food && drink)
			break;
		const ItemInst* inst = GetInv().GetItem(i);
		if (inst && inst->GetItem() && inst->IsType(ItemClassCommon)) {
			const Item_Struct *item=inst->GetItem();
			if (item->ItemType == ItemTypeFood && !food)
				food = true;
			else if (item->ItemType == ItemTypeDrink && !drink)
				drink = true;
			else
				continue;
			AddItemBonuses(inst, newbon);
		}
	}
}

void Client::CalcAABonuses(StatBonuses* newbon) {
	memset(newbon, 0, sizeof(StatBonuses));	//start fresh

	int i;
	uint32 slots = 0;
	uint32 aa_AA = 0;
	uint32 aa_value = 0;
	for (i = 0; i < MAX_PP_AA_ARRAY; i++) {	//iterate through all of the client's AAs
		if (this->aa[i]) {	// make sure aa exists or we'll crash zone
			aa_AA = this->aa[i]->AA;	//same as aaid from the aa_effects table
			aa_value = this->aa[i]->value;	//how many points in it
			if (aa_AA > 0 || aa_value > 0) {	//do we have the AA? if 1 of the 2 is set, we can assume we do
				//slots = database.GetTotalAALevels(aa_AA);	//find out how many effects from aa_effects table
				slots = zone->GetTotalAALevels(aa_AA);	//find out how many effects from aa_effects, which is loaded into memory
				if (slots > 0)	//and does it have any effects? may be able to put this above, not sure if it runs on each iteration
					ApplyAABonuses(aa_AA, slots, newbon);	//add the bonuses
			}
		}
	}

	// Not any other clear place to put this at the moment
	if(GetAA(aaTouchoftheDivine))
		this->SetDeathSaveChance(true);
}


//A lot of the normal spell functions (IsBlankSpellEffect, etc) are set for just spells (in zone/spdat.h). For now, we'll just put them directly into the code and comment with the corresponding normal function
//Maybe we'll fix it later? :-D
void Client::ApplyAABonuses(uint32 aaid, uint32 slots, StatBonuses* newbon) 
{
	if(slots == 0)	//sanity check. why bother if no slots to fill?
		return;

	//from AA_Ability struct
	int32 effect = 0;
	int32 base1 = 0;
	int32 base2 = 0;	//only really used for SE_RaiseStatCap & SE_ReduceSkillTimer in aa_effects table
	int32 slot = 0;

	std::map<uint32, std::map<uint32, AA_Ability> >::const_iterator find_iter = aa_effects.find(aaid);
	if(find_iter == aa_effects.end())
	{
		return;
	}

	for (map<uint32, AA_Ability>::const_iterator iter = aa_effects[aaid].begin(); iter != aa_effects[aaid].end(); ++iter) {
		effect = iter->second.skill_id;
		base1 = iter->second.base1;
		base2 = iter->second.base2;
		slot = iter->second.slot;

		//we default to 0 (SE_CurrentHP) for the effect, so if there aren't any base1/2 values, we'll just skip it
		if (effect == 0 && base1 == 0 && base2 == 0)
			continue;

		//IsBlankSpellEffect()
		if (effect == SE_Blank || (effect == SE_CHA && base1 == 0) || effect == SE_StackingCommand_Block || effect == SE_StackingCommand_Overwrite)
			continue;

		_log(AA__BONUSES, "Applying Effect %d from AA %u in slot %d (base1: %d, base2: %d) on %s", effect, aaid, slot, base1, base2, this->GetCleanName());
		switch (effect)
		{
			case SE_Accuracy:
				newbon->HitChance += base1;
				break;
			case SE_CurrentHP: //regens
				newbon->HPRegen += base1;
				break;
			case SE_CurrentEndurance: 
				newbon->EnduranceRegen += base1;
				break;
			case SE_MovementSpeed:
				newbon->movementspeed += base1;	//should we let these stack?
				/*if (base1 > newbon->movementspeed)	//or should we use a total value?
					newbon->movementspeed = base1;*/
				break;
			case SE_STR:
				newbon->STR += base1;
				break;
			case SE_DEX:
				newbon->DEX += base1;
				break;
			case SE_AGI:
				newbon->AGI += base1;
				break;
			case SE_STA:
				newbon->STA += base1;
				break;
			case SE_INT:
				newbon->INT += base1;
				break;
			case SE_WIS:
				newbon->WIS += base1;
				break;
			case SE_CHA:
				newbon->CHA += base1;
				break;
			case SE_WaterBreathing:
				//handled by client
				break;
			case SE_CurrentMana:
				newbon->ManaRegen += base1;
				break;
			case SE_ItemManaRegenCapIncrease:
				newbon->ItemManaRegenCap += base1;
				break;
			case SE_ResistFire:
				newbon->FR += base1;
				break;
			case SE_ResistCold:
				newbon->CR += base1;
				break;
			case SE_ResistPoison:
				newbon->PR += base1;
				break;
			case SE_ResistDisease:
				newbon->DR += base1;
				break;
			case SE_ResistMagic:
				newbon->MR += base1;
				break;
			case SE_ResistCorruption:
				newbon->Corrup += base1;
				break;
			case SE_IncreaseSpellHaste:
				break;
			case SE_IncreaseRange:
				break;
			case SE_MaxHPChange:
				newbon->MaxHP += base1;
				break;
			case SE_Packrat:
				newbon->Packrat += base1;
				break;
			case SE_TwoHandBash:
				break;
			case SE_SetBreathLevel:
				break;
			case SE_RaiseStatCap:
				switch(base2)
				{
					//are these #define'd somewhere?
					case 0: //str
						newbon->STRCapMod += base1;
						break;
					case 1: //sta
						newbon->STACapMod += base1;
						break;
					case 2: //agi
						newbon->AGICapMod += base1;
						break;
					case 3: //dex
						newbon->DEXCapMod += base1;
						break;
					case 4: //wis
						newbon->WISCapMod += base1;
						break;
					case 5: //int
						newbon->INTCapMod += base1;
						break;
					case 6: //cha
						newbon->CHACapMod += base1;
						break;
					case 7: //mr
						newbon->MRCapMod += base1;
						break;
					case 8: //cr
						newbon->CRCapMod += base1;
						break;
					case 9: //fr
						newbon->FRCapMod += base1;
						break;
					case 10: //pr
						newbon->PRCapMod += base1;
						break;
					case 11: //dr
						newbon->DRCapMod += base1;
						break;
					case 12: //corruption
						newbon->CorrupCapMod += base1;
						break;
				}
				break;
			case SE_PetDiscipline2:
				break;
			case SE_BaseMovementSpeed:
				break;
			case SE_SpellSlotIncrease:
				break;
			case SE_MysticalAttune:
				newbon->BuffSlotIncrease += base1;
				break;
			case SE_TotalHP:
				newbon->HP += base1;
				break;
			case SE_StunResist:
				newbon->StunResist += base1;
				break;
			case SE_TriggerOnCast:
				for(int i = 0; i < MAX_SPELL_TRIGGER; i+=2)
				{
					if(!newbon->SpellTriggers[i])
					{
						// base1 = SpellID to be triggered, base2 = chance to fire
						newbon->SpellTriggers[i] = base1;
						newbon->SpellTriggers[i+1] = base2;
						break;
					}
				}
				break;
			case SE_CriticalSpellChance:
			case SE_SpellCritChance:
				newbon->CriticalSpellChance += base1;
				break;

			case SE_SpellCritDmgIncrease:
				newbon->SpellCritDmgIncrease += base1;
				break;

			case SE_ResistSpellChance:
				newbon->ResistSpellChance += base1;
				break;

			case SE_CriticalHealChance:
				newbon->CriticalHealChance += base1;
				break;
			
			case SE_CriticalHealOverTime:
				newbon->CriticalHealOverTime += base1;
				break;
			
			case SE_CriticalDoTChance:
				newbon->CriticalDoTChance += base1;
				break;
				
			case SE_CriticalHitChance:
				if(base2 == -1)
					newbon->CriticalHitChance[HIGHEST_SKILL+1] += base1;
				else
					newbon->CriticalHitChance[base2] += base1;
				break;
				
			case SE_SpellOnKill:
				for(int i = 0; i < MAX_SPELL_TRIGGER; i+=2)
				{
					if(!newbon->SpellOnKill[i])
					{
						// base1 = SpellID to be triggered, base2 = chance to fire
						newbon->SpellOnKill[i] = base1;
						newbon->SpellOnKill[i+1] = base2;
						break;
					}
				}
				break;
				
			case SE_SpellOnDeath:
				for(int i = 0; i < MAX_SPELL_TRIGGER; i+=2)
				{
					if(!newbon->SpellOnDeath[i])
					{
						// base1 = SpellID to be triggered, base2 = chance to fire
						newbon->SpellOnDeath[i] = base1;
						newbon->SpellOnDeath[i+1] = base2;
						break;
					}
				}
				break;
				
			case SE_CriticalDamageMob:
			{
				// base1 = effect value, base2 = skill restrictions(-1 for all)
				if(base2 == -1)
					newbon->CritDmgMob[HIGHEST_SKILL+1] += base1;
				else
					newbon->CritDmgMob[base2] += base1;
				break;
			}
			case SE_ReduceSkillTimer:
			{
				newbon->SkillReuseTime[base2] += base1;
				break;
			}
			case SE_Twinproc:
			{
				newbon->TwinProc += base1;
				break;
			}
			case SE_ResistFearChance:
			{
				if(base1 == 100) // If we reach 100% in a single spell/item then we should be immune to negative fear resist effects until our immunity is over
					newbon->Fearless = true;
					
				newbon->ResistFearChance += base1; // these should stack
				break;
			}
			case SE_Fearless:
			{
				newbon->Fearless = true;  
				break;
			}
			case SE_PersistantCasting:
			{
				newbon->PersistantCasting += base1;
				break;
			}
			case SE_DelayDeath:
			{
				newbon->DelayDeath += base1;
				break;
			}
		}
	}
}

void Mob::CalcSpellBonuses(StatBonuses* newbon)
{
	int i;

	memset(newbon, 0, sizeof(StatBonuses));
	newbon->AggroRange = -1;
	newbon->AssistRange = -1;

	uint32 buff_count = GetMaxTotalSlots();
	for(i = 0; i < buff_count; i++) {
		if(buffs[i].spellid != SPELL_UNKNOWN)
			ApplySpellsBonuses(buffs[i].spellid, buffs[i].casterlevel, newbon, buffs[i].casterid, false, buffs[i].ticsremaining);
	}
	
	//this prolly suffer from roundoff error slightly...
	newbon->AC = newbon->AC * 10 / 34;	//ratio determined impirically from client.
}

void Mob::ApplySpellsBonuses(int16 spell_id, int8 casterlevel, StatBonuses* newbon, int16 casterId, bool item_bonus, int32 ticsremaining)
{
	int i, effect_value;
	Mob *caster = NULL;

	if(!IsValidSpell(spell_id))
		return;
	
	if(casterId > 0)
		caster = entity_list.GetMob(casterId);
	
	for (i = 0; i < EFFECT_COUNT; i++)
	{
		if(IsBlankSpellEffect(spell_id, i))
			continue;

		effect_value = CalcSpellEffectValue(spell_id, i, casterlevel, caster, ticsremaining);

		switch (spells[spell_id].effectid[i])
		{
			case SE_CurrentHP: //regens
				if(effect_value > 0) {
					newbon->HPRegen += effect_value;
				}
				break;
			case SE_CurrentEndurance: 
				newbon->EnduranceRegen += effect_value;
				break;
		
			case SE_ChangeFrenzyRad:
			{
				// redundant to have level check here
				if(newbon->AggroRange == -1 || effect_value < newbon->AggroRange)
				{
					newbon->AggroRange = effect_value;
				}
				break;
			}

			case SE_Harmony:
			{
				// neotokyo: Harmony effect as buff - kinda tricky
				// harmony could stack with a lull spell, which has better aggro range
				// take the one with less range in any case
				if(newbon->AssistRange == -1 || effect_value < newbon->AssistRange)
				{
					newbon->AssistRange = effect_value;
				}
				break;
			}

			case SE_AttackSpeed:
			{
				if ((effect_value - 100) > 0) { // Haste
					if (newbon->haste < 0) break; // Slowed - Don't apply haste
					if ((effect_value - 100) > newbon->haste) {
						newbon->haste = effect_value - 100;
					}
				} else if ((effect_value - 100) < 0) { // Slow
					//Slow Mitigation works by taking the amount that would be slowed, and adding a multiplied version of the difference.
					int new_effect_value;
					float slow_amount_mitigated = 100 - effect_value; //Gives us a value that actually represents the slow amount.
					slow_amount_mitigated *= this->slow_mitigation;
					new_effect_value = effect_value + slow_amount_mitigated;
					if (new_effect_value > 100)
						new_effect_value = 100;
					if ((new_effect_value - 100) < newbon->haste) 
					{
						newbon->haste = new_effect_value - 100;
					}
				}
				break;
			}

 			case SE_AttackSpeed2:
			{
				if ((effect_value - 100) > 0) { // Haste V2 - Stacks with V1 but does not Overcap
					if ((effect_value - 100) > newbon->hastetype2) {
						newbon->hastetype2 = effect_value - 100;
					}
				}
				break;
 			}
 
 			case SE_AttackSpeed3:
 			{
				if (effect_value > 0) { // Haste V3 - Stacks and Overcaps
					if (effect_value > newbon->hastetype3) {
						newbon->hastetype3 = effect_value;
					}
				}
				break;
 			}

			case SE_TotalHP:
			{
				newbon->HP += effect_value;
				break;
			}
			
			case SE_CurrentMana:
			{
				newbon->ManaRegen += effect_value;
				break;
			}
			case SE_ManaPool:
			{
				newbon->Mana += effect_value;
				break;
			}

			case SE_Stamina:
			{
				newbon->EnduranceReduction += effect_value;
				break;
			}
			
			case SE_ArmorClass:
			{
				newbon->AC += effect_value;
				break;
			}

			case SE_ATK:
			{
				newbon->ATK += effect_value;
				break;
			}

			case SE_STR:
			{
				newbon->STR += effect_value;
				break;
			}

			case SE_DEX:
			{
				newbon->DEX += effect_value;
				break;
			}

			case SE_AGI:
			{
				newbon->AGI += effect_value;
				break;
			}

			case SE_STA:
			{
				newbon->STA += effect_value;
				break;
			}

			case SE_INT:
			{
				newbon->INT += effect_value;
				break;
			}

			case SE_WIS:
			{
				newbon->WIS += effect_value;
				break;
			}

			case SE_CHA:
			{
				if (spells[spell_id].base[i] != 0) {
					newbon->CHA += effect_value;
				}
				break;
			}

			case SE_AllStats:
			{
				newbon->STR += effect_value;
				newbon->DEX += effect_value;
				newbon->AGI += effect_value;
				newbon->STA += effect_value;
				newbon->INT += effect_value;
				newbon->WIS += effect_value;
				newbon->CHA += effect_value;
				break;
			}

			case SE_ResistFire:
			{
				newbon->FR += effect_value;
				break;
			}

			case SE_ResistCold:
			{
				newbon->CR += effect_value;
				break;
			}

			case SE_ResistPoison:
			{
				newbon->PR += effect_value;
				break;
			}

			case SE_ResistDisease:
			{
				newbon->DR += effect_value;
				break;
			}

			case SE_ResistMagic:
			{
				newbon->MR += effect_value;
				break;
			}

			case SE_ResistAll:
			{
				newbon->MR += effect_value;
				newbon->DR += effect_value;
				newbon->PR += effect_value;
				newbon->CR += effect_value;
				newbon->FR += effect_value;
				break;
			}

			case SE_ResistCorruption:
			{
				newbon->Corrup += effect_value;
				break;
			}
			
			case SE_RaiseStatCap:
			{
				switch(spells[spell_id].base2[i])
				{
					//are these #define'd somewhere?
					case 0: //str
						newbon->STRCapMod += effect_value;
						break;
					case 1: //sta
						newbon->STACapMod += effect_value;
						break;
					case 2: //agi
						newbon->AGICapMod += effect_value;
						break;
					case 3: //dex
						newbon->DEXCapMod += effect_value;
						break;
					case 4: //wis
						newbon->WISCapMod += effect_value;
						break;
					case 5: //int
						newbon->INTCapMod += effect_value;
						break;
					case 6: //cha
						newbon->CHACapMod += effect_value;
						break;
					case 7: //mr
						newbon->MRCapMod += effect_value;
						break;
					case 8: //cr
						newbon->CRCapMod += effect_value;
						break;
					case 9: //fr
						newbon->FRCapMod += effect_value;
						break;
					case 10: //pr
						newbon->PRCapMod += effect_value;
						break;
					case 11: //dr
						newbon->DRCapMod += effect_value;
						break;
					case 12: // corruption
						newbon->CorrupCapMod += effect_value;
						break;
				}
				break;
			}

			case SE_CastingLevel:	// Brilliance of Ro
			{
				newbon->effective_casting_level += effect_value;
				break;
			}

			case SE_MovementSpeed:
			{
				newbon->movementspeed += effect_value;
				break;
			}
			
			case SE_SpellDamageShield:
			{
				newbon->SpellDamageShield += effect_value;
				break;
			}
			
			case SE_DamageShield:
			{
				newbon->DamageShield += effect_value;
				newbon->DamageShieldSpellID = spell_id;
				newbon->DamageShieldType = GetDamageShieldType(spell_id);
				break;
			}
			
			case SE_ReverseDS:
			{
				newbon->ReverseDamageShield += effect_value;
				newbon->ReverseDamageShieldSpellID = spell_id;
				newbon->ReverseDamageShieldType = GetDamageShieldType(spell_id);
				break;
			}

			case SE_Reflect:
			{
				newbon->reflect_chance += effect_value;
				break;
			}

			case SE_SingingSkill:
			{
				if(effect_value > newbon->singingMod)
					newbon->singingMod = effect_value;
				break;
			}
			
			case SE_ChangeAggro:
			{
				newbon->hatemod += effect_value;
				break;
			}
			case SE_MeleeMitigation:
			{
				//for some reason... this value is negative for increased mitigation
				newbon->MeleeMitigation -= effect_value;
				break;
			}
			
			/*
				Assuming that none of these chances stack... they just pick the highest
				
				perhaps some smarter logic is needed here to handle the case
				where there is a chance increase and a decrease
				because right now, the increase will completely offset the decrease...
				
			*/
			case SE_CriticalHitChance:
			{
				// For items,(cleave I, cleave II, etc) only the highest value should apply 
				if(item_bonus) {
					if(spells[spell_id].base2[i] == -1 && newbon->CriticalHitChance[HIGHEST_SKILL+1] < effect_value)
						newbon->CriticalHitChance[HIGHEST_SKILL+1] = effect_value;
					else if(spells[spell_id].base2[i] != -1 && newbon->CriticalHitChance[spells[spell_id].base2[i]] < effect_value)
						newbon->CriticalHitChance[spells[spell_id].base2[i]] = effect_value;
				}
				// For spells, the effects can add so that minor effects(ie speed of ellowind) are useful.
				else {
					if(spells[spell_id].base2[i] == -1)
						newbon->CriticalHitChance[HIGHEST_SKILL+1] += effect_value;
					else
						newbon->CriticalHitChance[spells[spell_id].base2[i]] += effect_value;
				}
				break;
			}
				
			case SE_CrippBlowChance:
			{
				if(newbon->CrippBlowChance < effect_value)
					newbon->CrippBlowChance = effect_value;
				break;
			}
				
			case SE_AvoidMeleeChance:
			{
				//multiplier is to be compatible with item effects
				//watching for overflow too
				effect_value = effect_value<3000? effect_value * 10 : 30000;
				if(newbon->AvoidMeleeChance < effect_value)
					newbon->AvoidMeleeChance = effect_value;
				break;
			}
				
			case SE_RiposteChance:
			{
				if(newbon->RiposteChance < effect_value)
					newbon->RiposteChance = effect_value;
				break;
			}
				
			case SE_DodgeChance:
			{
				if(newbon->DodgeChance < effect_value)
					newbon->DodgeChance = effect_value;
				break;
			}
				
			case SE_ParryChance:
			{
				if(newbon->ParryChance < effect_value)
					newbon->ParryChance = effect_value;
				break;
			}
				
			case SE_DualWieldChance:
			{
				if(newbon->DualWieldChance < effect_value)
					newbon->DualWieldChance = effect_value;
				break;
			}
				
			case SE_DoubleAttackChance:
			{
				if(newbon->DoubleAttackChance < effect_value)
					newbon->DoubleAttackChance = effect_value;
				break;
			}
				
			case SE_MeleeLifetap:
			{
				if(newbon->MeleeLifetap < spells[spell_id].base[i])
					newbon->MeleeLifetap = spells[spell_id].base[i];
				break;
			}
				
			case SE_AllInstrumentMod:
			{
				if(effect_value > newbon->singingMod)
					newbon->singingMod = effect_value;
				if(effect_value > newbon->brassMod)
					newbon->brassMod = effect_value;
				if(effect_value > newbon->percussionMod)
					newbon->percussionMod = effect_value;
				if(effect_value > newbon->windMod)
					newbon->windMod = effect_value;
				if(effect_value > newbon->stringedMod)
					newbon->stringedMod = effect_value;
				break;
			}
				
			case SE_ResistSpellChance:
			{
				newbon->ResistSpellChance += effect_value;
				break;
			}	
			case SE_ResistFearChance:
			{
				if(effect_value == 100) // If we reach 100% in a single spell/item then we should be immune to negative fear resist effects until our immunity is over
					newbon->Fearless = true;
					
				newbon->ResistFearChance += effect_value; // these should stack
				break;
			}
			case SE_Fearless:
			{
				newbon->Fearless = true;  
				break;
			}
 			case SE_HundredHands:
 			{
				if(IsBeneficialSpell(spell_id)){ //If it's a beneficial spell we switch it cause
					effect_value *= -1; //of the way it's stored by sony, negative for both ben and det spells
				}
				effect_value = effect_value > 120 ? 120 : (effect_value < -120 ? -120 : effect_value);
				newbon->HundredHands = newbon->HundredHands > effect_value ? newbon->HundredHands : effect_value;
 				break;
 			}
				
			case SE_MeleeSkillCheck:
			{
				if(newbon->MeleeSkillCheck < effect_value) {
					newbon->MeleeSkillCheck = effect_value;
					newbon->MeleeSkillCheckSkill = spells[spell_id].base2[i]==-1?255:spells[spell_id].base2[i];
				}
				break;
			}
				
			case SE_HitChance:
			{
				//multiplier is to be compatible with item effects
				//watching for overflow too
				effect_value = effect_value<2000? effect_value * 15 : 30000;
				if(newbon->HitChance < effect_value) {
					newbon->HitChance = effect_value;
					newbon->HitChanceSkill = spells[spell_id].base2[i]==-1?255:spells[spell_id].base2[i];
				}
				break;
			}
				
			case SE_DamageModifier:
			{
				if(spells[spell_id].base2[i] == -1)
					newbon->DamageModifier[HIGHEST_SKILL+1] += effect_value;
				else
					newbon->DamageModifier[spells[spell_id].base2[i]] += effect_value;
				break;
			}
				
			case SE_MinDamageModifier:
			{
				if(newbon->MinDamageModifier < effect_value)
					newbon->MinDamageModifier = effect_value;
				break;
			}
				
			case SE_StunResist:
			{
				if(newbon->StunResist < effect_value)
					newbon->StunResist = effect_value;
				break;
			}
				
			case SE_ProcChance:
			{
				//multiplier is to be compatible with item effects
				//watching for overflow too
				effect_value = effect_value<3000? effect_value : 3000;
				if(newbon->ProcChance < effect_value)
					newbon->ProcChance = effect_value;
				break;
			}
				
			case SE_ExtraAttackChance:
			{
				if(newbon->ExtraAttackChance < effect_value)
					newbon->ExtraAttackChance = effect_value;
				break;
			}
			case SE_PercentXPIncrease:
			{
				if(newbon->XPRateMod < effect_value)
					newbon->XPRateMod = effect_value;
				break;
			}
			case SE_DivineSave:
			{
				if(newbon->DivineSaveChance < effect_value)
				{
					newbon->DivineSaveChance = effect_value;
					SetDeathSaveChance(true);
				}
				break;
			}
			case SE_Flurry:
			{
				if(newbon->FlurryChance < effect_value)
					newbon->FlurryChance = effect_value;
				break;
			}
			case SE_Accuracy:
			{
				if(newbon->Accuracy < effect_value)
					newbon->Accuracy = effect_value;
				break;
			}
			case SE_MaxHPChange:
			{
				newbon->MaxHPChange += effect_value;
				break;
			}
			case SE_EndurancePool:
			{
				newbon->Endurance += effect_value;
				break;
			}
			case SE_HealRate:
			{
				newbon->HealRate += effect_value;
				break;
			}
			case SE_SkillDamageTaken:
			{
				if(spells[spell_id].base2[i] == -1)
					newbon->SkillDmgTaken[HIGHEST_SKILL+1] += effect_value;
				else
					newbon->SkillDmgTaken[spells[spell_id].base2[i]] += effect_value;
				break;
			}
			case SE_SpellDamage:
			{
				// Only used for worn effects and to trigger a buff check
				newbon->SpellDmg += effect_value;
				break;
			}
			
			case SE_TriggerOnCast:
			{
				for(int e = 0; e < MAX_SPELL_TRIGGER; e++)
				{
					if(!newbon->SpellTriggers[e])
					{
						newbon->SpellTriggers[e] = spell_id;
						break;
					}
				}
				break;
			}
			case SE_SpellCritChance:
				newbon->CriticalSpellChance += effect_value;
				break;
			
			case SE_CriticalSpellChance:
				newbon->CriticalSpellChance += effect_value;
				newbon->SpellCritDmgIncrease += spells[spell_id].base2[i];
				break;

			case SE_SpellCritDmgIncrease:
				newbon->SpellCritDmgIncrease += effect_value;
				break;

			case SE_CriticalHealChance:
				newbon->CriticalHealChance += effect_value;
				break;
			
			case SE_CriticalHealOverTime:
				newbon->CriticalHealOverTime += effect_value;
				break;
				
			case SE_MitigateDamageShield:
				newbon->DSMitigation += effect_value;
				break;
			
			case SE_CriticalDoTChance:
				newbon->CriticalDoTChance += effect_value;
				break;
				
			case SE_SpellOnKill:
			{
				for(int e = 0; e < MAX_SPELL_TRIGGER; e+=2)
				{
					if(!newbon->SpellOnKill[e])
					{
						// Base2 = Spell to fire | Base1 = % chance
						newbon->SpellOnKill[e] = spells[spell_id].base2[i];
						newbon->SpellOnKill[e+1] = effect_value;
						break;
					}
				}
				break;
			}
			
			case SE_SpellOnDeath:
			{
				for(int e = 0; e < MAX_SPELL_TRIGGER; e+=2)
				{
					if(!newbon->SpellOnDeath[e])
					{
						// Base2 = Spell to fire | Base1 = % chance
						newbon->SpellOnDeath[e] = spells[spell_id].base2[i];
						newbon->SpellOnDeath[e+1] = effect_value;
						break;
					}
				}
				break;
			}
			case SE_CriticalDamageMob:
			{
				if(spells[spell_id].base2[i] == -1)
					newbon->CritDmgMob[HIGHEST_SKILL+1] += effect_value;
				else
					newbon->CritDmgMob[spells[spell_id].base2[i]] += effect_value;
				break;
			}
			case SE_ReduceSkillTimer:
			{
				if(newbon->SkillReuseTime[spells[spell_id].base2[i]] < effect_value)
					newbon->SkillReuseTime[spells[spell_id].base2[i]] = effect_value;
				break;
			}
			case SE_SkillDamageAmount:
			{
				if(spells[spell_id].base2[i] == -1)
					newbon->SkillDamageAmount[HIGHEST_SKILL+1] += effect_value;
				else
					newbon->SkillDamageAmount[spells[spell_id].base2[i]] += effect_value;
				break;
			}
			case SE_Twinproc:
			{
				newbon->TwinProc += effect_value;
				break;
			}
			case SE_GravityEffect:
			{
				newbon->GravityEffect = 1;
				break;
			}
			case SE_AntiGate:
			{
				newbon->AntiGate = true;
				break;
			}
			case SE_MagicWeapon:
			{
				newbon->MagicWeapon = true;
				break;
			}
			case SE_IncreaseBlockChance:
			{
				newbon->IncreaseBlockChance += effect_value;
				break;
			}
			case SE_PersistantCasting:
			{
				newbon->PersistantCasting += effect_value;
				break;
			}
			case SE_LimitHPPercent:
			{
				if(newbon->HPPercCap != 0 && newbon->HPPercCap > effect_value)
					newbon->HPPercCap = effect_value;
				else if(newbon->HPPercCap == 0)
					newbon->HPPercCap = effect_value;
					
				break;
			}
			case SE_LimitManaPercent:
			{
				if(newbon->ManaPercCap != 0 && newbon->ManaPercCap > effect_value)
					newbon->ManaPercCap = effect_value;
				else if(newbon->ManaPercCap == 0)
					newbon->ManaPercCap = effect_value;

				break;
			}
			case SE_LimitEndPercent:
			{
				if(newbon->EndPercCap != 0 && newbon->EndPercCap > effect_value)
					newbon->EndPercCap = effect_value;
				else if(newbon->EndPercCap == 0)
					newbon->EndPercCap = effect_value;

				break;
			}
			case SE_BlockNextSpellFocus:
			{
				newbon->BlockNextSpell = true;
				break;
			}
			case SE_BlockSpellEffect:
			{
				newbon->BlockNextSpell = true;
				for(int e = 0; e < EFFECT_COUNT; e++)
				{
					if(!newbon->BlockSpellEffect[e])
					{
						newbon->BlockSpellEffect[e] = spells[spell_id].base2[i];
						break;
					}
				}
				break;
			}
			case SE_ImmuneFleeing:
			{
				newbon->ImmuneToFlee = true;
				break;
			}
			case SE_DelayDeath:
			{
				newbon->DelayDeath += effect_value;
				break;
			}
		}
	}
}

void NPC::CalcItemBonuses(StatBonuses *newbon)
{
	if(newbon){

		for(int i = 0; i < MAX_WORN_INVENTORY; i++){
			const Item_Struct *cur = database.GetItem(equipment[i]);
			if(cur){
				//basic stats
				newbon->AC += cur->AC;
				newbon->HP += cur->HP;
				newbon->Mana += cur->Mana;
				newbon->Endurance += cur->Endur;
				newbon->STR += (cur->AStr + cur->HeroicStr);
				newbon->STA += (cur->ASta + cur->HeroicSta);
				newbon->DEX += (cur->ADex + cur->HeroicDex);
				newbon->AGI += (cur->AAgi + cur->HeroicAgi);
				newbon->INT += (cur->AInt + cur->HeroicInt);
				newbon->WIS += (cur->AWis + cur->HeroicWis);
				newbon->CHA += (cur->ACha + cur->HeroicCha);
				newbon->MR += (cur->MR + cur->HeroicMR);
				newbon->FR += (cur->FR + cur->HeroicFR);
				newbon->CR += (cur->CR + cur->HeroicCR);
				newbon->PR += (cur->PR + cur->HeroicPR);
				newbon->DR += (cur->DR + cur->HeroicDR);
				newbon->Corrup += (cur->SVCorruption + cur->HeroicSVCorrup);

				//more complex stats
				if(cur->Regen > 0) {
					newbon->HPRegen += cur->Regen;
				}
				if(cur->ManaRegen > 0) {
					newbon->ManaRegen += cur->ManaRegen;
				}
				if(cur->Attack > 0) {
					newbon->ATK += cur->Attack;
				}
				if(cur->DamageShield > 0) {
					newbon->DamageShield += cur->DamageShield;
				}
				if(cur->SpellShield > 0) {
					newbon->SpellDamageShield += cur->SpellShield;
				}
				if(cur->Shielding > 0) {
					newbon->MeleeMitigation += cur->Shielding;
				}
				if(cur->StunResist > 0) {
					newbon->StunResist += cur->StunResist;
				}
				if(cur->StrikeThrough > 0) {
					newbon->StrikeThrough += cur->StrikeThrough;
				}
				if(cur->Avoidance > 0) {
					newbon->AvoidMeleeChance += cur->Avoidance;
				}
				if(cur->Accuracy > 0) {
					newbon->HitChance += cur->Accuracy;
				}
				if(cur->CombatEffects > 0) {
					newbon->ProcChance += cur->CombatEffects;
				}
				if (cur->Worn.Effect>0 && (cur->Worn.Type == ET_WornEffect)) { // latent effects
					ApplySpellsBonuses(cur->Worn.Effect, cur->Worn.Level, newbon);
				}
				if (cur->Haste > newbon->haste)
					newbon->haste = cur->Haste;
			}
		}
	
	}
}

void Client::CalcItemScale(bool login)
{
	bool changed = false;

	if(CalcItemScale(0, 21, login))
		changed = true;

	if(CalcItemScale(22, 30, login))
		changed = true;

	if(CalcItemScale(251, 341, login))
		changed = true;

	if(CalcItemScale(400, 405, login))
		changed = true;

	//Power Source Slot
	if (GetClientVersion() >= EQClientSoF)
	{
		if(CalcItemScale(9999, 10000, login))
			changed = true;
	}

	if(changed)
	{
		CalcBonuses();
	}
}

bool Client::CalcItemScale(int32 slot_x, int32 slot_y, bool login)
{
	bool changed = false;
	int i;
	for (i = slot_x; i < slot_y; i++) {
		const ItemInst* inst = m_inv[i];
		if(inst == 0)
			continue;
		
		bool update_slot = false;
		if(inst->IsScaling())
		{
			EvoItemInst* e_inst = (EvoItemInst*)inst;
			uint16 oldexp = e_inst->GetExp();

			if(login) {
                parse->EventItem(EVENT_ITEM_ENTERZONE, this, e_inst, e_inst->GetID(), 0);
            }
            parse->EventItem(EVENT_SCALE_CALC, this, e_inst, e_inst->GetID(), 0);

			if (e_inst->GetExp() != oldexp) {	// if the scaling factor changed, rescale the item and update the client
				e_inst->ScaleItem();
				changed = true;
				update_slot = true;
			}
		}

		//iterate all augments
		for(int x = 0; x < MAX_AUGMENT_SLOTS; ++x) 
		{
			ItemInst * a_inst = inst->GetAugment(x);
			if(!a_inst)
				continue;

			if(a_inst->IsScaling())
			{
				EvoItemInst* e_inst = (EvoItemInst*)a_inst;
				uint16 oldexp = e_inst->GetExp();

                if(login) {
                    parse->EventItem(EVENT_ITEM_ENTERZONE, this, e_inst, e_inst->GetID(), 0);
                }
                parse->EventItem(EVENT_SCALE_CALC, this, e_inst, e_inst->GetID(), 0);

				if (e_inst->GetExp() != oldexp) 
				{
					e_inst->ScaleItem();
					changed = true;
					update_slot = true;
				}
			}
		}

		if(update_slot)
		{
			SendItemPacket(i, inst, ItemPacketCharmUpdate);
		}
	}
	return changed;
}
