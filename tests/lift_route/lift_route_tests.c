#include <stdio.h>
#include <string.h>

#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "dynblock.h"
#include "bh_types.h"
#include "bh_plift.h"
#include "gamedef.h"
#include "acc_lift_route.h"

#define TEST_ACTIVE_CAPACITY 8

DISPLAYBLOCK *Player;
AVP_GAME_DESC AvP;
char LevelName[40] = "fixture";
STRATEGYBLOCK *ActiveStBlockList[TEST_ACTIVE_CAPACITY];
int NumActiveStBlocks;

static AIMODULE liftRoom, otherRoom;
static MODULE liftModule;
static DISPLAYBLOCK playerDisplay;
static STRATEGYBLOCK playerStrategy, liftStrategy;
static DYNAMICSBLOCK playerDynamics, liftDynamics;
static PLATFORMLIFT_BEHAVIOUR_BLOCK lift;
static COLLISIONREPORT playerContact;
static int checks, failures;
static STRATEGYBLOCK otherLiftStrategy;

static void check(int condition, const char *message)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL: %s\n", message); }
    else printf("PASS: %s\n", message);
}

static void reset_fixture(void)
{
    snprintf(LevelName,sizeof(LevelName),"fixture");
    AvP.PlayerType=I_Marine;
    memset(&liftRoom, 0, sizeof(liftRoom));
    memset(&otherRoom, 0, sizeof(otherRoom));
    memset(&liftModule, 0, sizeof(liftModule));
    memset(&playerDisplay, 0, sizeof(playerDisplay));
    memset(&playerStrategy, 0, sizeof(playerStrategy));
    memset(&liftStrategy, 0, sizeof(liftStrategy));
    memset(&playerDynamics, 0, sizeof(playerDynamics));
    memset(&liftDynamics, 0, sizeof(liftDynamics));
    memset(&lift, 0, sizeof(lift));
    memset(&playerContact, 0, sizeof(playerContact));
    memset(ActiveStBlockList, 0, sizeof(ActiveStBlockList));

    liftModule.m_aimodule = &liftRoom;
    lift.upHeight = 0;
    lift.downHeight = 4000;
    lift.state = PLBS_AtRest;
    lift.Enabled = 1;
    liftStrategy.I_SBtype = I_BehaviourPlatform;
    liftStrategy.SBdataptr = &lift;
    liftStrategy.DynPtr = &liftDynamics;
    liftStrategy.containingModule = &liftModule;
    liftDynamics.Position = (VECTORCH){0, 0, 0};

    playerStrategy.DynPtr = &playerDynamics;
    playerDisplay.ObStrategyBlock = &playerStrategy;
    Player = &playerDisplay;
    ActiveStBlockList[0] = &liftStrategy;
    NumActiveStBlocks = 1;
}

static int find(VECTORCH feet, VECTORCH target, VECTORCH *point)
{ return AccLiftRoute_Find(&liftRoom, &feet, &target, point); }

static int same_point(VECTORCH a, VECTORCH b)
{ return a.vx == b.vx && a.vy == b.vy && a.vz == b.vz; }

