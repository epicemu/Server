/*  EQEMu:  Everquest Server Emulator
    Copyright (C) 2001-2002  EQEMu Development Team (http://eqemu.org)

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
#ifndef MOB_H
#define MOB_H

#include "features.h"

/* solar: macros for IsAttackAllowed, IsBeneficialAllowed */
#define _CLIENT(x) (x && x->IsClient() && !x->CastToClient()->IsBecomeNPC())
#define _NPC(x) (x && x->IsNPC() && !x->CastToMob()->GetOwnerID())
#define _BECOMENPC(x) (x && x->IsClient() && x->CastToClient()->IsBecomeNPC())
#define _CLIENTCORPSE(x) (x && x->IsCorpse() && x->CastToCorpse()->IsPlayerCorpse() && !x->CastToCorpse()->IsBecomeNPCCorpse())
#define _NPCCORPSE(x) (x && x->IsCorpse() && (x->CastToCorpse()->IsNPCCorpse() || x->CastToCorpse()->IsBecomeNPCCorpse()))
#define _CLIENTPET(x) (x && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsClient())
#define _NPCPET(x) (x && x->IsNPC() && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsNPC())
#define _BECOMENPCPET(x) (x && x->CastToMob()->GetOwner() && x->CastToMob()->GetOwner()->IsClient() && x->CastToMob()->GetOwner()->CastToClient()->IsBecomeNPC())

//Spell specialization parameters, not sure of a better place for them
#define SPECIALIZE_FIZZLE 11		//% fizzle chance reduce at 200 specialized
#define SPECIALIZE_MANA_REDUCE 12	//% mana cost reduction at 200 specialized

#define MAX_SHIELDERS 2		//I dont know if this is based on a client limit

#define APPEAR_VISIBLE	0x0003
#define APPEAR_ANIM		0x000e
#define APPEAR_SNEAK	0x000f
#define	APPEAR_AFK		0x0018
#define	APPEAR_ANON		0x0015
#define	APPEAR_SPLIT	0x001c
#define APPEAR_HEIGHT	0x001d
#define	APPEAR_HP_TIC	0x0011

#define ARCHETYPE_HYBRID	1
#define ARCHETYPE_CASTER	2
#define ARCHETYPE_MELEE		3

#define CON_GREEN		2
#define CON_LIGHTBLUE	18
#define CON_BLUE		4
#define CON_WHITE		20
#define CON_YELLOW		15
#define CON_RED			13

//LOS Parameters:
#define HEAD_POSITION 0.9f	//ratio of GetSize() where NPCs see from
#define SEE_POSITION 0.5f	//ratio of GetSize() where NPCs try to see for LOS
#define CHECK_LOS_STEP 1.0f

#include "../common/logsys.h"
#include "entity.h"
#include "spdat.h"
#include "event_codes.h"
#include "hate_list.h"
#include "../common/Kaiyodo-LList.h"
#include "../common/skills.h"
#include "../common/bodytypes.h"
#include "../common/MiscFunctions.h"
#include "pathing.h"
#include "map.h"
#include <set>
#include <vector>
#include <string>


typedef enum {	//focus types
	focusSpellHaste = 1,
	focusSpellDuration,
	focusRange,
	focusReagentCost,
	focusManaCost,
	focusImprovedHeal,
	focusImprovedDamage,
	focusImprovedDOT,		//i dont know about this...
	focusImprovedCritical,
	focusImprovedUndeadDamage,
	focusPetPower,
	focusResistRate,
	focusSpellHateMod,
	focusTriggerOnCast,
	focusSpellVulnerability,
	focusTwincast,
	focusSympatheticProc,
	focusSpellDamage,
	focusSpellDurByTic,
	focusSwarmPetDuration,
	focusReduceRecastTime,
	focusBlockNextSpell,
} focusType;

/*
Used:
b,d,f,g,j,m,n,o,p,r,t
A,B,C,D,E,F,G,H,I,J,L,M,N,O,Q,R,S,T,U,W,Y

Unused:
a,c,e,h,i,k,l,q,s,u,v,w,x,y,z
K,P,V,X,Z
*/

enum {
	SPECATK_NONE = 0,
	SPECATK_SUMMON,				// S
	SPECATK_ENRAGE,				// E
	SPECATK_RAMPAGE,			// R
	SPECATK_AREA_RAMPAGE,		// r
	SPECATK_FLURRY,				// F
	SPECATK_TRIPLE,				// T
	SPECATK_QUAD,				// Q
	SPECATK_INNATE_DW,			// L
	SPECATK_BANE,				// b
	SPECATK_MAGICAL,			// m
	SPECATK_RANGED_ATK,			// Y
	UNSLOWABLE,					// U
	UNMEZABLE,					// M
	UNCHARMABLE,				// C
	UNSTUNABLE,					// N
	UNSNAREABLE,				// I
	UNFEARABLE,					// D
	IMMUNE_MELEE,				// A
	IMMUNE_MAGIC,				// B
	IMMUNE_FLEEING,				// f
	IMMUNE_MELEE_EXCEPT_BANE,	// O
	IMMUNE_MELEE_NONMAGICAL,	// W
	IMMUNE_AGGRO,				// H - Won't aggro, ever.
	IMMUNE_AGGRO_ON,			// G - Immune to being aggroed
	IMMUNE_CASTING_FROM_RANGE,	// g
	IMMUNE_FEIGN_DEATH,			// d
	NPC_TUNNELVISION,			// t
	NPC_NO_BUFFHEAL_FRIENDS,	// n
	IMMUNE_PACIFY,				// p
	LEASH,						// J - Dispell, wipe agro && return to spawn
	TETHER,						// j - Return to spawn
	DESTRUCTIBLE_OBJECT,		// o - This is only for destructible objects
	NO_HARM_FROM_CLIENT,			// Z - This is to prevent attacking NPC's period for clients
	SPECATK_MAXNUM
};

typedef enum {	//fear states
	fearStateNotFeared = 0,
	fearStateRunning,		//I am running, hoping to find a grid at my WP
	fearStateRunningForever,	//can run straight until spell ends
	fearStateGrid,			//I am allready on a fear grid
	fearStateStuck			//I cannot move somehow...
} FearState;

enum { FlyMode0 = 0, FlyMode1 = 1, Flymode2 = 2, FlyMode3 = 3 };

struct TradeEntity;
class Trade;
enum TradeState {
	TradeNone,
	Trading,
	TradeAccepted,
	TradeCompleting
};

//this is our internal representation of the BUFF struct, can put whatever we want in it
struct Buffs_Struct {
	uint16	spellid;
	uint8	casterlevel;
	uint16	casterid;		// Maybe change this to a pointer sometime, but gotta make sure it's 0'd when it no longer points to anything
	char    caster_name[64];
    sint32	ticsremaining;
	uint32  counters;
	uint32	numhits; //the number of physical hits this buff can take before it fades away, lots of druid armor spells take advantage of this mixed with powerful effects
	uint32	melee_rune;
	uint32	magic_rune;
	int8	deathSaveSuccessChance;
	int8	deathsaveCasterAARank;
    bool	persistant_buff;
    bool	client;  //True if the caster is a client
	bool	UpdateClient;
};

struct StatBonuses {
	sint16	AC;
	sint32	HP;
	sint32	HPRegen;
	sint32	MaxHP;
	sint32	ManaRegen;
	sint32	EnduranceRegen;
	sint32	Mana;
	sint32	Endurance;
	sint16	ATK;
	//would it be worth it to create a Stat_Struct?
	sint16	STR;
	sint16	STRCapMod;
	sint16	HeroicSTR;
	sint16	STA;
	sint16	STACapMod;
	sint16	HeroicSTA;
	sint16	DEX;
	sint16	DEXCapMod;
	sint16	HeroicDEX;
	sint16	AGI;
	sint16	AGICapMod;
	sint16	HeroicAGI;
	sint16	INT;
	sint16	INTCapMod;
	sint16	HeroicINT;
	sint16	WIS;
	sint16	WISCapMod;
	sint16	HeroicWIS;
	sint16	CHA;
	sint16	CHACapMod;
	sint16	HeroicCHA;
	sint16	MR;
	sint16	MRCapMod;
	sint16	HeroicMR;
	sint16	FR;
	sint16	FRCapMod;
	sint16	HeroicFR;
	sint16	CR;
	sint16	CRCapMod;
	sint16	HeroicCR;
	sint16	PR;
	sint16	PRCapMod;
	sint16	HeroicPR;
	sint16	DR;
	sint16	DRCapMod;
	sint16	HeroicDR;
	sint16	Corrup;
	sint16	CorrupCapMod;
	sint16	HeroicCorrup;
	uint16	DamageShieldSpellID;
	int		DamageShield;						// this is damage done to mobs that attack this
	DmgShieldType	DamageShieldType;
	int		SpellDamageShield;
	int		SpellShield;
	int		ReverseDamageShield; 				// this is damage done to the mob when it attacks
	uint16	ReverseDamageShieldSpellID;
	DmgShieldType	ReverseDamageShieldType;
	int		movementspeed;
	sint16	haste;
	sint16	hastetype2;
	sint16	hastetype3;
	float	AggroRange;							// when calculate just replace original value with this
	float	AssistRange;
	sint16	skillmod[HIGHEST_SKILL+1];
	int		effective_casting_level;
	int		reflect_chance;						// chance to reflect incoming spell
	int16	singingMod;
	int16	brassMod;
	int16	percussionMod;
	int16	windMod;
	int16	stringedMod;
	sint8	hatemod;
	sint32	EnduranceReduction;

