#ifdef BOTS

#include "bot.h"

bool Bot::AICastSpell(Mob* tar, int8 iChance, int16 iSpellTypes) {
	_ZP(Bot_AICastSpell);

	if (!tar) {
		return false;
	}

	if(!AI_HasSpells())
		return false;

	if(tar->GetAppearance() == eaDead) {
		if(tar->IsClient() && tar->CastToClient()->GetFeigned()) {
			// do nothing
		}
		else {
			return false;
		}
	}

	if (iChance < 100) {
		if (MakeRandomInt(0, 100) > iChance){
			return false;
		}
	}

	int8 botClass = GetClass();
	uint8 botLevel = GetLevel();

	bool checked_los = false;	//we do not check LOS until we are absolutely sure we need to, and we only do it once.

	bool castedSpell = false;

	BotSpell botSpell;
	botSpell.SpellId = 0;
	botSpell.SpellIndex = 0;
	botSpell.ManaCost = 0;

	switch (iSpellTypes) {
		case SpellType_Mez: {
			if (tar->GetBodyType() != BT_Giant) {
					if(!checked_los) {
						if(!CheckLosFN(tar))
							break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
						
						checked_los = true;
					}
					
					//TODO
					//Check if single target or AoE mez is best
					//if (TARGETS ON MT IS => 3 THEN botSpell = AoEMez)
					//if (TARGETS ON MT IS <= 2 THEN botSpell = BestMez)

					botSpell = GetBestBotSpellForMez(this);

					if(botSpell.SpellId == 0)
						break;

					Mob* addMob = GetFirstIncomingMobToMez(this, botSpell);

					if(!addMob){
						//Say("!addMob.");
						break;}

					if(!(!addMob->IsImmuneToSpell(botSpell.SpellId, this) && addMob->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0))
						break;
					
					castedSpell = AIDoSpellCast(botSpell.SpellIndex, addMob, botSpell.ManaCost);

			}
			break;
		}
		case SpellType_Heal: {
			if (tar->DontHealMeBefore() < Timer::GetCurrentTime()) {
				int8 hpr = (int8)tar->GetHPRatio();
				bool hasAggro = false;

				if(hpr < 95 || (tar->IsClient() && (hpr < 95)) || (botClass == BARD)) {
					if(tar->GetClass() == NECROMANCER) {
						// Give necromancers a chance to go lifetap something or cleric can spend too much mana on a necro
						if(hpr >= 40) {
							break;
						}
					}

					if(tar->FindType(SE_HealOverTime)) {
						// Let the heal over time buff do it's thing ...
						if(tar->IsEngaged() && hpr >= 85)
							break;
						else if(!tar->IsEngaged())
							break;
					}

					// Evaluate the situation
					if((IsEngaged()) && ((botClass == CLERIC) || (botClass == DRUID) || (botClass == SHAMAN) || (botClass == PALADIN))) {
						if(IsEngaged()) {
							if(tar->GetTarget() && tar->GetTarget()->GetHateTop() && tar->GetTarget()->GetHateTop() == tar) {
								hasAggro = true;
							}
						}

						if(hpr < 35) {
							botSpell = GetBestBotSpellForFastHeal(this);
						}
						else if(hpr >= 35 && hpr < 70){
							if(GetNumberNeedingHealedInGroup(60, false) >= 3)
								botSpell = GetBestBotSpellForGroupHeal(this);

							if(botSpell.SpellId == 0)
								botSpell = GetBestBotSpellForPercentageHeal(this);
						}
						else if(hpr >= 70 && hpr < 90){
							if(GetNumberNeedingHealedInGroup(80, false) >= 3)
								botSpell = GetBestBotSpellForGroupHealOverTime(this);

							if(hasAggro)
								botSpell = GetBestBotSpellForPercentageHeal(this);
						}
						else
							botSpell = GetBestBotSpellForHealOverTime(this);
					}
					else if ((botClass == CLERIC) || (botClass == DRUID) || (botClass == SHAMAN) || (botClass == PALADIN)) {
						if(GetNumberNeedingHealedInGroup(40, true) >= 2){
							botSpell = GetBestBotSpellForGroupCompleteHeal(this);

							if(botSpell.SpellId == 0)
								botSpell = GetBestBotSpellForGroupHeal(this);

							if(botSpell.SpellId == 0)
								botSpell = GetBestBotSpellForGroupHealOverTime(this);

							if(hpr < 40) {
								if(botSpell.SpellId == 0)
									botSpell = GetBestBotSpellForPercentageHeal(this);
							}
						}
						else if(GetNumberNeedingHealedInGroup(60, true) >= 2){
							botSpell = GetBestBotSpellForGroupHeal(this);

							if(botSpell.SpellId == 0)
								botSpell = GetBestBotSpellForGroupHealOverTime(this);

							if(hpr < 40) {
								if(botSpell.SpellId == 0)
									botSpell = GetBestBotSpellForPercentageHeal(this);
							}
						}
						else if(hpr < 40)
							botSpell = GetBestBotSpellForPercentageHeal(this);
						else if(hpr >= 40 && hpr < 75)
							botSpell = GetBestBotSpellForRegularSingleTargetHeal(this);
						else
							botSpell = GetBestBotSpellForHealOverTime(this);
					}	
			
					if(botSpell.SpellId == 0)
						botSpell = GetBestBotSpellForRegularSingleTargetHeal(this);

					if(botSpell.SpellId == 0)
						botSpell = GetFirstBotSpellForSingleTargetHeal(this);

					if(botSpell.SpellId == 0 && botClass == BARD){
						botSpell = GetFirstBotSpellBySpellType(this, SpellType_Heal);
					}

					// If there is still no spell id, then there isn't going to be one so we are done
					if(botSpell.SpellId == 0)
						break;

					// Can we cast this spell on this target?
					if(!(spells[botSpell.SpellId].targettype==ST_GroupTeleport || spells[botSpell.SpellId].targettype == ST_Target || tar == this)
						&& !(tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0))
						break;

					int32 TempDontHealMeBeforeTime = tar->DontHealMeBefore();

					castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost, &TempDontHealMeBeforeTime);

					if(castedSpell) {
						/*if(TempDontHealMeBeforeTime != tar->DontHealMeBefore())
							tar->SetDontHealMeBefore(TempDontHealMeBeforeTime);

						// For non-HoT heals, do a 4 second delay
						// TODO: Replace this code with logic that calculates the delay based on number of clerics in rotation
						//			and ignores heals for anyone except the main tank
						if(!IsHealOverTimeSpell(botSpell.SpellId)) {
							if(IsCompleteHealSpell(botSpell.SpellId)) { 
								// Complete Heal 4 second rotation
								tar->SetDontHealMeBefore(Timer::GetCurrentTime() + 4000);
							}
							else {
								tar->SetDontHealMeBefore(Timer::GetCurrentTime() + 1000);
							}
						}*/
						if(botClass != BARD) {
							if(IsGroupSpell(botSpell.SpellId)){
								Group *g;
		
								if(this->HasGroup()) {
									Group *g = this->GetGroup();

									if(g) {
										for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
											if(g->members[i] && !g->members[i]->qglobal) {
												g->members[i]->SetDontHealMeBefore(Timer::GetCurrentTime() + 1000);
											}
										}
									}
								}
							}
							else {
								tar->SetDontHealMeBefore(Timer::GetCurrentTime() + 2000);
							}
						}
					}
				}
			}
			break;
		}
		case SpellType_Root: {
			if (!tar->IsRooted() && tar->DontRootMeBefore() < Timer::GetCurrentTime()) {
					if(!checked_los) {
						if(!CheckLosFN(tar))
							break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
						
						checked_los = true;
					}

					// TODO: If there is a ranger in the group then don't allow root spells

					botSpell = GetFirstBotSpellBySpellType(this, iSpellTypes);

					if(botSpell.SpellId == 0)
						break;

					if(tar->CanBuffStack(botSpell.SpellId, botLevel, true) == 0)
						break;

					int32 TempDontRootMeBefore = tar->DontRootMeBefore();
					
					castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost, &TempDontRootMeBefore);
					
					if(TempDontRootMeBefore != tar->DontRootMeBefore())
						tar->SetDontRootMeBefore(TempDontRootMeBefore);
			}
			break;
		}
		case SpellType_Buff: {
			if (tar->DontBuffMeBefore() < Timer::GetCurrentTime()) {
				std::list<BotSpell> buffSpellList = GetBotSpellsBySpellType(this, SpellType_Buff);

				for(std::list<BotSpell>::iterator itr = buffSpellList.begin(); itr != buffSpellList.end(); itr++) {
					BotSpell selectedBotSpell = *itr;

					if(selectedBotSpell.SpellId == 0)
						continue;

					// no buffs with illusions.. use #bot command to cast illusions
					if(IsEffectInSpell(selectedBotSpell.SpellId, SE_Illusion))
						continue;

					//no teleport spells use #bot command to cast teleports
					if(IsEffectInSpell(selectedBotSpell.SpellId, SE_Teleport) || IsEffectInSpell(selectedBotSpell.SpellId, SE_Succor))
						continue;

					// can not cast buffs for your own pet only on another pet that isn't yours
					if((spells[selectedBotSpell.SpellId].targettype == ST_Pet) && (tar != this->GetPet()))
						continue;

					// Validate target

					if(!((spells[selectedBotSpell.SpellId].targettype == ST_Target || spells[selectedBotSpell.SpellId].targettype == ST_Pet || tar == this || 
						spells[selectedBotSpell.SpellId].targettype == ST_Group || spells[selectedBotSpell.SpellId].targettype == ST_GroupTeleport ||
						(botClass == BARD && spells[selectedBotSpell.SpellId].targettype == ST_AEBard))
						&& !tar->IsImmuneToSpell(selectedBotSpell.SpellId, this)
						&& (tar->CanBuffStack(selectedBotSpell.SpellId, botLevel, true) >= 0))) {
							continue;
					}

					// Put the zone levitate and movement check here since bots are able to bypass the client casting check
					if((IsEffectInSpell(selectedBotSpell.SpellId, SE_Levitate) && !zone->CanLevitate())
						|| (IsEffectInSpell(selectedBotSpell.SpellId, SE_MovementSpeed) && !zone->CanCastOutdoor())) {
							continue;
					}

					switch(tar->GetArchetype())
					{
						case ARCHETYPE_CASTER:
							//TODO: probably more caster specific spell effects in here
							if(IsEffectInSpell(selectedBotSpell.SpellId, SE_AttackSpeed) || IsEffectInSpell(selectedBotSpell.SpellId, SE_ATK) ||
								 IsEffectInSpell(selectedBotSpell.SpellId, SE_STR) || IsEffectInSpell(selectedBotSpell.SpellId, SE_ReverseDS))
							{
								continue;
							}
							break;
						case ARCHETYPE_MELEE:
							if(IsEffectInSpell(selectedBotSpell.SpellId, SE_IncreaseSpellHaste) || IsEffectInSpell(selectedBotSpell.SpellId, SE_ManaPool) ||
								IsEffectInSpell(selectedBotSpell.SpellId, SE_CastingLevel) || IsEffectInSpell(selectedBotSpell.SpellId, SE_ManaRegen_v2) ||
								IsEffectInSpell(selectedBotSpell.SpellId, SE_CurrentMana))
							{
								continue;
							}
							break;
						case ARCHETYPE_HYBRID:
							//Hybrids get all buffs
						default:
							break;
					}

					if(botClass == ENCHANTER && this->GetManaRatio() <= 75.0f && IsEffectInSpell(selectedBotSpell.SpellId, SE_Rune))
					{
						//If we're at 75% mana or below, don't rune as enchanter
						break;
					}

					if(CheckSpellRecastTimers(this, itr->SpellIndex))
					{

						int32 TempDontBuffMeBefore = tar->DontBuffMeBefore();

						castedSpell = AIDoSpellCast(selectedBotSpell.SpellIndex, tar, selectedBotSpell.ManaCost, &TempDontBuffMeBefore);

						if(TempDontBuffMeBefore != tar->DontBuffMeBefore())
							tar->SetDontBuffMeBefore(TempDontBuffMeBefore);
					}

					if(castedSpell)
						break;
				}
			}
			break;
		}
		case SpellType_Escape: {
			int8 hpr = (int8)GetHPRatio();
#ifdef IPC          
			if (hpr <= 5 || (IsNPC() && CastToNPC()->IsInteractive() && tar != this) )
#else
			if ((hpr <= 15) && (tar == this))
#endif
			{
				botSpell = GetFirstBotSpellBySpellType(this, iSpellTypes);

				if(botSpell.SpellId == 0)
					break;

				castedSpell = AIDoSpellCast(botSpell.SpellIndex, this, botSpell.ManaCost);
			}
			break;
		}
		case SpellType_Nuke: {
			if((tar->GetHPRatio() <= 95.0f) || ((botClass == BARD) || (botClass == SHAMAN) || (botClass == ENCHANTER)))
			{
				if(!checked_los) {
					if(!CheckLosFN(tar))
						break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
					
					checked_los = true;
				}

				if(botClass == CLERIC && this->GetManaRatio() <= 75.0f)
				{
					//If we're at 75% mana or below, don't nuke as a cleric or 50% as enchanter
					break;
				}
				if(botClass == ENCHANTER && this->GetManaRatio() <= 50.0f)
				{
					//If we're at 75% mana or below, don't nuke as a cleric or 50% as enchanter
					break;
				}

				if(botClass == MAGICIAN || botClass == SHADOWKNIGHT || botClass == NECROMANCER || botClass == PALADIN || botClass == RANGER || botClass == DRUID || botClass == CLERIC) {
					if(tar->GetBodyType() == BT_Undead || tar->GetBodyType() == BT_SummonedUndead || tar->GetBodyType() == BT_Vampire)
						botSpell = GetBestBotSpellForNukeByTargetType(this, ST_Undead);
					else if(tar->GetBodyType() == BT_Summoned || tar->GetBodyType() == BT_Summoned2 || tar->GetBodyType() == BT_Summoned3)
						botSpell = GetBestBotSpellForNukeByTargetType(this, ST_Summoned);
				}
				else if(botClass == WIZARD) {
					botSpell = GetBestBotWizardNukeSpellByTargetResists(this, tar);
				}

				if(botClass == PALADIN || botClass == DRUID || botClass == CLERIC || botClass == ENCHANTER) {
					if(botSpell.SpellId == 0) {
						int8 stunChance = (tar->IsCasting() ? 30: 15);

						if(!tar->IsStunned() && ( botClass == PALADIN || (MakeRandomInt(1, 100) <= stunChance))) {
							botSpell = GetBestBotSpellForStunByTargetType(this, ST_Target);
						}
					}
				}

				if(botClass == WIZARD && botSpell.SpellId == 0) {
					botSpell = GetBestBotWizardNukeSpellByTargetResists(this, tar);
				}

				if(botSpell.SpellId == 0)
					botSpell = GetBestBotSpellForNukeByTargetType(this, ST_Target);

				if(botSpell.SpellId == 0)
					break;

				if(!(!tar->IsImmuneToSpell(botSpell.SpellId, this) && (tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0)))
					break;
				
				if(IsFearSpell(botSpell.SpellId)) { 
					// don't let fear cast if the npc isn't snared or rooted
					if(tar->GetSnaredAmount() == -1) {
						if(!tar->IsRooted())
							break;
					}
				}

				castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
			}
			break;
		}
		case SpellType_Dispel: {
			if(tar->GetHPRatio() > 95.0f) {
				if(!checked_los) {
					if(!CheckLosFN(tar))
						break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
					
					checked_los = true;
				}
				
				botSpell = GetFirstBotSpellBySpellType(this, iSpellTypes);

				if(botSpell.SpellId == 0)
					break;

				// TODO: Check target to see if there is anything to dispel

				if(tar->CountDispellableBuffs() > 0) {
					castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
				}
			}
			break;
		}
		case SpellType_Pet: {
			//keep mobs from recasting pets when they have them.
			if (!IsPet() && !GetPetID() && !IsBotCharmer()) {
				if(botClass == MAGICIAN)
					botSpell = GetBestBotMagicianPetSpell(this);
				else
					botSpell = GetFirstBotSpellBySpellType(this, SpellType_Pet);

				if(botSpell.SpellId == 0)
					break;
				
				castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
			}
			break;
		}
		case SpellType_Lifetap: {
			if (GetHPRatio() < 90.0f) {
				if(!checked_los) {
					if(!CheckLosFN(tar))
						break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
					
					checked_los = true;
				}

				botSpell = GetFirstBotSpellBySpellType(this, iSpellTypes);

				if(botSpell.SpellId == 0)
					break;

				if(!(!tar->IsImmuneToSpell(botSpell.SpellId, this) && (tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0)))
					break;

				castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
			}
			break;
		}
		case SpellType_Snare: {
			if (tar->DontSnareMeBefore() < Timer::GetCurrentTime()) {
					if(!checked_los) {
						if(!CheckLosFN(tar))
							break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
						
						checked_los = true;
					}

					botSpell = GetFirstBotSpellBySpellType(this, iSpellTypes);

					if(botSpell.SpellId == 0)
						break;

					if(!(!tar->IsImmuneToSpell(botSpell.SpellId, this) && tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0))
						break;

					int32 TempDontSnareMeBefore = tar->DontSnareMeBefore();
					
					castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost, &TempDontSnareMeBefore);
					
					if(TempDontSnareMeBefore != tar->DontSnareMeBefore())
						tar->SetDontSnareMeBefore(TempDontSnareMeBefore);
			}
			break;
		}
		case SpellType_DOT: {
			if ((tar->GetHPRatio() <= 98.0f) && (tar->DontDotMeBefore() < Timer::GetCurrentTime()) && (tar->GetHPRatio() > 15.0f)) {
				if(!checked_los) {
					if(!CheckLosFN(tar))
						break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call

					checked_los = true;
				}

				std::list<BotSpell> dotList = GetBotSpellsBySpellType(this, SpellType_DOT);

				const int maxDotSelect = 5;
				int dotSelectCounter = 0;

				for(list<BotSpell>::iterator itr = dotList.begin(); itr != dotList.end(); itr++) {
					BotSpell selectedBotSpell = *itr;

					if(selectedBotSpell.SpellId == 0)
						continue;

					if(CheckSpellRecastTimers(this, itr->SpellIndex))
					{

						if(!(!tar->IsImmuneToSpell(selectedBotSpell.SpellId, this) && tar->CanBuffStack(selectedBotSpell.SpellId, botLevel, true) >= 0))
							continue;

						int32 TempDontDotMeBefore = tar->DontDotMeBefore();

						castedSpell = AIDoSpellCast(selectedBotSpell.SpellIndex, tar, selectedBotSpell.ManaCost, &TempDontDotMeBefore);

						if(TempDontDotMeBefore != tar->DontDotMeBefore())
							tar->SetDontDotMeBefore(TempDontDotMeBefore);
					}

					dotSelectCounter++;

					if((dotSelectCounter == maxDotSelect) || castedSpell)
						break;
				}
			}
			break;
		}
		case SpellType_Slow: {
			if (tar->GetHPRatio() <= 99.0f) {
				bool GroupHasEnchanter = false;
				bool GroupHasShaman = false;

					if(!checked_los) {
						if(!CheckLosFN(tar))
							break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
						
						checked_los = true;
					}
					
					if(botClass == SHAMAN && tar->GetDR() < tar->GetMR() && ((tar->GetMR() - tar->GetDR()) > 75)) {
						botSpell = GetBestBotSpellForDiseaseBasedSlow(this);
						GroupHasShaman = true;
					}
					else if (botClass == ENCHANTER){
						//Checking no adds before slow/debuff
						Mob* addMob = GetFirstIncomingMobToMez(this, botSpell);

						if(addMob){
							break;}
						else {
 						botSpell = GetBestBotSpellForMagicBasedSlow(this);
 						GroupHasEnchanter = true;
						}
					}
					else if (botClass == BEASTLORD && GroupHasEnchanter == false && GroupHasShaman == false){
						botSpell = GetBestBotSpellForDiseaseBasedSlow(this);
					}

					if(botSpell.SpellId == 0)
						break;

					if(!(!tar->IsImmuneToSpell(botSpell.SpellId, this) && tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0))
						break;
					
					castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
			}
			break;
		}
		case SpellType_Debuff: {
			if((tar->GetHPRatio() <= 99.0f) || ((botClass == BARD) || (botClass == SHAMAN) || (botClass == ENCHANTER) || (botClass == DRUID)) && (tar->GetHPRatio() > 25.0f))
			{
				if(!checked_los) {
					if(!CheckLosFN(tar))
						break;	//cannot see target... we assume that no spell is going to work since we will only be casting detrimental spells in this call
					
					checked_los = true;
				}
				//if enchanter checking if no adds
				if (botClass == ENCHANTER){
					Mob* addMob = GetFirstIncomingMobToMez(this, botSpell);
 
					if(addMob){
						break;}
				}

				botSpell = GetDebuffBotSpell(this, tar);

				if(botSpell.SpellId == 0)
					break;

				if(!(!tar->IsImmuneToSpell(botSpell.SpellId, this) && (tar->CanBuffStack(botSpell.SpellId, botLevel, true) >= 0)))
					break;

				castedSpell = AIDoSpellCast(botSpell.SpellIndex, tar, botSpell.ManaCost);
			}
			break;
		}
	}
	return castedSpell;
}

