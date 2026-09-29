#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "bh_ais.h"
#include "bh_marin.h"
#include "bh_pred.h"
#include "bh_fhug.h"
#include "bh_xeno.h"
#include "los.h"
#include "psnd.h"
#include "psndproj.h"
#include "acc_combat.h"
#include "acc_speech.h"
#include "acc_tracker.h"
#include "acc_bridge.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern DISPLAYBLOCK *Player;
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
extern VECTORCH GunMuzzleDirectionInWS;
extern void GetTargetingPointOfObject(DISPLAYBLOCK *, VECTORCH *);

static int Active, Scanned, Lost, Cue = SOUND_NOACTIVEINDEX;
static unsigned int LastScan, LastSeen, LastCue, LastSpeech;
/* Never dereferenced across frames. Match only against live enumerated entries,
   plus the engine identity bytes to reject a reused allocation. */
static STRATEGYBLOCK *Identity;
static char IdentityName[SB_NAME_LENGTH], LastInstruction[96];

const char *AccCombat_HostileName(STRATEGYBLOCK *s)
{
    if (!Player || !Player->ObStrategyBlock || !s || !s->SBdataptr || !s->SBdptr || s->SBdptr == Player ||
        s->SBflags.please_destroy_me || s->SBflags.destroyed_but_preserved ||
        s->SBDamageBlock.Health <= 0) return NULL;
    switch (s->I_SBtype) {
        case I_BehaviourAlien: return NPC_IsDead(s) ? NULL : "Alien";
        case I_BehaviourPredator:
            if (NPC_IsDead(s)) return NULL;
            /* Keep the original Marine encounter behavior: every living NPC
               Predator is an enemy. Predator players use the actual target
               pointer to distinguish other hunters from allies. */
            if (AvP.PlayerType == I_Predator &&
                ((PREDATOR_STATUS_BLOCK *)s->SBdataptr)->Target != Player->ObStrategyBlock) return NULL;
            return AvP.PlayerType == I_Predator ? "Hostile Predator" : "Predator";
        case I_BehaviourPredatorAlien: return NPC_IsDead(s) ? NULL : "Predalien";
        case I_BehaviourQueenAlien: return NPC_IsDead(s) ? NULL : "Queen alien";
        case I_BehaviourFaceHugger:
            return ((FACEHUGGER_STATUS_BLOCK *)s->SBdataptr)->nearBehaviourState == FHNS_Dying ? NULL : "Facehugger";
        case I_BehaviourXenoborg:
            return ((XENO_STATUS_BLOCK *)s->SBdataptr)->behaviourState == XS_Dying ? NULL : "Xenoborg";
        case I_BehaviourMarine:
        case I_BehaviourSeal: {
            MARINE_STATUS_BLOCK *m = s->SBdataptr;
            if (NPC_IsDead(s)) return NULL;
            /* Scientists and unarmed civilians are not combat targets. */
            if (AvP.PlayerType == I_Predator) {
                if (!m->My_Weapon || m->My_Weapon->id == MNPCW_MUnarmed ||
                    m->My_Weapon->id == MNPCW_Scientist_A ||
                    m->My_Weapon->id == MNPCW_Scientist_B) return NULL;
                return m->My_Weapon->Android ? "Android" : "Soldier";
            }
            /* Marine behavior remains target-specific, including when weapon
               data is temporarily unavailable. */
            if (m->Target != Player->ObStrategyBlock || NPC_IsDead(s)) return NULL;
            return m->My_Weapon && m->My_Weapon->Android ? "Hostile android" : "Hostile soldier";
        }
        default: return NULL;
    }
}

static int Same(STRATEGYBLOCK *s)
{ return s == Identity && !memcmp(s->SBname, IdentityName, SB_NAME_LENGTH); }

static DISPLAYBLOCK *Trace(VECTORCH direction)
{
    VECTORCH from = Global_VDB_Ptr->VDB_World;
    VECTORCH point = LOS_Point, normal = LOS_ObjectNormal;
    DISPLAYBLOCK *object = LOS_ObjectHitPtr, *hit;
    SECTION_DATA *section = LOS_HModel_Section;
    int distance = LOS_Lambda;
    double length = sqrt((double)direction.vx*direction.vx +
        (double)direction.vy*direction.vy + (double)direction.vz*direction.vz);
    if (length < 1) return NULL;
    direction.vx = (int)(direction.vx*65536.0/length);
    direction.vy = (int)(direction.vy*65536.0/length);
    direction.vz = (int)(direction.vz*65536.0/length);
    FindPolygonInLineOfSight(&direction, &from, 0, Player);
    hit = LOS_ObjectHitPtr;
    LOS_Point = point; LOS_ObjectNormal = normal; LOS_ObjectHitPtr = object;
    LOS_HModel_Section = section; LOS_Lambda = distance;
    return hit;
}

static void StopCue(void)
{ if (Cue != SOUND_NOACTIVEINDEX) Sound_Stop(Cue); Cue = SOUND_NOACTIVEINDEX; }

int AccCombat_GetSnapTarget(VECTORCH *point)
{
    int i;
    if(!point || !Active || Lost || !Global_VDB_Ptr) return 0;
    for(i=0;i<NumActiveStBlocks;++i) {
        STRATEGYBLOCK *s=ActiveStBlockList[i]; VECTORCH p,d;
        if(!s || !Same(s) || !AccCombat_HostileName(s)) continue;
        GetTargetingPointOfObject(s->SBdptr,&p);
        d.vx=p.vx-Global_VDB_Ptr->VDB_World.vx;
        d.vy=p.vy-Global_VDB_Ptr->VDB_World.vy;
        d.vz=p.vz-Global_VDB_Ptr->VDB_World.vz;
        if(sqrt((double)d.vx*d.vx+(double)d.vy*d.vy+(double)d.vz*d.vz)>24000 || Trace(d)!=s->SBdptr) return 0;
        *point=p; return 1;
    }
    return 0;
}

