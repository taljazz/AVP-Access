#include "acc_fall_route.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int Checks, Failures;
#define CHECK(expr) do { ++Checks; if(!(expr)) { ++Failures; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); } } while(0)

static ACC_FALL_ROUTE_INPUT Input(void)
{
    ACC_FALL_ROUTE_INPUT in;
    memset(&in,0,sizeof(in)); in.level_name="fall"; in.is_predator=1;
    in.room_index=94; in.opening_gate_unlocked=1; in.grounded=1;
    in.x=17955; in.y=4452; in.z=322; in.yaw=0;
    return in;
}

static void TestScopeFailsClosed(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_SECOND_JUMP,0};
    ACC_FALL_ROUTE_OUTPUT out;
    in.level_name="marine";
    CHECK(!AccFallRoute_Update(&in,&state,&out)); CHECK(state.phase==ACC_FALL_PHASE_NONE);
    in=Input(); in.is_predator=0; state.phase=ACC_FALL_PHASE_FIRST_RUNUP;
    CHECK(!AccFallRoute_Update(&in,&state,&out)); CHECK(state.phase==ACC_FALL_PHASE_NONE);
    in=Input(); in.room_index=93; state.phase=ACC_FALL_PHASE_FIRST_RUNUP;
    CHECK(!AccFallRoute_Update(&in,&state,&out)); CHECK(state.phase==ACC_FALL_PHASE_NONE);
    in=Input(); in.opening_gate_unlocked=0; state.phase=ACC_FALL_PHASE_FIRST_RUNUP;
    CHECK(!AccFallRoute_Update(&in,&state,&out)); CHECK(state.phase==ACC_FALL_PHASE_NONE);
    in=Input(); in.y=5700;
    CHECK(!AccFallRoute_Update(&in,&state,&out)); CHECK(state.phase==ACC_FALL_PHASE_NONE);
}

static void TestFirstRunupAndTakeoffWindow(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={0};
    ACC_FALL_ROUTE_OUTPUT out;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.phase==ACC_FALL_PHASE_FIRST_APPROACH && out.action==ACC_FALL_ACTION_WALK);
    CHECK(out.has_target && out.target_x==15750 && out.target_z==0);
    in.x=15750; in.y=4741; in.z=0;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_FIRST_RUNUP && out.action==ACC_FALL_ACTION_RUN_UP);
    CHECK(out.target_x==17680 && out.target_z==-460 && out.min_run_speed_mm_per_s==11000);
    /* Approx. 250 mm before the measured takeoff point, aligned at the observed
       12.8 m/s. The helper cues only with grounded position and measured speed. */
    in.x=17439; in.y=4741; in.z=-394; in.yaw=1196;
    in.velocity_x=12340; in.velocity_z=-3379;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_JUMP_NOW && out.jump_intent);
    CHECK(out.jump_window_start_mm==350 && out.jump_window_end_mm==150);
    CHECK(out.launch_distance_mm>0 && out.launch_distance_mm<=350);
    in.velocity_x=-12340; in.velocity_z=3379;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RUN_UP && !out.jump_intent);
    in.velocity_x=0; in.velocity_z=16000;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RECOVER && !out.jump_intent);
    in.velocity_x=12340; in.velocity_z=-3379;
    in.grounded=0; in.y=3300; in.x=18480; in.z=-680;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_FIRST_JUMP && out.action==ACC_FALL_ACTION_LAND);
    CHECK(out.landing_x==24311 && out.landing_z==-2274);
    CHECK(out.action==ACC_FALL_ACTION_LAND && !out.jump_intent);
}

static void TestFirstCueCannotSkipA30HzFrameWindow(void)
{
    const double ux=6631.0/6875.0, uz=-1814.0/6875.0;
    const int stepMm=544; /* 34 ms at the verified 16 m/s running speed. */
    int phase;
    for(phase=0;phase<stepMm;++phase) {
        ACC_FALL_ROUTE_INPUT in=Input();
        ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_FIRST_RUNUP,0};
        ACC_FALL_ROUTE_OUTPUT out;
        int distance=351+phase;
        in.y=4741; in.yaw=1196; in.velocity_x=15432; in.velocity_z=-4220;
        in.x=(int)lround(17680-ux*distance);
        in.z=(int)lround(-460-uz*distance);
        CHECK(AccFallRoute_Update(&in,&state,&out));
        if(phase==0)
            CHECK(out.jump_window_start_mm>350 && out.action==ACC_FALL_ACTION_JUMP_NOW);
        if(out.action==ACC_FALL_ACTION_RUN_UP) {
            distance-=stepMm;
            in.x=(int)lround(17680-ux*distance);
            in.z=(int)lround(-460-uz*distance);
            CHECK(AccFallRoute_Update(&in,&state,&out));
        }
        CHECK(out.action==ACC_FALL_ACTION_JUMP_NOW && out.jump_intent);
    }
}

static void TestMissedFirstTakeoffAndLandingMustNotAdvance(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_FIRST_RUNUP,0};
    ACC_FALL_ROUTE_OUTPUT out;
    in.x=18000; in.y=4741; in.z=-550; in.yaw=1196; in.velocity_x=11000; in.velocity_z=-3000;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RECOVER && out.target_x==15750 && out.target_z==0);
    state.phase=ACC_FALL_PHASE_FIRST_JUMP; in.grounded=1; in.x=24311; in.y=6000; in.z=-2274;
    CHECK(!AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NONE);
    CHECK(out.phase!=ACC_FALL_PHASE_NORTH_DECK);
    in.x=24311; in.y=4739; in.z=-2274; state.phase=ACC_FALL_PHASE_FIRST_JUMP;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NORTH_DECK && out.action==ACC_FALL_ACTION_WALK);
    CHECK(out.target_x==33242 && out.target_z==-4654);
}