	sint16  StrikeThrough;						// PoP: Strike Through %
	sint16 	MeleeMitigation;						//i = Shielding
	sint16 	CriticalHitChance[HIGHEST_SKILL+2];	//i
	sint16 	CriticalSpellChance;					//i
	sint16 	SpellCritDmgIncrease;				//i
	sint16 	CriticalHealChance;					//i
	sint16 	CriticalHealOverTime;				//i
	sint16 	CriticalDoTChance;					//i
	sint16 	CrippBlowChance;						//
	sint16 	AvoidMeleeChance;					//AvoidMeleeChance/10 == % chance i = Avoidance
	sint16 	RiposteChance;						//i
	sint16 	DodgeChance;							//i
	sint16 	ParryChance;							//i
	sint16 	DualWieldChance;						//i
	sint16 	DoubleAttackChance;					//i
	sint16 	ResistSpellChance;					//i
	sint16 	ResistFearChance;					//i
	bool   	Fearless;							//i
	sint16 	StunResist;							//i
	sint16 	MeleeSkillCheck;						//i
	uint8  	MeleeSkillCheckSkill;
	sint16 	HitChance;							//HitChance/15 == % increase i = Accuracy
	uint8  	HitChanceSkill;
	sint16 	DamageModifier[HIGHEST_SKILL+2];		//i
	sint16 	MinDamageModifier;   				//i
	sint16 	ProcChance;							// ProcChance/10 == % increase i = CombatEffects
	sint16 	ExtraAttackChance;
	sint16	DoTShielding;
	sint16 	DivineSaveChance;					// Second Chance
	sint16 	FlurryChance;
	sint16	Accuracy;							// Works like HitChance but on all skills	
	sint8	HundredHands;							//extra haste, stacks with all other haste  i
	sint8	MeleeLifetap;							//i
	sint16 	HealRate;							// Spell effect that influences effectiveness of heals
	sint16 	MaxHPChange;							// Spell Effect
	sint16 	SkillDmgTaken[HIGHEST_SKILL+2];		// All Skills + -1
	sint32 	HealAmt;								// Item Effect
	sint32 	SpellDmg;							// Item Effect
	sint32 	Clairvoyance;						// Item Effect
	sint16 	DSMitigation;						// Item Effect
	uint32 	SpellTriggers[MAX_SPELL_TRIGGER];	// Innate/Spell/Item Spells that trigger when you cast
	uint32 	SpellOnKill[MAX_SPELL_TRIGGER];		// Chance to proc after killing a mob
	uint32 	SpellOnDeath[MAX_SPELL_TRIGGER];		// Chance to have effect cast when you die
	sint16 	CritDmgMob[HIGHEST_SKILL+2];			// All Skills + -1
	sint16 	SkillReuseTime[HIGHEST_SKILL+1];		// Reduces skill timers
	sint16 	SkillDamageAmount[HIGHEST_SKILL+2];	// All Skills + -1
	uint16 	TwinProc;							// Proc twice
	uint16 	ItemManaRegenCap;					// Increases the amount of mana you have can over the cap(aa effect)
	sint16 	GravityEffect;						// Indictor of spell effect
	bool	AntiGate;							// spell effect that prevents gating
	bool	MagicWeapon;						// spell effect that makes weapon magical
	sint16	IncreaseBlockChance;				// overall block chance modifier
	uint16	PersistantCasting;					// chance to continue casting through a stun
	int 	XPRateMod;							//i
	int		HPPercCap;							//Spell effect that limits you to being healed/regening beyond a % of your max
	int		ManaPercCap;						// ^^
	int		EndPercCap;							// ^^
	bool	BlockNextSpell;						// Indicates whether the client can block a spell or not
	int16	BlockSpellEffect[EFFECT_COUNT];		// Prevents spells with certain effects from landing on you
	bool	ImmuneToFlee;						// Bypass the fleeing flag
	int16	VoiceGraft;							// Stores the ID of the mob with which to talk through
	
	// AAs
	sint8	Packrat;							//weight reduction for items, 1 point = 10%
	int8	BuffSlotIncrease;					// Increases number of available buff slots
	int16	DelayDeath;							// how far below 0 hp you can go
};

typedef struct
{
    int16 spellID;
    int16 chance;
    Timer *pTimer;
} tProc;

struct Shielders_Struct {
	int32   shielder_id;
	int16   shielder_bonus;
};

//eventually turn this into a typedef and
//make DoAnim take it instead of int, to enforce its use.
enum {	//type arguments to DoAnim
	animKick				= 1,
	animPiercing			= 2,	//might be piercing?
	anim2HSlashing			= 3,
	anim2HWeapon			= 4,
	anim1HWeapon			= 5,
	animDualWield			= 6,
	animTailRake			= 7,	//slam & Dpunch too
	animHand2Hand			= 8,
	animShootBow			= 9,
	animRoundKick			= 11,
	animSwarmAttack			= 20,	//dunno about this one..
	animFlyingKick			= 45,
	animTigerClaw			= 46,
	animEagleStrike			= 47,

};


typedef enum {
	petFamiliar,		//only listens to /pet get lost
	petAnimation,		//does not listen to any commands
	petOther,
	petCharmed,
	petNPCFollow,
	petHatelist			//remain active as long something is on the hatelist. Don't listen to any commands
} PetType;

class AA_SwarmPetInfo {
public:
	AA_SwarmPetInfo() { target = 0; owner_id = 0; duration = NULL;}
	~AA_SwarmPetInfo() { target = 0; owner_id = 0; safe_delete(duration); }
	Mob * GetOwner() { return entity_list.GetMobID(owner_id); }
	Timer *duration;
	int32 target; //the target ID
	uint32 owner_id;
};

typedef enum {
	SingleTarget,	// causes effect to spell_target
	AETarget,			// causes effect in aerange of target + target
	AECaster,			// causes effect in aerange of 'this'
	GroupSpell,		// causes effect to caster + target's group
	CAHateList,		// causes effect to all people on caster's hate list within some range
	DirectionalAE,
	CastActUnknown
} CastAction_type;

class EGNode;
class MobFearState;

class Mob : public Entity {
public:
bool logpos;
	enum CLIENT_CONN_STATUS { CLIENT_CONNECTING, CLIENT_CONNECTED, CLIENT_LINKDEAD,
                          CLIENT_KICKED, DISCONNECTED, CLIENT_ERROR, CLIENT_CONNECTINGALL };
	enum eStandingPetOrder { SPO_Follow, SPO_Sit, SPO_Guard };

	//all of this shit is public due to #pf:
	float	tarx;
	float	tary;
	float	tarz;
	int8	tar_ndx;
	float	tar_vector;
	float	tar_vx;
	float	tar_vy;
	float	tar_vz;
	float	test_vector;


	int32	GetPRange(float x, float y, float z);
	static	int32	RandomTimer(int min, int max);
	static	int8	GetDefaultGender(int16 in_race, int8 in_gender = 0xFF);
	static	void	CreateSpawnPacket(EQApplicationPacket* app, NewSpawn_Struct* ns);
//	static	int		CheckEffectIDMatch(int8 effectindex, int16 spellid1, int8 caster_level1, int16 spellid2, int8 caster_level2);


	virtual void	RogueBackstab(Mob* other, bool min_damage = false);
	virtual void	RogueAssassinate(Mob* other); // solar
	bool	BehindMob(Mob* other = 0, float playerx = 0.0f, float playery = 0.0f) const;

	void	TriggerDefensiveProcs(Mob *on);

	Mob(const char*   in_name,
	    const char*   in_lastname,
	    sint32  in_cur_hp,
	    sint32  in_max_hp,
	    int8    in_gender,
	    uint16	in_race,
	    int8    in_class,
        bodyType    in_bodytype,
	    int8    in_deity,
	    int8    in_level,
		int32   in_npctype_id,
		float	in_size,
		float	in_runspeed,
	    float   in_heading,
	    float	in_x_pos,
	    float	in_y_pos,
	    float	in_z_pos,
	    int8    in_light,
		int8	in_texture,
		int8	in_helmtexture,
		int16	in_ac,
		int16	in_atk,
		int16	in_str,
		int16	in_sta,
		int16	in_dex,
		int16	in_agi,
		int16	in_int,
		int16	in_wis,
		int16	in_cha,
		int8	in_haircolor,
		int8	in_beardcolor,
		int8	in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
		int8	in_eyecolor2,
		int8	in_hairstyle,
		int8	in_luclinface,
		int8	in_beard,
		int32	in_drakkin_heritage,
		int32	in_drakkin_tattoo,
		int32	in_drakkin_details,
		int32	in_armor_tint[MAX_MATERIALS],
		int8	in_aa_title,
		int8	in_see_invis,			// see through invis
		int8	in_see_invis_undead,		// see through invis vs. undead
		int8	in_see_hide,
		int8	in_see_improved_hide,
		sint16	in_hp_regen,
		sint16	in_mana_regen,
		int8	in_qglobal,
		float	in_slow_mitigation,	// Allows for mobs to mitigate how much they are slowed.
		int8	in_maxlevel,
		int32	in_scalerate
	);
	virtual ~Mob();