static void test_enabled_different_terminal_approach(void)
{
    VECTORCH point = {91, 92, 93};
    reset_fixture();
    check(find((VECTORCH){4000, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 1 &&
          same_point(point, (VECTORCH){0, -975, 0}),
          "enabled same-room lift routes player toward the opposite-terminal boarding point");
}

static void test_no_route_for_same_floor_or_ineligible_lift(void)
{
    VECTORCH point = {91, 92, 93};
    reset_fixture();
    check(find((VECTORCH){3000, 0, 0}, (VECTORCH){0, 100, 0}, &point) == 0,
          "no lift route when target is on the player's current terminal floor");

    reset_fixture(); lift.Enabled = 0;
    check(find((VECTORCH){3000, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 0,
          "disabled lift is not offered");

    reset_fixture(); liftStrategy.SBflags.please_destroy_me = 1;
    check(find((VECTORCH){3000, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 0,
          "destroy-pending lift is not offered");

    reset_fixture(); liftModule.m_aimodule = &otherRoom;
    check(find((VECTORCH){3000, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 0,
          "lift in a different AI room is not offered");
}

static void test_shaft_wait_if_platform_elsewhere(void)
{
    VECTORCH point = {91, 92, 93};
    reset_fixture();
    liftDynamics.Position.vy = lift.downHeight;
    check(find((VECTORCH){500, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 2 &&
          same_point(point, (VECTORCH){0, -975, 0}),
          "near shaft waits at the boarding point while platform is at the other terminal");
}

static void set_platform_contact(int normalY)
{
    playerContact.ObstacleSBPtr = &liftStrategy;
    playerContact.ObstacleNormal = (VECTORCH){0, normalY, 0};
    playerContact.NextCollisionReportPtr = NULL;
    playerDynamics.CollisionReportPtr = &playerContact;
}

static void test_aboard_wait_and_destination_exit(void)
{
    VECTORCH point = {91, 92, 93};
    reset_fixture();
    set_platform_contact(-50000);
    lift.state = PLBS_GoingUp;
    liftDynamics.Position.vy = 1800;
    check(find((VECTORCH){3000, 1800, 0}, (VECTORCH){0, 4000, 0}, &point) == 2 &&
          same_point(point, liftDynamics.Position),
          "actual upward-facing platform contact while lift moves returns aboard-wait");

    reset_fixture();
    set_platform_contact(-50000);
    lift.state = PLBS_AtRest;
    liftDynamics.Position.vy = lift.downHeight;
    check(find((VECTORCH){0, 4000, 0}, (VECTORCH){100, 4000, 200}, &point) == 3 &&
          same_point(point, (VECTORCH){100, 4000, 200}),
          "aboard platform at rest on destination terminal releases route to target");

    reset_fixture();
    set_platform_contact(-50000);
    lift.state = PLBS_Activating;
    liftDynamics.Position.vy = lift.downHeight;
    check(find((VECTORCH){0, 4000, 0}, (VECTORCH){100, 4000, 200}, &point) == 3 &&
          same_point(point, (VECTORCH){100, 4000, 200}),
          "aboard platform activating at destination terminal releases route to target");

    reset_fixture();
    set_platform_contact(-50000);
    lift.state = PLBS_Activating;
    liftDynamics.Position.vy = lift.upHeight;
    check(find((VECTORCH){0, 0, 0}, (VECTORCH){100, 4000, 200}, &point) == 2 &&
          same_point(point, liftDynamics.Position),
          "aboard platform activating at origin terminal remains in lift-wait state");
}

static void test_landing_hold_requires_same_lift_height_and_floor_contact(void)
{
    VECTORCH point={91,92,93};
    reset_fixture();
    lift.state=PLBS_Activating;
    liftDynamics.Position.vy=lift.downHeight;
    set_platform_contact(-50000);
    check(find((VECTORCH){0,4000,0},(VECTORCH){100,4000,200},&point)==3 &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "landing hold is granted for the queried lift with actual floor contact at destination height");

    memset(&otherLiftStrategy,0,sizeof(otherLiftStrategy));
    check(!AccLiftRoute_HoldAtLanding(&otherLiftStrategy),
          "landing hold does not apply to another platform lift");
    liftDynamics.Position.vy=lift.downHeight+200;
    check(!AccLiftRoute_HoldAtLanding(&liftStrategy),
          "landing hold releases when the platform leaves its remembered terminal height");
    liftDynamics.Position.vy=lift.downHeight;
    playerDynamics.CollisionReportPtr=NULL;
    check(!AccLiftRoute_HoldAtLanding(&liftStrategy),
          "landing hold releases when player steps off and floor contact disappears");

    set_platform_contact(-50000);
    check(find((VECTORCH){0,4000,0},(VECTORCH){100,4000,200},&point)==3 &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "fresh destination query reacquires landing hold");
    playerContact.ObstacleNormal.vy=0;
    check(!AccLiftRoute_HoldAtLanding(&liftStrategy),
          "side-facing contact is not enough to retain landing hold");
    playerContact.ObstacleNormal.vy=-50000;
    AccLiftRoute_Reset();
    check(!AccLiftRoute_HoldAtLanding(&liftStrategy),
          "lift-route reset clears remembered hold");

    check(find((VECTORCH){0,4000,0},(VECTORCH){100,4000,200},&point)==3 &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "destination query arms hold before requery-release check");
    check(find((VECTORCH){0,4000,0},(VECTORCH){0,0,0},&point)==2 &&
          !AccLiftRoute_HoldAtLanding(&liftStrategy),
          "a new query for the opposite terminal releases the previous landing hold");
}

static void test_side_contact_not_aboard_and_far_target_rejected(void)
{
    VECTORCH point = {91, 92, 93};
    reset_fixture();
    set_platform_contact(0); /* touching the side is not platform-floor contact */
    check(find((VECTORCH){4000, 0, 0}, (VECTORCH){0, 4000, 0}, &point) == 1,
          "side contact with platform does not falsely count as aboard");

    reset_fixture();
    point = (VECTORCH){91, 92, 93};
    check(find((VECTORCH){100, 0, 0}, (VECTORCH){0, 10000, 0}, &point) == 0 &&
          same_point(point, (VECTORCH){91, 92, 93}),
          "target too far from either terminal is rejected without output mutation");
}

static void test_aboard_platform_survives_room_change(void)
{
    VECTORCH point={91,92,93};
    reset_fixture();
    liftRoom.m_index=75; otherRoom.m_index=158;
    set_platform_contact(-50000);
    liftDynamics.Position.vy=lift.downHeight;
    check(find((VECTORCH){0,4000,0},(VECTORCH){100,4000,200},&point)==3 &&
          same_point(point,(VECTORCH){100,4000,200}),
          "aboard lift is recognized after player crosses into another AI room");
}

static void test_final_shaft_step_off_keeps_landing_hold(void)
{
    VECTORCH point={91,92,93};
    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"derelict");
    liftRoom.m_index=171; otherRoom.m_index=160;
    liftDynamics.Position=(VECTORCH){-116427,45229,-171227};
    lift.upHeight=29650; lift.downHeight=45229; lift.state=PLBS_Activating;
    set_platform_contact(-50000);
    check(AccLiftRoute_Find(&liftRoom,
          &(VECTORCH){-116427,45229,-171227},
          &(VECTORCH){-118480,47166,-171232},&point)==3,
          "final shaft lower landing arms hold while player is aboard");

    playerDynamics.Position=(VECTORCH){-118485,47174,-171205};
    playerDynamics.CollisionReportPtr=NULL;
    liftRoom.m_index=160;
    check(AccLiftRoute_Find(&otherRoom,&playerDynamics.Position,
          &(VECTORCH){-118480,47166,-171232},&point)==3 &&
          same_point(point,(VECTORCH){-118480,47166,-171232}) &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "contact gap in surveyed lower step-off area retains landing and exit route");

    playerDynamics.Position.vx=-116427+2701;
    check(AccLiftRoute_Find(&otherRoom,&playerDynamics.Position,
          &(VECTORCH){-118480,47166,-171232},&point)==0 &&
          !AccLiftRoute_HoldAtLanding(&liftStrategy),
          "leaving the bounded final-shaft step-off area releases hold");

    playerDynamics.Position=(VECTORCH){-118485,47174,-171205};
    set_platform_contact(-50000); liftRoom.m_index=171;
    check(AccLiftRoute_Find(&liftRoom,&playerDynamics.Position,
          &(VECTORCH){-118480,47166,-171232},&point)==3,
          "final shaft hold can be re-armed before explicit route reset");
    playerDynamics.CollisionReportPtr=NULL;
    AccLiftRoute_Reset();
    check(!AccLiftRoute_HoldAtLanding(&liftStrategy),
          "route reset releases final-shaft landing hold despite step-off proximity");
}

static void test_only_surveyed_derelict_shafts_cross_rooms(void)
{
    VECTORCH point={91,92,93},feet,target;
    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"derelict");
    liftRoom.m_index=75; otherRoom.m_index=158;
    liftDynamics.Position=(VECTORCH){-28055,30921,-192793};
    lift.upHeight=1818; lift.downHeight=30921;
    feet=(VECTORCH){-28055,1818,-192793}; target=(VECTORCH){-58871,30921,-198051};
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==2 &&
          same_point(point,(VECTORCH){-28055,843,-192793}),
          "surveyed Derelict SHIPCOR lift is recognized across to HOLE room");

    point=(VECTORCH){91,92,93}; liftDynamics.Position.vx=-28055+251;
    feet.vx=-28055;
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==0 &&
          same_point(point,(VECTORCH){91,92,93}),
          "known shaft rejects platform outside 250mm horizontal tolerance");

    point=(VECTORCH){91,92,93}; feet.vx=-28055; liftDynamics.Position.vx=-28055;
    lift.downHeight=30921+251;
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==0,
          "known shaft rejects terminal height outside 250mm tolerance");

    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"otherlevel");
    liftRoom.m_index=75; otherRoom.m_index=158;
    liftDynamics.Position=(VECTORCH){-28055,1818,-192793};
    lift.upHeight=1818; lift.downHeight=30921;
    feet=(VECTORCH){-28055,1818,-192793}; target=(VECTORCH){0,30921,0};
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==0,
          "known coordinates do not enable cross-room lift routing outside Derelict");

    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"derelict");
    liftRoom.m_index=75; otherRoom.m_index=74;
    liftDynamics.Position=(VECTORCH){-28055,1818,-192793};
    lift.upHeight=1818; lift.downHeight=30921;
    feet=(VECTORCH){-28055,1818,-192793}; target=(VECTORCH){0,30921,0};
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==0,
          "known shaft remains unavailable from an unlisted room");

    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"derelict");
    liftRoom.m_index=171; otherRoom.m_index=159;
    liftDynamics.Position=(VECTORCH){-116427,45229,-171227};
    lift.upHeight=29650; lift.downHeight=45229;
    feet=(VECTORCH){-116427,29650,-171227}; target=(VECTORCH){-118480,45229,-171232};
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==2 &&
          same_point(point,(VECTORCH){-116427,28675,-171227}),
          "surveyed Derelict lift-shaft platform is recognized across rooms 171 and 159");

    /* On the final shaft, platform origins sit 1945mm below the standing
       player's feet at both terminals. These are observed in-game heights. */
    liftDynamics.Position=(VECTORCH){-116427,29650,-171227};
    feet=(VECTORCH){-116427,31595,-171227}; target=(VECTORCH){-118480,47166,-171232};
    check(AccLiftRoute_Find(&liftRoom,&feet,&target,&point)==1 &&
          same_point(point,(VECTORCH){-116427,28675,-171227}),
          "final Derelict shaft accepts surveyed upper standing height 31595");

    liftRoom.m_index=159; otherRoom.m_index=171;
    liftDynamics.Position=(VECTORCH){-116427,45229,-171227};
    feet=(VECTORCH){-116427,47174,-171227}; target=(VECTORCH){-118480,29650,-171232};
    check(AccLiftRoute_Find(&liftRoom,&feet,&target,&point)==1 &&
          same_point(point,(VECTORCH){-116427,44254,-171227}),
          "final Derelict shaft accepts surveyed lower standing height 47174");

    reset_fixture();
    snprintf(LevelName,sizeof(LevelName),"derelict");
    liftRoom.m_index=75; otherRoom.m_index=158;
    liftDynamics.Position=(VECTORCH){-28055,1818,-192793};
    lift.upHeight=1818; lift.downHeight=30921;
    feet=(VECTORCH){-28055,3763,-192793}; target=(VECTORCH){-58871,30921,-198051};
    check(AccLiftRoute_Find(&liftRoom,&feet,&target,&point)==0,
          "final-shaft standing offset is not applied to the first Derelict shaft");
}