bool Bot::AIDoSpellCast(int8 i, Mob* tar, sint32 mana_cost, int32* oDontDoAgainBefore) {
	bool result = false;

	// manacost has special values, -1 is no mana cost, -2 is instant cast (no mana)
	sint32 manaCost = mana_cost;

	if (manaCost == -1)
		manaCost = spells[AIspells[i].spellid].mana;
	else if (manaCost == -2)
		manaCost = 0;

	sint32 extraMana = 0;
	sint32 hasMana = GetMana();

	// Allow bots to cast buff spells even if they are out of mana
	if(RuleB(Bots, BotFinishBuffing)) {
		if(manaCost > hasMana) {
			// Let's have the bots complete the buff time process
			if(AIspells[i].type & SpellType_Buff) {
				extraMana = manaCost - hasMana;
				SetMana(manaCost);
			}
		}
	}

	float dist2 = 0;

	if (AIspells[i].type & SpellType_Escape) {
		dist2 = 0; 
	} else 
		dist2 = DistNoRoot(*tar);

	if (((((spells[AIspells[i].spellid].targettype==ST_GroupTeleport && AIspells[i].type==2)
				|| spells[AIspells[i].spellid].targettype==ST_AECaster
				|| spells[AIspells[i].spellid].targettype==ST_Group
				|| spells[AIspells[i].spellid].targettype==ST_AEBard)
				&& dist2 <= spells[AIspells[i].spellid].aoerange*spells[AIspells[i].spellid].aoerange)
				|| dist2 <= spells[AIspells[i].spellid].range*spells[AIspells[i].spellid].range) && (mana_cost <= GetMana() || GetMana() == GetMaxMana()))
	{
		result = NPC::AIDoSpellCast(i, tar, mana_cost, oDontDoAgainBefore);

		if(IsCasting() && IsSitting())
			Stand();
	}

	// if the spell wasn't casted, then take back any extra mana that was given to the bot to cast that spell
	if(!result) {
		SetMana(hasMana);
		extraMana = false;
	}
	else {  //handle spell recast and recast timers
		if(GetClass() == BARD && IsGroupSpell(AIspells[i].spellid)) {
			AIspells[i].time_cancast = (spells[AIspells[i].spellid].recast_time > (spells[AIspells[i].spellid].buffduration * 6000)) ? Timer::GetCurrentTime() + spells[AIspells[i].spellid].recast_time : Timer::GetCurrentTime() + spells[AIspells[i].spellid].buffduration * 6000;
			//spellend_timer.Start(spells[AIspells[i].spellid].cast_time);
		}
		else
			AIspells[i].time_cancast = Timer::GetCurrentTime() + spells[AIspells[i].spellid].recast_time;
		if(spells[AIspells[i].spellid].EndurTimerIndex > 0) {
			SetSpellRecastTimer(spells[AIspells[i].spellid].EndurTimerIndex, spells[AIspells[i].spellid].recast_time);
		}
	}

	return result;
}

