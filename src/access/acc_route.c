#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "pfarlocs.h"
#include "pvisible.h"
#include "pheromon.h"
#include "psnd.h"
#include "psndproj.h"
#include "acc_route.h"
#include "acc_route_targets.h"
#include "acc_objectives.h"
#include "acc_speech.h"
#include "acc_tracker.h"
#include "acc_bridge.h"
#include "acc_pad.h"
#include "triggers.h"
#include "acc_traversal.h"
#include "acc_combat.h"
#include "acc_wayroute.h"
#include "acc_lift_route.h"
#include "acc_loot.h"
#include "acc_fall_route.h"
#include "acc_jump_assist_runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern char LevelName[];

extern DISPLAYBLOCK *Player;
static int Enabled, Fresh, CueHandle = SOUND_NOACTIVEINDEX;
static unsigned int LastCheck, LastSpeech;
static char LastText[256];
static int LastModule = -1;
static int LastTraversal;
static int SpokenModule = -1, SpokenSteering, SpokenTraversal, SpokenLift;
static int HaveWaypoint, WaypointRoom, WaypointLift, WaypointBodyY;
static int WaitingOnLift;
static int LootMode, ResumeMission;
static int HaveSnap, SnapAim;
static int SnapQuery;
static unsigned int FallLastCue;
static int FallLastCuePhase = -1, FallLastCueAction = -1;
static int SpokenFallPhase = -1;
static int SpokenFallAction = -1;
static VECTORCH SnapPoint;
static VECTORCH RouteWaypoint, WaypointGoal;
static ACC_FALL_ROUTE_STATE FallRouteState;
static int FallRouteHot;
static int FallFinalLiftRouteActive;
static STRATEGYBLOCK *FallFinalLiftPlatform;
static void *FallFinalLiftObjective;

/* A bounded manual approach from the surveyed south/lower stair segment of
   Fall room 85 to the upper stair landing beside the final-lift switch. */
static int FallFinalLiftNeedsSwitchCorner(const VECTORCH *feet,int grounded)
{
    if(!feet) return 0;
    if(grounded && feet->vz>=58700 && feet->vy<=29900) return 0;
    /* Stay on the measured upper-stair waypoint across the whole bounded
       south-to-switch band. A 1 s route tick can otherwise skip the narrow
       pre-corner interval; being beyond z alone is not the passed condition. */
    return feet->vx>=186500 && feet->vx<=191500 &&
           feet->vz>=48000 && feet->vz<=65000 &&
           feet->vy>=26000 && feet->vy<=35000;
}

/* Directional, live-surveyed descents, not a waiver of NPC vertical flags.
   Match the retail portal as well as its room IDs to fail closed on other maps. */
static int SurveyedDescent(AIMODULE *from, AIMODULE *to, FARENTRYPOINT *ep)
{
    VECTORCH expected, p;
    if(strcmp(LevelName,"derelict") || !from || !to || !ep) return 0;
    if(from->m_index==75 && to->m_index==76)
        expected=(VECTORCH){-28055,5614,-192795};
    else if(from->m_index==76 && to->m_index==158)
        expected=(VECTORCH){-31011,20591,-199200};
    else if(from->m_index==171 && to->m_index==159)
        expected=(VECTORCH){-116432,39485,-171232};
    else return 0;
    p=ep->position; p.vx+=to->m_world.vx; p.vy+=to->m_world.vy; p.vz+=to->m_world.vz;
    return abs(p.vx-expected.vx)<100 && abs(p.vy-expected.vy)<100 && abs(p.vz-expected.vz)<100;
}

/* Surveyed Marine approach from VIEWD-05's lower floor to SHIPCOR-01.
   The NPC volumes mark the raised lip Alien-only; slot-2 replay verified this
   walk-around and forward jump. Scope to this exact level, room and exit. */
static int SurveyedPassage(AIMODULE *room,const VECTORCH *body,const VECTORCH *exit,VECTORCH *out,int *jump)
{
    if(strcmp(LevelName,"derelict") || !room || room->m_index!=69 ||
       abs(exit->vx+46297)>100 || abs(exit->vz+209698)>100 ||
       body->vx < -49000 || body->vx > -35000 || body->vz < -227000 || body->vz > -210000 ||
       body->vy < 1000 || body->vy > 6500) return 0;
    if(body->vy<2600 && body->vz>-216000) return 0;
    *jump=0;
    if(body->vz>-221500 && body->vx<-43000)
        *out=(VECTORCH){-44860,5019,-223304}; /* back out of the recess */
    else if(body->vx<-42000)
        *out=(VECTORCH){-40361,5102,-224358}; /* cross the lower floor */
    else if(body->vz<-220400 && hypot(body->vx+40843.0,body->vz+220167.0)>900)
        *out=(VECTORCH){-40843,3400,-220167}; /* approach the raised opening */
    else {
        *out=(VECTORCH){-40159,1825,-214670};
        *jump=1;
    }
    return 1;
}

static int ValidModule(AIMODULE *m)
{
    return m && AIModuleArray && m->m_index >= 0 && m->m_index < AIModuleArraySize
        && &AIModuleArray[m->m_index] == m;
}

AIMODULE *AccRoute_NextModule(AIMODULE *source, AIMODULE *target)
{
    int *queue, *first, head = 0, tail = 0, i;
    AIMODULE *result = NULL;
    if (!ValidModule(source) || !ValidModule(target) || !FALLP_EntryPoints) return NULL;
    if (source == target) return source;
    /* One entry per module, no 100-entry circular queue overflow. */
    if (AIModuleArraySize <= 0 || AIModuleArraySize > 65536) return NULL;
    queue = (int *)malloc(sizeof(int) * AIModuleArraySize);
    first = (int *)malloc(sizeof(int) * AIModuleArraySize);
    if (!queue || !first) { free(queue); free(first); return NULL; }
    for (i = 0; i < AIModuleArraySize; ++i) first[i] = -1;
    first[source->m_index] = source->m_index;
    queue[tail++] = source->m_index;
    while (head < tail && !result) {
        AIMODULE *current = &AIModuleArray[queue[head++]];
        AIMODULE **link = current->m_link_ptrs;
        if (!link) continue;
        for (; *link; ++link) {
            AIMODULE *next = *link;
            FARENTRYPOINT *ep;
            if (!ValidModule(next) || first[next->m_index] != -1
                || !AIModuleIsPhysical(next) || !AIModuleAdmitsPheromones(next)) continue;
            ep = GetAIModuleEP(next, current);
            if (!ep || (ep->alien_only && !SurveyedDescent(current,next,ep))) continue;
            first[next->m_index] = current == source ? next->m_index : first[current->m_index];
            if (next == target) { result = &AIModuleArray[first[next->m_index]]; break; }
            queue[tail++] = next->m_index;
        }
    }
    free(queue); free(first);
    return result;
}

