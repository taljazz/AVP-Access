/* AVP Access: conservative Marine guidance; never moves the player. */
#ifndef ACC_ROUTE_H
#define ACC_ROUTE_H
#include <stddef.h>
struct aimodule;
struct vectorch;
#ifdef __cplusplus
extern "C" {
#endif
void AccRoute_Toggle(void);
void AccRoute_CycleLoot(void);
void AccRoute_ToggleLoot(void);
void AccRoute_Update(unsigned int nowMs);
void AccRoute_Reset(void);
int AccRoute_IsEnabled(void);
int AccRoute_GetSnapTarget(unsigned int nowMs, struct vectorch *point, int *aim);
/* Bounded breadth-first search using current doors and ground-species entry points.
   Does not modify the engine's NPC routefinder scratch space. */
struct aimodule *AccRoute_NextModule(struct aimodule *source, struct aimodule *target);
/* Reachable segment before a closed door or a passage the Marine NPC graph
   cannot traverse. 1=door, 2=restricted passage, 0=no known segment. */
int AccRoute_Frontier(struct aimodule *source, struct aimodule *target,
                      struct aimodule **approach, struct vectorch *point,
                      struct aimodule **blocked);
int AccRoute_Format(const struct vectorch *player, int yaw,
                    const struct vectorch *point, const char *label,
                    char *text, size_t size);
#ifdef __cplusplus
}
#endif
#endif