bool Bot::AI_PursueCastCheck() {
	bool result = false;
	
	if (AIautocastspell_timer->Check(false)) {
		_ZP(Bot_AI_Process_pursue_cast);
		
		AIautocastspell_timer->Disable();	//prevent the timer from going off AGAIN while we are casting.
		
		mlog(AI__SPELLS, "Bot Engaged (pursuing) autocast check triggered. Trying to cast offensive spells.");
		
		if(!AICastSpell(GetTarget(), 100, SpellType_Snare)) {
			if(!AICastSpell(GetTarget(), 100, SpellType_Lifetap)) {
				if(!AICastSpell(GetTarget(), 100, SpellType_Nuke)) {
					/*AIautocastspell_timer->Start(RandomTimer(500, 2000), false);
					result = true;*/
					result = true;
				}

				result = true;
			}

			result = true;
		} 
		
		if(!AIautocastspell_timer->Enabled())
			AIautocastspell_timer->Start(RandomTimer(100, 250), false);
	}
	
	return result;
}

bool Bot::AI_IdleCastCheck() {
	bool result = false;

	if (AIautocastspell_timer->Check(false)) {
		_ZP(Bot_AI_IdleCastCheck);
#if MobAI_DEBUG_Spells >= 25
		cout << "Non-Engaged autocast check triggered: " << this->GetCleanName() << endl;
#endif
		AIautocastspell_timer->Disable();	//prevent the timer from going off AGAIN while we are casting.

		//Ok, IdleCastCheck depends of class. 
		// Healers WITHOUT pets will check if a heal is needed before buffing.
		int8 botClass = GetClass();
		
		if(botClass == CLERIC || botClass == PALADIN || botClass == RANGER) {
			if (!AICastSpell(this, 100, SpellType_Buff)) {
				if (!AICastSpell(this, 100, SpellType_Heal)) {
					if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Heal)) {
						if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Buff)) {
							//
						}
					}
				}
			}

			result = true;
		}
		// Pets class will first cast their pet, then buffs
		else if(botClass == DRUID || botClass == MAGICIAN || botClass == SHADOWKNIGHT || botClass == SHAMAN || botClass == NECROMANCER || botClass == ENCHANTER || botClass == BEASTLORD  || botClass == WIZARD) {			
			if (!AICastSpell(this, 100, SpellType_Pet)) {
				if (!AICastSpell(this, 100, SpellType_Buff)) {
					if (!AICastSpell(this, 100, SpellType_Heal)) {
						if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Heal)) {
							if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
								if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Buff)) {
									//
								}
							}
						}
					}
				}
			}

			result = true;
		}		
		else if(botClass == BARD) {
			// bard bots
			if(!AICastSpell(this, 100, SpellType_Heal)) {
				if(!AICastSpell(this, 100, SpellType_Buff)) {
					//	
				}
			}
			
			result = true;
		}

		if(!AIautocastspell_timer->Enabled())
			AIautocastspell_timer->Start(RandomTimer(1000, 5000), false);
	}
	
	return result;
}

