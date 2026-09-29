#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "pvisible.h"
#include "weapons.h"
#include "language.h"
#include "avp_userprofile.h"
#include "los.h"
#include "acc_loot.h"
#include "acc_route.h"
#include "acc_speech.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern DISPLAYBLOCK *Player;
static STRATEGYBLOCK *Selected;
static char Identity[SB_NAME_LENGTH];
static int SelectedType, SelectedSubtype, Collected;

static PLAYER_STATUS *EligiblePlayer(void)
{
    PLAYER_STATUS *p=Player && Player->ObStrategyBlock ? Player->ObStrategyBlock->SBdataptr:NULL;
    return p && p->IsAlive && !p->DemoMode &&
        (AvP.PlayerType==I_Marine || AvP.PlayerType==I_Predator) &&
        AvP.Network==I_No_Network ? p:NULL;
}
static int Slot(PLAYER_STATUS *p,int weapon)
{
    int i;
    for(i=0;i<MAX_NO_OF_WEAPON_SLOTS;++i) if(p->WeaponSlot[i].WeaponIDNumber==weapon) return i;
    return -1;
}
static int AmmoUseful(PLAYER_STATUS *p,int ammo)
{
    int weapon=-1,slot;
    if(AvP.PlayerType==I_Predator) {
        switch(ammo) {
            case AMMO_PRED_RIFLE:
                slot=Slot(p,WEAPON_PRED_RIFLE);
                /* AbleToPickupAmmo rejects rifle ammo only at 99 reserve
                   magazines; it clamps spear rounds after accepting it. */
                return slot>=0 && p->WeaponSlot[slot].PrimaryMagazinesRemaining!=99;
            case AMMO_PRED_DISC:
                slot=Slot(p,WEAPON_PRED_DISC);
                return slot>=0 && !p->WeaponSlot[slot].PrimaryRoundsRemaining &&
                    !p->WeaponSlot[slot].PrimaryMagazinesRemaining;
            default: return 0;
        }
    }
    if(GRENADE_MODE && ammo!=AMMO_PULSE_GRENADE) return 0;
    switch(ammo) {
        case AMMO_10MM_CULW: case AMMO_PULSE_GRENADE: weapon=WEAPON_PULSERIFLE; break;
        case AMMO_SHOTGUN: weapon=WEAPON_AUTOSHOTGUN; break;
        case AMMO_SMARTGUN: weapon=WEAPON_SMARTGUN; break;
        case AMMO_FLAMETHROWER: weapon=WEAPON_FLAMETHROWER; break;
        case AMMO_PLASMA: weapon=WEAPON_PLASMAGUN; break;
        case AMMO_SADAR_TOW: weapon=WEAPON_SADAR; break;
        case AMMO_GRENADE: weapon=WEAPON_GRENADELAUNCHER; break;
        case AMMO_MINIGUN: weapon=WEAPON_MINIGUN; break;
        case AMMO_FRISBEE: weapon=WEAPON_FRISBEE_LAUNCHER; break;
        case AMMO_MARINE_PISTOL_PC: weapon=WEAPON_MARINE_PISTOL; break;
        default: return 0;
    }
    slot=Slot(p,weapon);
    if(slot<0) return 0;
    if(ammo==AMMO_PULSE_GRENADE) return p->WeaponSlot[slot].SecondaryRoundsRemaining<99U*65536U;
    return p->WeaponSlot[slot].PrimaryMagazinesRemaining<99;
}
static const char *Text(int id,const char *fallback)
{
    const char *s;
    if(!((id>=0 && id<MAX_NO_OF_TEXTSTRINGS) || (id>MIN_NEW_TEXTSTRINGS && id<MAX_NEW_TEXTSTRINGS))) return fallback;
    s=GetTextString(id); return s && *s?s:fallback;
}
int AccLoot_Describe(STRATEGYBLOCK *s,char *name,size_t size)
{
    PLAYER_STATUS *p=EligiblePlayer(); INANIMATEOBJECT_STATUSBLOCK *o;
    const char *label=NULL; const NPC_DATA *stats; int subtype,slot,weapon;
    NPC_TYPES type=AvP.Difficulty==I_Easy?I_PC_Marine_Easy:AvP.Difficulty==I_Hard?I_PC_Marine_Hard:
        AvP.Difficulty==I_Impossible?I_PC_Marine_Impossible:I_PC_Marine_Medium;
    if(!p || !s || !name || !size || s->I_SBtype!=I_BehaviourInanimateObject ||
       !s->DynPtr || !s->SBdataptr || s->SBflags.please_destroy_me || s->SBflags.destroyed_but_preserved) return 0;
    o=s->SBdataptr; subtype=o->subType;
    if(o->respawnTimer!=0 || o->ghosted_object || o->explosionTimer) return 0;
    switch(o->typeId) {
        case IOT_Health:
            if(AvP.PlayerType!=I_Marine) return 0;
            stats=GetThisNpcData(type);
            if(!stats || (!Player->ObStrategyBlock->SBDamageBlock.IsOnFire &&
                Player->ObStrategyBlock->SBDamageBlock.Health>=stats->StartingStats.Health*65536)) return 0;
            label="Medkit"; break;
        case IOT_Armour:
            if(AvP.PlayerType!=I_Marine) return 0;
            stats=GetThisNpcData(type);
            if(!stats || Player->ObStrategyBlock->SBDamageBlock.Armour>=stats->StartingStats.Armour*65536) return 0;
            label="Armor"; break;
        case IOT_Ammo:
            if(subtype<0 || subtype>=MAX_NO_OF_AMMO_TEMPLATES || !AmmoUseful(p,subtype)) return 0;
            label=Text(TemplateAmmo[subtype].ShortName,"Ammunition"); break;
        case IOT_Weapon:
            if(subtype<0 || subtype>=MAX_NO_OF_WEAPON_TEMPLATES) return 0;
            if(AvP.PlayerType==I_Predator &&
               (subtype<WEAPON_PRED_WRISTBLADE || subtype>WEAPON_PRED_STAFF)) return 0;
            if(GRENADE_MODE && subtype!=WEAPON_PULSERIFLE) return 0;
            weapon=subtype;
            if(weapon==WEAPON_MARINE_PISTOL && (slot=Slot(p,weapon))>=0 && p->WeaponSlot[slot].Possessed==1)
                weapon=WEAPON_TWO_PISTOLS;
            slot=Slot(p,weapon);
            if(slot<0) return 0;
            /* Match the inventory's ammo requirement for an unowned weapon;
               skip duplicate weapons which provide no useful ammunition. */
            if(TemplateWeapon[subtype].PrimaryAmmoID==AMMO_NONE) {
                if(p->WeaponSlot[slot].Possessed!=0) return 0;
            } else if(!AmmoUseful(p,TemplateWeapon[subtype].PrimaryAmmoID) &&
                      !(subtype==WEAPON_PULSERIFLE && p->WeaponSlot[slot].Possessed==1 && AmmoUseful(p,AMMO_PULSE_GRENADE))) return 0;
            label=Text(TemplateWeapon[weapon].Name,"Weapon"); break;
        case IOT_FieldCharge:
            /* The engine may add medicomp ammo first, but returns failure at
               max charge, leaving this pickup in place. It cannot be selected
               as collectible until the charge is below maximum. */
            if(AvP.PlayerType!=I_Predator || p->FieldCharge>=PLAYERCLOAK_MAXENERGY) return 0;
            label="Field charge"; break;
        default: return 0;
    }
    snprintf(name,size,o->typeId==IOT_Ammo?"%.80s ammunition":"%.96s",label); return 1;
}
void AccLoot_Reset(void) { Selected=NULL; Collected=0; memset(Identity,0,sizeof(Identity)); }
static int Matches(STRATEGYBLOCK *s)
{
    INANIMATEOBJECT_STATUSBLOCK *o;
    if(s!=Selected || !s || s->I_SBtype!=I_BehaviourInanimateObject || !s->SBdataptr ||
       memcmp(s->SBname,Identity,sizeof(Identity))) return 0;
    o=s->SBdataptr; return o->typeId==SelectedType && o->subType==SelectedSubtype;
}
void AccLoot_PickedUp(STRATEGYBLOCK *s)
{ if(EligiblePlayer() && Matches(s)) { Collected=1; Selected=NULL; } }
static STRATEGYBLOCK *LiveSelection(void)
{
    int i;
    /* Never dereference a retained pointer: objects can be destroyed/reallocated. */
    for(i=0;i<NumActiveStBlocks;++i) if(Matches(ActiveStBlockList[i])) return ActiveStBlockList[i];
    return NULL;
}
int AccLoot_GetTarget(VECTORCH *point,char *name,size_t size)
{
    STRATEGYBLOCK *s;
    if(Collected) { Collected=0; return -2; }
    if(!Selected) return 0;
    s=LiveSelection();
    if(!s || !AccLoot_Describe(s,name,size)) { Selected=NULL; return -1; }
    if(point) *point=s->DynPtr->Position;
    return 1;
}
typedef struct { STRATEGYBLOCK *object; double distance; int priority,ordinal; } ITEM;
static int Compare(const void *a,const void *b)
{
    const ITEM *x=a,*y=b;
    if(x->priority!=y->priority) return x->priority-y->priority;
    return x->distance<y->distance?-1:x->distance>y->distance?1:x->ordinal-y->ordinal;
}
int AccLoot_Cycle(void)
{
    ITEM *items; int i,count=0,index=-1; char name[100],text[240];
    STRATEGYBLOCK *player=Player?Player->ObStrategyBlock:NULL;
    if(!EligiblePlayer() || !player->DynPtr || !player->containingModule || NumActiveStBlocks<=0 || NumActiveStBlocks>65536) return 0;
    items=malloc(sizeof(*items)*NumActiveStBlocks); if(!items) return 0;
    for(i=0;i<NumActiveStBlocks;++i) {
        STRATEGYBLOCK *s=ActiveStBlockList[i]; MODULE *room; AIMODULE *approach;
        VECTORCH boundary; double dx,dy,dz;
        if(!AccLoot_Describe(s,name,sizeof(name))) continue;
        room=s->containingModule?s->containingModule:ModuleFromPosition(&s->DynPtr->Position,NULL);
        if(!room || (!AccRoute_NextModule(player->containingModule->m_aimodule,room->m_aimodule) &&
           AccRoute_Frontier(player->containingModule->m_aimodule,room->m_aimodule,&approach,&boundary,NULL)!=1)) continue;
        dx=(double)s->DynPtr->Position.vx-player->DynPtr->Position.vx;
        dy=(double)s->DynPtr->Position.vy-player->DynPtr->Position.vy;
        dz=(double)s->DynPtr->Position.vz-player->DynPtr->Position.vz;
        items[count].object=s; items[count].distance=dx*dx+dy*dy+dz*dz; items[count].ordinal=i;
        items[count++].priority=((INANIMATEOBJECT_STATUSBLOCK*)s->SBdataptr)->typeId==IOT_Health?0:1;
    }
    if(!count) { free(items); AccLoot_Reset(); AccSpeech_Say("No useful supplies with a known route.",1); return 0; }
    qsort(items,count,sizeof(*items),Compare);
    for(i=0;i<count;++i) if(Matches(items[i].object)) { index=i; break; }
    Selected=items[(index+1)%count].object; Collected=0;
    memcpy(Identity,Selected->SBname,sizeof(Identity));
    SelectedType=((INANIMATEOBJECT_STATUSBLOCK*)Selected->SBdataptr)->typeId;
    SelectedSubtype=((INANIMATEOBJECT_STATUSBLOCK*)Selected->SBdataptr)->subType;
    AccLoot_Describe(Selected,name,sizeof(name));
    AccRoute_Format(&player->DynPtr->Position,player->DynPtr->OrientEuler.EulerY,&Selected->DynPtr->Position,name,text,sizeof(text));
    AccSpeech_Say(text,1); free(items); return 1;
}
int AccLoot_CloseAndVisible(void)
{
    STRATEGYBLOCK *s=LiveSelection(),*p=Player?Player->ObStrategyBlock:NULL;
    VECTORCH from,direction,oldPoint=LOS_Point,oldNormal=LOS_ObjectNormal;
    DISPLAYBLOCK *oldHit=LOS_ObjectHitPtr; SECTION_DATA *oldSection=LOS_HModel_Section;
    int oldLambda=LOS_Lambda,visible; double dx,dy,dz,length;
    if(!s || !p || !p->DynPtr || !s->DynPtr) return 0;
    from=p->DynPtr->Position; from.vy+=(Player->ObMinY+Player->ObMaxY)/2;
    dx=(double)s->DynPtr->Position.vx-from.vx;dy=(double)s->DynPtr->Position.vy-from.vy;dz=(double)s->DynPtr->Position.vz-from.vz;
    length=sqrt(dx*dx+dy*dy+dz*dz);
    if(hypot(dx,dz)>1200 || fabs(dy)>1600) return 0;
    if(length<1) return 1;
    direction.vx=(int)(dx*65536/length);direction.vy=(int)(dy*65536/length);direction.vz=(int)(dz*65536/length);
    LOS_ObjectHitPtr=NULL; LOS_Lambda=(int)ceil(length);
    FindPolygonInLineOfSight(&direction,&from,0,Player);
    visible=LOS_Lambda>=length || (LOS_ObjectHitPtr && LOS_ObjectHitPtr->ObStrategyBlock==s);
    LOS_Point=oldPoint;LOS_ObjectNormal=oldNormal;LOS_ObjectHitPtr=oldHit;LOS_HModel_Section=oldSection;LOS_Lambda=oldLambda;
    return visible;
}
