#include "acc_jump_assist.h"

#include <stdio.h>
#include <string.h>

static int Checks, Failures;
#define CHECK(expr) do { ++Checks; if (!(expr)) { ++Failures; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); } } while(0)

static ACC_JUMP_ASSIST_INPUT Input(void)
{
    ACC_JUMP_ASSIST_INPUT in;
    memset(&in, 0, sizeof(in));
    in.level_name = "fall"; in.is_predator = 1; in.room_index = 94;
    in.gate_unlocked = 1; in.grounded = 1; in.foot_x = 15750;
    in.foot_y = 4741; in.foot_z = 0;
    return in;
}

static int BeginFirst(ACC_JUMP_ASSIST_INPUT *in, ACC_JUMP_ASSIST_STATE *state,
                     ACC_JUMP_ASSIST_OUTPUT *out)
{
    AccJumpAssist_Reset(state);
    *in = Input(); in->explicit_start = 1;
    if (!AccJumpAssist_Update(in, state, out) || out->action != ACC_JUMP_ASSIST_ALIGN_YAW) return 0;
    in->explicit_start = 0; in->now_ms += 33; in->yaw = out->desired_yaw;
    return AccJumpAssist_Update(in, state, out) && out->action == ACC_JUMP_ASSIST_RUN_FORWARD;
}

static void FirstJump(ACC_JUMP_ASSIST_INPUT *in, ACC_JUMP_ASSIST_STATE *state,
                      ACC_JUMP_ASSIST_OUTPUT *out)
{
    double ux = 6631.0 / 6875.0, uz = -1814.0 / 6875.0;
    in->foot_x = (int)(17680 - ux * 1200.0);
    in->foot_y = 4741;
    in->foot_z = (int)(-460 - uz * 1200.0);
    in->velocity_x = 15450; in->velocity_z = -4225;
    in->now_ms += 33;
    CHECK(AccJumpAssist_Update(in, state, out) && out->action == ACC_JUMP_ASSIST_RUN_FORWARD);
    in->foot_x = (int)(17680 - ux * 672.0);
    in->foot_z = (int)(-460 - uz * 672.0);
    in->now_ms += 33;
    CHECK(AccJumpAssist_Update(in, state, out) && out->action == ACC_JUMP_ASSIST_JUMP_ONCE);
    CHECK(out->jump_number == 1 && state->jump_issued == 1);
}

static void TestManualOptInAndScope(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(!AccJumpAssist_Update(&in, &state, &out) && state.phase == ACC_JUMP_ASSIST_PHASE_IDLE);
    in.explicit_start = 1; in.is_predator = 0;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_NOT_ELIGIBLE);
    CHECK(state.phase == ACC_JUMP_ASSIST_PHASE_IDLE);
    in = Input(); in.explicit_start = 1; in.room_index = 93;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.failure_reason == ACC_JUMP_ASSIST_FAILURE_SCOPE);
    in = Input(); in.explicit_start = 1; in.velocity_z = 501;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.failure_reason == ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA);
    in = Input(); in.explicit_start = 1; in.foot_x += 1000;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_NOT_ELIGIBLE);
    in = Input(); in.explicit_start = 1;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_ALIGN_YAW);
    CHECK(state.phase == ACC_JUMP_ASSIST_PHASE_ALIGN && out.jump_number == 1);
}

static void TestFirstFlightOneShotAndLanding(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(BeginFirst(&in, &state, &out));
    FirstJump(&in, &state, &out);
    in.now_ms += 33; in.grounded = 1;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_WAIT_TAKEOFF);
    CHECK(out.action != ACC_JUMP_ASSIST_JUMP_ONCE);
    in.now_ms += 68;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);

    CHECK(BeginFirst(&in, &state, &out));
    FirstJump(&in, &state, &out);
    in.grounded = 0; in.foot_x = 18000; in.foot_y = 4000; in.foot_z = -550;
    in.velocity_x = 1900; in.velocity_z = -523; in.now_ms += 33;
    in.room_index = 107; /* The observed jump can cross the source module boundary. */
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.foot_x = 18480; in.foot_y = 3521; in.foot_z = -680;
    in.velocity_x = 900; in.velocity_z = -200; in.now_ms += 200;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.foot_x = 24311; in.foot_y = 4739; in.foot_z = -2274;
    in.grounded = 1; in.velocity_x = 0; in.velocity_z = 0; in.now_ms += 500;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_DONE);
    CHECK(out.jump_number == 1 && state.phase == ACC_JUMP_ASSIST_PHASE_IDLE);
}

