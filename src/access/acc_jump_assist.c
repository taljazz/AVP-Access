#include "acc_jump_assist.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLOOR_Y_MIN 4300
#define FLOOR_Y_MAX 5200
#define START_SPEED_MAX 500
#define FIRST_MIN_START_RUNUP_MM 1800
#define YAW_TOLERANCE 45
#define MAX_SAMPLE_GAP_MS 100U
#define PREDICT_AHEAD_MS 50
#define MAX_ALIGN_MS 8000U
#define MAX_RUNUP_MS 8000U
#define MAX_NO_PROGRESS_MS 1800U
#define MAX_TAKEOFF_WAIT_MS 100U
#define MAX_FLIGHT_MS 2500U
#define MAX_TRANSIENT_GROUND_MS 200U
#define FIRST_ROOF_CONTACT_X_MIN 18000
#define FIRST_ROOF_CONTACT_X_MAX 21000
#define FIRST_ROOF_CONTACT_Y_MIN 3000
#define FIRST_ROOF_CONTACT_Y_MAX 4300
#define FIRST_ROOF_CONTACT_Z_MIN -1800
#define FIRST_ROOF_CONTACT_Z_MAX 0

typedef struct JUMP_POINT { int x, y, z; } JUMP_POINT;
typedef struct JUMP_PLAN {
    JUMP_POINT staging, launch, landing;
    int min_speed;
    int landing_x_min, landing_x_max, landing_z_min, landing_z_max;
    int flight_x_min, flight_x_max, flight_z_min, flight_z_max;
} JUMP_PLAN;

static const JUMP_PLAN Plans[2] = {
    {{15750,4741,0}, {17680,4741,-460}, {24311,4739,-2274}, 11000,
     23500,25500,-3800,-1500, 15500,27000,-5200,3200},
    {{40416,4738,-5049}, {40372,4741,-2528}, {40183,4737,8665}, 14000,
     39200,41000,7600,10100, 39000,41800,-3500,10500}
};

static uint32_t Elapsed(uint32_t now, uint32_t then)
{ return now - then; }

static int WrappedYaw(int yaw)
{ yaw %= 4096; if (yaw < 0) yaw += 4096; return yaw; }

static int YawTo(int from_x, int from_z, int to_x, int to_z)
{
    double angle = atan2((double)to_x - from_x, (double)to_z - from_z);
    return WrappedYaw((int)lround(angle * (4096.0 / (2.0 * 3.14159265358979323846))));
}

static int YawError(int a, int b)
{
    int difference = WrappedYaw(a) - WrappedYaw(b);
    if (difference > 2048) difference -= 4096;
    if (difference < -2048) difference += 4096;
    return difference < 0 ? -difference : difference;
}

static double HorizontalDistance(double x1, double z1, double x2, double z2)
{ return hypot(x2 - x1, z2 - z1); }

static int InBox(int x, int z, int xmin, int xmax, int zmin, int zmax)
{ return x >= xmin && x <= xmax && z >= zmin && z <= zmax; }

static double Speed(const ACC_JUMP_ASSIST_INPUT *in)
{ return hypot((double)in->velocity_x, (double)in->velocity_z); }

static int InLanding(const ACC_JUMP_ASSIST_INPUT *in, const JUMP_PLAN *plan)
{
    return in->grounded && in->foot_y >= FLOOR_Y_MIN && in->foot_y <= FLOOR_Y_MAX &&
        InBox(in->foot_x, in->foot_z, plan->landing_x_min, plan->landing_x_max,
              plan->landing_z_min, plan->landing_z_max);
}

static int InFlight(const ACC_JUMP_ASSIST_INPUT *in, const JUMP_PLAN *plan)
{
    return in->foot_y >= 1800 && in->foot_y <= 7500 &&
        InBox(in->foot_x, in->foot_z, plan->flight_x_min, plan->flight_x_max,
              plan->flight_z_min, plan->flight_z_max);
}

static int InFirstRoofContact(const ACC_JUMP_ASSIST_INPUT *in, const JUMP_PLAN *plan)
{
    return plan == &Plans[0] && in->grounded &&
        in->foot_x >= FIRST_ROOF_CONTACT_X_MIN && in->foot_x <= FIRST_ROOF_CONTACT_X_MAX &&
        in->foot_y >= FIRST_ROOF_CONTACT_Y_MIN && in->foot_y <= FIRST_ROOF_CONTACT_Y_MAX &&
        in->foot_z >= FIRST_ROOF_CONTACT_Z_MIN && in->foot_z <= FIRST_ROOF_CONTACT_Z_MAX;
}