static void TestNorthDeckAndSecondRunup(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_NORTH_DECK,0};
    ACC_FALL_ROUTE_OUTPUT out;
    in.x=33242; in.y=4739; in.z=-4654;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_WALK && out.target_x==40416 && out.target_z==-5049);
    in.x=34060; in.y=4739; in.z=-4700;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_WALK && out.target_x==40416 && out.target_z==-5049);
    in.x=35000; in.y=4739; in.z=-4800;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.target_x==40416 && out.target_z==-5049);
    in.x=40416; in.y=4738; in.z=-5049;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_SECOND_RUNUP && out.action==ACC_FALL_ACTION_RUN_UP);
    CHECK(out.target_x==40372 && out.target_z==-2528);
    /* Early but safe cue window for the measured 16 m/s long run. */
    in.x=40390; in.y=4741; in.z=-3378; in.yaw=4095;
    in.velocity_x=-220; in.velocity_z=13000;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RUN_UP && !out.jump_intent);
    in.velocity_x=-270; in.velocity_z=15998;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_JUMP_NOW && out.jump_intent);
    CHECK(out.jump_window_start_mm==900 && out.min_run_speed_mm_per_s==14000);
    in.grounded=0; in.x=40200; in.y=3200; in.z=3500;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_SECOND_JUMP && out.action==ACC_FALL_ACTION_LAND);
    CHECK(out.landing_x==40183 && out.landing_z==8665);
}

static void TestContinuousGroundRouteAndMeasuredRunup(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_FIRST_JUMP,0};
    ACC_FALL_ROUTE_OUTPUT out;
    in.x=24311; in.y=4739; in.z=-2274;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NORTH_DECK && out.target_x==33242);
    in.x=27000; in.y=4739; in.z=-2988;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NORTH_DECK && out.action==ACC_FALL_ACTION_WALK);
    CHECK(out.target_x==33242);
    in.x=33242; in.y=4739; in.z=-4654;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.target_x==40416 && out.target_z==-5049);
    in.x=36000; in.y=4739; in.z=-4800;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NORTH_DECK && out.target_x==40416);
    in.x=40416; in.y=4738; in.z=-5049;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_SECOND_RUNUP && out.action==ACC_FALL_ACTION_RUN_UP);
    /* Live run-up trace: after W250, grounded at launch with 16m/s forward. */
    in.x=40372; in.y=4741; in.z=-2528; in.yaw=4095;
    in.velocity_x=-270; in.velocity_z=15998;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_JUMP_NOW && out.jump_intent);
    /* Slow at the takeoff line is a recovery, never a late jump cue. */
    in.velocity_x=0; in.velocity_z=8000;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RECOVER && !out.jump_intent);
}

static void TestSecondMissedJumpAndGateWalk(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_SECOND_RUNUP,1};
    ACC_FALL_ROUTE_OUTPUT out;
    in.x=40370; in.y=4741; in.z=-2050; in.yaw=4095; in.velocity_z=16000;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_RECOVER && out.target_x==40416 && out.target_z==-5049);
    state.phase=ACC_FALL_PHASE_SECOND_JUMP; in.x=40183; in.y=6000; in.z=8665;
    CHECK(!AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NONE);
    in.x=40183; in.y=4737; in.z=8665; state.phase=ACC_FALL_PHASE_SECOND_JUMP;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_GATE_WALK && out.action==ACC_FALL_ACTION_WALK);
    CHECK(out.target_x==41427 && out.target_y==4740 && out.target_z==21739);
    in.x=40915; in.y=4740; in.z=21312;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_WALK && out.target_x==41427);
    in.x=41427; in.y=4740; in.z=21739;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(out.action==ACC_FALL_ACTION_WALK && out.has_target);
    in.room_index=9;
    CHECK(!AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NONE);
}

static void TestReconcileAndOffCorridorAirborne(void)
{
    ACC_FALL_ROUTE_INPUT in=Input(); ACC_FALL_ROUTE_STATE state={ACC_FALL_PHASE_GATE_WALK,1};
    ACC_FALL_ROUTE_OUTPUT out;
    /* A later persisted phase cannot guide across an earlier loaded save. */
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_FIRST_APPROACH && out.target_x==15750);
    state.phase=ACC_FALL_PHASE_NONE;
    in.x=24311; in.y=4739; in.z=-2274;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_NORTH_DECK);
    state.phase=ACC_FALL_PHASE_SECOND_RUNUP; in.grounded=0; in.x=45000; in.y=3300; in.z=3500;
    CHECK(AccFallRoute_Update(&in,&state,&out));
    CHECK(state.phase==ACC_FALL_PHASE_RECOVERY && out.action==ACC_FALL_ACTION_FALLING);
    CHECK(!out.has_target && out.action==ACC_FALL_ACTION_FALLING);
}

int main(void)
{
    TestScopeFailsClosed(); TestFirstRunupAndTakeoffWindow();
    TestFirstCueCannotSkipA30HzFrameWindow();
    TestMissedFirstTakeoffAndLandingMustNotAdvance(); TestNorthDeckAndSecondRunup();
    TestSecondMissedJumpAndGateWalk(); TestContinuousGroundRouteAndMeasuredRunup();
    TestReconcileAndOffCorridorAirborne();
    printf("Fall route checks: %d, failures: %d\n",Checks,Failures);
    return Failures?1:0;
}