	inline virtual bool IsMob() const { return true; }
	inline virtual bool InZone() const { return true; }
	virtual void SetLevel(uint8 in_level, bool command = false) { level = in_level; }
	void SendLevelAppearance();
	void SendAppearanceEffect(int32 parm1, int32 parm2, int32 parm3, int32 parm4, int32 parm5, Client *specific_target=NULL);
    void SendTargetable(bool on, Client *specific_target = NULL);
	void QuestReward(Client *c=NULL, int32 silver = 0, int32 gold = 0, int32 platinum = 0);
	void CameraEffect(uint32 duration, uint32 intensity, Client *c=NULL, bool global = false);
	void SendSpellEffect(uint32 effectid, int32 duration, int32 finish_delay, bool zone_wide, int32 unk020, bool perm_effect=false, Client *c=NULL);
	void TempName(const char *newname = NULL);
    void SetTargetable(bool on);
    bool IsTargetable() const { return m_targetable; }

	virtual inline sint32 GetPrimaryFaction() const { return 0; }
	virtual uint16 GetSkill(SkillType skill_num) const { return 0; } //overloaded by things which actually have skill (NPC|client)
	virtual void SendWearChange(int8 material_slot);
	virtual void SendTextureWC(int8 slot, int16 texture);
	virtual void SetSlotTint(int8 material_slot, int8 red_tint, int8 green_tint, int8 blue_tint);
	virtual void WearChange(int8 material_slot, int16 texture, uint32 color);
	virtual int32 GetEquipment(int8 material_slot) const { return(0); }
	virtual sint32 GetEquipmentMaterial(int8 material_slot) const;
	virtual uint32 GetEquipmentColor(int8 material_slot) const;
	virtual uint32 IsEliteMaterialItem(int8 material_slot) const;

	void Warp( float x, float y, float z );
	inline bool IsMoving() const { return moving; }
	virtual void SetMoving(bool move) { moving = move; delta_x=0; delta_y=0; delta_z=0; delta_heading=0; }
	virtual void GoToBind(uint8 bindnum = 0) {}
	virtual void Gate();
	virtual void RangedAttack(Mob* other) { }
	virtual void ThrowingAttack(Mob* other) { }
	uint16 GetThrownDamage(sint16 wDmg, sint32& TotalDmg, int& minDmg);
	bool AffectedExcludingSlot(int slot, int effect);

	// abstract methods
	virtual void Death(Mob* killerMob, sint32 damage, int16 spell_id, SkillType attack_skill) = 0;
	virtual void Damage(Mob* from, sint32 damage, int16 spell_id, SkillType attack_skill, bool avoidable = true, sint8 buffslot = -1, bool iBuffTic = false) = 0;
	virtual bool Attack(Mob* other, int Hand = 13, bool FromRiposte = false, bool IsStrikethrough = false) = 0;	// 13 = Primary (default), 14 = secondary
	virtual bool HasRaid() = 0;
	virtual bool HasGroup() = 0;
	virtual Raid* GetRaid() = 0;
	virtual Group* GetGroup() = 0;

	// methods with implementation
	virtual void Heal();
	virtual void HealDamage(uint32 ammount, Mob* caster = NULL);
	virtual void SetMaxHP() { cur_hp = max_hp; }
	static int32 GetLevelCon(int8 mylevel, int8 iOtherLevel);
	inline int32 GetLevelCon(int8 iOtherLevel) const { return(this?GetLevelCon(GetLevel(), iOtherLevel):CON_GREEN); }
	virtual void AddToHateList(Mob* other, sint32 hate = 0, sint32 damage = 0, bool iYellForHelp = true, bool bFrenzy = false, bool iBuffTic = false, bool fromDS = false);

	inline virtual void SetHP(sint32 hp) { if (hp >= max_hp) cur_hp = max_hp; else cur_hp = hp;}
	bool ChangeHP(Mob* other, sint32 amount, int16 spell_id = 0, sint8 buffslot = -1, bool iBuffTic = false);
	inline void SetOOCRegen(sint32 newoocregen) {oocregen = newoocregen;}
	int MonkSpecialAttack(Mob* other, int8 skill_used);
	virtual void TryBackstab(Mob *other);
	void DoAnim(const int animnum, int type=0, bool ackreq = true, eqFilterType filter = FilterNone);
	void ProjectileAnimation(Mob* to, int16 item_id, bool IsArrow = false, float speed = 0, float angle = 0, float tilt = 0, float arc = 0);

	void ChangeSize(float in_size, bool bNoRestriction = false);
	virtual void GMMove(float x, float y, float z, float heading = 0.01, bool SendUpdate = true);
	void SetDeltas(float delta_x, float delta_y, float delta_z, float delta_h);
	void SetTargetDestSteps(int8 target_steps) { tar_ndx = target_steps; }
	void SendPosUpdate(int8 iSendToSelf = 0);
	void MakeSpawnUpdateNoDelta(PlayerPositionUpdateServer_Struct* spu);
	void MakeSpawnUpdate(PlayerPositionUpdateServer_Struct* spu);
	void SendPosition();

	void CreateDespawnPacket(EQApplicationPacket* app, bool Decay);
	void CreateHorseSpawnPacket(EQApplicationPacket* app, const char* ownername, uint16 ownerid, Mob* ForWho = 0);
	void CreateSpawnPacket(EQApplicationPacket* app, Mob* ForWho = 0);
	virtual void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);
	void CreateHPPacket(EQApplicationPacket* app);
	void SendHPUpdate();

	bool AddRangedProc(int16 spell_id, int16 iChance = 3);
	bool RemoveRangedProc(int16 spell_id, bool bAll = false);
	bool HasRangedProcs() const;

	bool AddDefensiveProc(int16 spell_id, int16 iChance = 3);
	bool RemoveDefensiveProc(int16 spell_id, bool bAll = false);
	bool HasDefensiveProcs() const;

	bool AddProcToWeapon(int16 spell_id, bool bPerma = false, int16 iChance = 3);
	bool RemoveProcFromWeapon(int16 spell_id, bool bAll = false);
	bool HasProcs() const;

	inline int8 SeeInvisible() const { return see_invis; }				
	inline bool SeeInvisibleUndead() const { return see_invis_undead; } 
	inline bool SeeHide() const { return see_hide; }
	inline bool SeeImprovedHide() const { return see_improved_hide; }

	bool CheckLos(Mob* other);
	bool CheckLosFN(Mob* other);
	bool CheckLosFN(float posX, float posY, float posZ, float mobSize);
	inline bool GetQglobal() const {return qglobal;}		// SCORPIOUS2K - return quest global flag

	bool IsInvisible(Mob* other = 0) const;
	void SetInvisible(int8 state);

	void SetFlyMode(int8 flymode);

	bool AttackAnimation(SkillType &skillinuse, int Hand, const ItemInst* weapon);
	virtual bool AvoidDamage(Mob* attacker, sint32 &damage);
	virtual bool CheckHitChance(Mob* attacker, SkillType skillinuse, int Hand);
	virtual void TryCriticalHit(Mob *defender, int16 skill, sint32 &damage);
	void TryPetCriticalHit(Mob *defender, int16 skill, sint32 &damage);
	virtual bool TryFinishingBlow(Mob *defender, SkillType skillinuse);
	virtual bool TryHeadShot(Mob* defender, SkillType skillInUse);
	virtual void DoRiposte(Mob* defender);
	void ApplyMeleeDamageBonus(int16 skill, sint32 &damage);
	virtual void MeleeMitigation(Mob *attacker, sint32 &damage, sint32 minhit);

	void	DamageShield(Mob* other, bool spell_ds=false);
	bool	FindBuff(int16 spellid);
	bool	FindType(int8 type, bool bOffensive = false, int16 threshold = 100);
	sint16	GetBuffSlotFromType(int16 type);
	int16	GetSpellIDFromSlot(int8 slot);
	int		CountDispellableBuffs();
	bool	HasBuffIcon(Mob* caster, Mob* target, int16 spell_id);
	bool	CheckHitsRemaining(uint32 buff_slot, bool when_spell_done=false, bool negate=false);
	void 	SpreadVirus(int16 spell_id, int16 casterID);

	virtual void MakePet(int16 spell_id, const char* pettype, const char *petname = NULL);
	virtual void MakePoweredPet(int16 spell_id, const char* pettype, sint16 petpower, const char *petname = NULL);