static void prepare_fall_lift(void)
{
    snprintf(LevelName,sizeof(LevelName),"fall");
    AvP.PlayerType=I_Predator;
    playerStrategy.I_SBtype=I_BehaviourMarinePlayer; /* real player strategy type is shared across species */
    liftRoom.m_index=85; otherRoom.m_index=92;
    liftDynamics.Position=(VECTORCH){187235,-2076,70510};
    lift.upHeight=-2076; lift.downHeight=25226;
    lift.Enabled=0; /* the surveyed one-use platform disables after its ride */
}

static void test_fall_shaft_identity_and_exit(void)
{
    VECTORCH feet,target,point={1,2,3};
    STRATEGYBLOCK *found;
    reset_fixture(); prepare_fall_lift();
    check(AccLiftRoute_IsFallShaftRoom(&liftRoom) &&
          AccLiftRoute_IsFallShaftRoom(&otherRoom),
          "Fall shaft recognition covers the surveyed lower and upper AI rooms");
    otherRoom.m_index=90;
    check(!AccLiftRoute_IsFallShaftRoom(&otherRoom),
          "Fall shaft recognition rejects unlisted neighboring AI rooms");
    otherRoom.m_index=92;
    found=AccLiftRoute_FindFallPlatform(&otherRoom);
    check(found==&liftStrategy,
          "exact Fall platform is identified across rooms even after one-use disable");
    memset(&otherLiftStrategy,0,sizeof(otherLiftStrategy));
    otherLiftStrategy.I_SBtype=I_BehaviourPlatform;
    otherLiftStrategy.SBdataptr=&lift;
    otherLiftStrategy.DynPtr=&liftDynamics;
    ActiveStBlockList[1]=&otherLiftStrategy; NumActiveStBlocks=2;
    check(AccLiftRoute_FindFallPlatform(&otherRoom)==NULL,
          "ambiguous duplicate Fall platform signatures fail closed");
    lift.Enabled=1;
    check(AccLiftRoute_Find(&liftRoom,&(VECTORCH){187235,845,70510},
          &(VECTORCH){187235,25226,70510},&point)==0,
          "ambiguous Fall platform signature cannot fall back to generic same-room boarding");
    ActiveStBlockList[1]=NULL; NumActiveStBlocks=1;
    liftDynamics.Position.vx+=251;
    check(AccLiftRoute_FindFallPlatform(&otherRoom)==NULL,
          "Fall platform identity rejects a shifted platform outside surveyed tolerance");
    liftDynamics.Position.vx-=251;
    snprintf(LevelName,sizeof(LevelName),"otherlevel");
    check(AccLiftRoute_FindFallPlatform(&otherRoom)==NULL,
          "Fall coordinates do not activate shaft identity on another level");
    snprintf(LevelName,sizeof(LevelName),"fall");
    AvP.PlayerType=I_Marine;
    check(!AccLiftRoute_FindFallPlatform(&otherRoom) &&
          AccLiftRoute_Find(&otherRoom,&(VECTORCH){187235,28139,70510},
          &(VECTORCH){190000,-420,76000},&point)==0,
          "Fall shaft identity and cross-room boarding support are Predator-only");
    AvP.PlayerType=I_Predator;

    lift.Enabled=1;
    check(AccLiftRoute_Find(&liftRoom,&(VECTORCH){187235,28139,70510},
          &(VECTORCH){190000,-420,76000},&point)==2,
          "measured 2913mm floor offset routes the lower landing to the upper terminal");
    check(AccLiftRoute_Find(&liftRoom,&(VECTORCH){190216,28139,63594},
          &(VECTORCH){190000,-420,76000},&point)==2 &&
          same_point(point,(VECTORCH){190216,28139,63594}),
          "while the exact platform is high, lower-shaft guidance waits at current safe pose");
    liftDynamics.Position.vy=lift.upHeight;
    check(AccLiftRoute_Find(&liftRoom,&(VECTORCH){188123,28139,69715},
          &(VECTORCH){190000,-420,76000},&point)==2 &&
          same_point(point,(VECTORCH){188123,28139,69715}),
          "Fall lower-terminal boarding targets the surveyed east-side approach and waits there");
    liftDynamics.Position.vy=lift.downHeight;
    check(AccLiftRoute_Find(&liftRoom,&(VECTORCH){188123,28139,69715},
          &(VECTORCH){190000,-420,76000},&point)==1 &&
          same_point(point,(VECTORCH){188123,28139,69715}),
          "Fall approach remains stable when the platform arrives at the lower terminal");
    liftDynamics.Position.vy=lift.upHeight;
    check(AccLiftRoute_Find(&otherRoom,&(VECTORCH){187235,845,70510},
          &(VECTORCH){190000,-420,76000},&point)==0,
          "upper-room floor at the destination does not restart boarding toward that same terminal");

    set_platform_contact(-50000);
    feet=(VECTORCH){188151,846,71308};
    target=(VECTORCH){190000,-420,76000};
    check(AccLiftRoute_IsFallPlatformContact(&otherRoom,&feet),
          "actual upward collision contact identifies the same platform in an intermediate shaft room");
    playerDynamics.IsInContactWithFloor=1;
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,&feet,&target,&point)==1 &&
          same_point(point,(VECTORCH){188123,846,72000}),
          "upper platform contact starts the measured northbound exit stage");
    check(AccLiftRoute_HoldAtLanding(&liftStrategy),
          "upper exit stage retains the lift at its measured top terminal");

    feet=(VECTORCH){188151,845,72052};
    playerDynamics.Position=feet;
    playerDynamics.CollisionReportPtr=NULL;
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,&feet,&target,&point)==2 &&
          same_point(point,(VECTORCH){182800,845,72000}),
          "verified north stage advances to the westbound floor handoff");
    check(!AccLiftRoute_CompleteFallExit(&otherRoom,
          &(VECTORCH){184100,845,72000}),
          "grounded position east of the measured floor edge cannot complete the handoff");
    feet=(VECTORCH){184000,845,72000}; playerDynamics.Position=feet;
    playerDynamics.IsInContactWithFloor=1;
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==3 &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "west-leg contact gap retains the exact upper platform while grounded in the verified exit corridor");
    playerDynamics.IsInContactWithFloor=0;
    check(AccLiftRoute_Find(&otherRoom,&feet,&target,&point)==3 &&
          AccLiftRoute_HoldAtLanding(&liftStrategy),
          "brief ungrounded contact gap in the bounded west exit leg retains the upper lift");
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,
          &(VECTORCH){188151,845,71308},&target,&point)==1,
          "returning before the north waypoint restores stage one");
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,
          &(VECTORCH){184000,4000,72000},&target,&point)==0 &&
          !AccLiftRoute_HoldAtLanding(&liftStrategy),
          "falling outside the upper-floor corridor cancels exit guidance and releases the hold");
    playerDynamics.IsInContactWithFloor=0;
    check(!AccLiftRoute_CompleteFallExit(&otherRoom,
          &(VECTORCH){181870,845,72221}),
          "airborne proximity cannot complete the upper-floor handoff");
    playerDynamics.IsInContactWithFloor=1;
    check(!AccLiftRoute_CompleteFallExit(&otherRoom,
          &(VECTORCH){184500,845,72221}),
          "ground contact outside the measured handoff radius cannot release the lift");
    check(AccLiftRoute_CompleteFallExit(&otherRoom,
          &(VECTORCH){181870,845,72221}) &&
          !AccLiftRoute_HoldAtLanding(&liftStrategy),
          "grounded measured handoff completes and releases the held Fall platform");
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,
          &(VECTORCH){181870,845,72221},&target,&point)==0,
          "completed handoff does not restart the itinerary on the upper room floor");

    AccLiftRoute_Reset();
    playerDynamics.IsInContactWithFloor=1;
    check(AccLiftRoute_GetFallExitPoint(&otherRoom,
          &(VECTORCH){186000,845,72000},&target,&point)==0,
          "unrelated room92 floor outside the measured corridor cannot start exit guidance");
}

int main(void)
{
    test_enabled_different_terminal_approach();
    test_no_route_for_same_floor_or_ineligible_lift();
    test_shaft_wait_if_platform_elsewhere();
    test_aboard_wait_and_destination_exit();
    test_landing_hold_requires_same_lift_height_and_floor_contact();
    test_side_contact_not_aboard_and_far_target_rejected();
    test_aboard_platform_survives_room_change();
    test_final_shaft_step_off_keeps_landing_hold();
    test_only_surveyed_derelict_shafts_cross_rooms();
    test_fall_shaft_identity_and_exit();
    printf("Lift-route checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
