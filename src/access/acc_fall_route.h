#ifndef ACC_FALL_ROUTE_H
#define ACC_FALL_ROUTE_H

/* A narrow, surveyed Predator route through the opening of Waterfall. This
   helper computes advice only; it never moves the player or starts a jump. */
enum ACC_FALL_ROUTE_PHASE {
    ACC_FALL_PHASE_NONE = 0,
    ACC_FALL_PHASE_FIRST_APPROACH,
    ACC_FALL_PHASE_FIRST_RUNUP,
    ACC_FALL_PHASE_FIRST_JUMP,
    ACC_FALL_PHASE_NORTH_DECK,
    ACC_FALL_PHASE_SECOND_RUNUP,
    ACC_FALL_PHASE_SECOND_JUMP,
    ACC_FALL_PHASE_GATE_WALK,
    ACC_FALL_PHASE_RECOVERY
};

enum ACC_FALL_ROUTE_ACTION {
    ACC_FALL_ACTION_NONE = 0,
    ACC_FALL_ACTION_WALK,
    ACC_FALL_ACTION_RUN_UP,
    ACC_FALL_ACTION_JUMP_NOW,
    ACC_FALL_ACTION_LAND,
    ACC_FALL_ACTION_RECOVER,
    ACC_FALL_ACTION_FALLING
};

typedef struct ACC_FALL_ROUTE_INPUT {
    const char *level_name;
    int is_predator;
    int room_index;
    int opening_gate_unlocked;
    int grounded;
    int x, y, z;                 /* Player foot position in world millimetres. */
    int yaw;                      /* Engine 0..4095 turn units. */
    int velocity_x, velocity_z;  /* Measured world velocity in mm/s. */
} ACC_FALL_ROUTE_INPUT;

typedef struct ACC_FALL_ROUTE_STATE {
    int phase; /* Caller-owned across updates; zero requests position reconciliation. */
    int waypoint; /* Persisted north-deck leg: 0=intermediate, 1=northeast staging. */
} ACC_FALL_ROUTE_STATE;

typedef struct ACC_FALL_ROUTE_OUTPUT {
    int phase;
    int action;
    int has_target;
    int target_x, target_y, target_z;
    int landing_x, landing_y, landing_z;
    int desired_yaw;
    int horizontal_speed;
    int projected_run_speed_mm_per_s;
    int lateral_speed_mm_per_s;
    int launch_distance_mm;       /* Positive is before takeoff; negative is past it. */
    int cross_track_mm;
    int min_run_speed_mm_per_s;  /* Minimum positive projected speed along jump. */
    int jump_window_start_mm;     /* Cue may fire this far before launch. */
    int jump_window_end_mm;       /* Small grounded tolerance past launch. */
    int jump_intent;              /* Grounded, aligned, fast, and inside the window. */
} ACC_FALL_ROUTE_OUTPUT;

void AccFallRoute_Reset(ACC_FALL_ROUTE_STATE *state);
/* Returns 1 only for the exact scoped route. Wrong scope and lower-floor states
   clear persisted phase and return 0. */
int AccFallRoute_Update(const ACC_FALL_ROUTE_INPUT *input,
    ACC_FALL_ROUTE_STATE *state, ACC_FALL_ROUTE_OUTPUT *output);

#endif