static double LaunchDistance(const ACC_JUMP_ASSIST_INPUT *in, const JUMP_PLAN *plan,
                             double *ux_out, double *uz_out, double *cross_out)
{
    double dx = (double)plan->landing.x - plan->launch.x;
    double dz = (double)plan->landing.z - plan->launch.z;
    double length = hypot(dx, dz);
    double ux = dx / length, uz = dz / length;
    double to_launch_x = (double)plan->launch.x - in->foot_x;
    double to_launch_z = (double)plan->launch.z - in->foot_z;
    if (ux_out) *ux_out = ux;
    if (uz_out) *uz_out = uz;
    if (cross_out) *cross_out = fabs(((double)in->foot_x - plan->launch.x) * uz -
                                      ((double)in->foot_z - plan->launch.z) * ux);
    return to_launch_x * ux + to_launch_z * uz;
}

static int ProjectedSpeed(const ACC_JUMP_ASSIST_INPUT *in, double ux, double uz)
{ return (int)lround((double)in->velocity_x * ux + (double)in->velocity_z * uz); }

static void FillPlan(const ACC_JUMP_ASSIST_INPUT *in, const JUMP_PLAN *plan,
                     ACC_JUMP_ASSIST_OUTPUT *out)
{
    double ux, uz;
    out->desired_yaw = YawTo(plan->launch.x, plan->launch.z,
                             plan->landing.x, plan->landing.z);
    out->launch_x = plan->launch.x; out->launch_y = plan->launch.y; out->launch_z = plan->launch.z;
    out->landing_x = plan->landing.x; out->landing_y = plan->landing.y; out->landing_z = plan->landing.z;
    out->launch_distance_mm = (int)lround(LaunchDistance(in, plan, &ux, &uz, NULL));
    out->projected_speed_mm_per_s = ProjectedSpeed(in, ux, uz);
    out->jump_number = plan == &Plans[0] ? 1 : 2;
}

static void SetRecovery(ACC_JUMP_ASSIST_STATE *state, ACC_JUMP_ASSIST_OUTPUT *out, int reason)
{
    int number = state->jump_number;
    AccJumpAssist_Reset(state);
    out->action = ACC_JUMP_ASSIST_RECOVER;
    out->phase = ACC_JUMP_ASSIST_PHASE_IDLE;
    out->jump_number = number;
    out->failure_reason = reason;
}

static int Scoped(const ACC_JUMP_ASSIST_INPUT *in)
{ return in && in->level_name && !strcmp(in->level_name, "fall") &&
         in->is_predator && in->gate_unlocked; }

void AccJumpAssist_Reset(ACC_JUMP_ASSIST_STATE *state)
{ if (state) memset(state, 0, sizeof(*state)); }