bool Bot::AI_EngagedCastCheck() {
	bool result = false;

	if (GetTarget() && AIautocastspell_timer->Check(false)) {
		_ZP(Bot_AI_Process_engaged_cast);

		AIautocastspell_timer->Disable();	//prevent the timer from going off AGAIN while we are casting.

		int8 botClass = GetClass();
		uint8 botLevel = GetLevel();

		mlog(AI__SPELLS, "Engaged autocast check triggered (BOTS). Trying to cast healing spells then maybe offensive spells.");



		if(botClass == CLERIC) {
			if(!AICastSpell(this, 100, SpellType_Heal)) {
				if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Heal)) {
					if(!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
						//AIautocastspell_timer->Start(RandomTimer(100, 250), false);		// Do not give healer classes a lot of time off or your tank's die
					}
				}
			}

			result = true;
		}
		else if(botClass == DRUID) {
			if (!AICastSpell(GetTarget(), 50, SpellType_DOT)) {
				if(!AICastSpell(this, 100, SpellType_Heal)) {
					if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Heal)) {
						if (!AICastSpell(GetTarget(), 25, SpellType_Debuff)) {
							if(!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
								//AIautocastspell_timer->Start(RandomTimer(100, 250), false);		// Do not give healer classes a lot of time off or your tank's die
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == SHAMAN) {
			if (!AICastSpell(GetTarget(), 100, SpellType_Slow)) {
				if (!AICastSpell(GetTarget(), 25, SpellType_Debuff)) {
					if(!AICastSpell(this, 100, SpellType_Heal)) {
						if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
							if(!entity_list.Bot_AICheckCloseBeneficialSpells(this, 100, BotAISpellRange, SpellType_Heal)) {
								if (!AICastSpell(this, 100, SpellType_Pet)) {
									if (!AICastSpell(GetTarget(), 25, SpellType_DOT)) {
										if(!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
											//AIautocastspell_timer->Start(RandomTimer(100, 250), false);		// Do not give healer classes a lot of time off or your tank's die
										}
									}
								}
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == RANGER) {
			if (!AICastSpell(GetTarget(), 25, SpellType_DOT)) {
				if (!AICastSpell(this, 100, SpellType_Heal)) {
					if (!entity_list.Bot_AICheckCloseBeneficialSpells(this, 10, BotAISpellRange, SpellType_Heal)) {
						if (!AICastSpell(GetTarget(), 15, SpellType_Nuke)) {
							//
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == BEASTLORD) {
			if (!AICastSpell(GetTarget(), 100, SpellType_Slow)) {
				if (!AICastSpell(this, 100, SpellType_Pet)) {
					if (!AICastSpell(GetTarget(), 25, SpellType_DOT)) {
						if (!AICastSpell(this, 50, SpellType_Heal)) {
							if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
								if (!entity_list.Bot_AICheckCloseBeneficialSpells(this, 10, BotAISpellRange, SpellType_Heal)) {
									if (!AICastSpell(GetTarget(), 15, SpellType_Debuff)) {
										if(!AICastSpell(GetTarget(), 15, SpellType_Nuke)) {
											//
										}
									}
								}
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == WIZARD) {
			if (!AICastSpell(GetTarget(), 100, SpellType_Nuke)) {
				if(!AICastSpell(GetTarget(), 60, SpellType_Escape)) {
					//
				}
			}
		}
		else if(botClass == PALADIN) {
			if (!AICastSpell(this, 100, SpellType_Heal)) {
				if (!entity_list.Bot_AICheckCloseBeneficialSpells(this, 15, BotAISpellRange, SpellType_Heal)) {
					if (!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
						//
					}
				}
			}

			result = true;
		}
		else if(botClass == SHADOWKNIGHT) {
			if (!AICastSpell(this, 100, SpellType_Pet)) {
				if (!AICastSpell(GetTarget(), 100, SpellType_Lifetap)) {
					if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
						if (!AICastSpell(GetTarget(), 25, SpellType_DOT)) {
							if (!AICastSpell(GetTarget(), 25, SpellType_Debuff)) {
								if (!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
									//
								}
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == MAGICIAN) {
			if (!AICastSpell(this, 100, SpellType_Pet)) {
				if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
					if (!AICastSpell(GetTarget(), 25, SpellType_Debuff)) {
						if (!AICastSpell(GetTarget(), 100, SpellType_Nuke)) {
							//
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == NECROMANCER) {
			if (!AICastSpell(this, 100, SpellType_Pet)) {
				if (!AICastSpell(GetTarget(), 100, SpellType_Lifetap)) {
					if (!AICastSpell(GetTarget(), 50, SpellType_DOT)) {
						if (!AICastSpell(GetTarget(), 50, SpellType_Debuff)) {
							if (!AICastSpell(GetPet(), 100, SpellType_Heal)) {
								if (!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
									//
								}
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == ENCHANTER) {
			if (!AICastSpell(GetTarget(), 100, SpellType_Mez)) {
				if (!AICastSpell(GetTarget(), 100, SpellType_Slow)) {
					if (!AICastSpell(GetTarget(), 50, SpellType_Debuff)) {
						if (!AICastSpell(this, 100, SpellType_Pet)) {
							if (!AICastSpell(GetTarget(), 25, SpellType_DOT)) {
								if (!AICastSpell(GetTarget(), 25, SpellType_Nuke)) {
									if(!AICastSpell(GetTarget(), 50, SpellType_Escape)) {
										//
									}
								}
							}
						}
					}
				}
			}

			result = true;
		}
		else if(botClass == BARD) {
			if(!AICastSpell(this, 100, SpellType_Buff)) {
				if(!AICastSpell(this, 50, SpellType_Heal)) {
					if(!AICastSpell(GetTarget(), 50, SpellType_Dispel)) {// Bards will use their debuff songs
						if(!AICastSpell(GetTarget(), 50, SpellType_Nuke)) {// Bards will use their debuff songs
							if(!AICastSpell(GetTarget(), 100, SpellType_Escape)) {// Bards will use their debuff songs
								//
							}
						}
					}	
				}
			}

			result = true;
		}

		if(!AIautocastspell_timer->Enabled()) {
			AIautocastspell_timer->Start(RandomTimer(100, 250), false);
		}
	}

	return result;
}

std::list<BotSpell> Bot::GetBotSpellsForSpellEffect(Bot* botCaster, int spellEffect) {
	std::list<BotSpell> result;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsEffectInSpell(botSpellList[i].spellid, spellEffect)) {
				BotSpell botSpell;
				botSpell.SpellId = botSpellList[i].spellid;
				botSpell.SpellIndex = i;
				botSpell.ManaCost = botSpellList[i].manacost;

				result.push_back(botSpell);
			}
		}
	}

	return result;
}

std::list<BotSpell> Bot::GetBotSpellsForSpellEffectAndTargetType(Bot* botCaster, int spellEffect, SpellTargetType targetType) {
	std::list<BotSpell> result;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsEffectInSpell(botSpellList[i].spellid, spellEffect)) {
				if(spells[botSpellList[i].spellid].targettype == targetType) {
					BotSpell botSpell;
					botSpell.SpellId = botSpellList[i].spellid;
					botSpell.SpellIndex = i;
					botSpell.ManaCost = botSpellList[i].manacost;

					result.push_back(botSpell);
				}
			}
		}
	}

	return result;
}

std::list<BotSpell> Bot::GetBotSpellsBySpellType(Bot* botCaster, int16 spellType) {
	std::list<BotSpell> result;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(botSpellList[i].type & spellType) {
				BotSpell botSpell;
				botSpell.SpellId = botSpellList[i].spellid;
				botSpell.SpellIndex = i;
				botSpell.ManaCost = botSpellList[i].manacost;

				result.push_back(botSpell);
			}
		}
	}

	return result;
}

BotSpell Bot::GetFirstBotSpellBySpellType(Bot* botCaster, int16 spellType) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((botSpellList[i].type & spellType) && CheckSpellRecastTimers(botCaster, i)) {
				result.SpellId = botSpellList[i].spellid;
				result.SpellIndex = i;
				result.ManaCost = botSpellList[i].manacost;
				
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForFastHeal(Bot *botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_CurrentHP);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsFastHealSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;

				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForHealOverTime(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_HealOverTime);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsHealOverTimeSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;

				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForPercentageHeal(Bot *botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsCompleteHealSpell(botSpellList[i].spellid) && CheckSpellRecastTimers(botCaster, i)) {
				result.SpellId = botSpellList[i].spellid;
				result.SpellIndex = i;
				result.ManaCost = botSpellList[i].manacost;
				
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForRegularSingleTargetHeal(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_CurrentHP);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsRegularSingleTargetHealSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;

				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetFirstBotSpellForSingleTargetHeal(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_CurrentHP);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if((IsRegularSingleTargetHealSpell(botSpellListItr->SpellId) || IsFastHealSpell(botSpellListItr->SpellId)) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;

				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForGroupHeal(Bot* botCaster) {
        BotSpell result;
        
        result.SpellId = 0;
        result.SpellIndex = 0;
        result.ManaCost = 0;

        if(botCaster) {
                std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_CurrentHP);

				for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
					// Assuming all the spells have been loaded into this list by level and in descending order
					if(IsRegularGroupHealSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
						result.SpellId = botSpellListItr->SpellId;
						result.SpellIndex = botSpellListItr->SpellIndex;
						result.ManaCost = botSpellListItr->ManaCost;
						
						break;
					}
				}
        }

        return result;
}

BotSpell Bot::GetBestBotSpellForGroupHealOverTime(Bot* botCaster) {
        BotSpell result;
        
        result.SpellId = 0;
        result.SpellIndex = 0;
        result.ManaCost = 0;

        if(botCaster) {
                std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_HealOverTime);

				for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
					// Assuming all the spells have been loaded into this list by level and in descending order
					if(IsGroupHealOverTimeSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
						result.SpellId = botSpellListItr->SpellId;
						result.SpellIndex = botSpellListItr->SpellIndex;
						result.ManaCost = botSpellListItr->ManaCost;
						
						break;
					}
				}
        }

        return result;
}

BotSpell Bot::GetBestBotSpellForGroupCompleteHeal(Bot* botCaster) {
        BotSpell result;
        
        result.SpellId = 0;
        result.SpellIndex = 0;
        result.ManaCost = 0;

        if(botCaster) {
                std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_CompleteHeal);

				for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
					// Assuming all the spells have been loaded into this list by level and in descending order
					if(IsGroupCompleteHealSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
						result.SpellId = botSpellListItr->SpellId;
						result.SpellIndex = botSpellListItr->SpellIndex;
						result.ManaCost = botSpellListItr->ManaCost;
						
						break;
					}
				}
        }

	return result;
}

BotSpell Bot::GetBestBotSpellForMez(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_Mez);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsMezSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;
				
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForMagicBasedSlow(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_AttackSpeed);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsSlowSpell(botSpellListItr->SpellId) && spells[botSpellListItr->SpellId].resisttype == RESIST_MAGIC && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;
				
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForDiseaseBasedSlow(Bot* botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_AttackSpeed);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsSlowSpell(botSpellListItr->SpellId) && spells[botSpellListItr->SpellId].resisttype == RESIST_DISEASE && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;
				
				break;
			}
		}
	}

	return result;
}

Mob* Bot::GetFirstIncomingMobToMez(Bot* botCaster, BotSpell botSpell) {
	Mob* result = 0;

	if(botCaster && IsMezSpell(botSpell.SpellId)) {

		std::list<NPC*> npc_list;
		entity_list.GetNPCList(npc_list);
		
		for(std::list<NPC*>::iterator itr = npc_list.begin(); itr != npc_list.end(); itr++) {
			NPC* npc = *itr;

			if(npc->DistNoRootNoZ(*botCaster) <= spells[botSpell.SpellId].range) {
				if(!npc->IsMezzed()) {
					if(botCaster->HasGroup()) {
						Group* g = botCaster->GetGroup();

						if(g) {
							for(int counter = 0; counter < g->GroupCount(); counter++) {
								if(npc->IsOnHatelist(g->members[counter]) && g->members[counter]->GetTarget() != npc && g->members[counter]->IsEngaged()) {
									result = npc;
									break;
								}
							}
						}
					}
				}
			}

			if(result)
				break;
		}
	}

	return result;
}

BotSpell Bot::GetBestBotMagicianPetSpell(Bot *botCaster) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffect(botCaster, SE_SummonPet);

		std::string petType = GetBotMagicianPetType(botCaster);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsSummonPetSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				if(!strncmp(spells[botSpellListItr->SpellId].teleport_zone, petType.c_str(), petType.length())) {
					result.SpellId = botSpellListItr->SpellId;
					result.SpellIndex = botSpellListItr->SpellIndex;
					result.ManaCost = botSpellListItr->ManaCost;

					break;
				}
			}
		}
	}

	return result;
}

std::string Bot::GetBotMagicianPetType(Bot* botCaster) {
	std::string result;

	if(botCaster) {
		if(botCaster->IsPetChooser()) {
			switch(botCaster->GetPetChooserID()) {
				case 0:
					result = std::string("SumWater");
					break;
				case 1:
					result = std::string("SumFire");
					break;
				case 2:
					result = std::string("SumAir");
					break;
				case 3:
					result = std::string("SumEarth");
					break;
				default:
					result = std::string("MonsterSum");
					break;
			}
		}
		else {
			if(botCaster->GetLevel() == 2)
				result = std::string("SumWater");
			else if(botCaster->GetLevel() == 3)
				result = std::string("SumFire");
			else if(botCaster->GetLevel() == 4)
				result = std::string("SumAir");
			else if(botCaster->GetLevel() == 5)
				result = std::string("SumEarth");
			else if(botCaster->GetLevel() < 30) {
				// Under level 30
				int counter = MakeRandomInt(0, 3);
				
				switch(counter) {
					case 0:
					result = std::string("SumWater");
					break;
				case 1:
					result = std::string("SumFire");
					break;
				case 2:
					result = std::string("SumAir");
					break;
				case 3:
					result = std::string("SumEarth");
					break;
				default:
					result = std::string("MonsterSum");
					break;
				}
			}
			else {
				// Over level 30
				int counter = MakeRandomInt(0, 4);
				
				switch(counter) {
					case 0:
					result = std::string("SumWater");
					break;
				case 1:
					result = std::string("SumFire");
					break;
				case 2:
					result = std::string("SumAir");
					break;
				case 3:
					result = std::string("SumEarth");
					break;
				default:
					result = std::string("MonsterSum");
					break;
				}
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForNukeByTargetType(Bot* botCaster, SpellTargetType targetType) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster) {
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffectAndTargetType(botCaster, SE_CurrentHP, targetType);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsPureNukeSpell(botSpellListItr->SpellId) && IsDamageSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;
				
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotSpellForStunByTargetType(Bot* botCaster, SpellTargetType targetType)
{
	BotSpell result;

	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster)
	{
		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffectAndTargetType(botCaster, SE_Stun, targetType);

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++)
		{
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsStunSpell(botSpellListItr->SpellId) && CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex))
			{
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;
				break;
			}
		}
	}

	return result;
}

BotSpell Bot::GetBestBotWizardNukeSpellByTargetResists(Bot* botCaster, Mob* target) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster && target) {
		const int lureResisValue = -100;
		const int maxTargetResistValue = 300;
		bool selectLureNuke = false;

		if((target->GetMR() > maxTargetResistValue) && (target->GetCR() > maxTargetResistValue) && (target->GetFR() > maxTargetResistValue))
			selectLureNuke = true;
				

		std::list<BotSpell> botSpellList = GetBotSpellsForSpellEffectAndTargetType(botCaster, SE_CurrentHP, ST_Target);

		BotSpell firstWizardMagicNukeSpellFound;
		firstWizardMagicNukeSpellFound.SpellId = 0;
		firstWizardMagicNukeSpellFound.SpellIndex = 0;
		firstWizardMagicNukeSpellFound.ManaCost = 0;

		for(std::list<BotSpell>::iterator botSpellListItr = botSpellList.begin(); botSpellListItr != botSpellList.end(); botSpellListItr++) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			bool spellSelected = false;

			if(CheckSpellRecastTimers(botCaster, botSpellListItr->SpellIndex)) {
				if(selectLureNuke && (spells[botSpellListItr->SpellId].ResistDiff < lureResisValue)) {
					spellSelected = true;
				}
				else if(IsPureNukeSpell(botSpellListItr->SpellId)) {
					if(((target->GetMR() < target->GetCR()) || (target->GetMR() < target->GetFR())) && (GetSpellResistType(botSpellListItr->SpellId) == RESIST_MAGIC) 
						&& (spells[botSpellListItr->SpellId].ResistDiff > lureResisValue))
					{
						spellSelected = true;
					}
					else if(((target->GetCR() < target->GetMR()) || (target->GetCR() < target->GetFR())) && (GetSpellResistType(botSpellListItr->SpellId) == RESIST_COLD) 
						&& (spells[botSpellListItr->SpellId].ResistDiff > lureResisValue)) 
					{
						spellSelected = true;
					}
					else if(((target->GetFR() < target->GetCR()) || (target->GetFR() < target->GetMR())) && (GetSpellResistType(botSpellListItr->SpellId) == RESIST_FIRE)
						&& (spells[botSpellListItr->SpellId].ResistDiff > lureResisValue)) 
					{
						spellSelected = true;
					}
					else if((GetSpellResistType(botSpellListItr->SpellId) == RESIST_MAGIC) && (spells[botSpellListItr->SpellId].ResistDiff > lureResisValue) && !IsStunSpell(botSpellListItr->SpellId)) {
						firstWizardMagicNukeSpellFound.SpellId = botSpellListItr->SpellId;
						firstWizardMagicNukeSpellFound.SpellIndex = botSpellListItr->SpellIndex;
						firstWizardMagicNukeSpellFound.ManaCost = botSpellListItr->ManaCost;
					}
				}
			}

			if(spellSelected) {
				result.SpellId = botSpellListItr->SpellId;
				result.SpellIndex = botSpellListItr->SpellIndex;
				result.ManaCost = botSpellListItr->ManaCost;

				break;
			}
		}

		if(result.SpellId == 0) {
			result = firstWizardMagicNukeSpellFound;
		}
	}

	return result;
}

BotSpell Bot::GetDebuffBotSpell(Bot* botCaster, Mob *tar) {
	BotSpell result;
	
	result.SpellId = 0;
	result.SpellIndex = 0;
	result.ManaCost = 0;

	if(botCaster && botCaster->AI_HasSpells()) {
		std::vector<AISpells_Struct> botSpellList = botCaster->GetBotSpells();

		for (int i = botSpellList.size() - 1; i >= 0; i--) {
			if (botSpellList[i].spellid <= 0 || botSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((IsDebuffSpell(botSpellList[i].spellid) || (botSpellList[i].type & SpellType_Debuff)) && (!tar->IsImmuneToSpell(botSpellList[i].spellid, botCaster) 
				&& tar->CanBuffStack(botSpellList[i].spellid, botCaster->GetLevel(), true) >= 0) 
				&& CheckSpellRecastTimers(botCaster, i) && !(botSpellList[i].type & SpellType_Dispel)) {
				result.SpellId = botSpellList[i].spellid;
				result.SpellIndex = i;
				result.ManaCost = botSpellList[i].manacost;
				
				break;
			}
		}
	}

	return result;
}

void Bot::SetSpellRecastTimer(int timer_index, sint32 recast_delay) {
	if(timer_index > 0 && timer_index <= MaxSpellTimer) {
		spellRecastTimers[timer_index - 1] = Timer::GetCurrentTime() + recast_delay;
	}
}

sint32 Bot::GetSpellRecastTimer(Bot *caster, int timer_index) {
	sint32 result = 0;
	if(caster) {
		if(timer_index > 0 && timer_index <= MaxSpellTimer) {
			result = caster->spellRecastTimers[timer_index - 1];
		}
	}
	return result;
}

bool Bot::CheckSpellRecastTimers(Bot *caster, int SpellIndex) {
	if(caster) {
		if(caster->AIspells[SpellIndex].time_cancast < Timer::GetCurrentTime()) {  //checks spell recast
			if(GetSpellRecastTimer(caster, spells[caster->AIspells[SpellIndex].spellid].EndurTimerIndex) < Timer::GetCurrentTime()) {   //checks for spells on the same timer
				return true;    //can cast spell
			}
		}
	}
	return false;
}

#endif