//	inline void	MakePetType(int16 spell_id, const char* pettype, const char *petname = NULL) { MakePet(spell_id, pettype, petname); }	//for perl
//	void	MakePet(int16 spell_id, int8 in_level, int8 in_class, int16 in_race, int8 in_texture = 0, int8 in_pettype = 0, float in_size = 0, int8 type = 0, int32 min_dmg = 0, int32 max_dmg = 0, const char *petname = NULL);

	bool	CombatRange(Mob* other);
//	int8	flag[60];		//this is for quests or something...

	virtual inline int16	GetBaseRace()	const { return base_race; }
	virtual inline int8	GetBaseGender()		const { return base_gender; }
	virtual inline int8	GetDeity()			const { return deity; }
	inline int16	GetRace()				const { return race; }
	inline int8	GetGender()					const { return gender; }
	inline int8	GetTexture()				const { return texture; }
	inline int8	GetHelmTexture()			const { return helmtexture; }
	inline int8	GetHairColor()				const { return haircolor; }
	inline int8	GetBeardColor()				const { return beardcolor; }
	inline int8	GetEyeColor1()				const { return eyecolor1; }
	inline int8	GetEyeColor2()				const { return eyecolor2; }
	inline int8	GetHairStyle()				const { return hairstyle; }
	inline int8	GetLuclinFace()				const { return luclinface; }
	inline int8	GetBeard()					const { return beard; }
	inline int8	GetDrakkinHeritage()		const { return drakkin_heritage; }
	inline int8	GetDrakkinTattoo()			const { return drakkin_tattoo; }
	inline int8	GetDrakkinDetails()			const { return drakkin_details; }
	inline int32  GetArmorTint(int8 i)		const { return armor_tint[(i < MAX_MATERIALS) ? i : 0]; }
	inline int8	GetClass()					const { return class_; }
	inline uint8	GetLevel()				const { return level; }
	inline const char*	GetName()			const { return name; }
	inline const char*	GetOrigName()			const { return orig_name; }
	inline const char*	GetLastName()			const { return lastname; }
	const char *GetCleanName();
	virtual void SetName(const char *new_name = NULL) { new_name ? strn0cpy(name, new_name, 64) : strn0cpy(name, GetName(), 64); return;};
	inline Mob*			GetTarget()			const { return target; }
	virtual void SetTarget(Mob* mob);
	virtual inline float		GetHPRatio() const { return max_hp == 0 ? 0 : ((float)cur_hp/max_hp*100); }

	bool IsLoggingEnabled() const { return(logging_enabled); }
	void EnableLogging() { logging_enabled = true; }
	void DisableLogging() { logging_enabled = false; }
	bool IsWarriorClass() const;
	virtual bool IsAttackAllowed(Mob *target, bool isSpellAttack = false);
	bool IsBeneficialAllowed(Mob *target);

	inline sint32	GetHP()			const { return cur_hp; }
	inline sint32	GetMaxHP()		const { return max_hp; }
	virtual sint32	CalcMaxHP();
	float GetWalkspeed() const { return(_GetMovementSpeed(-47)); }
	float GetRunspeed() const { return(_GetMovementSpeed(0)); }
	float GetBaseRunspeed() const { return runspeed; }
	float GetMovespeed() const {
		if(IsRunning())
		{
			return GetRunspeed();
		}
		else
		{
			return GetWalkspeed();
		}
	}
	bool IsRunning() const { return m_is_running; }
	void SetRunning(bool val) { m_is_running = val; }

	virtual int GetCasterLevel(int16 spell_id);
	void ApplySpellsBonuses(int16 spell_id, int8 casterlevel, StatBonuses* newbon, int16 casterID = 0, bool item_bonus = false, int32 ticsremaining = 0);

	inline sint32	GetMaxMana()	const { return max_mana; }
	inline sint32	GetMana()		const { return cur_mana; }
	sint32	GetItemHPBonuses();
	sint32	GetSpellHPBonuses();
	virtual const sint32& SetMana(sint32 amount);
	inline float	GetManaRatio()	const { return max_mana == 0 ? 100 : (((float)cur_mana/max_mana)*100); }
	void			SetZone(int32 zone_id, int32 instance_id);

	// moved from client to use in NPC too
 	char GetCasterClass() const;
	uint8 GetArchetype() const;
 	virtual sint32 CalcMaxMana();
 
	inline virtual sint16	GetAC()		const { return AC + itembonuses.AC + spellbonuses.AC; } // Quagmire - this is NOT the right math
	inline virtual sint16	GetATK()	const { return ATK + itembonuses.ATK + spellbonuses.ATK; }
	inline virtual sint16	GetATKBonus()	const { return itembonuses.ATK + spellbonuses.ATK; }
	inline virtual sint16	GetSTR()	const { return STR + itembonuses.STR + spellbonuses.STR; }
	inline virtual sint16	GetSTA()	const { return STA + itembonuses.STA + spellbonuses.STA; }
	inline virtual sint16	GetDEX()	const { return DEX + itembonuses.DEX + spellbonuses.DEX; }
	inline virtual sint16	GetAGI()	const { return AGI + itembonuses.AGI + spellbonuses.AGI; }
	inline virtual sint16	GetINT()	const { return INT + itembonuses.INT + spellbonuses.INT; }
	inline virtual sint16	GetWIS()	const { return WIS + itembonuses.WIS + spellbonuses.WIS; }
	inline virtual sint16	GetCHA()	const { return CHA + itembonuses.CHA + spellbonuses.CHA; }
	inline virtual sint16	GetMR() const { return MR + itembonuses.MR + spellbonuses.MR; }
	inline virtual sint16	GetFR()	const { return FR + itembonuses.FR + spellbonuses.FR; }
	inline virtual sint16	GetDR()	const { return DR + itembonuses.DR + spellbonuses.DR; }
	inline virtual sint16	GetPR()	const { return PR + itembonuses.PR + spellbonuses.PR; }
	inline virtual sint16	GetCR() const { return CR + itembonuses.CR + spellbonuses.CR; }
	inline virtual sint16	GetCorrup() const { return Corrup + itembonuses.Corrup + spellbonuses.Corrup; }
	inline StatBonuses GetItemBonuses() const { return itembonuses; }
	inline StatBonuses GetSpellBonuses() const { return spellbonuses; }
	inline StatBonuses GetAABonuses() const { return aabonuses; }

	inline virtual sint16  GetMaxSTR() const { return GetSTR(); }
	inline virtual sint16  GetMaxSTA() const { return GetSTA(); }
	inline virtual sint16  GetMaxDEX() const { return GetDEX(); }
	inline virtual sint16  GetMaxAGI() const { return GetAGI(); }
	inline virtual sint16  GetMaxINT() const { return GetINT(); }
	inline virtual sint16  GetMaxWIS() const { return GetWIS(); }
	inline virtual sint16  GetMaxCHA() const { return GetCHA(); }
	inline virtual sint16  GetMaxMR() const { return 255; }
	inline virtual sint16  GetMaxPR() const { return 255; }
	inline virtual sint16  GetMaxDR() const { return 255; }
	inline virtual sint16  GetMaxCR() const { return 255; }
	inline virtual sint16  GetMaxFR() const { return 255; }

	bool IsNimbusEffectActive(uint32 nimbus_effect);
	void SetNimbusEffect(uint32 nimbus_effect);
	inline virtual uint32  GetNimbusEffect1() const { return nimbus_effect1; }
	inline virtual uint32  GetNimbusEffect2() const { return nimbus_effect2; }
	inline virtual uint32  GetNimbusEffect3() const { return nimbus_effect3; }
	void RemoveNimbusEffect(int effectid);

	virtual float GetActSpellRange(int16 spell_id, float range){ return range;}
	virtual sint32  GetActSpellDamage(int16 spell_id, sint32 value) { return value; }
	virtual sint32  GetActSpellHealing(int16 spell_id, sint32 value) { return value; }
	virtual sint32 GetActSpellCost(int16 spell_id, sint32 cost){ return cost;}
	virtual sint32 GetActSpellDuration(int16 spell_id, sint32 duration){ return duration;}
	virtual sint32 GetActSpellCasttime(int16 spell_id, sint32 casttime);
	float ResistSpell(int8 resist_type, int16 spell_id, Mob *caster, bool use_resist_override = false, int resist_override = 0);
	uint16 GetSpecializeSkillValue(int16 spell_id) const;

	void ShowStats(Client* client);
	void ShowBuffs(Client* client);
	void ShowBuffList(Client* client);
	int32 GetNPCTypeID()			const { return npctype_id; } // rembrant, Dec. 20, 2001

	float Dist(const Mob &) const;
	float DistNoZ(const Mob &) const;
	float DistNoRoot(const Mob &) const;
	float DistNoRoot(float x, float y, float z) const;
	float DistNoRootNoZ(float x, float y) const;
	float DistNoRootNoZ(const Mob &) const;

	static float GetReciprocalHeading(Mob* target);
	bool PlotPositionAroundTarget(Mob* target, float &x_dest, float &y_dest, float &z_dest, bool lookForAftArc = true);

	bool IsTargeted() const { return (targeted > 0); }
	inline void IsTargeted(int in_tar) { targeted += in_tar; if(targeted < 0) targeted = 0;}

	inline void		Teleport(VERTEX NewPosition) { x_pos = NewPosition.x; y_pos = NewPosition.y; z_pos = NewPosition.z; };
	inline const float	GetX() const		{ return x_pos; }
	inline const float	GetY() const		{ return y_pos; }
	inline const float	GetZ() const		{ return z_pos; }
	inline const float	GetHeading() const	{ return heading; }
	inline const float	GetSize() const		{ return size; }
	inline const float	GetBaseSize() const		{ return base_size; }
	inline void			SetChanged()		{ pLastChange = Timer::GetCurrentTime(); }
	inline const int32	LastChange() const	{ return pLastChange; }
    bool IsBoat();

	void	SetFollowID(int32 id) { follow = id; }
	void	SetFollowDistance(int32 dist) { follow_dist = dist; }
	int32	GetFollowID()		  const { return follow; }
	int32	GetFollowDistance()		  const { return follow_dist; }

	virtual void	Message(int32 type, const char* message, ...) {} // fake so dont have to worry about typecasting
	virtual void	Message_StringID(int32 type, int32 string_id, int32 distance = 0) {}
	virtual void	Message_StringID(int32 type, int32 string_id, const char* message,const char* message2=0,const char* message3=0,const char* message4=0,const char* message5=0,const char* message6=0,const char* message7=0,const char* message8=0, const char* message9=0, int32 distance = 0) {}
	void Say(const char *format, ...);
	void Say_StringID(int32 string_id, const char *message3 = 0, const char *message4 = 0, const char *message5 = 0, const char *message6 = 0, const char *message7 = 0, const char *message8 = 0, const char *message9 = 0);
	void Shout(const char *format, ...);
	void Emote(const char *format, ...);
	void QuestJournalledSay(Client *QuestInitiator, const char *str);
	int32 GetItemStat(int32 itemid, const char *identifier);

	//Casting related
 	void SendSpellBarDisable();
 	void SendSpellBarEnable(int16 spellid);
	void ZeroCastingVars();
	virtual void SpellProcess();
	//TODO: put these ridiculous options in a damned struct or something
	virtual bool CastSpell(int16 spell_id, int16 target_id, int16 slot = 10, sint32 casttime = -1, sint32 mana_cost = -1, int32* oSpellWillFinish = 0, int32 item_slot = 0xFFFFFFFF, uint32 timer = 0xFFFFFFFF, uint32 timer_duration = 0, uint32 type = 0, sint16 *resist_adjust = NULL);
	virtual bool DoCastSpell(int16 spell_id, int16 target_id, int16 slot = 10, sint32 casttime = -1, sint32 mana_cost = -1, int32* oSpellWillFinish = 0, int32 item_slot = 0xFFFFFFFF, uint32 timer = 0xFFFFFFFF, uint32 timer_duration = 0, uint32 type = 0, sint16 resist_adjust = 0);
	void CastedSpellFinished(int16 spell_id, int32 target_id, int16 slot, int16 mana_used, int32 inventory_slot = 0xFFFFFFFF, sint16 resist_adjust = 0);
	bool SpellFinished(int16 spell_id, Mob *target, int16 slot = 10, int16 mana_used = 0, int32 inventory_slot = 0xFFFFFFFF, sint16 resist_adjust = 0);
	virtual bool SpellOnTarget(int16 spell_id, Mob* spelltar, bool reflect = false, bool use_resist_adjust = false, sint16 resist_adjust = 0);
	virtual bool SpellEffect(Mob* caster, int16 spell_id, float partial = 100);
	virtual bool DetermineSpellTargets(uint16 spell_id, Mob *&spell_target, Mob *&ae_center, CastAction_type &CastAction);
	virtual bool CheckFizzle(int16 spell_id);
	virtual bool IsImmuneToSpell(int16 spell_id, Mob *caster);
	virtual float GetAOERange(uint16 spell_id);
	void InterruptSpell(int16 spellid = SPELL_UNKNOWN);
	void InterruptSpell(int16, int16, int16 spellid = SPELL_UNKNOWN);
	inline bool IsCasting() const { return((casting_spell_id != 0)); }
	uint16 CastingSpellID() const { return casting_spell_id; }

	//Song related
	bool UseBardSpellLogic(int16 spell_id = 0xffff, int slot = -1);
	bool ApplyNextBardPulse(int16 spell_id, Mob *spell_target, int16 slot);
	void BardPulse(uint16 spell_id, Mob *caster);

	//Buff related
	void BuffProcess();
	virtual void DoBuffTic(int16 spell_id, int32 ticsremaining, int8 caster_level, Mob* caster = 0);
	void BuffFadeBySpellID(int16 spell_id);
	void BuffFadeByEffect(int effectid, int skipslot = -1);
	void BuffFadeAll();
	void BuffFadeDetrimental();
	void BuffFadeBySlot(int slot, bool iRecalcBonuses = true);
	void BuffFadeDetrimentalByCaster(Mob *caster);
	void BuffFadeBySitModifier();
	void BuffModifyDurationBySpellID(int16 spell_id, sint32 newDuration);
	int	AddBuff(Mob *caster, const int16 spell_id, int duration = 0, sint32 level_override = -1);
	int	CanBuffStack(int16 spellid, int8 caster_level, bool iFailIfOverwrite = false);
	int	CalcBuffDuration(Mob *caster, Mob *target, int16 spell_id, sint32 caster_level_override = -1);
	void SendPetBuffsToClient();
	virtual int GetCurrentBuffSlots() const { return 0; }
	virtual int GetCurrentSongSlots() const { return 0; }
	virtual int GetCurrentDiscSlots() const { return 0; }
	virtual int GetMaxBuffSlots() const { return 0; }
	virtual int GetMaxSongSlots() const { return 0; }
	virtual int GetMaxDiscSlots() const { return 0; }
	virtual int GetMaxTotalSlots() const { return 0; }
	virtual void InitializeBuffSlots() { buffs = NULL; current_buff_count = 0; }
	virtual void UninitializeBuffSlots() { }
	inline bool HasRune() const { return m_hasRune; }
	inline bool HasSpellRune() const { return m_hasSpellRune; }
	inline bool HasPartialMeleeRune() const { return m_hasPartialMeleeRune; }
	inline bool HasPartialSpellRune() const { return m_hasPartialSpellRune; }
	inline void SetHasRune(bool hasRune) { m_hasRune = hasRune; }
	inline void SetHasSpellRune(bool hasSpellRune) { m_hasSpellRune = hasSpellRune; }
	inline void SetHasPartialMeleeRune(bool hasPartialMeleeRune) { m_hasPartialMeleeRune = hasPartialMeleeRune; }
	inline void SetHasPartialSpellRune(bool hasPartialSpellRune) { m_hasPartialSpellRune = hasPartialSpellRune; }
	inline bool HasDeathSaveChance() const { return m_hasDeathSaveChance; }
	inline void SetDeathSaveChance(bool hasDeathSaveChance) { m_hasDeathSaveChance = hasDeathSaveChance; }
	EQApplicationPacket *MakeBuffsPacket(bool for_target = true);
	void SendBuffsToClient(Client *c);
	inline Buffs_Struct* GetBuffs() { return buffs; }
	void DoGravityEffect();
	Timer GravityTimer;

	//effect related
	sint16 CalcFocusEffect(focusType type, int16 focus_id, int16 spell_id, bool best_focus=false);
	void SendIllusionPacket(int16 in_race, int8 in_gender = 0xFF, int8 in_texture = 0xFF, int8 in_helmtexture = 0xFF, int8 in_haircolor = 0xFF, int8 in_beardcolor = 0xFF, int8 in_eyecolor1 = 0xFF, int8 in_eyecolor2 = 0xFF, int8 in_hairstyle = 0xFF, int8 in_luclinface = 0xFF, int8 in_beard = 0xFF, int8 in_aa_title = 0xFF, int32 in_drakkin_heritage = 0xFFFFFFFF, int32 in_drakkin_tattoo = 0xFFFFFFFF, int32 in_drakkin_details = 0xFFFFFFFF, float in_size = 0xFFFFFFFF);
	virtual void Stun(int duration);
	virtual void UnStun();
	inline void Silence(bool newval) { silenced = newval; }
	inline void Amnesia(bool newval) { amnesiad = newval; }
	void TemporaryPets(int16 spell_id, Mob *target, const char *name_override = NULL, uint32 duration_override = 0);
	void TypesTemporaryPets(int32 typesid, Mob *target, const char *name_override = NULL, uint32 duration_override = 0, bool followme = false);
	void WakeTheDead(int16 spell_id, Mob *target, uint32 duration);
	void TryDotCritical(int16 spell_id, int &damage);
	void Spin();
	void Kill();
	bool PassCharismaCheck(Mob* caster, Mob* spellTarget, int16 spell_id);
	bool TryDeathSave();
	void DoBuffWearOffEffect(uint32 index);
	void TryTriggerOnCast(uint32 spell_id, bool aa_trigger);
	void TriggerOnCast(uint32 focus_spell, uint32 spell_id, uint8 aa_chance);
	void TrySpellTrigger(Mob *target, uint32 spell_id);
	void TryApplyEffect(Mob *target, uint32 spell_id);
	void TryTwincast(Mob *caster, Mob *target, uint32 spell_id);
	void TrySympatheticProc(Mob *target, uint32 spell_id);
	bool TryFadeEffect(int slot);
	sint16 GetHealRate();
	sint32 GetVulnerability(sint32 damage, Mob *caster, uint32 spell_id, int32 ticsremaining);
	sint16 GetSkillDmgTaken(const SkillType skill_used);
	void DoKnockback(Mob *caster, uint32 pushback, uint32 pushup);
	sint16 CalcResistChanceBonus();
	sint16 CalcFearResistChance();
	void TrySpellOnKill();
	bool TrySpellOnDeath();
	sint16 GetCritDmgMob(int16 skill);
	sint16 GetMeleeDamageMod_SE(int16 skill);
	sint16 GetCrippBlowChance();
	sint16 GetSkillReuseTime(int16 skill);
	sint16 GetCriticalChanceBonus(int16 skill, bool aa_bonus=false);
	sint16 GetSkillDmgAmt(int16 skill);
	bool TryReflectSpell(uint32 spell_id);
	bool CanBlockSpell() const { return(spellbonuses.BlockNextSpell); }

	static int32 GetAppearanceValue(EmuAppearance iAppearance);
	void SendAppearancePacket(int32 type, int32 value, bool WholeZone = true, bool iIgnoreSelf = false, Client *specific_target=NULL);
	void SetAppearance(EmuAppearance app, bool iIgnoreSelf = true);
	inline EmuAppearance GetAppearance() const { return _appearance; }
	inline const int8 GetRunAnimSpeed()	const { return pRunAnimSpeed; }
	inline void	SetRunAnimSpeed(sint8 in) { if (pRunAnimSpeed != in) { pRunAnimSpeed = in; pLastChange = Timer::GetCurrentTime(); } }
	bool IsDestructibleObject() { return destructibleobject; }
	void SetDestructibleObject(bool in) { destructibleobject = in; }

	Mob* GetPet();
	void SetPet(Mob* newpet);
	virtual Mob* GetOwner();
	virtual Mob* GetOwnerOrSelf();
	Mob* GetUltimateOwner();
	void SetPetID(int16 NewPetID);
	inline int16 GetPetID()	const { return petid; }
	inline PetType GetPetType() const { return typeofpet; }
	void SetPetType(PetType p) { typeofpet = p; } 
	inline sint16 GetPetPower() const { return (petpower < 0) ? 0 : petpower; }
	void SetPetPower(sint16 p) { if (p < 0) petpower = 0; else petpower = p; }
	bool IsFamiliar() const { return(typeofpet == petFamiliar); }
	bool IsAnimation() const { return(typeofpet == petAnimation); }
	bool IsCharmed() const { return(typeofpet == petCharmed); }
	void SetOwnerID(int16 NewOwnerID);
	inline int16 GetOwnerID() const { return ownerid; }
	inline virtual bool HasOwner() { if(GetOwnerID()==0){return false;} return( entity_list.GetMob(GetOwnerID()) != 0); }
	inline virtual bool IsPet() { return(HasOwner()); }
	inline bool HasPet() const { if(GetPetID()==0){return false;} return (entity_list.GetMob(GetPetID()) != 0);}
	bool HadTempPets() const { return(hasTempPet); }
	void TempPets(bool i) { hasTempPet = i; }

	inline const bodyType GetBodyType() const { return bodytype; }
	inline const bodyType GetOrigBodyType() const { return orig_bodytype; }
	void SetBodyType(bodyType new_body, bool overwrite_orig);

	int8	invisible, see_invis;
	bool	invulnerable, invisible_undead, invisible_animals, sneaking, hidden, improved_hidden;
	bool	see_invis_undead, see_hide, see_improved_hide;   
	bool	qglobal;		

	virtual void SetAttackTimer();
	inline void	SetInvul(bool invul) { invulnerable=invul; }
	inline bool	GetInvul(void) { return invulnerable; }
	inline void	SetExtraHaste(int Haste) { ExtraHaste = Haste; }
	virtual int GetHaste();

	int8	GetWeaponDamageBonus(const Item_Struct* Weapon);
	int16	GetDamageTable(SkillType skillinuse);
	virtual int GetMonkHandToHandDamage(void);

	bool	CanThisClassDoubleAttack(void) const;
	bool	CanThisClassDualWield(void) const;
	bool	CanThisClassRiposte(void) const;
	bool	CanThisClassDodge(void) const;
	bool	CanThisClassParry(void) const;
	bool	CanThisClassBlock(void) const;

	int	GetMonkHandToHandDelay(void);
	int16	GetClassLevelFactor();
	void	Mesmerize();
	inline bool	IsMezzed() const { return mezzed; }
	inline bool	IsStunned() const { return stunned; }
	inline bool	IsSilenced() const { return silenced; }
	inline bool	IsAmnesiad() const { return amnesiad; }
	inline int16	GetErrorNumber() const {return adverrorinfo;}

	sint32	ReduceDamage(sint32 damage);
	sint32  AffectMagicalDamage(sint32 damage, int16 spell_id, const bool iBuffTic, Mob* attacker);

	virtual void DoSpecialAttackDamage(Mob *who, SkillType skill, sint32 max_damage, sint32 min_damage = 1, sint32 hate_override = -1);
    bool Flurry();
    bool Rampage();
    bool AddRampage(Mob*);
	void ClearRampage();
	void AreaRampage();

    void StartEnrage();
	void ProcessEnrage();
    bool IsEnraged();
	void Taunt(NPC* who, bool always_succeed);

	virtual void		AI_Init();
	virtual void		AI_Start(int32 iMoveDelay = 0);
	virtual void		AI_Stop();
	virtual void		AI_Process();

	const char* GetEntityVariable(const char *id);
	void SetEntityVariable(const char *id, const char *m_var);
	bool EntityVariableExists(const char *id);

	void				AI_Event_Engaged(Mob* attacker, bool iYellForHelp = true);
	void				AI_Event_NoLongerEngaged();

	FACTION_VALUE		GetSpecialFactionCon(Mob* iOther);
	inline const bool	IsAIControlled() const { return pAIControlled; }
	inline const float GetAggroRange() const { return (spellbonuses.AggroRange == -1) ? pAggroRange : spellbonuses.AggroRange; }
	inline const float GetAssistRange() const { return (spellbonuses.AssistRange == -1) ? pAssistRange : spellbonuses.AssistRange; }


	inline void			SetPetOrder(eStandingPetOrder i) { pStandingPetOrder = i; }
	inline const eStandingPetOrder GetPetOrder() const { return pStandingPetOrder; }
	inline void			SetHeld(bool nState) { held = nState; }
	inline const bool	IsHeld() const { return held; }
	inline const bool	IsRoamer() const { return roamer; }
	inline const bool   IsRooted() const { return rooted || permarooted; }
	inline const bool   HasVirus() const { return has_virus; }
	int					GetSnaredAmount();

	bool				RemoveFromHateList(Mob* mob);
	void				SetHate(Mob* other, sint32 hate = 0, sint32 damage = 0) {hate_list.Set(other,hate,damage);}
	int32				GetHateAmount(Mob* tmob, bool is_dam = false)  {return hate_list.GetEntHate(tmob,is_dam);}
	int32				GetDamageAmount(Mob* tmob)  {return hate_list.GetEntHate(tmob, true);}
	Mob*				GetHateTop()  {return hate_list.GetTop(this);}
	Mob*				GetHateDamageTop(Mob* other)  {return hate_list.GetDamageTop(other);}
	Mob*				GetHateRandom()  {return hate_list.GetRandom();}
	//this is diff from GetHateTop as it does not account for things like frenzy, just the person with the most hate
	Mob*				GetHateMost()	{return hate_list.GetMostHate();}
	bool				IsEngaged()   {return(!hate_list.IsEmpty()); }
	bool				HateSummon();
	bool				IsInAttackCone(Mob *target);//Jeremy's Cone Script
	void				FaceTarget(Mob* MobToFace = 0);
	void				SetHeading(float iHeading) { if (heading != iHeading) { pLastChange = Timer::GetCurrentTime(); heading = iHeading; } }
	void				WipeHateList();
	void				AddFeignMemory(Client* attacker);
	void				RemoveFromFeignMemory(Client* attacker);
	void				ClearFeignMemory();
	void				PrintHateListToClient(Client *who) { hate_list.PrintToClient(who); }
	void GetHateList(std::list<tHateEntry*> &h_list) { return hate_list.GetHateList(h_list); }


	int					GetCurWp(){ return cur_wp; }

	//old fear function
	//void SetFeared(Mob *caster, int32 duration, bool flee = false);
	float GetFearSpeed();
	bool IsFeared() { return curfp; } // This returns true if the mob is feared or fleeing due to low HP
	//old fear: inline void StartFleeing() { SetFeared(GetHateTop(), FLEE_RUN_DURATION, true); }
	inline void StartFleeing() { flee_mode = true; CalculateNewFearpoint(); }
	void ProcessFlee();
	void CheckFlee();

	inline bool			CheckAggro(Mob* other) {return hate_list.IsOnHateList(other);}
    float				CalculateHeadingToTarget(float in_x, float in_y);
    bool				CalculateNewPosition(float x, float y, float z, float speed, bool checkZ = false);
	virtual bool		CalculateNewPosition2(float x, float y, float z, float speed, bool checkZ = true);
    float				CalculateDistance(float x, float y, float z);
	float				GetGroundZ(float new_x, float new_y, float z_offset=0.0);
	void				SendTo(float new_x, float new_y, float new_z);
	void				SendToFixZ(float new_x, float new_y, float new_z);
	void				NPCSpecialAttacks(const char* parse, int permtag, bool reset = true, bool remove = false);
	inline int32		DontHealMeBefore() const { return pDontHealMeBefore; }
	inline int32		DontBuffMeBefore() const { return pDontBuffMeBefore; }
	inline int32		DontDotMeBefore() const { return pDontDotMeBefore; }
	inline int32		DontRootMeBefore() const { return pDontRootMeBefore; }
	inline int32		DontSnareMeBefore() const { return pDontSnareMeBefore; }
	void				SetDontRootMeBefore(int32 time) { pDontRootMeBefore = time; }
	void				SetDontHealMeBefore(int32 time) { pDontHealMeBefore = time; }
	void				SetDontBuffMeBefore(int32 time) { pDontBuffMeBefore = time; }
	void				SetDontDotMeBefore(int32 time) { pDontDotMeBefore = time; }
	void				SetDontSnareMeBefore(int32 time) { pDontSnareMeBefore = time; }

	// calculate interruption of spell via movement of mob
	void SaveSpellLoc() {spell_x = x_pos; spell_y = y_pos; spell_z = z_pos; }
	inline float GetSpellX() const {return spell_x;}
	inline float GetSpellY() const {return spell_y;}
	inline float GetSpellZ() const {return spell_z;}
	inline bool	IsGrouped()	 const { return isgrouped; }
	void SetGrouped(bool v);
	inline bool IsRaidGrouped() const { return israidgrouped; }
	void SetRaidGrouped(bool v);
	inline bool	IsLooting()	 const { return islooting; }
	void SetLooting(bool val) { islooting = val; }

	bool CheckWillAggro(Mob *mob);

	void	InstillDoubt(Mob *who);
	sint16	GetResist(int8 type) const;
	Mob*	GetShieldTarget()			const { return shield_target; }
	void	SetShieldTarget(Mob* mob)	{ shield_target = mob; }
	bool	HasActiveSong() const { return(bardsong != 0); }
	bool	Charmed() const { return charmed; }
	static int32	GetLevelHP(int8 tlevel);
	int32	GetZoneID() const;	//for perl
	virtual sint32 CheckAggroAmount(int16 spellid);
	virtual sint32 CheckHealAggroAmount(int16 spellid, int32 heal_possible = 0);
	virtual int32 GetAA(int32 aa_id) const { return(0); }

	int16	GetInstrumentMod(int16 spell_id) const;
	int CalcSpellEffectValue(int16 spell_id, int effect_id, int caster_level = 1, Mob *caster = NULL, int ticsremaining = 0);
	int CalcSpellEffectValue_formula(int formula, int base, int max, int caster_level, int16 spell_id, int ticsremaining = 0);
	virtual int CheckStackConflict(int16 spellid1, int caster_level1, int16 spellid2, int caster_level2, Mob* caster1 = NULL, Mob* caster2 = NULL);
	int32 GetCastedSpellInvSlot() const { return casting_spell_inventory_slot; }

	// HP Event
	inline int GetNextHPEvent() const { return nexthpevent; }
	void SetNextHPEvent( int hpevent );
	void SendItemAnimation(Mob *to, const Item_Struct *item, SkillType skillInUse);
	inline int& GetNextIncHPEvent() { return nextinchpevent; }
	void SetNextIncHPEvent( int inchpevent );

	bool DivineAura() const;
    bool SpecAttacks[SPECATK_MAXNUM];
	bool HasNPCSpecialAtk(const char* parse);
	Shielders_Struct shielder[MAX_SHIELDERS];
	Trade* trade;

	//temporary:
	bool fix_pathing;
	float rewind_x; //Lieka:  Used for storing /rewind values
	float rewind_y; //Lieka:  Used for storing /rewind values
	float rewind_z; //Lieka:  Used for storing /rewind values
	Timer rewind_timer; //Lieka:  Used for measuring how long a player stays in one spot
	inline float GetCWPX() const { return(cur_wp_x); }
	inline float GetCWPY() const { return(cur_wp_y); }
	inline float GetCWPZ() const { return(cur_wp_z); }
	inline float GetCWPH() const { return(cur_wp_heading); }
	inline float GetCWPP() const { return(cur_wp_pause); }
	inline int GetCWP() const { return(cur_wp); }
	void SetCurrentWP(int16 waypoint) { cur_wp = waypoint; }
	virtual FACTION_VALUE GetReverseFactionCon(Mob* iOther) { return FACTION_INDIFFERENT; }
	
	inline bool IsTrackable() const { return(trackable); }
	Timer* GetAIThinkTimer() { return AIthink_timer; }
	Timer* GetAIMovementTimer() { return AImovement_timer; }
	Timer GetAttackTimer() { return attack_timer; }
	Timer GetAttackDWTimer() { return attack_dw_timer; }
	inline bool IsFindable() { return findable; }
	inline uint8 GetManaPercent() { return (uint8)((float)cur_mana / (float)max_mana * 100.0f); }
	virtual uint8 GetEndurancePercent() { return 0; }

	inline virtual bool IsBlockedBuff(sint16 SpellID) { return false; }
	inline virtual bool IsBlockedPetBuff(sint16 SpellID) { return false; }

	void SetGlobal(const char *varname, const char *newvalue, int options, const char *duration, Mob *other = NULL);
	void TarGlobal(const char *varname, const char *value, const char *duration, int npcid, int charid, int zoneid);
	void DelGlobal(const char *varname);
	void SetDSKill() {m_deadFromDS = true;}
    bool GetDSKill() {return m_deadFromDS;}

	void Push(Mob *pusher, SkillType st);//Melee/Spell Push

