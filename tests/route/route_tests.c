#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "pfarlocs.h"
#include "acc_route.h"
#include "acc_traversal.h"
#include "acc_wayroute.h"
#include <stdio.h>
#include <string.h>

static AIMODULE nodes[180];
AIMODULE *AIModuleArray = nodes;
int AIModuleArraySize = 180;
int AccPadTrace;
char LevelName[]="route-test";
AVP_GAME_DESC AvP;
static FARENTRYPOINTSHEADER headers[180];
FARENTRYPOINTSHEADER *FALLP_EntryPoints = headers;
static FARENTRYPOINT entries[180];
static AIMODULE *links[180][3];
static MODULE modules[180];
static int blocked = -1, alien = -1, failures, checks, speech, cues, stops;
static int objective = 1, target = 1, located = 129;
static int openingStage;
static int gate05ControlStage;
static int gate05ProbeCalls;
static int finalLiftStage, finalLiftProbeCalls;
static VECTORCH finalLiftControlPosition;
static STRATEGYBLOCK finalLiftSwitch, finalLiftPlatform;
static int finalLiftLive, finalLiftContact, finalLiftExitStage, finalLiftComplete;
static int finalLiftFindCalls, finalLiftExitCalls, finalLiftCompleteCalls;
static VECTORCH finalLiftExitPoint;
static char spoken[256];
static DISPLAYBLOCK display;
DISPLAYBLOCK *Player = &display;
static STRATEGYBLOCK strategy;
static STRATEGYBLOCK door, control;
static DISPLAYBLOCK controlDisplay;
static DYNAMICSBLOCK controlDynamics;
static MODULE *moduleParts[180][2];
static int controlCount, controlReady;
static int interactionObstruction;
static int combatActive, combatResets, combatCalls, jumpAssistActive;
int AccJumpAssist_IsActive(void) { return jumpAssistActive; }
static int lootResult, lootClose, lootCycles;
void AccLoot_Reset(void) { lootResult=0; }
int AccLoot_Cycle(void) { ++lootCycles; lootResult=1; return 1; }
int AccLoot_GetTarget(VECTORCH *p,char *name,size_t size)
{ *p=(VECTORCH){0,0,800}; snprintf(name,size,"Medkit"); return lootResult; }
int AccLoot_CloseAndVisible(void) { return lootClose; }
int AccCombat_Update(unsigned int now) { (void)now; ++combatCalls; return combatActive; }
int AccCombat_GetSnapTarget(VECTORCH *p) { *p=(VECTORCH){1000,-1000,2000}; return combatActive==1; }
void AccCombat_Reset(void) { ++combatResets; combatActive = 0; }
static int traversalHint, traversalCalls;
static int steerResult, steerCalls, lastInterrupt;
static int liftResult, authoredResult;
static VECTORCH liftPoint, authoredPoint;
static VECTORCH lastSteerTarget;
static int lastStandOff;
static int wayrouteResult, wayrouteCalls;
static VECTORCH wayroutePoint, objectivePoint={0,0,10000};
static int exitLiftCalls;
static int liftRouteResets, liftRouteHeld;
int AccTraversal_WaypointLink(const VECTORCH *a,const VECTORCH *b) { (void)a; (void)b; return 0; }
int AccLiftRoute_Find(AIMODULE *room,const VECTORCH *feet,const VECTORCH *target,VECTORCH *out)
{ (void)room; (void)feet; (void)target; ++finalLiftFindCalls; if (liftResult) *out=liftPoint; return liftResult; }
int AccLiftRoute_IsFallShaftRoom(AIMODULE *room)
{ return room && AvP.PlayerType==I_Predator && !strcmp(LevelName,"fall") &&
  (room->m_index==85 || room->m_index==87 || room->m_index==89 || room->m_index==91 || room->m_index==92); }
STRATEGYBLOCK *AccLiftRoute_FindFallPlatform(AIMODULE *room)
{ return finalLiftLive && AccLiftRoute_IsFallShaftRoom(room) ? &finalLiftPlatform : NULL; }
int AccLiftRoute_IsFallPlatformContact(AIMODULE *room,const VECTORCH *feet)
{ (void)feet; return finalLiftContact && AccLiftRoute_FindFallPlatform(room)!=NULL; }
int AccLiftRoute_GetFallExitPoint(AIMODULE *room,const VECTORCH *feet,const VECTORCH *target,VECTORCH *out)
{ (void)feet; (void)target; ++finalLiftExitCalls; if(!finalLiftExitStage || !AccLiftRoute_IsFallShaftRoom(room)) return 0; *out=finalLiftExitPoint; return finalLiftExitStage; }
int AccLiftRoute_CompleteFallExit(AIMODULE *room,const VECTORCH *feet)
{ (void)feet; ++finalLiftCompleteCalls; return finalLiftComplete && AccLiftRoute_IsFallShaftRoom(room); }
void AccLiftRoute_Reset(void) { ++liftRouteResets; liftRouteHeld=0; finalLiftLive=finalLiftContact=finalLiftExitStage=finalLiftComplete=0; }
int AccWayRoute_FindWithProbe(AIMODULE *room,const VECTORCH *body,const VECTORCH *target,int alien,VECTORCH *out,ACC_WAYROUTE_PROBE probe)
{ (void)room; (void)body; (void)target; (void)alien; (void)probe; ++wayrouteCalls;
  if (wayrouteResult==2) *out=wayroutePoint; return wayrouteResult; }
static VECTORCH steerPoint, cuePosition;
void AccTraversal_Reset(void) { }
int AccTraversal_Steer(const VECTORCH *target, int standOff, VECTORCH *point)
{ ++steerCalls; lastSteerTarget=*target; lastStandOff=standOff;
  *point = steerResult == ACC_STEER_DETOUR ? steerPoint : *target; return steerResult; }
int AccTraversal_ExitLift(const VECTORCH *target, VECTORCH *point)
{ ++exitLiftCalls; return AccTraversal_Steer(target,0,point); }
int AccTraversal_Probe(const VECTORCH *target) { (void)target; ++traversalCalls; return traversalHint; }
const char *AccTraversal_Text(int hint) { return hint == ACC_TRAVERSAL_JUMP ? "Jump here." : "Strafe left."; }
static int secondBlocked = -1;
static int controlRoom[2] = {0, 0};
static DYNAMICSBLOCK dynamics;

int AIModuleIsPhysical(AIMODULE *m) { return m != NULL; }
int AIModuleAdmitsPheromones(AIMODULE *m)
{
    if (m->m_index == blocked || m->m_index == secondBlocked) return 0;
    return 1;
}
FARENTRYPOINT *GetAIModuleEP(AIMODULE *to, AIMODULE *from)
{ (void)from; entries[to->m_index].alien_only = to->m_index == alien; return &entries[to->m_index]; }
MODULE *ModuleFromPosition(VECTORCH *p, MODULE *start)
{ (void)start; if (p->vx == 1000 || p->vx == 2000) return &modules[controlRoom[p->vx / 1000 - 1]];
  return located >= 0 ? &modules[located] : NULL; }
