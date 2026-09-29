#ifndef ACC_WAYROUTE_H
#define ACC_WAYROUTE_H
struct aimodule;
struct vectorch;
/* Authored intra-room graph, independent of NPC scratch state.
   0: no usable endpoint coverage; -1: covered but disconnected;
   1: shared volume/direct; 2: intermediate waypoint. No movement is applied.
   Body position should be the player's centre, not their feet. */
int AccWayRoute_Find(struct aimodule *room, const struct vectorch *body,
    const struct vectorch *target, int alien, struct vectorch *out);
typedef int (*ACC_WAYROUTE_PROBE)(const struct vectorch *,const struct vectorch *);
int AccWayRoute_FindWithProbe(struct aimodule *room, const struct vectorch *body,
    const struct vectorch *target,int alien,struct vectorch *out,ACC_WAYROUTE_PROBE probe);
#endif