static void TestFirstJumpTransientRoofContact(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(BeginFirst(&in, &state, &out));
    FirstJump(&in, &state, &out);
    in.grounded = 0; in.foot_x = 17774; in.foot_y = 4468; in.foot_z = -531;
    in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);

    in.grounded = 1; in.foot_x = 18729; in.foot_y = 3651; in.foot_z = -801;
    in.velocity_x = 1929; in.velocity_z = -528; in.now_ms += 500;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    CHECK(state.transient_contact_active && state.transient_contact_started_ms == in.now_ms);

    in.grounded = 0; in.foot_x = 20380; in.foot_y = 3754; in.foot_z = -1091;
    in.velocity_x = 12348; in.velocity_z = -3377; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    CHECK(!state.transient_contact_active);

    in.grounded = 1; in.foot_x = 18729; in.foot_y = 3651; in.foot_z = -801;
    in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.now_ms += 201;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
}

static void TestTransientContactDoesNotHideOtherGrounding(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(BeginFirst(&in, &state, &out));
    FirstJump(&in, &state, &out);
    in.grounded = 0; in.foot_x = 17774; in.foot_y = 4468; in.foot_z = -531; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.grounded = 1; in.foot_x = 18729; in.foot_y = 4740; in.foot_z = -801; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);

    in = Input(); in.foot_x = 40416; in.foot_y = 4738; in.foot_z = -5049;
    AccJumpAssist_Reset(&state); in.explicit_start = 1;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_ALIGN_YAW);
    in.explicit_start = 0; in.now_ms += 33; in.yaw = out.desired_yaw;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RUN_FORWARD);
    in.foot_x = 40382; in.foot_y = 4741; in.foot_z = -3128;
    in.velocity_x = -270; in.velocity_z = 15998; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_JUMP_ONCE);
    in.grounded = 0; in.foot_x = 40200; in.foot_y = 3200; in.foot_z = 3500; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.grounded = 1; in.foot_x = 40200; in.foot_y = 3651; in.foot_z = 3500; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
}

static void TestSecondJumpAndUnsignedTimerWrap(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    in.foot_x = 40416; in.foot_y = 4738; in.foot_z = -5049;
    in.now_ms = 0xfffffff0u; in.explicit_start = 1;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_ALIGN_YAW && out.jump_number == 2);
    in.explicit_start = 0; in.now_ms += 33u; in.yaw = out.desired_yaw;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RUN_FORWARD);
    in.foot_x = 40382; in.foot_y = 4741; in.foot_z = -3128;
    in.velocity_x = -270; in.velocity_z = 15998; in.now_ms += 33u;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_JUMP_ONCE);
    CHECK(out.jump_number == 2 && out.projected_speed_mm_per_s >= 14000);
    in.grounded = 0; in.foot_x = 40200; in.foot_y = 3200; in.foot_z = 3500;
    in.now_ms += 33u;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.grounded = 1; in.foot_x = 40183; in.foot_y = 4737; in.foot_z = 8665;
    in.now_ms += 600u;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_DONE && out.jump_number == 2);
}

static void TestStaleSampleMissedLaunchAndCancel(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(BeginFirst(&in, &state, &out));
    in.foot_x = 17439; in.foot_y = 4741; in.foot_z = -394;
    in.velocity_x = 15450; in.velocity_z = -4225; in.now_ms += 101;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_STALE_SAMPLE);

    CHECK(BeginFirst(&in, &state, &out));
    in.foot_x = 17720; in.foot_y = 4741; in.foot_z = -470;
    in.velocity_x = 15450; in.velocity_z = -4225; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);

    CHECK(BeginFirst(&in, &state, &out));
    in.cancel = 1; in.is_predator = 0;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CANCELLED);
    CHECK(state.phase == ACC_JUMP_ASSIST_PHASE_IDLE);
    in.cancel = 0;
    CHECK(!AccJumpAssist_Update(&in, &state, &out));
}