int AccObjectives_Count(void) { return 1; }
void *AccObjectives_RouteObjective(int index) { (void)index; return objective ? &objective : NULL; }
int AccRoute_FindTarget(void *o, const VECTORCH *p, VECTORCH *out, int *area)
{ (void)o; (void)p; *out=objectivePoint; *area = 1; return target; }
void AccSpeech_Say(const char *s, int interrupt)
{ lastInterrupt = interrupt; ++speech; snprintf(spoken, sizeof(spoken), "%s", s); }
void Sound_Stop(int handle) { (void)handle; ++stops; }
void AccTracker_PlayContact(int sound, const VECTORCH *p, int range, int *handle, int volume)
{ (void)sound; cuePosition = *p; (void)range; (void)volume; ++cues; *handle = 4; }
void AccBridge_BeginCue(const char *source, int sound) { (void)source; (void)sound; }
void AccBridge_EndCue(void) { }
static int surveyMode;
int AccBridge_IsSurvey(void) { return surveyMode; }
DISPLAYBLOCK *GetOperableObjectInLineOfSight(void) { return controlReady ? &controlDisplay : NULL; }
int GetInteractionObstruction(DISPLAYBLOCK *object) { (void)object; return interactionObstruction; }
void AccRoute_TraceDoor(STRATEGYBLOCK *door) { (void)door; }
int AccRoute_DoorControl(STRATEGYBLOCK *d, int index, VECTORCH *position, int *area, STRATEGYBLOCK **out)
{
    if (d != &door || index < 0 || index >= controlCount) return 0;
    *out = &control; *area = 0; position->vx = (index+1)*1000; position->vy = position->vz = 0;
    return 1;
}
int AccRoute_FallPredatorOpening(const VECTORCH *player, VECTORCH *position, STRATEGYBLOCK **out)
{
    (void)player;
    if (!openingStage) return 0;
    *position = openingStage == 1 ? (VECTORCH){0,0,1000} : (VECTORCH){0,0,10000};
    *out = openingStage == 1 ? &control : NULL;
    return openingStage;
}
int AccRoute_FallPredatorGate05Control(const char *level, int predator,
                                      int source, int blockedRoom,
                                      STRATEGYBLOCK *candidateDoor,
                                      VECTORCH *position, STRATEGYBLOCK **out)
{
    ++gate05ProbeCalls;
    if (!gate05ControlStage || strcmp(level,"fall") || !predator || source != 76 ||
        blockedRoom != 77 || candidateDoor != &door) return 0;
    *position=(VECTORCH){206976,12533,144370};
    *out=&control;
    return 1;
}
int AccRoute_FallPredatorFinalLift(const char *level, int predator, int room,
                                  VECTORCH *position, STRATEGYBLOCK **switchOut,
                                  STRATEGYBLOCK **platformOut)
{
    ++finalLiftProbeCalls;
    if ((finalLiftStage != 1 && finalLiftStage != 2) || strcmp(level, "fall") || !predator || room != 85) return 0;
    *position=finalLiftStage==1 ? finalLiftControlPosition : (VECTORCH){187235,-2076,70510};
    *switchOut=finalLiftStage==1 ? &finalLiftSwitch : NULL;
    *platformOut=&finalLiftPlatform;
    return 1;
}
static void check(int ok, const char *name)
{ ++checks; if (!ok) { ++failures; printf("FAIL: %s [spoken: %s]\n", name, spoken); } }