protected:
	void CommonDamage(Mob* other, sint32 &damage, const uint16 spell_id, const SkillType attack_skill, bool &avoidable, const sint8 buffslot, const bool iBuffTic);
	static uint16 GetProcID(uint16 spell_id, uint8 effect_index);
	float _GetMovementSpeed(int mod) const;
	virtual bool MakeNewPositionAndSendUpdate(float x, float y, float z, float speed, bool checkZ);

	virtual bool AI_EngagedCastCheck() { return(false); }
	virtual bool AI_PursueCastCheck() { return(false); }
	virtual bool AI_IdleCastCheck() { return(false); }
	
	//used by mlog() for VC6
	#ifdef NO_VARIADIC_MACROS
	void mob_log(LogType type, const char *fmt, ...);
	void mob_hex(LogType type, const char *data, unsigned long length, unsigned char padding=4);
	#endif

	bool	IsFullHP;
	bool	moved;

	std::vector<std::string> RampageArray;
	std::map<std::string, std::string> m_EntityVariables;

	bool	isgrouped; //These meant to be private?
	bool	israidgrouped;
	bool	pendinggroup;
	bool	islooting;
	int8	texture;
	int8	helmtexture;

	int	AC;
	sint16 ATK;
	sint16 STR;
	sint16 STA;
	sint16 DEX;
	sint16 AGI;
	sint16 INT;
	sint16 WIS;
	sint16 CHA;
	sint16 MR;
	sint16 CR;
	sint16 FR;
	sint16 DR;
	sint16 PR;
	sint16 Corrup;
	bool moving;
	int targeted;
	bool findable;
	bool trackable;
	sint32  cur_hp;
	sint32  max_hp;
	sint32	base_hp;
	sint32	cur_mana;
	sint32	max_mana;
	sint16	hp_regen;
	sint16	mana_regen;
	sint32	oocregen; // Out of Combat Regen, % per tick
	float slow_mitigation;	// Allows for a slow mitigation based on a % in decimal form.  IE, 1 = 100% mitigation, .5 is 50%
	int8 maxlevel;
	int32 scalerate;
	Buffs_Struct *buffs;
	uint32 current_buff_count;
	Timer *buff_tic_timer;
	StatBonuses	itembonuses;
	StatBonuses	spellbonuses;
	StatBonuses	aabonuses;
	int16 petid;
	int16 ownerid;
	PetType typeofpet;
	int16 petpower;		// Should be part of class Pet, like PetType
	int32 follow;
	int32 follow_dist;

	int8    gender;
	int16	race;
	int8	base_gender;
	int16	base_race;
	int8    class_;
	bodyType    bodytype;
	bodyType    orig_bodytype;
	int16	deity;
	uint8    level;
	int32   npctype_id;
	float	x_pos;
	float	y_pos;
	float	z_pos;
	float	heading;
	uint16	animation;
	float	base_size;
	float	size;
	float	runspeed;
	int32 pLastChange;
	bool held;
	void CalcSpellBonuses(StatBonuses* newbon);
	virtual void CalcBonuses();
	void TryDefensiveProc(Mob *on);
	void TryWeaponProc(const Item_Struct* weapon, Mob *on, int16 hand = 13);
	void TryWeaponProc(const ItemInst* weapon, Mob *on, int16 hand = 13);
	void ExecWeaponProc(uint16 spell_id, Mob *on);
	virtual float GetProcChances(float &ProcBonus, float &ProcChance, uint16 weapon_speed = 30);
	int GetWeaponDamage(Mob *against, const Item_Struct *weapon_item);
	int GetWeaponDamage(Mob *against, const ItemInst *weapon_item, int32 *hate = NULL);
	int GetKickDamage() const;
	int GetBashDamage() const;
	bool HasDied();
	void CalculateNewFearpoint();
	float FindGroundZ(float new_x, float new_y, float z_offset=0.0);
	VERTEX UpdatePath(float ToX, float ToY, float ToZ, float Speed, bool &WaypointChange, bool &NodeReached);
	void PrintRoute();
	void UpdateRuneFlags();

	enum {MAX_PROCS = 4};
	tProc PermaProcs[MAX_PROCS];
	tProc SpellProcs[MAX_PROCS];
	tProc DefensiveProcs[MAX_PROCS];
	tProc RangedProcs[MAX_PROCS];

	char    name[64];
	char    orig_name[64];
	char	clean_name[64];
	char    lastname[64];

    bool bEnraged;
    Timer *SpecAttackTimers[SPECATK_MAXNUM];
	bool destructibleobject;

	sint32	delta_heading;
    float	delta_x;
	float	delta_y;
	float	delta_z;

	int8    light;

	float	fixedZ;
	EmuAppearance    _appearance;
	int8	pRunAnimSpeed;
	bool	m_is_running;

	
	Timer	attack_timer;
	Timer	attack_dw_timer;
	Timer	ranged_timer;
	float	attack_speed;		//% increase/decrease in attack speed (not haste)
	Timer	tic_timer;
	Timer	mana_timer;

	//spell casting vars
	Timer spellend_timer;
	int16 casting_spell_id;
    float spell_x, spell_y, spell_z;
	int	attacked_count;
	bool delaytimer;
	uint16 casting_spell_targetid;
	uint16 casting_spell_slot;
	uint16 casting_spell_mana;
	uint32 casting_spell_inventory_slot;
	uint32 casting_spell_timer;
	uint32 casting_spell_timer_duration;
	uint32 casting_spell_type;
	sint16 casting_spell_resist_adjust;
	uint16 bardsong;
	uint8 bardsong_slot;
	uint32 bardsong_target_id;

	// Currently 3 max nimbus particle effects at a time
	uint32 nimbus_effect1;
	uint32 nimbus_effect2;
	uint32 nimbus_effect3;

	int8 haircolor;
	int8 beardcolor;
	int8 eyecolor1; // the eyecolors always seem to be the same, maybe left and right eye?
	int8 eyecolor2;
	int8 hairstyle;
	int8 luclinface; // 
	int8 beard;
	int32 drakkin_heritage;
	int32 drakkin_tattoo;
	int32 drakkin_details;
	int32 armor_tint[MAX_MATERIALS];

	int8 aa_title;

	Mob* shield_target;

	int		ExtraHaste;	// for the #haste command
	bool	mezzed;
	bool	stunned;
	bool	charmed;	//this isnt fully implemented yet
	bool	rooted;
	bool	silenced;
	bool	amnesiad;
	bool	inWater;	// Set to true or false by Water Detection code if enabled by rules
	bool	has_virus;	// whether this mob has a viral spell on them
	int16	viral_spells[MAX_SPELL_TRIGGER*2]; // Stores the spell ids of the viruses on target and caster ids

	Timer	stunned_timer;
	Timer	spun_timer;
	Timer	bardsong_timer;
	Timer 	ViralTimer;

	int8	viral_timer_counter;
	int16	adverrorinfo;

	// MobAI stuff
	eStandingPetOrder pStandingPetOrder;
	int32	minLastFightingDelayMoving;
	int32	maxLastFightingDelayMoving;
	float	pAggroRange;
	float	pAssistRange;
	Timer*	AIthink_timer;
	Timer*	AImovement_timer;
	Timer*	AItarget_check_timer;
	bool	movetimercompleted;
	bool	permarooted;
	Timer*	AIscanarea_timer;
	Timer*	AIwalking_timer;
	Timer*	AIfeignremember_timer;
	int32	pLastFightingDelayMoving;
	HateList hate_list;
	std::set<int32> feign_memory_list;
	// This is to keep track of mobs we cast faction mod spells on
	std::map<uint32,sint32> faction_bonuses; // Primary FactionID, Bonus
	void	AddFactionBonus(uint32 pFactionID,sint32 bonus);
	sint32	GetFactionBonus(uint32 pFactionID);
	// This is to keep track of item faction modifiers
	std::map<uint32,sint32> item_faction_bonuses; // Primary FactionID, Bonus
	void	AddItemFactionBonus(uint32 pFactionID,sint32 bonus);
	sint32	GetItemFactionBonus(uint32 pFactionID);
	void	ClearItemFactionBonuses();

	void CalculateFearPosition();
	int32 move_tic_count;
	//bool FearTryStraight(Mob *caster, int32 duration, bool flee, VERTEX &hit, VERTEX &fv);
