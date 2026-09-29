#ifndef ACC_ROUTE_TARGETS_H
#define ACC_ROUTE_TARGETS_H

struct vectorch;
struct strategyblock;

/* Returns eligible, direct physical activators. position/isArea describe the
 * nearest one; count is the number of distinct eligible activators. */
int AccRoute_FindTarget(void *objective, const struct vectorch *player,
                        struct vectorch *position, int *isArea);

/* fall / Predator only: report the stable first research-facility control.
 * Returns 1 while it is usable and its linked gate is locked, 2 after that
 * gate has unlocked (position is the surveyed start-side gate approach), or
 * 0 when this exact switch/door relationship is not present. */
int AccRoute_FallPredatorOpening(const struct vectorch *player,
                                 struct vectorch *position,
                                 struct strategyblock **control);

/* fall / Predator / room 85 only: identify the linked final-shaft control.
 * Returns 1 with an eligible switch target while the matching lift is disabled;
 * returns 2 with the platform target after it is enabled; returns 0 when the
 * caller scope, signature, live link, or required switch eligibility fails.
 * On return 1, position/control identify the switch. On return 2, position is
 * the current platform position and control is NULL. Outputs are unchanged on
 * failure. This is a read-only matcher, not a boarding or ride instruction. */
int AccRoute_FallPredatorFinalLift(const char *level_name, int is_predator,
                                   int room_index,
                                   struct vectorch *position,
                                   struct strategyblock **control,
                                   struct strategyblock **platform);

/* fall / Predator only: resolve the surveyed gate05 front-side binary switch
 * from the room-76 side of its locked 76->77 boundary. On success, position is
 * the verified interaction approach pose and control is the linked switch.
 * This does not permit traversal into room 77. */
int AccRoute_FallPredatorGate05Control(const char *level_name, int is_predator,
                                       int source_room, int blocked_room,
                                       struct strategyblock *door,
                                       struct vectorch *position,
                                       struct strategyblock **control);

/* Returns 1 only when the zero-based index selects an eligible direct
 * activator for a live door. On success control and position are populated;
 * failure leaves all outputs unchanged. */
int AccRoute_DoorControl(struct strategyblock *door, int index,
                         struct vectorch *position, int *isArea,
                         struct strategyblock **control);

/* Read-only diagnostic for direct door edges and upstream switch chains. */
void AccRoute_TraceDoor(struct strategyblock *door);

#endif
