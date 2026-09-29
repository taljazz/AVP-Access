#ifndef ACC_JUMP_ASSIST_H
#define ACC_JUMP_ASSIST_H

#include <stdint.h>

enum ACC_JUMP_ASSIST_ACTION {
    ACC_JUMP_ASSIST_IDLE = 0,
    ACC_JUMP_ASSIST_NOT_ELIGIBLE,
    ACC_JUMP_ASSIST_ALIGN_YAW,
    ACC_JUMP_ASSIST_RUN_FORWARD,
    ACC_JUMP_ASSIST_JUMP_ONCE,
    ACC_JUMP_ASSIST_WAIT_TAKEOFF,
    ACC_JUMP_ASSIST_CONTINUE_FLIGHT,
    ACC_JUMP_ASSIST_DONE,
    ACC_JUMP_ASSIST_RECOVER,
    ACC_JUMP_ASSIST_CANCELLED
};

enum ACC_JUMP_ASSIST_PHASE {
    ACC_JUMP_ASSIST_PHASE_IDLE = 0,
    ACC_JUMP_ASSIST_PHASE_ALIGN,
    ACC_JUMP_ASSIST_PHASE_RUNUP,
    ACC_JUMP_ASSIST_PHASE_JUMP_PENDING,
    ACC_JUMP_ASSIST_PHASE_FLIGHT
};

typedef struct ACC_JUMP_ASSIST_INPUT {
    const char *level_name;
    int is_predator;
    int room_index;
    int gate_unlocked;
    int grounded;
    int foot_x, foot_y, foot_z;
    int yaw;                      /* Engine turn units, 0..4095. */
    int velocity_x, velocity_z;  /* Measured world velocity in mm/s. */
    uint32_t now_ms;              /* Monotonic tick; unsigned wrap is supported. */
    int explicit_start;           /* Edge-triggered user request; never inferred. */
    int cancel;                   /* Manual cancel, focus loss, or menu transition. */
} ACC_JUMP_ASSIST_INPUT;

typedef struct ACC_JUMP_ASSIST_STATE {
    int phase;
    int jump_number;              /* 1 or 2 while active. */
    int jump_issued;
    uint32_t phase_started_ms;
    uint32_t last_update_ms;
    uint32_t last_progress_ms;
    uint32_t jump_issued_ms;
    uint32_t transient_contact_started_ms;
    int best_launch_distance_mm;
    int transient_contact_active;
} ACC_JUMP_ASSIST_STATE;

typedef struct ACC_JUMP_ASSIST_OUTPUT {
    int action;
    int phase;
    int jump_number;
    int desired_yaw;
    int launch_x, launch_y, launch_z;
    int landing_x, landing_y, landing_z;
    int launch_distance_mm;       /* Positive before takeoff, negative after. */
    int projected_speed_mm_per_s;
    int predicted_crossing_ms;
    int failure_reason;           /* 0 when not recovering/not eligible. */
} ACC_JUMP_ASSIST_OUTPUT;

enum ACC_JUMP_ASSIST_FAILURE {
    ACC_JUMP_ASSIST_FAILURE_NONE = 0,
    ACC_JUMP_ASSIST_FAILURE_SCOPE,
    ACC_JUMP_ASSIST_FAILURE_LEFT_STAGING_AREA,
    ACC_JUMP_ASSIST_FAILURE_STALE_SAMPLE,
    ACC_JUMP_ASSIST_FAILURE_MISSED_LAUNCH,
    ACC_JUMP_ASSIST_FAILURE_BAD_RUNUP,
    ACC_JUMP_ASSIST_FAILURE_TIMEOUT,
    ACC_JUMP_ASSIST_FAILURE_BAD_LANDING
};

void AccJumpAssist_Reset(ACC_JUMP_ASSIST_STATE *state);
/* Pure advisory state machine. Starting requires an explicit request in one
 * surveyed staging zone. It returns a one-shot jump intent but never sends
 * input or changes position/velocity. Room 94 is required to start/run up;
 * during flight, the exact scoped jump corridor allows a room transition. */
int AccJumpAssist_Update(const ACC_JUMP_ASSIST_INPUT *input,
                         ACC_JUMP_ASSIST_STATE *state,
                         ACC_JUMP_ASSIST_OUTPUT *output);

#endif