void AccCombat_Reset(void)
{
    StopCue(); Active = Scanned = Lost = 0; Identity = NULL;
    LastInstruction[0] = 0;
}

int AccCombat_Update(unsigned int now)
{
    STRATEGYBLOCK *best = NULL, *previous = NULL;
    VECTORCH bestPoint = {0,0,0}, previousPoint = {0,0,0}, view;
    double bestDistance = 1e30, previousDistance = 1e30;
    int i, changed, aligned;
    char instruction[96], speech[160];
    const char *name;
    if (!Player || !Global_VDB_Ptr ||
        (AvP.PlayerType != I_Marine && AvP.PlayerType != I_Predator) || AvP.Network != I_No_Network) {
        AccCombat_Reset(); return 0;
    }
    if (Scanned && (unsigned int)(now-LastScan) < 100) return Active;
    Scanned = 1; LastScan = now;
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *s = ActiveStBlockList[i];
        VECTORCH p, direction;
        double distance;
        if (!AccCombat_HostileName(s)) continue;
        GetTargetingPointOfObject(s->SBdptr, &p);
        direction.vx = p.vx - Global_VDB_Ptr->VDB_World.vx;
        direction.vy = p.vy - Global_VDB_Ptr->VDB_World.vy;
        direction.vz = p.vz - Global_VDB_Ptr->VDB_World.vz;
        distance = sqrt((double)direction.vx*direction.vx + (double)direction.vy*direction.vy +
                        (double)direction.vz*direction.vz);
        if (distance > (Same(s) ? 24000 : 20000) || Trace(direction) != s->SBdptr) continue;
        if (Same(s)) { previous = s; previousPoint = p; previousDistance = distance; }
        if (distance < bestDistance) { best = s; bestPoint = p; bestDistance = distance; }
    }
    /* Keep the current visible target unless another is substantially closer. */
    if (previous && previousDistance <= bestDistance*1.8) {
        best = previous; bestPoint = previousPoint;
    }
    if (!best) {
        StopCue();
        if (!Active) return 0;
        if (!Lost) { AccSpeech_Say("Target no longer in sight.", 1); Lost = 1; }
        if ((unsigned int)(now-LastSeen) < 1200) return 1;
        AccCombat_Reset();
        AccSpeech_Say("No visible target nearby. Route guidance resumed.", 0);
        return 0;
    }
    changed = !Active || !Same(best) || Lost;
    Active = 1; Lost = 0; LastSeen = now;
    Identity = best; memcpy(IdentityName, best->SBname, SB_NAME_LENGTH);
    name = AccCombat_HostileName(best);
    view.vx = bestPoint.vx-Global_VDB_Ptr->VDB_World.vx;
    view.vy = bestPoint.vy-Global_VDB_Ptr->VDB_World.vy;
    view.vz = bestPoint.vz-Global_VDB_Ptr->VDB_World.vz;
    _RotateVector(&view, &Global_VDB_Ptr->VDB_Mat);
    /* Match the weapon's actual current ray, not a generous angle cone. This
       is alignment feedback, not a promise about recoil/projectile flight. */
    aligned = Trace(GunMuzzleDirectionInWS) == best->SBdptr;
    if (aligned) strcpy(instruction, "On target.");
    else {
        double horizontal = atan2((double)view.vx, (double)view.vz)*57.295779513;
        double vertical = atan2((double)view.vy, sqrt((double)view.vx*view.vx+(double)view.vz*view.vz))*57.295779513;
        if (view.vz < 0 || fabs(horizontal) > 15)
            snprintf(instruction, sizeof(instruction), "Turn %s, %d degrees.",
                horizontal < 0 ? "left" : "right", (int)(fabs(horizontal)/15+0.5)*15);
        else if (fabs(horizontal) >= 1.0 && fabs(horizontal) >= fabs(vertical))
            snprintf(instruction, sizeof(instruction), "Aim %s%s.", horizontal < 0 ? "left" : "right",
                fabs(vertical) >= 2 ? (vertical < 0 ? " and up" : " and down") : "");
        else if (fabs(vertical) >= 1.0)
            snprintf(instruction, sizeof(instruction), "Aim %s.", vertical < 0 ? "up" : "down");
        else strcpy(instruction, "Adjust aim slightly.");
    }
    if (changed || ((strcmp(instruction, LastInstruction) || (unsigned int)(now-LastSpeech) >= 4000)
                    && (unsigned int)(now-LastSpeech) >= 700)) {
        snprintf(speech, sizeof(speech), "%s%s%s", changed ? "Targeting. " : "",
                 changed ? name : "", changed ? ". " : "");
        strncat(speech, instruction, sizeof(speech)-strlen(speech)-1);
        AccSpeech_Say(speech, 1); strcpy(LastInstruction, instruction); LastSpeech = now;
    }
    if (changed || (unsigned int)(now-LastCue) >= (aligned ? 200u : 450u)) {
        StopCue();
        AccBridge_BeginCue("combat", aligned ? SID_TRACKER_WHEEP_HIGH : SID_TRACKER_WHEEP);
        AccTracker_PlayContact(aligned ? SID_TRACKER_WHEEP_HIGH : SID_TRACKER_WHEEP,
                              &bestPoint, 24000, &Cue, 80);
        AccBridge_EndCue(); LastCue = now;
    }
    return 1;
}