static int RouteFrontierSearch(AIMODULE *source, AIMODULE *target, AIMODULE **approach, VECTORCH *point,
                       AIMODULE **blocked, const unsigned char *excluded, int allowAlien)
{
    int *queue, *parent, head = 0, tail = 0, i, at, reason = 0;
    if (blocked) *blocked = NULL;
    if (!approach || !point || !ValidModule(source) || !ValidModule(target)
        || !FALLP_EntryPoints || AIModuleArraySize <= 0 || AIModuleArraySize > 65536) return 0;
    queue = (int *)malloc(sizeof(int) * AIModuleArraySize);
    parent = (int *)malloc(sizeof(int) * AIModuleArraySize);
    if (!queue || !parent) { free(queue); free(parent); return 0; }
    for (i = 0; i < AIModuleArraySize; ++i) parent[i] = -1;
    parent[source->m_index] = source->m_index;
    queue[tail++] = source->m_index;
    /* This second search identifies a boundary, never licenses crossing it. */
    while (head < tail && parent[target->m_index] < 0) {
        AIMODULE *current = &AIModuleArray[queue[head++]];
        AIMODULE **link = current->m_link_ptrs;
        if (!link) continue;
        for (; *link; ++link) {
            AIMODULE *next = *link;
            FARENTRYPOINT *entry;
            if (!ValidModule(next) || parent[next->m_index] >= 0
                || (excluded && excluded[next->m_index])
                || !AIModuleIsPhysical(next)) continue;
            entry=GetAIModuleEP(next,current);
            if (!entry || (!allowAlien && entry->alien_only && !SurveyedDescent(current,next,entry))) continue;
            parent[next->m_index] = current->m_index;
            queue[tail++] = next->m_index;
        }
    }
    if (parent[target->m_index] >= 0) {
        /* Walk back, replacing the boundary until the first one is reached. */
        for (at = target->m_index; at != source->m_index; at = parent[at]) {
            AIMODULE *next = &AIModuleArray[at], *before = &AIModuleArray[parent[at]];
            FARENTRYPOINT *ep = GetAIModuleEP(next, before);
            if ((ep->alien_only && !SurveyedDescent(before,next,ep)) || !AIModuleAdmitsPheromones(next)) {
                FARENTRYPOINT *nearSide = GetAIModuleEP(before, next);
                /* A point on the reachable side is essential. */
                reason = 0;
                if (blocked) *blocked = NULL;
                if (nearSide) {
                    if (blocked) *blocked = next;
                    *approach = before;
                    *point = nearSide->position;
                    point->vx += before->m_world.vx;
                    point->vy += before->m_world.vy;
                    point->vz += before->m_world.vz;
                    reason = ep->alien_only && !SurveyedDescent(before,next,ep) ? 2 : 1;
                }
            }
        }
    }
    free(queue); free(parent);
    return reason;
}

static int RouteFrontier(AIMODULE *source, AIMODULE *target, AIMODULE **approach, VECTORCH *point,
                       AIMODULE **blocked, const unsigned char *excluded)
{
    /* A closed Marine door can have an opening control. An Alien-only shortcut
       must not win just because it uses fewer rooms or appears first in links.
       Retain the restricted-boundary report only when no Marine chain exists. */
    int reason=RouteFrontierSearch(source,target,approach,point,blocked,excluded,0);
    if (reason) return reason;
    return RouteFrontierSearch(source,target,approach,point,blocked,excluded,1);
}

int AccRoute_Frontier(AIMODULE *source, AIMODULE *target, AIMODULE **approach, VECTORCH *point,
                       AIMODULE **blocked)
{
    return RouteFrontier(source, target, approach, point, blocked, NULL);
}

int AccRoute_Format(const VECTORCH *player, int yaw, const VECTORCH *point,
                    const char *label, char *text, size_t size)
{
    double dx, dz, distance, angle;
    int clock, metres, written;
    if (!player || !point || !label || !text || !size) return 0;
    dx = (double)point->vx - player->vx;
    dz = (double)point->vz - player->vz;
    distance = sqrt(dx * dx + dz * dz);
    angle = atan2(dx, dz) - (yaw & 4095) * (6.283185307179586 / 4096.0);
    clock = ((int)floor(angle * 6.0 / 3.141592653589793 + 0.5) % 12 + 12) % 12;
    if (!clock) clock = 12;
    metres = (int)floor(distance / 1000.0 + 0.5);
    if (distance < 1000)
        written = snprintf(text, size, "%s, %d o'clock, within one metre.", label, clock);
    else
        written = snprintf(text, size, "%s, %d o'clock, %d %s.", label, clock,
                           metres, metres == 1 ? "metre" : "metres");
    return written >= 0 && (size_t)written < size;
}

int AccRoute_IsEnabled(void)
{
    return Enabled;
}

void AccRoute_Reset(void)
{
    HaveSnap=0;
    AccLoot_Reset();
    LootMode=ResumeMission=0;
    AccCombat_Reset();
    AccTraversal_Reset();
    AccLiftRoute_Reset();
    FallFinalLiftRouteActive=0;
    FallFinalLiftPlatform=NULL;
    FallFinalLiftObjective=NULL;
    HaveWaypoint=0;
    WaitingOnLift=0;
    AccFallRoute_Reset(&FallRouteState);
    FallRouteHot=0;
    FallLastCue=0; FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1;
    Enabled = Fresh = 0;
    LastText[0] = 0;
    LastModule = -1;
    LastTraversal = ACC_TRAVERSAL_NONE;
    SpokenModule = -1;
    SpokenSteering = ACC_STEER_DIRECT;
    SpokenTraversal = ACC_TRAVERSAL_NONE;
    SpokenLift=0;
    if (CueHandle != SOUND_NOACTIVEINDEX) Sound_Stop(CueHandle);
    CueHandle = SOUND_NOACTIVEINDEX;
}