static void TestExactLaunchSpeedFloorAndStartAlignment(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    CHECK(BeginFirst(&in, &state, &out));
    in.foot_x = 17680; in.foot_y = 4741; in.foot_z = -460;
    in.velocity_x = 15450; in.velocity_z = -4225; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_JUMP_ONCE &&
          out.launch_distance_mm == 0);

    CHECK(BeginFirst(&in, &state, &out));
    in.foot_x = 17680; in.foot_y = 4741; in.foot_z = -460;
    in.velocity_x = 7700; in.velocity_z = -2100; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);

    in = Input(); in.explicit_start = 1; in.foot_x = 15618; in.foot_z = -482;
    AccJumpAssist_Reset(&state);
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_NOT_ELIGIBLE);

    /* This point is inside the broad staging radius but leaves only ~1.35 m
       before the launch line; live trace showed it crossed before 11 m/s. */
    in = Input(); in.explicit_start = 1; in.foot_x = 16334; in.foot_y = 4739; in.foot_z = -266;
    AccJumpAssist_Reset(&state);
    CHECK(AccJumpAssist_Update(&in, &state, &out) &&
          out.action == ACC_JUMP_ASSIST_NOT_ELIGIBLE &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA &&
          state.phase == ACC_JUMP_ASSIST_PHASE_IDLE);

    CHECK(BeginFirst(&in, &state, &out));
    in.foot_y = 4200; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_BAD_RUNUP);
}

static void TestFirstJumpPredictiveCueAcrossThirtyHzFrameOffsets(void)
{
    int phase;
    for (phase = 1; phase <= 33; ++phase) {
        ACC_JUMP_ASSIST_INPUT in = Input();
        ACC_JUMP_ASSIST_STATE state = {0};
        ACC_JUMP_ASSIST_OUTPUT out;
        const double ux = 6631.0 / 6875.0, uz = -1814.0 / 6875.0;
        unsigned int elapsed = 0;
        unsigned int delta = (unsigned int)phase;
        int step, issued = 0;
        CHECK(BeginFirst(&in, &state, &out));
        in.velocity_x = 15450; in.velocity_z = -4225;
        for (step = 0; step < 8 && !issued; ++step) {
            elapsed += delta;
            in.now_ms += delta;
            in.foot_x = (int)(15750 + ux * elapsed * 16.0 + 0.5);
            in.foot_z = (int)(uz * elapsed * 16.0 + (uz < 0 ? -0.5 : 0.5));
            CHECK(AccJumpAssist_Update(&in, &state, &out));
            if (out.action == ACC_JUMP_ASSIST_JUMP_ONCE) issued = 1;
            else CHECK(out.action == ACC_JUMP_ASSIST_RUN_FORWARD);
            delta = (step & 1) ? 33U : 34U;
        }
        CHECK(issued && out.jump_number == 1 && out.predicted_crossing_ms <= 50);
    }
}

static void TestWrongLandingAndRunupTimeout(void)
{
    ACC_JUMP_ASSIST_INPUT in = Input();
    ACC_JUMP_ASSIST_STATE state = {0};
    ACC_JUMP_ASSIST_OUTPUT out;
    unsigned int tick;
    CHECK(BeginFirst(&in, &state, &out));
    FirstJump(&in, &state, &out);
    in.grounded = 0; in.foot_x = 18000; in.foot_y = 4000; in.foot_z = -550; in.now_ms += 33;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT);
    in.grounded = 1; in.foot_x = 26000; in.foot_y = 4740; in.foot_z = 0; in.now_ms += 200;
    CHECK(AccJumpAssist_Update(&in, &state, &out) && out.action == ACC_JUMP_ASSIST_RECOVER &&
          out.failure_reason == ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);

    CHECK(BeginFirst(&in, &state, &out));
    for (tick = 0; tick < 19; ++tick) {
        in.now_ms += 100;
        CHECK(AccJumpAssist_Update(&in, &state, &out));
    }
    CHECK(out.action == ACC_JUMP_ASSIST_RECOVER && out.failure_reason == ACC_JUMP_ASSIST_FAILURE_TIMEOUT);
}

int main(void)
{
    TestManualOptInAndScope();
    TestFirstFlightOneShotAndLanding();
    TestFirstJumpTransientRoofContact();
    TestTransientContactDoesNotHideOtherGrounding();
    TestSecondJumpAndUnsignedTimerWrap();
    TestStaleSampleMissedLaunchAndCancel();
    TestExactLaunchSpeedFloorAndStartAlignment();
    TestFirstJumpPredictiveCueAcrossThirtyHzFrameOffsets();
    TestWrongLandingAndRunupTimeout();
    printf("Jump assist checks: %d, failures: %d\n", Checks, Failures);
    return Failures ? 1 : 0;
}
