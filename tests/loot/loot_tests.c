#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "pvisible.h"
#include "weapons.h"
#include "language.h"
#include "los.h"
#include "avp_userprofile.h"
#include "acc_loot.h"
#include "acc_route.h"
#include <stdio.h>
#include <string.h>

DISPLAYBLOCK *Player;
int NumActiveStBlocks;
STRATEGYBLOCK *ActiveStBlockList[16];
VECTORCH LOS_Point, LOS_ObjectNormal;
DISPLAYBLOCK *LOS_ObjectHitPtr;
SECTION_DATA *LOS_HModel_Section;
int LOS_Lambda;
AVP_GAME_DESC AvP;
int CheatMode_Active;
TEMPLATE_WEAPON_DATA TemplateWeapon[MAX_NO_OF_WEAPON_TEMPLATES];
TEMPLATE_AMMO_DATA TemplateAmmo[MAX_NO_OF_AMMO_TEMPLATES];
static DISPLAYBLOCK playerDisplay, lootDisplay;
static STRATEGYBLOCK playerSB, loot[16];
static DYNAMICSBLOCK playerDyn, lootDyn[16];
static PLAYER_STATUS ps;
static INANIMATEOBJECT_STATUSBLOCK obj[16];
static MODULE room;
static AIMODULE airoom;
static NPC_DATA npc;
static int checks, failures, losMode;