void AccRoute_Toggle(void)
{
    if (Enabled) { AccRoute_Reset(); AccSpeech_Say("Route guidance off.", 1); }
    else {
        Enabled = Fresh = 1;
        LastText[0] = 0;
        AccSpeech_Say("Route guidance on.", 1);
    }
}

static void EndLoot(int result)
{
    int resume=ResumeMission;
    AccRoute_Reset();
    Enabled=Fresh=resume;
    AccSpeech_Say(result==-2 ?
        (resume?"Collected. Mission guidance resumed.":"Collected. Guidance off.") :
        result==-1 ? (resume?"Supply unavailable. Mission guidance resumed.":"Supply unavailable. Guidance off.") :
        (resume?"Mission guidance resumed.":"Supply guidance off."),0);
}

void AccRoute_CycleLoot(void)
{
    if(!AccLoot_Cycle()) { if(LootMode) EndLoot(-1); return; }
    if(LootMode) {
        HaveWaypoint=WaitingOnLift=0;
        AccTraversal_Reset(); AccLiftRoute_Reset(); Fresh=1;
    }
}

void AccRoute_ToggleLoot(void)
{
    VECTORCH point; char name[96];
    if(LootMode) { EndLoot(0); return; }
    if(AccLoot_GetTarget(&point,name,sizeof(name))!=1) {
        if(!AccLoot_Cycle() || AccLoot_GetTarget(&point,name,sizeof(name))!=1) return;
    }
    ResumeMission=Enabled; LootMode=1; Enabled=Fresh=1;
    HaveWaypoint=WaitingOnLift=0;
    AccFallRoute_Reset(&FallRouteState); FallRouteHot=0; FallLastCue=0;
    FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1;
    AccTraversal_Reset(); AccLiftRoute_Reset();
    AccSpeech_Say("Supply guidance. Walk into the pickup to collect it.",0);
}