int AccJumpAssist_Update(const ACC_JUMP_ASSIST_INPUT *in,
                         ACC_JUMP_ASSIST_STATE *state,
                         ACC_JUMP_ASSIST_OUTPUT *out)
{
    const JUMP_PLAN *plan = NULL;
    double ux, uz, cross_track, launch_distance, projected_speed;
    uint32_t sample_gap, phase_elapsed;
    int yaw_error;

    if (!state || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->action = ACC_JUMP_ASSIST_IDLE;
    if (!in) return 0;

    if (in->cancel) {
        int was_active = state->phase != ACC_JUMP_ASSIST_PHASE_IDLE;
        int number = state->jump_number;
        AccJumpAssist_Reset(state);
        if (was_active) {
            out->action = ACC_JUMP_ASSIST_CANCELLED;
            out->jump_number = number;
            return 1;
        }
        return 0;
    }

    if (state->phase == ACC_JUMP_ASSIST_PHASE_IDLE) {
        int which = -1;
        if (!in->explicit_start) return 0;
        if (!Scoped(in) || in->room_index != 94) {
            out->action = ACC_JUMP_ASSIST_NOT_ELIGIBLE;
            out->failure_reason = ACC_JUMP_ASSIST_FAILURE_SCOPE;
            return 1;
        }
        if (!in->grounded || in->foot_y < FLOOR_Y_MIN || in->foot_y > FLOOR_Y_MAX ||
            Speed(in) > START_SPEED_MAX) {
            out->action = ACC_JUMP_ASSIST_NOT_ELIGIBLE;
            out->failure_reason = ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA;
            return 1;
        }
        if (HorizontalDistance(in->foot_x, in->foot_z, Plans[0].staging.x, Plans[0].staging.z) <= 700 &&
            abs(in->foot_y - Plans[0].staging.y) <= 250 &&
            LaunchDistance(in, &Plans[0], NULL, NULL, &cross_track) >= 0.0 && cross_track <= 500.0) which = 0;
        else if (HorizontalDistance(in->foot_x, in->foot_z, Plans[1].staging.x, Plans[1].staging.z) <= 700 &&
                 abs(in->foot_y - Plans[1].staging.y) <= 250 &&
                 LaunchDistance(in, &Plans[1], NULL, NULL, &cross_track) >= 0.0 && cross_track <= 500.0) which = 1;
        if (which < 0) {
            out->action = ACC_JUMP_ASSIST_NOT_ELIGIBLE;
            out->failure_reason = ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA;
            return 1;
        }
        plan = &Plans[which];
        state->phase = ACC_JUMP_ASSIST_PHASE_ALIGN;
        state->jump_number = which + 1;
        state->phase_started_ms = in->now_ms;
        state->last_update_ms = in->now_ms;
        state->last_progress_ms = in->now_ms;
        launch_distance = LaunchDistance(in, plan, NULL, NULL, NULL);
        /* A start near the launch-facing edge of the 700 mm staging radius
           leaves too little sampled runway to reach the measured 11 m/s
           minimum before the takeoff line. The center staging point provides
           about 1,982 mm; reject closer starts rather than guaranteeing a
           missed launch. */
        if (which == 0 && launch_distance < FIRST_MIN_START_RUNUP_MM) {
            AccJumpAssist_Reset(state);
            out->action = ACC_JUMP_ASSIST_NOT_ELIGIBLE;
            out->failure_reason = ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA;
            return 1;
        }
        state->best_launch_distance_mm = (int)lround(launch_distance);
        FillPlan(in, plan, out);
        out->phase = state->phase;
        out->action = ACC_JUMP_ASSIST_ALIGN_YAW;
        return 1;
    }

    if (!Scoped(in)) {
        int number = state->jump_number;
        AccJumpAssist_Reset(state);
        out->action = ACC_JUMP_ASSIST_CANCELLED;
        out->jump_number = number;
        out->failure_reason = ACC_JUMP_ASSIST_FAILURE_SCOPE;
        return 1;
    }
    if (state->jump_number < 1 || state->jump_number > 2) {
        AccJumpAssist_Reset(state);
        out->action = ACC_JUMP_ASSIST_CANCELLED;
        out->failure_reason = ACC_JUMP_ASSIST_FAILURE_SCOPE;
        return 1;
    }
    plan = &Plans[state->jump_number - 1];
    sample_gap = Elapsed(in->now_ms, state->last_update_ms);
    phase_elapsed = Elapsed(in->now_ms, state->phase_started_ms);
    FillPlan(in, plan, out);
    out->phase = state->phase;

    if ((state->phase == ACC_JUMP_ASSIST_PHASE_ALIGN ||
         state->phase == ACC_JUMP_ASSIST_PHASE_RUNUP) && in->room_index != 94) {
        SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_SCOPE);
        return 1;
    }

    if (state->phase == ACC_JUMP_ASSIST_PHASE_ALIGN) {
        if (!in->grounded || abs(in->foot_y - plan->staging.y) > 350 ||
            HorizontalDistance(in->foot_x, in->foot_z, plan->staging.x, plan->staging.z) > 1000) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA);
            return 1;
        }
        if (phase_elapsed > MAX_ALIGN_MS) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_TIMEOUT);
            return 1;
        }
        yaw_error = YawError(in->yaw, out->desired_yaw);
        if (yaw_error > YAW_TOLERANCE || Speed(in) > START_SPEED_MAX) {
            out->action = ACC_JUMP_ASSIST_ALIGN_YAW;
        } else {
            state->phase = ACC_JUMP_ASSIST_PHASE_RUNUP;
            state->phase_started_ms = in->now_ms;
            state->last_update_ms = in->now_ms;
            state->last_progress_ms = in->now_ms;
            state->best_launch_distance_mm = out->launch_distance_mm;
            out->phase = state->phase;
            out->action = ACC_JUMP_ASSIST_RUN_FORWARD;
        }
        state->last_update_ms = in->now_ms;
        return 1;
    }

    if (state->phase == ACC_JUMP_ASSIST_PHASE_RUNUP) {
        if (!in->grounded) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);
            return 1;
        }
        if (sample_gap > MAX_SAMPLE_GAP_MS) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_STALE_SAMPLE);
            return 1;
        }
        launch_distance = LaunchDistance(in, plan, &ux, &uz, &cross_track);
        projected_speed = (double)ProjectedSpeed(in, ux, uz);
        out->launch_distance_mm = (int)lround(launch_distance);
        out->projected_speed_mm_per_s = (int)lround(projected_speed);
        yaw_error = YawError(in->yaw, out->desired_yaw);
        if (launch_distance < 0.0) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);
            return 1;
        }
        if (launch_distance > 3500.0 || cross_track > 500.0 || yaw_error > 140 ||
            abs(in->foot_y - plan->launch.y) > 350) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_BAD_RUNUP);
            return 1;
        }
        if (launch_distance < state->best_launch_distance_mm - 25.0) {
            state->best_launch_distance_mm = (int)lround(launch_distance);
            state->last_progress_ms = in->now_ms;
        }
        if (phase_elapsed > MAX_RUNUP_MS ||
            Elapsed(in->now_ms, state->last_progress_ms) > MAX_NO_PROGRESS_MS) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_TIMEOUT);
            return 1;
        }
        if (launch_distance == 0.0 && projected_speed < plan->min_speed) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);
            return 1;
        }
        if (projected_speed < plan->min_speed) {
            out->action = ACC_JUMP_ASSIST_RUN_FORWARD;
            state->last_update_ms = in->now_ms;
            return 1;
        }
        out->predicted_crossing_ms = (int)lround(launch_distance * 1000.0 / projected_speed);
        if (out->predicted_crossing_ms <= PREDICT_AHEAD_MS && yaw_error <= YAW_TOLERANCE &&
            cross_track <= 500.0) {
            state->phase = ACC_JUMP_ASSIST_PHASE_JUMP_PENDING;
            state->jump_issued = 1;
            state->jump_issued_ms = in->now_ms;
            state->phase_started_ms = in->now_ms;
            out->phase = state->phase;
            out->action = ACC_JUMP_ASSIST_JUMP_ONCE;
        } else out->action = ACC_JUMP_ASSIST_RUN_FORWARD;
        state->last_update_ms = in->now_ms;
        return 1;
    }

    if (state->phase == ACC_JUMP_ASSIST_PHASE_JUMP_PENDING) {
        if (state->jump_issued != 1) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_SCOPE);
            return 1;
        }
        if (!in->grounded) {
            if (!InFlight(in, plan)) {
                SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
                return 1;
            }
            state->phase = ACC_JUMP_ASSIST_PHASE_FLIGHT;
            state->transient_contact_active = 0;
            out->phase = state->phase;
            out->action = ACC_JUMP_ASSIST_CONTINUE_FLIGHT;
        } else if (Elapsed(in->now_ms, state->jump_issued_ms) <= MAX_TAKEOFF_WAIT_MS) {
            out->action = ACC_JUMP_ASSIST_WAIT_TAKEOFF;
        } else {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH);
            return 1;
        }
        state->last_update_ms = in->now_ms;
        return 1;
    }

    if (state->phase == ACC_JUMP_ASSIST_PHASE_FLIGHT) {
        if (phase_elapsed > MAX_FLIGHT_MS) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_TIMEOUT);
            return 1;
        }
        if (in->grounded) {
            if (InLanding(in, plan)) {
                out->action = ACC_JUMP_ASSIST_DONE;
                state->phase = ACC_JUMP_ASSIST_PHASE_IDLE;
                state->jump_issued = 0;
                state->jump_number = 0;
                state->transient_contact_active = 0;
                out->phase = ACC_JUMP_ASSIST_PHASE_IDLE;
                state->last_update_ms = in->now_ms;
                return 1;
            }
            if (!InFirstRoofContact(in, plan) || !InFlight(in, plan)) {
                SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
                return 1;
            }
            /* The surveyed first jump briefly reports grounded under the rock
               roof at this specific position. Continue the normal forward run
               for a short, bounded grace; elsewhere contact fails closed. */
            if (!state->transient_contact_active) {
                state->transient_contact_active = 1;
                state->transient_contact_started_ms = in->now_ms;
            } else if (Elapsed(in->now_ms, state->transient_contact_started_ms) >
                       MAX_TRANSIENT_GROUND_MS) {
                SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
                return 1;
            }
            out->action = ACC_JUMP_ASSIST_CONTINUE_FLIGHT;
            state->last_update_ms = in->now_ms;
            return 1;
        }
        state->transient_contact_active = 0;
        if (!InFlight(in, plan)) {
            SetRecovery(state, out, ACC_JUMP_ASSIST_FAILURE_BAD_LANDING);
            return 1;
        }
        /* Do not reject brief collision-induced speed loss: only scoped position,
           continued airborne state, and the bounded flight clock govern. */
        out->action = ACC_JUMP_ASSIST_CONTINUE_FLIGHT;
        state->last_update_ms = in->now_ms;
        return 1;
    }

    AccJumpAssist_Reset(state);
    out->action = ACC_JUMP_ASSIST_CANCELLED;
    out->failure_reason = ACC_JUMP_ASSIST_FAILURE_SCOPE;
    return 1;
}