int main(void)
{
    int i, beforeStops;
    AIMODULE *approach;
    VECTORCH boundary, originalPoint;
    VECTORCH origin = {0,0,0}, point = {0,0,10000};
    char text[256];
    for (i = 0; i < 180; ++i) {
        nodes[i].m_index = i;
        nodes[i].m_link_ptrs = links[i];
        if (i < 129) links[i][0] = &nodes[i+1];
        modules[i].m_aimodule = &nodes[i];
        moduleParts[i][0] = &modules[i]; nodes[i].m_module_ptrs = moduleParts[i];
    }
    check(AccRoute_NextModule(&nodes[0], &nodes[129]) == &nodes[1], "route exceeds old 100-node queue capacity");
    check(AccRoute_NextModule(&nodes[12], &nodes[12]) == &nodes[12], "same module");
    check(!AccRoute_NextModule(NULL, &nodes[1]), "null source");
    blocked = 40;
    check(!AccRoute_NextModule(&nodes[0], &nodes[129]), "closed door blocks route");
    check(AccRoute_Frontier(&nodes[0], &nodes[129], &approach, &boundary, NULL) == 1
          && approach == &nodes[39], "partial guidance stops on near side of first closed door");
    blocked = -1; alien = 40;
    check(!AccRoute_NextModule(&nodes[0], &nodes[129]), "alien-only link blocks Marine");
    check(AccRoute_Frontier(&nodes[0], &nodes[129], &approach, &boundary, NULL) == 2
          && approach == &nodes[39], "restricted passage is boundary not permitted traversal");
    alien = -1;
    links[5][1] = &nodes[0];
    check(AccRoute_NextModule(&nodes[0], &nodes[129]) == &nodes[1], "cycle terminates");
    check(!AccRoute_NextModule(&nodes[129], &nodes[0]), "no fabricated reverse edges");
    links[0][1] = &nodes[129];
    check(AccRoute_NextModule(&nodes[0], &nodes[129]) == &nodes[129], "shortest room path");
    links[0][1] = NULL;
    AccRoute_Format(&origin, 0, &point, "Opening", text, sizeof(text));
    check(strstr(text, "12 o'clock") && strstr(text, "10 metres"), "front bearing and distance");
    AccRoute_Format(&origin, 1024, &point, "Opening", text, sizeof(text));
    check(strstr(text, "9 o'clock") != NULL, "relative heading left");
    AccRoute_Format(&origin, 3072, &point, "Opening", text, sizeof(text));
    check(strstr(text, "3 o'clock") != NULL, "relative heading right");
    point.vz = 500;
    AccRoute_Format(&origin, 0, &point, "Opening", text, sizeof(text));
    check(strstr(text, "within one metre") != NULL, "near precision");
    check(!AccRoute_Format(&origin, 0, &point, "Opening", text, 2), "truncation rejected");
    display.ObStrategyBlock = &strategy; strategy.DynPtr = &dynamics;
    strategy.containingModule = &modules[0];
    AccRoute_Update(0); check(!speech && !cues, "off does nothing");
    AccRoute_Toggle(); AccRoute_Update(0);
    check(speech == 2 && cues == 1 && strstr(spoken, "Opening"), "toggle starts speech and beacon");
    AccRoute_Update(999); check(cues == 1, "beacon rate limited");
    AccRoute_Update(1000); check(cues == 2 && speech == 2, "stable direction does not repeat speech");
    blocked = 40; AccRoute_Update(7000);
    check(cues >= 3 && strstr(spoken, "door"), "closed door during guidance reported with reachable approach");
    blocked = -1; target = 0; AccRoute_Update(13000);
    check(strstr(spoken, "No reliable location") != NULL, "unknown target truthful");
    objective = 0; AccRoute_Update(19000);
    check(strstr(spoken, "No unfinished") != NULL, "completed or unavailable objectives not routed");
    AccRoute_Reset(); i = cues; AccRoute_Update(20000);
    check(cues == i && stops > 0, "reset disables guidance and stops cue");
    objective = target = 1;
    AccRoute_Toggle(); AccRoute_Update(21000);
    i = speech; strategy.containingModule = &modules[1];
    entries[2].position.vx = 9000;
    AccRoute_Update(22000);
    check(speech == i && strstr(spoken, "Opening"), "room change does not spam speech at one second");
    AccRoute_Update(24000);
    check(speech == i + 1 && strstr(spoken, "3 o'clock"), "latest room direction is announced after material-change throttle");
    AccRoute_Toggle(); i = cues; AccRoute_Update(25000);
    check(cues == i && !strcmp(spoken, "Route guidance off."), "toggle off silences subsequent updates");
    strategy.containingModule = &modules[0];
    modules[40].m_sbptr = &door; blocked = 40;
    controlCount = 2; controlRoom[0] = 129; controlRoom[1] = 0;
    controlDisplay.ObStrategyBlock = &control;
    control.SBdptr = &controlDisplay;
    controlDisplay.ObView.vz = 1000;
    AccRoute_Toggle(); AccRoute_Update(26000);
    check(strstr(spoken, "Door control") && strstr(spoken, "2 metres"),
          "unreachable nearest control is discarded in favor of reachable alternative");
    check(!strstr(spoken, "Press Interact"), "proximity alone never claims control is ready");
    interactionObstruction = 2;
    AccRoute_Reset(); AccRoute_Toggle();
    AccRoute_Update(27000);
    check(strstr(spoken, "Shoot the cover") && strstr(spoken, "then Interact") &&
          !strstr(spoken, "Press Interact"),
          "obstruction type 2 gives breakable guidance without claiming control is ready");
    interactionObstruction = 0;
    controlReady = 1; AccRoute_Update(28000);
    check(!strcmp(spoken, "Press Interact."),
          "actual operable control gets immediate interaction prompt");
    i = speech; AccRoute_Update(29000);
    check(speech == i, "ready prompt does not repeat each second");
    blocked = -1; AccRoute_Update(35000);
    check(strstr(spoken, "Opening") && !strstr(spoken, "control"),
          "opened door restores objective route automatically");
    AccRoute_Reset();

    /* Two equally short paths. The first door has no opening control. */
    memset(links, 0, sizeof(links));
    links[0][0] = &nodes[1]; links[0][1] = &nodes[2];
    links[1][0] = links[2][0] = &nodes[3];
    blocked = 1; secondBlocked = 2; located = 3;
    modules[1].m_sbptr = NULL; modules[2].m_sbptr = &door;
    controlCount = 1; controlRoom[0] = 0; controlReady = 0;
    strategy.containingModule = &modules[0];
    AccRoute_Toggle(); AccRoute_Update(36000);
    check(strstr(spoken, "Door control") && strstr(spoken, "1 metre"),
          "alternative path finds usable control instead of first lock-only door");
    AccRoute_Reset();
    /* An Alien shortcut must not hide a Marine route through an operable door. */
    blocked=-1; secondBlocked=2; alien=1;
    AccRoute_Toggle(); AccRoute_Update(36500);
    check(strstr(spoken,"Door control") && !strstr(spoken,"Unverified"),
          "Marine door route takes precedence over Alien-only shortcut");
    AccRoute_Reset(); alien=-1;
    blocked = secondBlocked = -1; located = 3;
    AccRoute_Toggle(); AccRoute_Update(37000);
    i = speech; traversalHint = ACC_TRAVERSAL_LEFT;
    AccRoute_Update(38000);
    check(speech == i, "strafe change does not spam speech at one second");
    AccRoute_Update(40000);
    check(speech == i+1 && !strcmp(spoken, "Strafe left."), "deferred strafe speaks only the short instruction");
    traversalHint = ACC_TRAVERSAL_JUMP; AccRoute_Update(41000);
    check(strstr(spoken, "Jump here.") != NULL, "new jump hint announces without waiting four seconds");
    AccRoute_Reset(); i = traversalCalls; AccRoute_Update(42000);
    check(traversalCalls == i, "off guidance does not probe geometry");
    blocked = 1; secondBlocked = 2; controlCount = 0;
    AccRoute_Toggle(); i = traversalCalls; AccRoute_Update(43000);
    check(traversalCalls == i && !strstr(spoken, "Jump here."),
          "local hints never suggest jumping across unresolved door boundary");
    controlCount = 1; controlReady = 1;
    AccRoute_Reset(); AccRoute_Toggle(); i = traversalCalls; AccRoute_Update(44000);
    check(traversalCalls == i && strstr(spoken, "Press Interact"),
          "interaction instruction takes precedence over local movement hint");
    AccRoute_Reset();
    /* Authored graph routing chooses the destination; local steering then checks that leg. */
    blocked = secondBlocked = -1; located = 0; controlCount = 0;
    traversalHint = ACC_TRAVERSAL_NONE;
    steerResult = ACC_STEER_DETOUR; steerPoint = (VECTORCH){3000, 0, 1000};
    wayrouteResult=2; wayroutePoint = (VECTORCH){0, 0, 1800};
    i = steerCalls; AccRoute_Toggle(); AccRoute_Update(44500);
    check(strstr(spoken, "Detour") && cuePosition.vx==steerPoint.vx && cuePosition.vz==steerPoint.vz &&
          steerCalls==i+1 && lastSteerTarget.vx==wayroutePoint.vx && lastSteerTarget.vz==wayroutePoint.vz &&
          lastStandOff==0,
          "authored waypoint feeds local steer and checked detour owns speech and beacon");
    AccRoute_Reset(); steerResult=ACC_STEER_BLOCKED; i=steerCalls;
    AccRoute_Toggle(); AccRoute_Update(44550);
    check(strstr(spoken, "Route") && strstr(spoken, "Check path") &&
          cuePosition.vx==wayroutePoint.vx && cuePosition.vz==wayroutePoint.vz &&
          steerCalls==i+1 && lastSteerTarget.vx==wayroutePoint.vx,
          "blocked local steer retains authored route bearing and beacon with clearance caveat");
    AccRoute_Reset(); steerResult=ACC_STEER_DIRECT; i=steerCalls;
    AccRoute_Toggle(); AccRoute_Update(44600);
    check(strstr(spoken, "Route") && cuePosition.vx==wayroutePoint.vx && cuePosition.vz==wayroutePoint.vz &&
          steerCalls==i+1 && lastSteerTarget.vx==wayroutePoint.vx,
          "direct local steer preserves authored waypoint guidance");
    AccRoute_Reset(); wayrouteResult=0; steerResult = ACC_STEER_DIRECT;

    /* A lift wait suppresses control readiness; an exit remains a directional target. */
    blocked = 40; modules[40].m_sbptr = &door; controlCount = 1; controlRoom[0] = 0;
    controlReady = 1; controlDisplay.ObStrategyBlock = &control; control.SBdptr = &controlDisplay;
    controlDisplay.ObView.vz = 1000;
    traversalHint = ACC_TRAVERSAL_NONE;
    liftResult = 2; liftPoint = (VECTORCH){0, 0, 2500};
    AccRoute_Toggle(); AccRoute_Update(44600);
    check(!strcmp(spoken, "Lift. Wait.") && !strstr(spoken, "Interact") &&
          cuePosition.vx==liftPoint.vx && cuePosition.vz==liftPoint.vz,
          "lift wait suppresses ready-control prompt and cues the lift boarding point");
    liftResult=3; liftPoint=(VECTORCH){0,0,5000}; i=speech;
    AccRoute_Update(44700);
    check(speech==i+1 && strstr(spoken,"Exit lift"),"lift arrival is detected at 100ms rather than waiting a full second");
    i=speech; liftPoint.vz=4000; AccRoute_Update(45700);
    check(speech==i,"subsequent lift distance update obeys ordinary speech throttle");
    AccRoute_Reset(); liftResult = 3; liftPoint = (VECTORCH){0, 0, 5000};
    i=exitLiftCalls;
    AccRoute_Toggle(); AccRoute_Update(44700);
    check(strstr(spoken, "Exit lift") && !strstr(spoken, "Press Interact") &&
          cuePosition.vx==liftPoint.vx && cuePosition.vz==liftPoint.vz && exitLiftCalls==i+1,
          "lift exit uses long-range exit steering and retains directional label");
    AccRoute_Reset(); steerResult=ACC_STEER_DETOUR; steerPoint=(VECTORCH){900,0,4200};
    AccRoute_Toggle(); AccRoute_Update(44800);
    check(strstr(spoken,"Exit lift") && cuePosition.vx==steerPoint.vx && cuePosition.vz==steerPoint.vz,
          "lift exit detour retains exit label while beacon follows checked detour");
    AccRoute_Reset(); liftResult=0; steerResult=ACC_STEER_DIRECT; controlReady=0; blocked=-1; controlCount=0; located=3;

    blocked = secondBlocked = -1;
    AccRoute_Toggle(); AccRoute_Update(45000);
    i = cues; beforeStops = stops; combatActive = 1; AccRoute_Update(45100);
    check(cues == i && stops == beforeStops+1, "combat takeover stops route cue even before one-second route tick");
    i = speech; AccRoute_Update(45200);
    check(speech == i, "combat ownership suppresses route speech");
    combatActive = 0; AccRoute_Update(45500);
    check(speech == i+1, "route immediately refreshes after combat releases ownership");
    {
        int priorCombat=combatCalls, priorSpeech=speech, priorCues=cues;
        jumpAssistActive=1; combatActive=1;
        AccRoute_Update(45600);
        check(combatCalls==priorCombat && speech==priorSpeech && cues==priorCues,
              "explicit jump assist pauses combat and route announcements");
        jumpAssistActive=0; combatActive=0;
        AccRoute_Update(45601);
        check(cues==priorCues+1, "route refreshes immediately when jump assist releases control");
    }
    combatActive = 1; i = combatResets; AccRoute_Reset();
    check(!combatActive && combatResets == i+1, "guidance reset cancels combat too");
    traversalHint = 0; blocked = secondBlocked = -1;
    steerResult = ACC_STEER_DETOUR; steerPoint.vx=1200; steerPoint.vz=0;
    AccRoute_Toggle(); AccRoute_Update(46000);
    check(strstr(spoken,"Detour") && strstr(spoken,"3 o'clock") && cuePosition.vx==1200 && cuePosition.vz==0,
          "detour speech and beacon point to the same reachable intermediate waypoint");
    originalPoint=entries[1].position;
    originalPoint.vx+=nodes[1].m_world.vx; originalPoint.vy+=nodes[1].m_world.vy; originalPoint.vz+=nodes[1].m_world.vz;
    i = cues; beforeStops=stops; steerResult = ACC_STEER_BLOCKED; AccRoute_Update(47000);
    check(cues == i+1 && cuePosition.vx==originalPoint.vx && cuePosition.vy==originalPoint.vy &&
          cuePosition.vz==originalPoint.vz && strstr(spoken,"Detour"),
          "failed local search cues original route target while recent speech remains throttled");
    i=speech; AccRoute_Update(48000);
    check(speech==i && strstr(spoken,"Detour") && stops==beforeStops+2,
          "repeated uncertain scan does not interrupt or spam speech");
    steerResult=ACC_STEER_DIRECT; AccRoute_Update(49000);
    check(speech==i+1 && strstr(spoken,"Opening") && !strstr(spoken,"Check path") &&
          cuePosition.vx==originalPoint.vx && cuePosition.vz==originalPoint.vz,
          "direct transition is announced at ordinary material-change interval");
    i=speech; steerResult=ACC_STEER_BLOCKED; AccRoute_Update(50000); AccRoute_Update(51000);
    check(speech==i && !lastInterrupt,"blocked/direct toggles remain quiet before the ordinary throttle expires");
    AccRoute_Update(52000);
    check(speech==i+1 && strstr(spoken,"Check path") && !strstr(spoken,"Stop") && !lastInterrupt &&
          cuePosition.vx==originalPoint.vx && cuePosition.vz==originalPoint.vz,
          "persistent failed search speaks concise uncertainty and keeps target beacon");
    i=speech; steerResult=ACC_STEER_DIRECT; AccRoute_Update(53000);
    check(speech==i && cuePosition.vx==originalPoint.vx && cuePosition.vz==originalPoint.vz,
          "resume transition does not bypass ordinary speech throttle");
    AccRoute_Reset();
    blocked = 1; secondBlocked = 2; controlCount = 1; controlReady = 0;
    dynamics.Position.vx = -1400; dynamics.Position.vz = -2400;
    steerResult = ACC_STEER_DETOUR;
    AccRoute_Toggle(); i = steerCalls; AccRoute_Update(49000);
    check(steerCalls == i+1 && strstr(spoken,"Detour"),
          "diagonally distant same-room control receives local steering");
    AccRoute_Reset();
    blocked = secondBlocked = -1; steerResult = ACC_STEER_DIRECT;
    dynamics.Position.vx = dynamics.Position.vz = 0;
    entries[1].position.vx = 0; entries[1].position.vz = 9000;
    AccRoute_Toggle(); AccRoute_Update(50000); i = speech;
    entries[1].position.vz = 8000; AccRoute_Update(53000);
    check(speech == i, "distance-only change stays quiet at three seconds");
    AccRoute_Update(56000);
    check(speech == i+1 && strstr(spoken, "8 metres"), "distance update speaks latest value at six seconds");

    /* Keep a selected authored waypoint stable until within 650 mm, then reselect. */
    AccRoute_Reset();
    located=0; dynamics.Position=(VECTORCH){0,0,0}; objectivePoint=(VECTORCH){0,0,10000};
    wayrouteResult=2; wayroutePoint=(VECTORCH){0,0,3000}; traversalHint=ACC_TRAVERSAL_NONE;
    steerResult=ACC_STEER_DIRECT; i=wayrouteCalls;
    AccRoute_Toggle(); AccRoute_Update(57000);
    check(wayrouteCalls==i+1 && cuePosition.vz==3000,
          "route selects and cues the initial authored waypoint");
    dynamics.Position.vz=1800; wayroutePoint=(VECTORCH){0,0,4200};
    AccRoute_Update(58000);
    check(wayrouteCalls==i+1 && cuePosition.vz==3000,
          "authored waypoint persists while player remains more than 650 mm away");
    dynamics.Position.vz=2500;
    AccRoute_Update(59000);
    check(wayrouteCalls==i+2 && cuePosition.vz==4200,
          "route reselects an authored waypoint after reaching within 650 mm");

    objectivePoint.vx=500; wayroutePoint=(VECTORCH){500,0,4600};
    AccRoute_Update(60000);
    check(wayrouteCalls==i+3 && cuePosition.vx==500 && cuePosition.vz==4600,
          "changed objective goal invalidates the retained authored waypoint");
    liftRouteHeld=1; i=liftRouteResets;
    AccRoute_Reset();
    check(liftRouteResets==i+1 && !liftRouteHeld,
          "route reset clears the remembered lift landing hold");
    i=wayrouteCalls;
    AccRoute_Toggle(); AccRoute_Update(61000);
    check(wayrouteCalls==i+1 && cuePosition.vx==500 && cuePosition.vz==4600,
          "route reset clears retained authored waypoint before guidance resumes");
    AccRoute_Reset(); wayrouteResult=0; objectivePoint=(VECTORCH){0,0,10000}; located=3;
    located=0; lootClose=1; traversalHint=ACC_TRAVERSAL_NONE; steerResult=ACC_STEER_DIRECT;
    AccRoute_Toggle(); AccRoute_ToggleLoot(); i=steerCalls; AccRoute_Update(62000);
    check(strstr(spoken,"Medkit") && strstr(spoken,"Walk into it") && steerCalls==i,
          "near visible pickup prompts touch instead of obstacle detour");
    dynamics.Position=(VECTORCH){0,0,0}; dynamics.OrientEuler.EulerY=1024;
    AccRoute_Update(62100);
    AccRoute_Update(63100);
    check(strstr(spoken,"9 o'clock") && strstr(spoken,"within one metre") && strstr(spoken,"Walk into it"),
          "near pickup to the left retains its collection bearing");
    dynamics.OrientEuler.EulerY=2048; AccRoute_Update(64100);
    check(strstr(spoken,"6 o'clock") && steerCalls==i,
          "near pickup behind retains bearing without obstacle detour");
    dynamics.OrientEuler.EulerY=0; AccRoute_Update(65100);
    check(strstr(spoken,"12 o'clock")!=NULL,"collection bearing updates when facing pickup");
    lootResult=-2; AccRoute_Update(67000);
    check(strstr(spoken,"Collected. Mission guidance resumed")!=NULL,"confirmed pickup restores prior mission guidance");
    AccRoute_Update(68000); check(strstr(spoken,"Objective")!=NULL,"mission target used after pickup");
    AccRoute_ToggleLoot(); AccRoute_ToggleLoot();
    check(!strcmp(spoken,"Mission guidance resumed."),"cancel supplies restores enabled mission");
    AccRoute_ToggleLoot(); lootResult=-1; AccRoute_Update(69000);
    check(strstr(spoken,"unavailable") && !strstr(spoken,"Collected"),"vanished pickup never claims collection");
    AccRoute_Reset(); AccRoute_ToggleLoot(); lootResult=-2; AccRoute_Update(70000);
    check(!strcmp(spoken,"Collected. Guidance off."),"pickup returns to off when mission was off");
    i=cues; AccRoute_Update(71000); check(cues==i,"off state retained after supplies");
    AccRoute_ToggleLoot(); combatActive=1; i=cues; AccRoute_Update(72000);
    check(cues==i,"combat suppresses supply beacon");
    combatActive=0; AccRoute_Update(73000); check(strstr(spoken,"Medkit")!=NULL,"supply guidance resumes after combat");
    AccRoute_Toggle(); i=cues; AccRoute_Update(74000); check(cues==i,"normal off toggle stops loot guidance");
    { int aim;
      check(!AccRoute_GetSnapTarget(71000,&point,&aim),"off guidance has no snap point");
      AccRoute_Toggle(); i=speech;
      check(AccRoute_GetSnapTarget(72000,&point,&aim) && !aim,"navigation provides horizontal snap point");
      check(speech==i,"snap refresh does not queue pre-turn bearing speech");
      combatActive=1; check(AccRoute_GetSnapTarget(72100,&point,&aim) && aim && point.vy==-1000,"combat owns snap target and elevation");
      combatActive=2; check(!AccRoute_GetSnapTarget(72200,&point,&aim),"lost combat target cannot snap to stale navigation");
      combatActive=0; liftResult=2; check(!AccRoute_GetSnapTarget(73000,&point,&aim),"waiting lift has no walking direction to snap");
      AccRoute_Reset();
    }
    /* Slot-2 VIEWD-05 survey must stay scoped to its lower floor and exit. */
    AccRoute_Reset(); strcpy(LevelName,"derelict");
    liftResult=0; controlCount=0; combatActive=0; blocked=secondBlocked=alien=-1;
    wayrouteResult=0; steerResult=ACC_STEER_DIRECT; traversalHint=0;
    located=70; target=objective=1; strategy.containingModule=&modules[69];
    links[69][0]=&nodes[70]; links[69][1]=NULL;
    entries[70].position=(VECTORCH){-46297,1606,-209698};
    dynamics.Position=(VECTORCH){-45803,3390,-217209};
    dynamics.OrientEuler.EulerY=4040; dynamics.IsInContactWithFloor=1;
    AccRoute_Toggle(); i=steerCalls; AccRoute_Update(90000);
    check(cuePosition.vx==-44860 && cuePosition.vz==-223304 && steerCalls==i,
          "saved recess guides back out instead of into its wall");
    dynamics.Position=(VECTORCH){-44860,5019,-223304}; AccRoute_Update(97000);
    check(cuePosition.vx==-40361 && cuePosition.vz==-224358,"survey crosses lower floor");
    dynamics.Position=(VECTORCH){-40361,5102,-224358}; AccRoute_Update(104000);
    check(cuePosition.vx==-40843 && cuePosition.vz==-220167,"survey approaches raised opening");
    dynamics.Position=(VECTORCH){-40843,3400,-220167}; dynamics.OrientEuler.EulerY=80;
    AccRoute_Update(111000);
    check(strstr(spoken,"Jump and keep moving forward")!=NULL && cuePosition.vz==-214670,
          "aligned opening gives forward jump instruction and matching beacon");
    dynamics.OrientEuler.EulerY=2048; AccRoute_Update(118000);
    check(!strstr(spoken,"Jump and"),"survey never tells player to jump facing away");
    dynamics.Position=(VECTORCH){-40061,2009,-215789}; AccRoute_Update(125000);
    check(cuePosition.vx==-46297 && cuePosition.vz==-209698,"upper landing resumes ordinary portal routing");
    strcpy(LevelName,"other"); dynamics.Position=(VECTORCH){-45803,3390,-217209};
    AccRoute_Update(132000);
    check(cuePosition.vx==-46297,"survey does not affect another level");
    strcpy(LevelName,"derelict"); entries[70].position.vx+=1000; AccRoute_Update(139000);
    check(cuePosition.vx==-45297,"survey does not affect a different exit");
    AccRoute_Reset();
    strcpy(LevelName,"derelict"); alien=76; blocked=secondBlocked=-1;
    links[75][0]=&nodes[76]; links[75][1]=NULL;
    entries[76].position=(VECTORCH){-28055,5614,-192795};
    check(AccRoute_NextModule(&nodes[75],&nodes[76])==&nodes[76],"verified downward ship descent allowed");
    entries[76].position.vx+=500;
    check(!AccRoute_NextModule(&nodes[75],&nodes[76]),"changed portal fails closed");
    entries[76].position.vx-=500; strcpy(LevelName,"other");
    check(!AccRoute_NextModule(&nodes[75],&nodes[76]),"descent exception cannot leak into other levels");
    strcpy(LevelName,"derelict"); alien=75; links[76][0]=&nodes[75]; links[76][1]=NULL;
    check(!AccRoute_NextModule(&nodes[76],&nodes[75]),"descent does not license climbing back up");
    alien=158; links[76][0]=&nodes[158]; entries[158].position=(VECTORCH){-31011,20591,-199200};
    check(AccRoute_NextModule(&nodes[76],&nodes[158])==&nodes[158],"verified lower descent allowed");
    alien=159; links[171][0]=&nodes[159]; entries[159].position=(VECTORCH){-116432,39485,-171232};
    check(AccRoute_NextModule(&nodes[171],&nodes[159])==&nodes[159],"verified final shaft descent allowed");
    alien=171; links[159][0]=&nodes[171];
    check(!AccRoute_NextModule(&nodes[159],&nodes[171]),"final descent does not license reverse climbing");
    strategy.containingModule=&modules[75]; located=76; alien=76;
    dynamics.Position=(VECTORCH){-26211,1616,-192777}; liftResult=1; liftPoint=(VECTORCH){-28055,843,-192793}; AccRoute_Toggle(); i=steerCalls;
    AccRoute_Update(150000);
    check(strstr(spoken,"Lift") && steerCalls==i,"descent cue bypasses floor avoidance at known descent");
    liftResult=0; AccRoute_Update(157000);
    check(!strcmp(spoken,"Lift. Wait."),"missing shaft platform cannot become a walking instruction");
    surveyMode=1; combatActive=1; liftResult=1; AccRoute_Update(164000);
    check(strstr(spoken,"Lift")!=NULL,"explicit survey mode keeps navigation during combat");
    surveyMode=0; i=cues; AccRoute_Update(171000);
    check(cues==i,"normal mode still yields guidance to combat");
    /* The first Predator switch and post-unlock gate approach are a narrow
       fall-only opening stage, and must not alter Marine objective guidance. */
    AccRoute_Reset(); strcpy(LevelName,"fall"); AvP.PlayerType=I_Predator;
    strategy.containingModule=&modules[94]; located=94; dynamics.Position=(VECTORCH){0,0,0};
    openingStage=1; controlReady=0; wayrouteResult=0; liftResult=0; AccRoute_Toggle();
    AccRoute_Update(180000);
    check(strstr(spoken,"Facility switch")!=NULL,
          "Predator opening guidance identifies the verified first facility switch");
    AccRoute_Reset(); openingStage=2; wayrouteResult=2; wayroutePoint=(VECTORCH){0,0,3000};
    dynamics.Position=(VECTORCH){17955,4452,322}; dynamics.IsInContactWithFloor=1;
    dynamics.LinVelocity=(VECTORCH){0,0,0}; dynamics.OrientEuler.EulerY=1195;
    AccRoute_Toggle(); i=wayrouteCalls; AccRoute_Update(181000);
    check(strstr(spoken,"First staging point") && cuePosition.vx==15750 && cuePosition.vz==0 && wayrouteCalls==i,
          "unlocked Predator route approaches the surveyed first staging point directly");
    dynamics.Position=(VECTORCH){15750,4741,0}; dynamics.LinVelocity=(VECTORCH){0,0,0};
    AccRoute_Update(182000);
    check(strstr(spoken,"Run forward") && cuePosition.vx==17680 && cuePosition.vz==-460,
          "first run-up instruction and beacon point at the takeoff line");
    { int aim;
      check(AccRoute_GetSnapTarget(182001,&point,&aim) && !aim &&
            point.vx==24311 && point.vz==-2274,
            "first run-up snap points toward the landing direction");
    }
    dynamics.Position=(VECTORCH){17390,4741,-381};
    dynamics.LinVelocity=(VECTORCH){12348,0,-3339}; dynamics.OrientEuler.EulerY=1195;
    AccRoute_Update(182020);
    check(!strcmp(spoken,"Jump now and keep moving forward."),
          "measured first takeoff window gives an immediate jump cue");
    dynamics.IsInContactWithFloor=0; dynamics.Position=(VECTORCH){23232,4501,-1980};
    AccRoute_Update(182040);
    check(strstr(spoken,"Keep moving forward") && cuePosition.vx==24311 && cuePosition.vz==-2274,
          "first airborne guidance continues toward the verified landing");
    { int aim;
      check(AccRoute_GetSnapTarget(182041,&point,&aim) && !aim &&
            point.vx==24311 && point.vz==-2274,
            "first jump snap remains pointed at the landing");
    }
    { int priorSpeech=speech;
      dynamics.Position=(VECTORCH){23600,4500,-2100}; AccRoute_Update(182141);
      check(speech==priorSpeech,"moving landing beacon does not repeat 100ms airborne speech");
    }
    dynamics.IsInContactWithFloor=1; dynamics.Position=(VECTORCH){24311,4739,-2274};
    AccRoute_Update(182261);
    check(strstr(spoken,"North deck") && cuePosition.vx==33242 && cuePosition.vz==-4654,
          "verified first landing advances onto the north-deck leg");
    dynamics.Position=(VECTORCH){27000,4740,-2990}; AccRoute_Update(182381);
    check(strstr(spoken,"North deck")!=NULL,"continuous north-deck corridor stays in scoped fall guidance");
    dynamics.Position=(VECTORCH){33242,4739,-4654}; AccRoute_Update(182501);
    { int aim;
      check(AccRoute_GetSnapTarget(182502,&point,&aim) && point.vx==40416 && point.vz==-5049,
            "north-deck progress selects the second run-up staging point");
    }
    dynamics.Position=(VECTORCH){40416,4738,-5049};
    dynamics.LinVelocity=(VECTORCH){-280,0,15998}; dynamics.OrientEuler.EulerY=4085;
    AccRoute_Update(182620);
    check(strstr(spoken,"Run forward") && cuePosition.vx==40372 && cuePosition.vz==-2528,
          "second run-up gives an early instruction before its narrow jump window");
    dynamics.Position=(VECTORCH){40387,4740,-3428};
    dynamics.LinVelocity=(VECTORCH){-140,0,8000}; AccRoute_Update(182640);
    check(strstr(spoken,"Run forward")!=NULL,"second run-up does not cue below measured minimum speed");
    dynamics.Position=(VECTORCH){40377,4740,-2828};
    dynamics.LinVelocity=(VECTORCH){-280,0,15998}; dynamics.OrientEuler.EulerY=4085;
    AccRoute_Update(182660);
    check(!strcmp(spoken,"Jump now and keep moving forward."),
          "second jump cue uses forward projected speed inside the wider early window");
    dynamics.IsInContactWithFloor=0; dynamics.Position=(VECTORCH){40183,4000,8665};
    AccRoute_Update(182680);
    check(strstr(spoken,"Keep moving forward") && cuePosition.vx==40183 && cuePosition.vz==8665,
          "second airborne guidance aims at the measured landing");
    dynamics.IsInContactWithFloor=1; dynamics.Position=(VECTORCH){40183,4737,8665};
    AccRoute_Update(182800);
    check(strstr(spoken,"Cross the opening") && cuePosition.vx==41427 && cuePosition.vz==21739,
          "verified second landing guides beyond the portal entry");
    dynamics.Position=(VECTORCH){41427,4740,21739}; AccRoute_Update(182920);
    check(strstr(spoken,"Continue through the opening") && !strstr(spoken,"arrived") &&
          cuePosition.vz==21739,"room-94 portal approach never announces false arrival");
    /* A save reload at the earlier checkpoint must reconcile from position,
       even while helper state still contains the late gate-walk phase. */
    dynamics.Position=(VECTORCH){17955,4452,322}; AccRoute_Update(183040);
    check(strstr(spoken,"First staging point") && cuePosition.vx==15750,
          "earlier checkpoint reload is reconciled from current position");
    dynamics.IsInContactWithFloor=0; dynamics.Position=(VECTORCH){30000,3000,0};
    AccRoute_Update(183160);
    check(strstr(spoken,"Falling outside the verified route")!=NULL,
          "unverified airborne position reports falling without inventing a landing route");
    { int priorSpeech=speech;
    AccRoute_Update(183260);
      check(speech==priorSpeech,"persistent falling state does not repeat its speech every 100ms");
    }
    { int aim;
      check(!AccRoute_GetSnapTarget(183261,&point,&aim),
            "unverified fall has no stale snap target");
    }
    AccRoute_Reset(); openingStage=2; AvP.PlayerType=I_Predator;
    strategy.containingModule=&modules[94]; located=94; dynamics.IsInContactWithFloor=1;
    dynamics.Position=(VECTORCH){17955,3300,322}; wayrouteResult=2;
    AccRoute_Toggle(); AccRoute_Update(183400);
    check(strstr(spoken,"Gate approach")!=NULL,
          "lower-floor guidance keeps the existing coarse gate approach fallback");
    strategy.containingModule=&modules[9]; located=9; wayrouteResult=0;
    dynamics.Position=(VECTORCH){41427,4740,21739};
    AccRoute_Update(186400);
    check(cuePosition.vx==objectivePoint.vx && cuePosition.vz==objectivePoint.vz,
          "entering room 9 hands off to ordinary objective guidance");

    /* The real gate05 switch is in blocked room 77 but is operable from the
       surveyed room-76 approach. Guidance selects it without opening graph edges. */
    AccRoute_Reset(); openingStage=0; strcpy(LevelName,"fall"); AvP.PlayerType=I_Predator;
    blocked=77; secondBlocked=alien=-1; gate05ControlStage=1; gate05ProbeCalls=0;
    for(i=76;i<129;++i) { links[i][0]=&nodes[i+1]; links[i][1]=links[i][2]=NULL; }
    modules[77].m_sbptr=&door; controlCount=0; controlReady=0;
    control.SBdptr=&controlDisplay; controlDisplay.ObStrategyBlock=&control;
    control.DynPtr=&controlDynamics;
    controlDynamics.Position=(VECTORCH){209310,11271,145250};
    controlDisplay.ObView.vz=1000; interactionObstruction=0;
    strategy.containingModule=&modules[76]; located=129; wayrouteResult=0;
    steerResult=ACC_STEER_DIRECT; dynamics.IsInContactWithFloor=1;
    dynamics.LinVelocity=(VECTORCH){0,0,0};
    dynamics.Position=(VECTORCH){200000,12533,144370};
    AccRoute_Toggle(); i=wayrouteCalls; AccRoute_Update(187000);
    check(gate05ProbeCalls>0 && strstr(spoken,"Door control") && cuePosition.vx==206976 && cuePosition.vy==12533 &&
          cuePosition.vz==144370 && wayrouteCalls==i+1,
          "locked gate05 guidance routes to the verified front-side approach pose");
    dynamics.Position=(VECTORCH){206976,12533,144370};
    { int aim=0;
      check(AccRoute_GetSnapTarget(187100,&point,&aim) && aim &&
            point.vx==209310 && point.vy==11271 && point.vz==145250,
            "near gate05 approach snaps toward the physical linked switch, not its movement waypoint");
    }
    controlReady=1;
    { int aim=0;
      check(AccRoute_GetSnapTarget(187200,&point,&aim) && aim &&
            point.vx==209310 && point.vy==11271 && point.vz==145250,
            "operable gate05 control retains switch-directed snap aim");
    }
    AccRoute_Update(188200);
    check(!strcmp(spoken,"Press Interact.") && cuePosition.vx==206976 && cuePosition.vz==144370,
          "gate05 route reports Interact only when the exact linked switch is operable in line of sight");

    AccRoute_Reset(); controlReady=0; gate05ControlStage=0; wayrouteResult=2;
    wayroutePoint=(VECTORCH){205000,12533,144370}; dynamics.Position=(VECTORCH){200000,12533,144370};
    AccRoute_Toggle(); i=wayrouteCalls; AccRoute_Update(189200);
    check(strstr(spoken,"Closed door approach") && strstr(spoken,"No reachable usable control identified") &&
          cuePosition.vx==wayroutePoint.vx && wayrouteCalls==i+1,
          "unresolved closed-door boundary follows local authored waypoints without implying traversal");
    AccRoute_Reset(); blocked=-1; alien=77; gate05ControlStage=1;
    wayrouteResult=2; i=wayrouteCalls; AccRoute_Toggle(); AccRoute_Update(190200);
    check(strstr(spoken,"Unverified passage on the route") && !strstr(spoken,"Door control") &&
          wayrouteCalls==i,
          "gate05 control and boundary-one waypoint exceptions never waive alien-only boundary-two limits");

    /* In the exact fall/Predator room-85 final objective, the linked eligible
       disabled-lift switch takes precedence over the inaccessible upper-room
       graph frontier. Other rooms/species/goals keep ordinary routing. */
    AccRoute_Reset(); blocked=secondBlocked=alien=-1; gate05ControlStage=0;
    strcpy(LevelName,"fall"); AvP.PlayerType=I_Predator;
    strategy.containingModule=&modules[85]; located=97; objectivePoint=(VECTORCH){0,0,10000};
    dynamics.Position=(VECTORCH){1000,0,2000};
    finalLiftStage=1; finalLiftProbeCalls=0;
    finalLiftControlPosition=(VECTORCH){1000,0,2000}; controlRoom[0]=85;
    controlReady=0; control.SBdptr=&controlDisplay; controlDisplay.ObStrategyBlock=&control;
    controlDisplay.ObView.vz=1000; wayrouteResult=0; liftResult=0;
    i=wayrouteCalls;
    AccRoute_Toggle(); AccRoute_Update(199000);
    check(finalLiftProbeCalls==1 && strstr(spoken,"Final lift switch") &&
          cuePosition.vx==1000 && cuePosition.vz==2000 && wayrouteCalls==i,
          "final lift matcher routes to its eligible switch before generic frontier search");
    i=finalLiftProbeCalls; strategy.containingModule=&modules[84]; located=97;
    AccRoute_Reset(); AccRoute_Toggle(); AccRoute_Update(199100);
    check(finalLiftProbeCalls==i,
          "final lift control interception does not leak outside source room 85");
    i=finalLiftProbeCalls; strategy.containingModule=&modules[85]; located=98;
    AccRoute_Reset(); AccRoute_Toggle(); AccRoute_Update(199200);
    check(finalLiftProbeCalls==i,
          "final lift matcher requires verified trigger room 97, not neighboring room 98");
    i=finalLiftProbeCalls; located=96;
    AccRoute_Reset(); AccRoute_Toggle(); AccRoute_Update(199200);
    check(finalLiftProbeCalls==i,
          "final lift control interception does not replace unrelated objectives");

    /* The disabled shaft control's straight stair approach stalled on the
       south side. Route first to the measured upper landing; do not announce
       Interact or snap to that waypoint as though it were the switch. */
    AccRoute_Reset(); finalLiftStage=1; finalLiftControlPosition=(VECTORCH){189886,28119,62923};
    strategy.containingModule=&modules[85]; located=97; dynamics.Position=(VECTORCH){190386,32010,49019};
    dynamics.IsInContactWithFloor=1; finalLiftLive=0; controlReady=1;
    control.SBdptr=&controlDisplay; controlDisplay.ObStrategyBlock=&control;
    controlDisplay.ObView.vz=1000; wayrouteResult=2;
    wayroutePoint=(VECTORCH){250000,-5000,250000}; steerResult=ACC_STEER_DETOUR;
    { int authored=wayrouteCalls, steers=steerCalls, exits=exitLiftCalls, aim=0;
      AccRoute_Toggle(); AccRoute_Update(199300);
      check(strstr(spoken,"Final lift approach") && strstr(spoken,"upper stair landing") &&
            !strstr(spoken,"Press Interact") && wayrouteCalls==authored && steerCalls==steers &&
            exitLiftCalls==exits && cuePosition.vx==187966 && cuePosition.vy==29675 && cuePosition.vz==58900,
            "south-stair final-lift approach cues its measured upper landing without generic detours");
      check(AccRoute_GetSnapTarget(199301,&point,&aim) && !aim &&
            point.vx==187966 && point.vy==29675 && point.vz==58900,
            "approach-stage snap remains on the landing waypoint, not the switch");
      dynamics.Position=(VECTORCH){190076,32016,54465};
      check(AccRoute_GetSnapTarget(199350,&point,&aim) && !aim &&
            point.vx==187966 && point.vy==29675 && point.vz==58900,
            "original direct-stairs stall pose remains on the bounded corner approach");
    }
    dynamics.Position=(VECTORCH){187909,30066,58142};
    { int aim=0;
      check(AccRoute_GetSnapTarget(199400,&point,&aim) && !aim &&
            point.vx==187966 && point.vy==29675 && point.vz==58900,
          "grounded pre-step position does not prematurely skip the upper landing");
    }
    dynamics.Position=(VECTORCH){187940,30066,58800};
    { int aim=0;
      check(AccRoute_GetSnapTarget(199500,&point,&aim) && !aim &&
            point.vx==187966 && point.vy==29675 && point.vz==58900,
            "z beyond the landing alone cannot skip while foot height has not passed");
    }
    dynamics.IsInContactWithFloor=0;
    dynamics.Position=(VECTORCH){187966,29675,58900};
    { int aim=0;
      check(AccRoute_GetSnapTarget(199600,&point,&aim) && !aim &&
            point.vx==187966 && point.vy==29675 && point.vz==58900,
            "airborne upper-landing proximity cannot release the approach stage");
    }
    dynamics.IsInContactWithFloor=1;
    dynamics.Position=(VECTORCH){187966,29675,58900}; steerResult=ACC_STEER_DIRECT;
    wayrouteResult=0;
    { int aim=0;
      check(AccRoute_GetSnapTarget(200401,&point,&aim) && !aim &&
            point.vx==finalLiftControlPosition.vx && point.vy==finalLiftControlPosition.vy &&
            point.vz==finalLiftControlPosition.vz,
            "grounded upper landing advances toward the actual final-lift switch without backtracking");
    }
    dynamics.Position=finalLiftControlPosition;
    { int aim=0;
      check(AccRoute_GetSnapTarget(201402,&point,&aim) && aim &&
            point.vx==finalLiftControlPosition.vx && point.vy==finalLiftControlPosition.vy &&
            point.vz==finalLiftControlPosition.vz,
            "nearby operable final-lift switch retains switch-directed snap and interaction target");
    }
    AccRoute_Reset(); finalLiftStage=0; controlReady=0; located=98;

    /* Once the exact final switch is enabled, preserve lift states and the
       measured two-leg top exit instead of falling back to graph frontiers. */
    located=97; strategy.containingModule=&modules[85]; finalLiftStage=2;
    finalLiftLive=1; finalLiftContact=1; liftResult=2;
    liftPoint=(VECTORCH){187235,25226,70510}; finalLiftFindCalls=0;
    AccRoute_Toggle(); AccRoute_Update(199300);
    check(finalLiftFindCalls==1 && !strcmp(spoken,"Lift. Wait.") &&
          cuePosition.vx==liftPoint.vx && cuePosition.vy==liftPoint.vy,
          "enabled final shaft preserves the measured occupied-lift wait state");

    /* Guidance may begin mid-ride after a load; exact contact plus signature
       rehydrates the route in every surveyed intermediate shaft room. */
    strategy.containingModule=&modules[89]; liftResult=2;
    liftPoint=(VECTORCH){187235,14000,70510};
    AccRoute_Update(199400);
    check(!strcmp(spoken,"Lift. Wait.") && cuePosition.vy==liftPoint.vy,
          "mid-ride guidance in an intermediate shaft room resumes safely");
    finalLiftLive=0; AccRoute_Update(199500);
    check(strstr(spoken,"unverified") && cuePosition.vx==dynamics.Position.vx &&
          cuePosition.vz==dynamics.Position.vz,
          "lost platform identity during an active shaft leg stops at the current position");

    /* The terminal itinerary must bypass generic graph/authored waypoints. */
    finalLiftLive=1; finalLiftContact=0; strategy.containingModule=&modules[92];
    finalLiftExitStage=1; finalLiftExitPoint=(VECTORCH){188123,845,72000};
    wayrouteResult=2; wayroutePoint=(VECTORCH){999,845,999}; i=wayrouteCalls;
    { int exits=exitLiftCalls;
      int steers=steerCalls;
      steerResult=ACC_STEER_DETOUR; steerPoint=(VECTORCH){250000,-5000,250000};
      AccRoute_Update(199600);
      check(exitLiftCalls==exits && steerCalls==steers && wayrouteCalls==i &&
            cuePosition.vx==finalLiftExitPoint.vx && cuePosition.vz==finalLiftExitPoint.vz,
            "upper shaft north stage bypasses generic lift detours and preserves its measured cue");
      { int aim=0;
        check(AccRoute_GetSnapTarget(199601,&point,&aim) && !aim &&
              point.vx==finalLiftExitPoint.vx && point.vz==finalLiftExitPoint.vz,
              "upper shaft north-stage snap remains on the measured exit point");
      }
    }
    finalLiftExitStage=2; finalLiftExitPoint=(VECTORCH){182800,845,72000};
    AccRoute_Update(199702);
    check(cuePosition.vx==finalLiftExitPoint.vx && cuePosition.vz==finalLiftExitPoint.vz,
          "upper shaft exit advances to the measured west stage");

    /* Successful measured handoff resumes ordinary room-graph guidance. */
    finalLiftComplete=1; finalLiftExitStage=0; finalLiftCompleteCalls=0;
    liftResult=0; wayrouteResult=2; wayroutePoint=(VECTORCH){4000,845,80000};
    steerResult=ACC_STEER_DIRECT;
    strategy.containingModule=&modules[92]; i=wayrouteCalls;
    AccRoute_Update(199803);
    check(finalLiftCompleteCalls==1 && wayrouteCalls==i+1 &&
          cuePosition.vx==wayroutePoint.vx && cuePosition.vz==wayroutePoint.vz,
          "verified grounded top handoff releases the shaft route to the normal graph");

    /* With no exact platform while already in the shaft, guidance waits and
       never reactivates from an arbitrary room-92 floor after completion. */
    AccRoute_Reset(); finalLiftStage=2; finalLiftComplete=0; finalLiftExitStage=0;
    finalLiftLive=0; finalLiftContact=0; liftResult=0;
    strategy.containingModule=&modules[91]; located=97; wayrouteResult=0;
    AccRoute_Toggle(); AccRoute_Update(199900);
    check(strstr(spoken,"unverified") && cuePosition.vx==dynamics.Position.vx &&
          cuePosition.vz==dynamics.Position.vz,
          "missing final-shaft platform in an intermediate room fails closed");
    AccRoute_Reset(); strategy.containingModule=&modules[92]; located=97;
    AccRoute_Toggle(); AccRoute_Update(200000);
    check(!strstr(spoken,"Final lift") && finalLiftExitCalls>0,
          "unrelated room-92 floor cannot reacquire completed shaft guidance");
    AccRoute_Reset(); finalLiftStage=0; finalLiftLive=finalLiftContact=0;
    located=98; wayrouteResult=0;

    strategy.containingModule=&modules[9]; located=9;
    openingStage=1; AvP.PlayerType=I_Marine;
    strcpy(LevelName,"fall"); target=1; objective=1; wayrouteResult=0;
    AccRoute_Toggle(); AccRoute_Update(200000);
    check(strstr(spoken,"Objective area")!=NULL,
          "Marine guidance continues to use its objective target on the same map");
    AccRoute_Reset(); openingStage=0; AvP.PlayerType=I_Marine;
    AccRoute_Reset();
    printf("route: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}

