#ifndef ACC_TRAVERSAL_H
#define ACC_TRAVERSAL_H
struct vectorch;
enum ACC_TRAVERSAL_HINT {
    ACC_TRAVERSAL_NONE, ACC_TRAVERSAL_LEFT, ACC_TRAVERSAL_RIGHT, ACC_TRAVERSAL_JUMP
};
/* Local, read-only Marine geometry probes. These are sampled clearances, not
   a collision simulation or a guarantee of a complete route. */
int AccTraversal_Probe(const struct vectorch *target);
const char *AccTraversal_Text(int hint);
enum ACC_STEER_RESULT { ACC_STEER_DIRECT, ACC_STEER_DETOUR, ACC_STEER_BLOCKED, ACC_STEER_UNAVAILABLE };
/* A detour is a locally checked intermediate waypoint. standOff keeps the
   player's body away from a wall-mounted control. Output is unchanged on failure. */
int AccTraversal_Steer(const struct vectorch *target, int standOff, struct vectorch *waypoint);
/* Check the departure beyond the platform, not just the first short step. */
int AccTraversal_ExitLift(const struct vectorch *target, struct vectorch *waypoint);
void AccTraversal_Reset(void);
/* Full length level-floor corridor check between authored volume centres. */
int AccTraversal_WaypointLink(const struct vectorch *a,const struct vectorch *b);
#endif