//	VERTEX fear_vector;
	//FearState fear_state;
	//MobFearState *fear_path_state;
	bool flee_mode;
	Timer flee_timer;

	bool pAIControlled;
	bool roamer;
	bool logging_enabled;

	int wandertype;
	int pausetype;

	int cur_wp;
	float cur_wp_x;
	float cur_wp_y;
	float cur_wp_z;
	int cur_wp_pause;
	float cur_wp_heading;

	int patrol;
	float fear_walkto_x;
	float fear_walkto_y;
	float fear_walkto_z;
	bool curfp;

	// Pathing
	//
	VERTEX PathingDestination;
	VERTEX PathingLastPosition;
	int PathingLoopCount;
	int PathingLastNodeVisited;
	list<int> Route;
	LOSType PathingLOSState;
	Timer *PathingLOSCheckTimer;
	Timer *PathingRouteUpdateTimerShort;
	Timer *PathingRouteUpdateTimerLong;
	bool DistractedFromGrid;
	int PathingTraversedNodes;

	int32 pDontHealMeBefore;
	int32 pDontBuffMeBefore;
	int32 pDontDotMeBefore;
	int32 pDontRootMeBefore;
	int32 pDontSnareMeBefore;

	// Bind wound
	Timer bindwound_timer;
	Mob* bindwound_target;
	// hp event
	int nexthpevent;
	int nextinchpevent;

	//temppet
	bool hasTempPet;

	EGNode *_egnode;	//the EG node we are in

	bool m_hasRune;
	bool m_hasSpellRune;
	bool m_hasPartialMeleeRune;
	bool m_hasPartialSpellRune;
	bool m_hasDeathSaveChance;
	uint32 m_spellHitsLeft[38]; // Used to track which spells will have their numhits incremented when spell finishes casting, 38 Buffslots
	int	flymode;
    bool m_targetable;
	int QGVarDuration(const char *fmt);
	void InsertQuestGlobal(int charid, int npcid, int zoneid, const char *name, const char *value, int expdate);

	bool m_deadFromDS;
private:
	void _StopSong();		//this is not what you think it is
	Mob* target;
};

// All data associated with a single trade
class Trade
{
public:
	Trade(Mob* in_owner);
	virtual ~Trade();

	void Reset();
	void SetTradeCash(uint32 in_pp, uint32 in_gp, uint32 in_sp, uint32 in_cp);

	// Initiate a trade with another mob
	// Also puts other mob into trader mode with this mob
	void Start(uint32 mob_id, bool initiate_with=true);

	// Mob the owner is trading with
	Mob* With();

	// Add item from cursor slot to trade bucket (automatically does bag data too)
	void AddEntity(int16 from_slot_id, int16 trade_slot_id);

	// Audit trade
	void LogTrade();

	// Debug only method
	#if (EQDEBUG >= 9)
		void DumpTrade();
	#endif

public:
	// Object state
	TradeState state;
	sint32 pp;
	sint32 gp;
	sint32 sp;
	sint32 cp;

private:
	// Send item data for trade item to other person involved in trade
	void SendItemData(const ItemInst* inst, sint16 dest_slot_id);

	uint32 with_id;
	Mob* owner;
};

#endif