void AccRoute_Update(unsigned int nowMs)
{
    STRATEGYBLOCK *sb;
    VECTORCH point, controlAimPoint;
    VECTORCH finalLiftGoalPoint, finalLiftRoutePoint, finalLiftSwitchPoint;
    MODULE *destination;
    AIMODULE *source, *next, *approach, *blocked = NULL;
    STRATEGYBLOCK *control = NULL;
    STRATEGYBLOCK *finalLiftPlatform = NULL;
    void *routeObjective = NULL;
    int controlArea = 0, noControl = 0, ready = 0, fallGate05Control = 0;
    int fallFinalLiftControl = 0, fallFinalLiftRoute = 0, fallFinalLiftWait = 0;
    int fallFinalLiftExitStage = 0, fallFinalLiftResult = 0, fallFinalLiftCorner = 0;
    int lift=0;
    int surveyed=0,surveyedJump=0,descent=0;
    int traversal = ACC_TRAVERSAL_NONE;
    int steering = ACC_STEER_DIRECT;
    int i, count, area = 0, found = 0, actionable = 0, boundary = 0;
    int predatorOpening = 0, gateApproach = 0;
    int fallGuidance = 0, fallImmediate = 0;
    ACC_FALL_ROUTE_OUTPUT fallOutput;
    char text[256], lootName[96];
    if (!Enabled) return;
    if (AccJumpAssist_IsActive()) {
        HaveSnap=0;
        Fresh=1;
        if (CueHandle != SOUND_NOACTIVEINDEX) Sound_Stop(CueHandle);
        CueHandle=SOUND_NOACTIVEINDEX;
        return;
    }
    if (!AccBridge_IsSurvey() && AccCombat_Update(nowMs)) {
        HaveSnap=AccCombat_GetSnapTarget(&SnapPoint); SnapAim=1;
        AccTraversal_Reset();
        if (CueHandle != SOUND_NOACTIVEINDEX) Sound_Stop(CueHandle);
        CueHandle = SOUND_NOACTIVEINDEX;
        Fresh = 1;
        return;
    }
    /* Automatic lifts pause only briefly at a landing. Check arrival promptly;
       speech remains deduplicated while the passenger waits. */
    if (!Fresh && FallRouteHot!=2 &&
        (unsigned int)(nowMs - LastCheck) < ((WaitingOnLift || FallRouteHot || FallFinalLiftRouteActive)?100U:1000U)) return;
    LastCheck = nowMs;
    HaveSnap=0;
    WaitingOnLift=0;
    sb = Player ? Player->ObStrategyBlock : NULL;
    if (!sb || !sb->DynPtr || !sb->containingModule) {
        AccFallRoute_Reset(&FallRouteState); FallRouteHot=0; FallLastCue=0;
        FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1;
        return;
    }
    source = sb->containingModule->m_aimodule;
    if (LastModule != (source ? source->m_index : -1)) AccTraversal_Reset();
    if(LootMode) {
        found=AccLoot_GetTarget(&point,lootName,sizeof(lootName));
        if(found!=1) { EndLoot(found==-2?-2:-1); return; }
        actionable=area=1;
    }
    if (!LootMode && AvP.PlayerType == I_Predator && LevelName &&
        !strcmp(LevelName, "fall") && source && source->m_index == 94) {
        predatorOpening = AccRoute_FallPredatorOpening(&sb->DynPtr->Position,
                                                       &point, &control);
        if (predatorOpening == 1) {
            AccFallRoute_Reset(&FallRouteState); FallRouteHot=0; FallLastCue=0;
            FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1;
            actionable = found = 1;
        } else if (predatorOpening == 2) {
            ACC_FALL_ROUTE_INPUT fallInput;
            fallInput.level_name=LevelName;
            fallInput.is_predator=(AvP.PlayerType==I_Predator);
            fallInput.room_index=source->m_index;
            fallInput.opening_gate_unlocked=1;
            fallInput.grounded=sb->DynPtr->IsInContactWithFloor;
            fallInput.x=sb->DynPtr->Position.vx;
            fallInput.y=sb->DynPtr->Position.vy;
            fallInput.z=sb->DynPtr->Position.vz;
            fallInput.yaw=sb->DynPtr->OrientEuler.EulerY;
            fallInput.velocity_x=sb->DynPtr->LinVelocity.vx;
            fallInput.velocity_z=sb->DynPtr->LinVelocity.vz;
            if (AccFallRoute_Update(&fallInput,&FallRouteState,&fallOutput)) {
                fallGuidance=found=actionable=gateApproach=1;
                if(fallOutput.has_target)
                    point=(VECTORCH){fallOutput.target_x,fallOutput.target_y,fallOutput.target_z};
                FallRouteHot=(fallOutput.phase==ACC_FALL_PHASE_FIRST_RUNUP ||
                              fallOutput.phase==ACC_FALL_PHASE_SECOND_RUNUP)?2:1;
                fallImmediate=(fallOutput.action==ACC_FALL_ACTION_JUMP_NOW ||
                               fallOutput.action==ACC_FALL_ACTION_LAND ||
                               fallOutput.action==ACC_FALL_ACTION_RECOVER ||
                               fallOutput.action==ACC_FALL_ACTION_FALLING);
            } else {
                destination=ModuleFromPosition(&point,NULL);
                if(destination && destination->m_aimodule==source)
                    actionable=found=gateApproach=1;
                else predatorOpening=0;
                FallRouteHot=0;
                FallLastCue=0; FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1;
            }
        } else { AccFallRoute_Reset(&FallRouteState); FallRouteHot=0; FallLastCue=0;
                 FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1; }
    } else { AccFallRoute_Reset(&FallRouteState); FallRouteHot=0; FallLastCue=0;
             FallLastCuePhase=FallLastCueAction=-1; SpokenFallPhase=SpokenFallAction=-1; }
    count = LootMode ? 0 : AccObjectives_Count();
    for (i = 0; !found && i < count; ++i) {
        void *objective = AccObjectives_RouteObjective(i);
        if (!objective) continue;
        actionable = 1;
        found = AccRoute_FindTarget(objective, &sb->DynPtr->Position, &point, &area);
        if (found) { routeObjective=objective; break; }
    }
    /* The final Waterfall lift is a live linked control plus a measured
       platform itinerary, never a graph waiver. Scope all special handling to
       the verified final trigger in room 97 and the five exact shaft rooms. */
    if (!LootMode && found && !fallGuidance && source &&
        AvP.PlayerType == I_Predator && LevelName && !strcmp(LevelName, "fall")) {
        MODULE *goal = ModuleFromPosition(&point, NULL);
        int finalLiftGoal = goal && goal->m_aimodule && goal->m_aimodule->m_index == 97;
        if (FallFinalLiftRouteActive &&
            (!finalLiftGoal || routeObjective != FallFinalLiftObjective ||
             !AccLiftRoute_IsFallShaftRoom(source))) {
            FallFinalLiftRouteActive=0;
            FallFinalLiftPlatform=NULL;
            FallFinalLiftObjective=NULL;
            AccLiftRoute_Reset();
        }
        if (finalLiftGoal && source->m_index == 85) {
            VECTORCH shaftPosition;
            STRATEGYBLOCK *shaftControl = NULL, *shaftPlatform = NULL;
            STRATEGYBLOCK *livePlatform=AccLiftRoute_FindFallPlatform(source);
            int onPlatform=AccLiftRoute_IsFallPlatformContact(source,&sb->DynPtr->Position);
                int shaft=AccRoute_FallPredatorFinalLift(LevelName,1,source->m_index,
                                                       &shaftPosition,&shaftControl,&shaftPlatform);
            finalLiftGoalPoint=point;
            if ((livePlatform && onPlatform) ||
                (FallFinalLiftRouteActive && livePlatform &&
                 (!FallFinalLiftPlatform || livePlatform==FallFinalLiftPlatform))) {
                FallFinalLiftRouteActive=1;
                FallFinalLiftPlatform=livePlatform;
                FallFinalLiftObjective=routeObjective;
            } else if (shaft==1 && shaftControl) {
                finalLiftSwitchPoint=shaftPosition;
                point=shaftPosition;
                control=shaftControl;
                controlArea=0;
                fallFinalLiftControl=1;
                if(FallFinalLiftNeedsSwitchCorner(&sb->DynPtr->Position,
                                                  sb->DynPtr->IsInContactWithFloor)) {
                    point=(VECTORCH){187966,29675,58900};
                    fallFinalLiftCorner=1;
                }
            } else if (shaft==2 && shaftPlatform) {
                FallFinalLiftRouteActive=1;
                FallFinalLiftPlatform=shaftPlatform;
                FallFinalLiftObjective=routeObjective;
            } else {
                fallFinalLiftWait=1;
            }
            if (!fallFinalLiftControl && FallFinalLiftRouteActive &&
                FallFinalLiftPlatform && !fallFinalLiftWait) {
                fallFinalLiftRoute=1;
                fallFinalLiftResult=AccLiftRoute_Find(source,&sb->DynPtr->Position,
                    &finalLiftGoalPoint,&finalLiftRoutePoint);
                if (fallFinalLiftResult) point=finalLiftRoutePoint;
                else fallFinalLiftWait=1;
            }
        } else if (finalLiftGoal && AccLiftRoute_IsFallShaftRoom(source)) {
            STRATEGYBLOCK *livePlatform=AccLiftRoute_FindFallPlatform(source);
            int platformContact=AccLiftRoute_IsFallPlatformContact(source,&sb->DynPtr->Position);
            finalLiftGoalPoint=point;
            if (source->m_index==92 && AccLiftRoute_CompleteFallExit(source,&sb->DynPtr->Position)) {
                FallFinalLiftRouteActive=0;
                FallFinalLiftPlatform=NULL;
                FallFinalLiftObjective=NULL;
            } else {
                if (source->m_index==92)
                    fallFinalLiftExitStage=AccLiftRoute_GetFallExitPoint(
                        source,&sb->DynPtr->Position,&finalLiftGoalPoint,&finalLiftRoutePoint);
                if (fallFinalLiftExitStage>0) {
                    fallFinalLiftRoute=1;
                    fallFinalLiftResult=3;
                    point=finalLiftRoutePoint;
                    FallFinalLiftRouteActive=1;
                    FallFinalLiftPlatform=livePlatform;
                    FallFinalLiftObjective=routeObjective;
                } else if (FallFinalLiftRouteActive) {
                    if (!livePlatform || (FallFinalLiftPlatform && livePlatform!=FallFinalLiftPlatform)) {
                        fallFinalLiftWait=1;
                    } else if (platformContact || source->m_index==85) {
                        fallFinalLiftRoute=1;
                        fallFinalLiftResult=AccLiftRoute_Find(source,&sb->DynPtr->Position,
                            &finalLiftGoalPoint,&finalLiftRoutePoint);
                        if (fallFinalLiftResult) point=finalLiftRoutePoint;
                        else fallFinalLiftWait=1;
                    } else {
                        /* A transient contact gap must not turn a shaft ride
                           into a guessed walking route. */
                        fallFinalLiftWait=1;
                    }
                } else if (livePlatform && platformContact) {
                    FallFinalLiftRouteActive=1;
                    FallFinalLiftPlatform=livePlatform;
                    FallFinalLiftObjective=routeObjective;
                    fallFinalLiftRoute=1;
                    fallFinalLiftResult=AccLiftRoute_Find(source,&sb->DynPtr->Position,
                        &finalLiftGoalPoint,&finalLiftRoutePoint);
                    if (fallFinalLiftResult) point=finalLiftRoutePoint;
                    else fallFinalLiftWait=1;
                } else if (source->m_index!=92) {
                    /* If guidance starts in a surveyed intermediate shaft
                       room but the exact lift/contact is missing, wait safely. */
                    fallFinalLiftWait=1;
                }
            }
        } else if (FallFinalLiftRouteActive) {
            FallFinalLiftRouteActive=0;
            FallFinalLiftPlatform=NULL;
            FallFinalLiftObjective=NULL;
            AccLiftRoute_Reset();
        }
    } else if (FallFinalLiftRouteActive) {
        FallFinalLiftRouteActive=0;
        FallFinalLiftPlatform=NULL;
        FallFinalLiftObjective=NULL;
        AccLiftRoute_Reset();
    }
    if (!actionable) strcpy(text, "No unfinished, available objective to guide to.");
    else if (!found) strcpy(text, "No reliable location is recorded for the current objective.");
    else if (fallGuidance) {
        const char *label="Route";
        switch(fallOutput.phase) {
        case ACC_FALL_PHASE_FIRST_APPROACH: label="First staging point"; break;
        case ACC_FALL_PHASE_FIRST_RUNUP: label="First takeoff line"; break;
        case ACC_FALL_PHASE_FIRST_JUMP: label="First landing"; break;
        case ACC_FALL_PHASE_NORTH_DECK: label="North deck"; break;
        case ACC_FALL_PHASE_SECOND_RUNUP: label="Second takeoff line"; break;
        case ACC_FALL_PHASE_SECOND_JUMP: label="Second landing"; break;
        case ACC_FALL_PHASE_GATE_WALK: label="Cross the opening"; break;
        default: label="Run-up recovery"; break;
        }
        if(fallOutput.action==ACC_FALL_ACTION_FALLING) {
            strcpy(text,"Falling outside the verified route. No landing direction is established.");
        } else if(!fallOutput.has_target) {
            strcpy(text,"Outside the verified landing path. Stop and return to a known staging area.");
        } else {
            char formatted[256];
            if(!AccRoute_Format(&sb->DynPtr->Position,sb->DynPtr->OrientEuler.EulerY,
                                &point,label,formatted,sizeof(formatted))) return;
            strcpy(text,formatted);
            if(fallOutput.action==ACC_FALL_ACTION_WALK && fallOutput.phase==ACC_FALL_PHASE_FIRST_APPROACH)
                strncat(text," Walk to the first run-up staging point.",sizeof(text)-strlen(text)-1);
            else if(fallOutput.action==ACC_FALL_ACTION_RUN_UP)
                strncat(text," Run forward on this line; jump when cued.",sizeof(text)-strlen(text)-1);
            else if(fallOutput.action==ACC_FALL_ACTION_JUMP_NOW)
                strcpy(text,"Jump now and keep moving forward.");
            else if(fallOutput.action==ACC_FALL_ACTION_LAND)
                snprintf(text,sizeof(text),"Keep moving forward. %s",formatted);
            else if(fallOutput.action==ACC_FALL_ACTION_RECOVER)
                strncat(text," Turn back to the staging point.",sizeof(text)-strlen(text)-1);
            else if(fallOutput.phase==ACC_FALL_PHASE_GATE_WALK)
                strncat(text," Continue through the opening into the next room.",sizeof(text)-strlen(text)-1);
        }
        SnapPoint=point; SnapAim=0; HaveSnap=fallOutput.has_target;
        if(fallOutput.has_target &&
           (fallOutput.action==ACC_FALL_ACTION_RUN_UP ||
            fallOutput.action==ACC_FALL_ACTION_JUMP_NOW ||
            fallOutput.action==ACC_FALL_ACTION_LAND)) {
            if(fallOutput.phase==ACC_FALL_PHASE_FIRST_RUNUP || fallOutput.phase==ACC_FALL_PHASE_FIRST_JUMP)
                SnapPoint=(VECTORCH){24311,4739,-2274};
            else if(fallOutput.phase==ACC_FALL_PHASE_SECOND_RUNUP || fallOutput.phase==ACC_FALL_PHASE_SECOND_JUMP)
                SnapPoint=(VECTORCH){40183,4737,8665};
        }
        if(!SnapQuery && fallOutput.has_target &&
           (fallOutput.phase!=FallLastCuePhase || fallOutput.action!=FallLastCueAction ||
            !FallLastCue || (unsigned int)(nowMs-FallLastCue)>=1000U)) {
            if(CueHandle!=SOUND_NOACTIVEINDEX) Sound_Stop(CueHandle);
            CueHandle=SOUND_NOACTIVEINDEX;
            AccBridge_BeginCue("route",SID_TRACKER_WHEEP_HIGH);
            AccTracker_PlayContact(SID_TRACKER_WHEEP_HIGH,&point,20000,&CueHandle,80);
            AccBridge_EndCue();
            FallLastCue=nowMs;
            FallLastCuePhase=fallOutput.phase; FallLastCueAction=fallOutput.action;
        }
    }
    else {
        destination = ModuleFromPosition(&point, NULL);
        if (Fresh && AccPadTrace) {
            int n;
            fprintf(stderr, "ACCROUTE: target=(%d,%d,%d) area=%d candidates=%d source=%d destination=%d modules=%d\n",
                point.vx, point.vy, point.vz, area, found, source ? source->m_index : -1,
                destination && destination->m_aimodule ? destination->m_aimodule->m_index : -1, AIModuleArraySize);
            for (n = 0; n < AIModuleArraySize; ++n) {
                AIMODULE *m = &AIModuleArray[n];
                AIMODULE **edge = m->m_link_ptrs;
                fprintf(stderr, "ACCROUTE: module=%d name=%s passable=%d links=", n,
                    m->m_module_ptrs && *m->m_module_ptrs ? (*m->m_module_ptrs)->name : "(none)",
                    AIModuleIsPhysical(m) && AIModuleAdmitsPheromones(m));
                if (edge) for (; *edge; ++edge) {
                    FARENTRYPOINT *ep = FALLP_EntryPoints ? GetAIModuleEP(*edge,m) : NULL;
                    fprintf(stderr, "%d:%s,", (*edge)->m_index, !ep ? "missing" : ep->alien_only ? "alien" : "marine");
                }
                fprintf(stderr, "\n");
            }
            fflush(stderr);
        }
        next = (fallFinalLiftControl || fallFinalLiftRoute || fallFinalLiftWait) ? source :
            (destination ? AccRoute_NextModule(source, destination->m_aimodule) : NULL);
        if (!next && destination) {
            unsigned char *excluded = AIModuleArraySize > 0 && AIModuleArraySize <= 65536 ?
                (unsigned char *)calloc(AIModuleArraySize, 1) : NULL;
            AIMODULE *fallbackStep = NULL;
            VECTORCH fallbackPoint = point;
            int fallbackBoundary = 0, attempt;
            /* A geometrically short path can end at a lock-only door. Try
               alternatives before sending the player to an unusable barrier. */
            for (attempt = 0; attempt < 16; ++attempt) {
                boundary = RouteFrontier(source, destination->m_aimodule, &approach, &point, &blocked, excluded);
                if (!boundary) break;
                if (boundary) next = AccRoute_NextModule(source, approach);
                if (attempt == 0) {
                    fallbackStep = next; fallbackPoint = point; fallbackBoundary = boundary;
                }
                if (boundary == 1 && blocked && blocked->m_module_ptrs) {
                    MODULE **part;
                    double best = -1;
                    noControl = 1;
                    /* Enumerate every direct control, discarding ones behind a
                       closed door before choosing the closest reachable one. */
                    for (part = blocked->m_module_ptrs; *part; ++part) {
                        int index;
                        STRATEGYBLOCK *candidate;
                        VECTORCH location;
                        int isArea;
                        if (Fresh && AccPadTrace) {
                            fprintf(stderr, "ACCROUTE: inspecting door %s\n", (*part)->name);
                            AccRoute_TraceDoor((*part)->m_sbptr);
                        }
                        for (index = 0; AccRoute_DoorControl((*part)->m_sbptr, index,
                                  &location, &isArea, &candidate); ++index) {
                            MODULE *room = ModuleFromPosition(&location, NULL);
                            AIMODULE *step = room ? AccRoute_NextModule(source, room->m_aimodule) : NULL;
                            double dx = (double)location.vx - sb->DynPtr->Position.vx;
                            double dy = (double)location.vy - sb->DynPtr->Position.vy;
                            double dz = (double)location.vz - sb->DynPtr->Position.vz;
                            double distance = dx*dx + dy*dy + dz*dz;
                            if (Fresh && AccPadTrace) fprintf(stderr,
                                "ACCROUTE: door=%s control=%d area=%d position=(%d,%d,%d) reachable=%d\n",
                                (*part)->name, index, isArea, location.vx, location.vy, location.vz, step != NULL);
                            if (!step || (best >= 0 && distance >= best)) continue;
                            best = distance;
                            control = candidate;
                            controlArea = isArea;
                            point = location;
                            next = step;
                            noControl = 0;
                        }
                        /* In Fall/Predator, the surveyed gate05 switch is
                           operable from the room-76 side even though its AI
                           module is the blocked room 77. Route to that proven
                           approach pose; this does not make the edge passable. */
                        if (!control && AccRoute_FallPredatorGate05Control(
                                LevelName, AvP.PlayerType == I_Predator,
                                source->m_index, blocked->m_index, (*part)->m_sbptr,
                                &location, &candidate)) {
                            control = candidate;
                            controlArea = 0;
                            point = location;
                            next = source;
                            boundary = 0;
                            noControl = 0;
                            fallGate05Control = 1;
                        }
                    }
                }
                if (control || boundary == 2 || !excluded || !blocked) break;
                excluded[blocked->m_index] = 1;
                next = NULL;
            }
            if (!next) {
                next = fallbackStep; point = fallbackPoint; boundary = fallbackBoundary;
                noControl = boundary == 1;
            }
            free(excluded);
        }
        if (!next) strcpy(text, "No accessible route found. A door or an unmapped connection may block the way.");
        else {
            const char *label = LootMode ? lootName : area ? "Objective area" : "Objective switch";
            if (predatorOpening == 1) label = "Facility switch";
            else if (gateApproach) label = "Gate approach";
            if (boundary) label = boundary == 1 ? "Closed door on the route" : "Unverified passage on the route";
            if (control) label = predatorOpening == 1 ? "Facility switch" :
                fallFinalLiftCorner ? "Final lift approach" :
                fallFinalLiftControl ? "Final lift switch" :
                controlArea ? "Door trigger area" : "Door control";
            controlAimPoint=fallFinalLiftCorner ? finalLiftSwitchPoint : point;
            if (next != source) {
                FARENTRYPOINT *ep = GetAIModuleEP(next, source);
                if (!ep) return;
                point = ep->position;
                point.vx += next->m_world.vx;
                point.vy += next->m_world.vy;
                point.vz += next->m_world.vz;
                /* Warn about the vertical boundary when it is actually reached,
                   not at every ordinary doorway several rooms before it. */
                label = boundary == 1 ? "Opening toward closed door" : "Opening";
                if (control) label = "Opening";
                descent=SurveyedDescent(source,next,ep);
                if(descent) {
                    label="Lift down";
                    point=next->m_index==159 ? (VECTORCH){-118480,47166,-171232} :
                                               (VECTORCH){-31011,30921,-199200};
                }
            }
            if (fallFinalLiftWait) {
                /* Missing identity/contact data inside the verified shaft is
                   uncertainty, not permission to walk toward a guessed lift. */
                lift=2;
                point=sb->DynPtr->Position;
                label="Final lift";
            } else if (fallFinalLiftRoute) {
                /* The final shaft's staged exit is a measured local itinerary.
                   Keep its original objective Y for lift selection, and do not
                   replace either leg with a generic authored waypoint. */
                lift=fallFinalLiftResult;
                label=fallFinalLiftExitStage ? "Exit lift" : "Final lift";
            } else {
                VECTORCH boarding;
                lift=AccLiftRoute_Find(source,&sb->DynPtr->Position,&point,&boarding);
                /* A known shaft is not an invitation to walk into empty space
                   if its platform cannot currently be identified. */
                if(descent && !lift) { lift=2; boarding=sb->DynPtr->Position; }
                WaitingOnLift=lift==2;
                if(lift) {
                    point=boarding;
                    label=lift==3?"Exit lift": "Lift";
                }
            }
            if(!lift && !control) {
                VECTORCH leg;
                surveyed=SurveyedPassage(source,&sb->DynPtr->Position,&point,&leg,&surveyedJump);
                if(surveyed) { point=leg; label="Passage"; }
            }
            if (!AccRoute_Format(&sb->DynPtr->Position, sb->DynPtr->OrientEuler.EulerY,
                                 &point, label, text, sizeof(text))) return;
            if(surveyedJump && sb->DynPtr->IsInContactWithFloor) {
                double yaw=sb->DynPtr->OrientEuler.EulerY*(6.283185307179586/4096.0);
                double dx=(double)point.vx-sb->DynPtr->Position.vx,dz=(double)point.vz-sb->DynPtr->Position.vz;
                if(dx*sin(yaw)+dz*cos(yaw)>hypot(dx,dz)*0.94) {
                    traversal=ACC_TRAVERSAL_JUMP;
                    strcpy(text,"Jump and keep moving forward.");
                }
            }
            if(LootMode && !lift && !boundary && !control && next==source && AccLoot_CloseAndVisible()) {
                ready=1;
                /* A nearby pickup may still be beside or behind the player. */
                strncat(text," Walk into it.",sizeof(text)-strlen(text)-1);
            }
            if (!lift && control && !controlArea && next == source && !fallFinalLiftCorner) {
                DISPLAYBLOCK *operable = GetOperableObjectInLineOfSight();
                ready = operable && operable->ObStrategyBlock == control;
                if (ready) strcpy(text, "Press Interact.");
                else if (control->SBdptr && control->SBdptr->ObView.vz > 0
                     && control->SBdptr->ObView.vz < 3000) {
                    int obstruction = GetInteractionObstruction(control->SBdptr);
                    const char *hint = obstruction == 2 ? " Shoot the cover, then Interact." :
                        obstruction == 1 ? " The control is obstructed." :
                        control->SBdptr->ObView.vy < -1000 ? " Look up." :
                        control->SBdptr->ObView.vy > 1000 ? " Look down." : " Face control; move closer.";
                    strncat(text, hint, sizeof(text) - strlen(text) - 1);
                }
            }
            if (noControl && next == source)
                strncat(text, " No reachable usable control identified.", sizeof(text) - strlen(text) - 1);
            /* Match the spoken direction and beacon to a checked local leg.
               Preserve near-control operation/cover instructions and don't
               route across an explicitly unresolved graph boundary. */
            if (lift==2) { strcpy(text,"Lift. Wait."); AccTraversal_Reset(); HaveWaypoint=0; }
            else if (!descent && !surveyed && !ready && !fallFinalLiftWait &&
                (lift || (boundary != 2 && !control) || next != source ||
                (control && next == source &&
                 hypot((double)point.vx-sb->DynPtr->Position.vx,
                       (double)point.vz-sb->DynPtr->Position.vz) > 2500))) {
                VECTORCH waypoint, body=sb->DynPtr->Position;
                int authored;
                body.vy+=(Player->ObMinY+Player->ObMaxY)/2;
                if(fallFinalLiftCorner) {
                    HaveWaypoint=0; authored=0;
                } else if(HaveWaypoint && WaypointRoom==source->m_index && WaypointLift==lift &&
                   abs(body.vy-WaypointBodyY)<1000 &&
                   hypot((double)point.vx-WaypointGoal.vx,(double)point.vz-WaypointGoal.vz)<200 &&
                   abs(point.vy-WaypointGoal.vy)<500 &&
                   hypot((double)body.vx-RouteWaypoint.vx,(double)body.vz-RouteWaypoint.vz)>650) {
                    waypoint=RouteWaypoint; authored=2;
                } else {
                    HaveWaypoint=0;
                    authored=fallFinalLiftExitStage ? 0 :
                        AccWayRoute_FindWithProbe(source,&body,&point,0,&waypoint,AccTraversal_WaypointLink);
                    if(authored==2) {
                        HaveWaypoint=1; WaypointRoom=source->m_index; WaypointLift=lift;
                        WaypointBodyY=body.vy;
                        RouteWaypoint=waypoint; WaypointGoal=point;
                    }
                }
                if(authored==2) {
                    point=waypoint;
                    AccRoute_Format(&sb->DynPtr->Position,sb->DynPtr->OrientEuler.EulerY,
                                    &point,lift==3?"Exit lift":gateApproach?"Gate approach":
                                    boundary==1?"Closed door approach":"Route",text,sizeof(text));
                    if(boundary==1)
                        strncat(text," No reachable usable control identified.",sizeof(text)-strlen(text)-1);
                }
                steering = (fallFinalLiftExitStage || fallFinalLiftCorner) ? ACC_STEER_DIRECT :
                    lift==3 ? AccTraversal_ExitLift(&point,&waypoint) :
                    AccTraversal_Steer(&point, authored!=2 && !lift && control && next == source && !controlArea ? 1600 : 0, &waypoint);
                if (steering == ACC_STEER_DETOUR) {
                    point = waypoint;
                    AccRoute_Format(&sb->DynPtr->Position, sb->DynPtr->OrientEuler.EulerY,
                                    &point, lift==3?"Exit lift":gateApproach?"Gate approach":"Detour", text, sizeof(text));
                } else if (steering == ACC_STEER_BLOCKED) {
                    traversal = boundary ? ACC_TRAVERSAL_NONE : AccTraversal_Probe(&point);
                    if (traversal == ACC_TRAVERSAL_JUMP)
                        snprintf(text, sizeof(text), "%s", AccTraversal_Text(traversal));
                    else {
                        /* Failure of the sampled search is not proof that the
                           route is blocked. Keep the destination bearing as an
                           orientation aid, with an explicit clearance caveat. */
                        strncat(text, " Check path.", sizeof(text)-strlen(text)-1);
                        traversal = ACC_TRAVERSAL_NONE;
                    }
                }
                if (!fallFinalLiftExitStage && !fallFinalLiftCorner &&
                    (steering == ACC_STEER_DIRECT || steering == ACC_STEER_DETOUR)) {
                    traversal = boundary ? ACC_TRAVERSAL_NONE : AccTraversal_Probe(&point);
                    /* An evasive sidestep against a different forward obstacle
                       must not contradict the waypoint beacon. Only describe
                       lateral alignment when that waypoint is actually lateral. */
                    if (traversal == ACC_TRAVERSAL_LEFT || traversal == ACC_TRAVERSAL_RIGHT) {
                        double yaw = sb->DynPtr->OrientEuler.EulerY*(6.283185307179586/4096.0);
                        double dx = (double)point.vx-sb->DynPtr->Position.vx;
                        double dz = (double)point.vz-sb->DynPtr->Position.vz;
                        if (fabs(dx*sin(yaw)+dz*cos(yaw)) > 800) traversal = ACC_TRAVERSAL_NONE;
                    }
                    if (traversal && lift!=3) {
                        snprintf(text, sizeof(text), "%s", AccTraversal_Text(traversal));
                    }
                }
            } else { AccTraversal_Reset(); HaveWaypoint=0; }
            SnapPoint=point; SnapAim=0; HaveSnap=lift!=2;
            if(control && !controlArea && next==source && !lift &&
               hypot((double)controlAimPoint.vx-sb->DynPtr->Position.vx,(double)controlAimPoint.vz-sb->DynPtr->Position.vz)<3000) {
                SnapAim=1;
                SnapPoint=fallGate05Control && control->DynPtr ? control->DynPtr->Position : controlAimPoint;
            }
            if (fallFinalLiftWait) {
                strcpy(text,"Final lift position is unverified. Stop and wait for a reliable reading.");
                AccTraversal_Reset(); HaveWaypoint=0;
            }
            if (fallFinalLiftCorner)
                strncat(text," Reach the upper stair landing before continuing to the switch.",
                        sizeof(text)-strlen(text)-1);
            if(!SnapQuery) {
                if (CueHandle != SOUND_NOACTIVEINDEX) Sound_Stop(CueHandle);
                CueHandle = SOUND_NOACTIVEINDEX;
                AccBridge_BeginCue("route", SID_TRACKER_WHEEP_HIGH);
                AccTracker_PlayContact(SID_TRACKER_WHEEP_HIGH, &point, 20000, &CueHandle, 80);
                AccBridge_EndCue();
            }
        }
    }
    /* Space ordinary updates to avoid filling the speech queue. Room/detour changes
       get at least three seconds; distance-only updates wait six. Uncertain
       obstacle scans obey that spacing too. Jump and readiness stay immediate. */
    if (!SnapQuery && (Fresh || (fallGuidance && fallImmediate &&
        fallOutput.action!=SpokenFallAction && strcmp(text,LastText)) ||
        (fallFinalLiftWait && strcmp(text,LastText)) ||
        (fallGuidance && fallOutput.phase!=SpokenFallPhase && strcmp(text,LastText)) || ((ready || ((lift==2 || lift==3) && lift!=SpokenLift) ||
         (traversal == ACC_TRAVERSAL_JUMP && traversal != LastTraversal) ||
         ((unsigned int)(nowMs - LastSpeech) >= 3000 &&
          (steering != SpokenSteering || traversal != SpokenTraversal ||
           SpokenModule != (source ? source->m_index : -1))) ||
         (unsigned int)(nowMs - LastSpeech) >= 6000) && strcmp(text, LastText)))) {
        AccSpeech_Say(text, fallGuidance && fallOutput.action==ACC_FALL_ACTION_JUMP_NOW);
        if (boundary==2 && blocked) {
            /* Capture the actual restricted edge for reports without requiring
               a special launcher; use the speech gate to avoid per-frame spam. */
            fprintf(stderr,"AVP Access: unverified passage level=%s source=%d approach=%d restricted=%d mode=%s\n",
                LevelName,source?source->m_index:-1,approach?approach->m_index:-1,
                blocked->m_index,LootMode?"supply":"mission");
        }
        strcpy(LastText, text);
        LastSpeech = nowMs;
        SpokenModule = source ? source->m_index : -1;
        SpokenSteering = steering;
        SpokenTraversal = traversal;
        SpokenLift=lift;
        SpokenFallPhase=fallGuidance?fallOutput.phase:-1;
        SpokenFallAction=fallGuidance?fallOutput.action:-1;
    }
    LastModule = source ? source->m_index : -1;
    LastTraversal = traversal;
    Fresh = 0;
}

int AccRoute_GetSnapTarget(unsigned int nowMs,VECTORCH *point,int *aim)
{
    if(!Enabled || !point || !aim) return 0;
    /* Refresh navigation at the player's current position before the turn. */
    Fresh=1; HaveSnap=0;
    SnapQuery=1;
    AccRoute_Update(nowMs);
    SnapQuery=0;
    if(!HaveSnap) return 0;
    *point=SnapPoint; *aim=SnapAim; return 1;
}