AIMODULE *AccRoute_NextModule(AIMODULE *a,AIMODULE *b) { return a && a==b ? a:NULL; }
int AccRoute_Frontier(AIMODULE *a,AIMODULE *b,AIMODULE **c,VECTORCH *d,AIMODULE **e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
int AccRoute_Format(const VECTORCH *a,int y,const VECTORCH *b,const char *n,char *t,size_t z)
{ (void)a;(void)y;(void)b; snprintf(t,z,"%s",n); return 1; }
void AccSpeech_Say(const char *s,int i) { (void)s;(void)i; }
NPC_DATA *GetThisNpcData(NPC_TYPES t) { (void)t; return &npc; }
char *GetTextString(int id) { (void)id; return "Test pickup"; }
MODULE *ModuleFromPosition(VECTORCH *p,MODULE *m) { (void)p; return m?m:&room; }
void FindPolygonInLineOfSight(VECTORCH *d,VECTORCH *p,int u,DISPLAYBLOCK *i)
{ (void)d;(void)p;(void)u;(void)i;
  LOS_Lambda=losMode==1?10000:losMode==2?500:0;
  LOS_ObjectHitPtr=losMode==1?&lootDisplay:losMode==2?&playerDisplay:NULL; }

static void check(int ok,const char *label)
{ ++checks; if(!ok){++failures; printf("FAIL %s\n",label);} }
static void reset(void)
{
    int i;
    memset(&ps,0,sizeof(ps)); memset(&playerSB,0,sizeof(playerSB)); memset(&playerDyn,0,sizeof(playerDyn));
    memset(&playerDisplay,0,sizeof(playerDisplay)); memset(obj,0,sizeof(obj)); memset(loot,0,sizeof(loot)); memset(lootDyn,0,sizeof(lootDyn));
    ps.IsAlive=1; AvP.PlayerType=I_Marine; AvP.Network=I_No_Network; AvP.Difficulty=I_Medium;
    playerSB.SBdataptr=&ps; playerSB.DynPtr=&playerDyn; playerSB.containingModule=&room;
    playerDisplay.ObStrategyBlock=&playerSB; playerDisplay.ObMinY=-500; playerDisplay.ObMaxY=500; Player=&playerDisplay;
    room.m_aimodule=&airoom; playerDyn.Position.vz=0;
    npc.StartingStats.Health=100; npc.StartingStats.Armour=100;
    playerSB.SBDamageBlock.Health=100*65536; playerSB.SBDamageBlock.Armour=100*65536;
    for(i=0;i<MAX_NO_OF_WEAPON_SLOTS;i++) ps.WeaponSlot[i].WeaponIDNumber=(enum WEAPON_ID)i;
    ps.WeaponSlot[WEAPON_PULSERIFLE].Possessed=1;
    ps.WeaponSlot[WEAPON_PULSERIFLE].PrimaryMagazinesRemaining=0;
    ps.WeaponSlot[WEAPON_PULSERIFLE].SecondaryRoundsRemaining=0;
    TemplateWeapon[WEAPON_PULSERIFLE].PrimaryAmmoID=AMMO_10MM_CULW;
    TemplateWeapon[WEAPON_MARINE_PISTOL].PrimaryAmmoID=AMMO_MARINE_PISTOL_PC;
    TemplateWeapon[WEAPON_AUTOSHOTGUN].PrimaryAmmoID=AMMO_SHOTGUN;
    TemplateWeapon[WEAPON_SMARTGUN].PrimaryAmmoID=AMMO_SMARTGUN;
    TemplateWeapon[WEAPON_FLAMETHROWER].PrimaryAmmoID=AMMO_FLAMETHROWER;
    TemplateWeapon[WEAPON_PLASMAGUN].PrimaryAmmoID=AMMO_PLASMA;
    TemplateWeapon[WEAPON_SADAR].PrimaryAmmoID=AMMO_SADAR_TOW;
    TemplateWeapon[WEAPON_GRENADELAUNCHER].PrimaryAmmoID=AMMO_GRENADE;
    TemplateWeapon[WEAPON_MINIGUN].PrimaryAmmoID=AMMO_MINIGUN;
    TemplateWeapon[WEAPON_FRISBEE_LAUNCHER].PrimaryAmmoID=AMMO_FRISBEE;
    for(i=0;i<16;i++) ActiveStBlockList[i]=NULL;
    NumActiveStBlocks=0; CheatMode_Active=CHEATMODE_NONACTIVE; losMode=1;
    LOS_Point.vx=11; LOS_Point.vy=12; LOS_Point.vz=13;
    LOS_ObjectNormal.vx=21; LOS_ObjectNormal.vy=22; LOS_ObjectNormal.vz=23;
    LOS_ObjectHitPtr=&playerDisplay; LOS_HModel_Section=(SECTION_DATA*)1; LOS_Lambda=321;
    AccLoot_Reset();
}
static STRATEGYBLOCK *add(int n,int type,int subtype,int z)
{
    memset(&loot[n],0,sizeof(loot[n])); memset(&lootDyn[n],0,sizeof(lootDyn[n]));
    memset(&obj[n],0,sizeof(obj[n]));
    loot[n].I_SBtype=I_BehaviourInanimateObject; loot[n].SBdataptr=&obj[n]; loot[n].DynPtr=&lootDyn[n];
    loot[n].containingModule=&room; snprintf(loot[n].SBname,sizeof(loot[n].SBname),"pickup-%d",n);
    obj[n].typeId=(INANIMATEOBJECT_TYPE)type; obj[n].subType=subtype; lootDyn[n].Position.vz=z;
    ActiveStBlockList[NumActiveStBlocks++]=&loot[n]; lootDisplay.ObStrategyBlock=&loot[n]; return &loot[n];
}
int main(void)
{
    char name[128]; VECTORCH point; STRATEGYBLOCK *s;
    reset(); s=add(0,IOT_Health,0,1000);
    check(!AccLoot_Describe(s,name,sizeof(name)),"full health medkit omitted");
    playerSB.SBDamageBlock.Health=50*65536;
    check(AccLoot_Describe(s,name,sizeof(name)),"injured Marine medkit offered");
    playerSB.SBDamageBlock.IsOnFire=1; playerSB.SBDamageBlock.Health=100*65536;
    check(AccLoot_Describe(s,name,sizeof(name)),"burning Marine may use medkit at full health");
    s=add(1,IOT_Armour,0,1100);
    check(!AccLoot_Describe(s,name,sizeof(name)),"full armor omitted");
    playerSB.SBDamageBlock.Armour=1;
    check(AccLoot_Describe(s,name,sizeof(name)),"missing armor offered");
    s=add(2,IOT_Ammo,AMMO_10MM_CULW,1200);
    check(AccLoot_Describe(s,name,sizeof(name)),"ammo below capacity and with weapon slot offered");
    ps.WeaponSlot[WEAPON_PULSERIFLE].PrimaryMagazinesRemaining=99;
    check(!AccLoot_Describe(s,name,sizeof(name)),"full magazine reserve omitted");
    ps.WeaponSlot[WEAPON_PULSERIFLE].PrimaryMagazinesRemaining=0;
    CheatMode_Active=CHEATMODE_GRENADE;
    check(!AccLoot_Describe(s,name,sizeof(name)),"grenade mode filters ordinary ammo");
    s=add(3,IOT_Ammo,AMMO_PULSE_GRENADE,1250);
    check(AccLoot_Describe(s,name,sizeof(name)),"grenade mode permits pulse grenade ammo");
    s=add(4,IOT_Weapon,WEAPON_AUTOSHOTGUN,1300);
    check(!AccLoot_Describe(s,name,sizeof(name)),"grenade mode filters non-pulse weapon");
    s=add(5,IOT_Weapon,WEAPON_PULSERIFLE,1400);
    check(AccLoot_Describe(s,name,sizeof(name)),"grenade mode permits pulse rifle");
    CheatMode_Active=CHEATMODE_NONACTIVE;
    s=ActiveStBlockList[2];
    s->SBflags.please_destroy_me=1;
    check(!AccLoot_Describe(s,name,sizeof(name)),"destroying object omitted");
    s->SBflags.please_destroy_me=0; s->SBflags.destroyed_but_preserved=1;
    check(!AccLoot_Describe(s,name,sizeof(name)),"preserved destroyed object omitted");
    s->SBflags.destroyed_but_preserved=0; obj[2].respawnTimer=1;
    check(!AccLoot_Describe(s,name,sizeof(name)),"hidden respawning pickup omitted");
    obj[2].respawnTimer=0; obj[2].ghosted_object=1;
    check(!AccLoot_Describe(s,name,sizeof(name)),"ghost pickup omitted");
    obj[2].ghosted_object=0;
    NumActiveStBlocks=1; ActiveStBlockList[0]=s;
    check(AccLoot_Cycle()==1,"cycle selects useful pickup");
    check(AccLoot_GetTarget(&point,name,sizeof(name))==1 && point.vz==1200,"target point returned");
    AccLoot_PickedUp(s);
    check(AccLoot_GetTarget(&point,name,sizeof(name))==-2,"matching pickup confirms collection");
    check(AccLoot_GetTarget(&point,name,sizeof(name))==0,"collection confirmation is one-shot");
    reset(); s=add(0,IOT_Health,0,1000); playerSB.SBDamageBlock.Health=20*65536;
    check(AccLoot_Cycle()==1,"select before disappearance");
    NumActiveStBlocks=0;
    check(AccLoot_GetTarget(&point,name,sizeof(name))==-1,"vanished object is lost without false collection");
    check(AccLoot_GetTarget(&point,name,sizeof(name))==0,"vanished selection cleared");
    reset(); s=add(0,IOT_Health,0,1000); playerSB.SBDamageBlock.Health=20*65536;
    check(AccLoot_Cycle()==1,"select before identity reuse");
    loot[0].SBname[0]='x';
    check(AccLoot_GetTarget(&point,name,sizeof(name))==-1,"reused slot with changed identity rejected");
    reset();
    s=add(0,IOT_Health,0,1000); playerSB.SBDamageBlock.Health=20*65536;
    add(1,IOT_Ammo,AMMO_10MM_CULW,1100);
    check(AccLoot_Cycle()==1 && AccLoot_GetTarget(&point,name,sizeof(name))==1 && !strcmp(name,"Medkit"),
          "selection prioritizes useful medkit");
    check(AccLoot_Cycle()==1 && AccLoot_GetTarget(&point,name,sizeof(name))==1 && !strcmp(name,"Test pickup ammunition"),
          "cycle advances through remaining useful supplies");
    reset(); s=add(0,IOT_Health,0,1000); playerSB.SBDamageBlock.Health=20*65536;
    AccLoot_Cycle(); losMode=2;
    check(!AccLoot_CloseAndVisible(),"occluded nearby loot rejected");
    check(LOS_Point.vx==11 && LOS_Point.vy==12 && LOS_Point.vz==13 &&
          LOS_ObjectNormal.vx==21 && LOS_ObjectNormal.vy==22 && LOS_ObjectNormal.vz==23 &&
          LOS_ObjectHitPtr==&playerDisplay && LOS_HModel_Section==(SECTION_DATA*)1 && LOS_Lambda==321,
          "LOS globals restored after obstruction query");
    losMode=1; lootDyn[0].Position.vz=3000;
    check(!AccLoot_CloseAndVisible(),"distant loot not close");
    lootDyn[0].Position.vz=1000;
    check(AccLoot_CloseAndVisible(),"near visible loot accepted");
    check(LOS_Point.vx==11 && LOS_ObjectHitPtr==&playerDisplay && LOS_HModel_Section==(SECTION_DATA*)1 && LOS_Lambda==321,
          "LOS globals restored after visible query");
    reset(); s=add(0,IOT_Health,0,1000); playerSB.SBDamageBlock.Health=20*65536;
    playerSB.containingModule=NULL;
    check(!AccLoot_Cycle(),"selection unavailable without current room route");
    playerSB.containingModule=&room; { MODULE unreachable={0};
        s->containingModule=&unreachable;
        check(!AccLoot_Cycle(),"disconnected pickup excluded from selection");
    }
    reset(); AvP.PlayerType=I_Predator;
    ps.WeaponSlot[0].WeaponIDNumber=WEAPON_PRED_RIFLE;
    ps.WeaponSlot[1].WeaponIDNumber=WEAPON_PRED_DISC;
    ps.WeaponSlot[2].WeaponIDNumber=WEAPON_PRED_MEDICOMP;
    TemplateWeapon[WEAPON_PRED_RIFLE].PrimaryAmmoID=AMMO_PRED_RIFLE;
    TemplateWeapon[WEAPON_PRED_PISTOL].PrimaryAmmoID=AMMO_PRED_PISTOL;
    TemplateWeapon[WEAPON_PRED_SHOULDERCANNON].PrimaryAmmoID=AMMO_PRED_ENERGY_BOLT;
    TemplateWeapon[WEAPON_PRED_MEDICOMP].PrimaryAmmoID=AMMO_NONE;
    s=add(0,IOT_FieldCharge,0,1000);
    ps.FieldCharge=PLAYERCLOAK_MAXENERGY-1;
    check(AccLoot_Describe(s,name,sizeof(name)) && !strcmp(name,"Field charge"),"depleted Predator field charge offered");
    ps.WeaponSlot[2].PrimaryRoundsRemaining=0;
    ps.FieldCharge=PLAYERCLOAK_MAXENERGY;
    check(!AccLoot_Describe(s,name,sizeof(name)),"full Predator field charge omitted because engine would not collect it");
    check(ps.WeaponSlot[2].PrimaryRoundsRemaining==0,"loot inspection does not apply engine's medicomp-ammo side effect");
    s=add(1,IOT_Ammo,AMMO_PRED_RIFLE,1100);
    ps.WeaponSlot[0].PrimaryRoundsRemaining=0;
    check(AccLoot_Describe(s,name,sizeof(name)),"Predator spear ammunition offered below capacity");
    ps.WeaponSlot[0].PrimaryRoundsRemaining=MAX_SPEARS*ONE_FIXED;
    check(AccLoot_Describe(s,name,sizeof(name)),"engine-accepted spear pickup remains selectable at round cap when magazine reserve is below 99");
    ps.WeaponSlot[0].PrimaryMagazinesRemaining=99;
    check(!AccLoot_Describe(s,name,sizeof(name)),"Predator spear ammo omitted at the engine magazine limit");
    s=add(2,IOT_Ammo,AMMO_PRED_DISC,1200);
    ps.WeaponSlot[1].PrimaryRoundsRemaining=0;
    ps.WeaponSlot[1].PrimaryMagazinesRemaining=0;
    check(AccLoot_Describe(s,name,sizeof(name)),"Predator disc offered when no disc is carried");
    ps.WeaponSlot[1].PrimaryRoundsRemaining=ONE_FIXED;
    check(!AccLoot_Describe(s,name,sizeof(name)),"duplicate Predator disc omitted");
    s=add(3,IOT_Health,0,1300);
    check(!AccLoot_Describe(s,name,sizeof(name)),"Predator does not receive Marine medkits");
    s=add(4,IOT_Armour,0,1400);
    check(!AccLoot_Describe(s,name,sizeof(name)),"Predator does not receive Marine armor");
    s=add(5,IOT_Ammo,AMMO_10MM_CULW,1500);
    check(!AccLoot_Describe(s,name,sizeof(name)),"Predator ignores Marine ammunition");
    s=add(6,IOT_Weapon,WEAPON_PRED_RIFLE,1600);
    ps.WeaponSlot[0].PrimaryRoundsRemaining=0; ps.WeaponSlot[0].PrimaryMagazinesRemaining=0;
    check(AccLoot_Describe(s,name,sizeof(name)),"unowned Predator rifle pickup uses its real supported ammo type");
    ps.WeaponSlot[0].PrimaryMagazinesRemaining=99;
    check(!AccLoot_Describe(s,name,sizeof(name)),"unowned rifle hidden when engine refuses its ammo pickup");
    s=add(7,IOT_Weapon,WEAPON_PRED_PISTOL,1700);
    check(!AccLoot_Describe(s,name,sizeof(name)),"unowned Predator pistol hidden because engine rejects its ammo type");
    s=add(8,IOT_Weapon,WEAPON_PRED_SHOULDERCANNON,1800);
    check(!AccLoot_Describe(s,name,sizeof(name)),"unowned shoulder cannon hidden because engine rejects energy bolt ammo");
    s=add(9,IOT_Weapon,WEAPON_PRED_MEDICOMP,1900);
    check(AccLoot_Describe(s,name,sizeof(name)),"unowned no-ammo Predator medicomp pickup is surfaced");
    ps.WeaponSlot[2].Possessed=1;
    check(!AccLoot_Describe(s,name,sizeof(name)),"duplicate no-ammo medicomp pickup is suppressed like engine");
    puts(failures?"loot tests failed":"loot tests passed");
    printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
