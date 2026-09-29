#ifndef ACC_LIFT_ROUTE_H
#define ACC_LIFT_ROUTE_H
struct aimodule;
struct vectorch;
struct strategyblock;
/* 0 none; 1 approach boarding point; 2 aboard/wait; 3 exit at destination.
   Reads live same-room lifts, occupied platforms, and surveyed Derelict shafts.
   Never activates one; the selected landing may be held while stepping off. */
int AccLiftRoute_Find(struct aimodule *room,const struct vectorch *feet,
    const struct vectorch *target,struct vectorch *point);
/* Exact final Fall/Predator shaft identity is valid across the five surveyed
   shaft rooms, including when the one-use platform has disabled itself. */
int AccLiftRoute_IsFallShaftRoom(struct aimodule *room);
struct strategyblock *AccLiftRoute_FindFallPlatform(struct aimodule *room);
int AccLiftRoute_IsFallPlatformContact(struct aimodule *room,
    const struct vectorch *feet);
/* Returns staged north-then-west exit points after reaching the upper
   platform. 0 means this is not an active final-shaft exit. */
int AccLiftRoute_GetFallExitPoint(struct aimodule *room,
    const struct vectorch *feet,const struct vectorch *target,
    struct vectorch *point);
/* Releases the platform hold only at the measured, grounded upper-floor
   handoff. */
int AccLiftRoute_CompleteFallExit(struct aimodule *room,
    const struct vectorch *feet);
void AccLiftRoute_Reset(void);
int AccLiftRoute_HoldAtLanding(struct strategyblock *lift);
#endif
