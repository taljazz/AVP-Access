#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3dc.h"
#include "module.h"
#include "acc_wayroute.h"

#define MAX_TEST_NODES 180
#define MAX_TEST_EDGES 4

static AIMODULE room;
static WAYPOINT_HEADER header;
static WAYPOINT_VOLUME volumes[MAX_TEST_NODES];
static WAYPOINT_LINK edges[MAX_TEST_NODES][MAX_TEST_EDGES];
static int failures, checks;
static int probe_calls, probe_approve;
static VECTORCH probe_from, probe_to;
static int standing_probe_policy, standing_probe_calls;
static VECTORCH standing_probe_allow;

static void check(int condition, const char *message)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL: %s\n", message); }
    else printf("PASS: %s\n", message);
}

static void setup(int count, int ox, int oy, int oz)
{
    int i;
    memset(&room, 0, sizeof(room));
    memset(volumes, 0, sizeof(volumes));
    memset(edges, 0, sizeof(edges));
    room.m_world = (VECTORCH){ox, oy, oz};
    header.num_waypoints = count;
    header.first_waypoint = volumes;
    room.m_waypoints = &header;
    for (i = 0; i < count; ++i) {
        volumes[i].first_link = edges[i];
        volumes[i].workspace = (i * 17) & 0x3fff;
        volumes[i].contains_npc = (unsigned)(i & 1);
        volumes[i].contains_target = (unsigned)((i >> 1) & 1);
    }
}

static void set_volume(int index, int x, int y, int z, int hx, int hy, int hz)
{
    WAYPOINT_VOLUME *v = &volumes[index];
    v->centre = (VECTORCH){x, y, z};
    v->min_extents = (VECTORCH){-hx, -hy, -hz};
    v->max_extents = (VECTORCH){ hx,  hy,  hz};
}

static void add_link(int from, int to, int flags)
{
    int n = volumes[from].num_links++;
    if (n >= MAX_TEST_EDGES) { fprintf(stderr, "fixture edge overflow\n"); exit(2); }
    edges[from][n].link_target_index = to;
    edges[from][n].link_flags = flags;
}

static VECTORCH world_point(int x, int y, int z)
{
    return (VECTORCH){room.m_world.vx + x, room.m_world.vy + y, room.m_world.vz + z};
}

static int same_point(VECTORCH a, VECTORCH b)
{ return a.vx == b.vx && a.vy == b.vy && a.vz == b.vz; }

static int probe(const struct vectorch *from, const struct vectorch *to)
{
    probe_calls++;
    probe_from = *(const VECTORCH *)from;
    probe_to = *(const VECTORCH *)to;
    if (probe_to.vx==probe_from.vx+1 && probe_to.vy==probe_from.vy && probe_to.vz==probe_from.vz) {
        standing_probe_calls++;
        if (standing_probe_policy==1) return 0;
        if (standing_probe_policy==2)
            return probe_to.vx==standing_probe_allow.vx+1 &&
                probe_to.vy==standing_probe_allow.vy && probe_to.vz==standing_probe_allow.vz;
    }
    return probe_approve;
}

static void test_same_volume_direct(void)
{
    VECTORCH body, target, out = {91, 92, 93};
    setup(1, 12000, -4000, 7300);
    set_volume(0, 0, 0, 0, 1500, 900, 1500);
    body = world_point(-300, 0, 200);
    target = world_point(400, 0, 500);
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 1 && same_point(out, target),
          "body and target in one authored volume return the exact target");
}

static void test_corner_route_and_scratch(void)
{
    VECTORCH body, target, out = {91, 92, 93};
    WAYPOINT_VOLUME before[MAX_TEST_NODES];
    WAYPOINT_LINK edgeBefore[MAX_TEST_NODES][MAX_TEST_EDGES];
    setup(4, 9000, 2000, -7000);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 2000, 0, 0, 700, 400, 700);
    set_volume(2, 2000, 0, 2000, 700, 400, 700);
    set_volume(3, 4000, 0, 2000, 700, 400, 700);
    add_link(0, 1, 0); add_link(1, 2, 0); add_link(2, 3, 0);
    body = world_point(0, 0, 0);
    target = world_point(4000, 0, 2000);
    memcpy(before, volumes, sizeof(before));
    memcpy(edgeBefore, edges, sizeof(edgeBefore));
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 2 &&
          same_point(out, world_point(2000, 0, 0)),
          "multi-volume corner path returns its first waypoint in world coordinates");
    check(memcmp(before, volumes, sizeof(before)) == 0 &&
          memcmp(edgeBefore, edges, sizeof(edgeBefore)) == 0,
          "BFS leaves authored volumes and NPC scratch fields unchanged");
}

static void test_vertical_stacks(void)
{
    VECTORCH body, target, out = {91, 92, 93};
    setup(4, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 2000, 0, 0, 700, 400, 700);
    set_volume(2, 4000, 0, 0, 700, 400, 700);
    set_volume(3, 2000, -3000, 0, 700, 400, 700); /* directly above the lower corridor */
    add_link(0, 1, 0); add_link(1, 2, 0);
    body = (VECTORCH){0, 0, 0}; target = (VECTORCH){4000, 0, 0};
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 2 &&
          same_point(out, (VECTORCH){2000, 0, 0}),
          "overlapping XY footprint selects the connected lower floor");
    target = (VECTORCH){2000, -3000, 0}; out = (VECTORCH){91, 92, 93};
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == -1 &&
          same_point(out, (VECTORCH){91, 92, 93}),
          "a vertically stacked but unconnected endpoint is disconnected");
}

static void test_direction_and_species_flags(void)
{
    VECTORCH lower, upper, out;
    setup(2, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 2000, 0, 0, 700, 400, 700);
    add_link(0, 1, linkflag_oneway);
    add_link(1, 0, linkflag_reversed_oneway);
    lower = (VECTORCH){0, 0, 0}; upper = (VECTORCH){2000, 0, 0};
    check(AccWayRoute_Find(&room, &lower, &upper, 0, &out) == 2,
          "one-way edge permits its authored forward direction");
    check(AccWayRoute_Find(&room, &upper, &lower, 0, &out) == -1,
          "reversed-oneway edge blocks travel against that direction");

    setup(2, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 2000, 0, 0, 700, 400, 700);
    add_link(0, 1, linkflag_alienonly);
    check(AccWayRoute_Find(&room, &lower, &upper, 0, &out) == -1,
          "Marine search refuses an alien-only waypoint link");
    check(AccWayRoute_Find(&room, &lower, &upper, 1, &out) == 2,
          "Alien search may use the alien-only waypoint link");
}

static void test_large_cyclic_graph(void)
{
    VECTORCH body, target, out = {91, 92, 93};
    int i;
    setup(150, -1000, 250, 6000);
    for (i = 0; i < 150; ++i) set_volume(i, i * 2000, 0, 0, 700, 400, 700);
    for (i = 0; i < 149; ++i) {
        add_link(i, i + 1, 0);
        if (i) add_link(i, i - 1, 0);
    }
    add_link(149, 0, 0); /* cycle back to start */
    body = world_point(0, 0, 0); target = world_point(149 * 2000, 0, 0);
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 2 &&
          same_point(out, world_point(2000, 0, 0)),
          "150-node cyclic graph terminates and returns the first route step");
}

static void test_invalid_graph_and_failures(void)
{
    VECTORCH body, target, out = {91, 92, 93}, sentinel = {91, 92, 93};
    setup(2, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 2000, 0, 0, 700, 400, 700);
    add_link(0, -1, 0); add_link(0, 2, 0);
    body = (VECTORCH){0, 0, 0}; target = (VECTORCH){2000, 0, 0};
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == -1 && same_point(out, sentinel),
          "negative and out-of-range link indices are ignored without changing output");

    setup(1, 0, 0, 0); set_volume(0, 0, 0, 0, 700, 400, 700);
    body = (VECTORCH){0, 3000, 0}; target = (VECTORCH){0, 0, 0}; out = sentinel;
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 0 && same_point(out, sentinel),
          "uncovered body endpoint returns no-coverage and preserves output");

    body = (VECTORCH){0, 0, 0}; target = (VECTORCH){5000, 0, 0}; out = sentinel;
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 0 && same_point(out, sentinel),
          "target beyond authored endpoint coverage returns no-coverage");

    header.num_waypoints = 65537;
    out = sentinel;
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == 0 && same_point(out, sentinel),
          "header count beyond the hard safety cap fails cleanly");
}

static void test_probe_connects_touching_volumes_only_when_approved(void)
{
    VECTORCH body, target, out = {91, 92, 93};
    setup(2, 5000, -300, 7000);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 1400, 0, 0, 700, 400, 700); /* X bounds touch exactly */
    body = world_point(0, 0, 0); target = world_point(1400, 0, 0);
    check(AccWayRoute_Find(&room, &body, &target, 0, &out) == -1,
          "legacy Find wrapper does not infer an edge between touching volumes");

    probe_calls = 0; probe_approve = 0; out = (VECTORCH){91, 92, 93};
    check(AccWayRoute_FindWithProbe(&room, &body, &target, 0, &out, probe) == -1 && probe_calls > 0,
          "touching-volume candidate stays disconnected when the probe rejects it");
    check(same_point(probe_from, world_point(0, 0, 0)) &&
          same_point(probe_to, world_point(1400, 0, 0)),
          "probe receives the candidate segment endpoints in world coordinates");

    probe_calls = 0; probe_approve = 1; out = (VECTORCH){91, 92, 93};
    check(AccWayRoute_FindWithProbe(&room, &body, &target, 0, &out, probe) == 2 &&
          probe_calls > 0 && same_point(out, world_point(1400, 0, 0)),
          "approved probe connects touching volumes and returns the next waypoint");
}

static void test_probe_cannot_override_restricted_edges(void)
{
    VECTORCH body, target, out = {91, 92, 93}, sentinel = {91, 92, 93};
    setup(2, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 1400, 0, 0, 700, 400, 700);
    body = (VECTORCH){0, 0, 0}; target = (VECTORCH){1400, 0, 0};
    probe_approve = 1; probe_calls = 0;
    add_link(0, 1, linkflag_alienonly);
    check(AccWayRoute_FindWithProbe(&room, &body, &target, 0, &out, probe) == -1 &&
          same_point(out, sentinel) && probe_calls == 0,
          "probe cannot bypass an explicit Alien-only Marine edge");

    setup(2, 0, 0, 0);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 1400, 0, 0, 700, 400, 700);
    add_link(0, 1, linkflag_reversed_oneway);
    probe_calls = 0; out = sentinel;
    check(AccWayRoute_FindWithProbe(&room, &body, &target, 0, &out, probe) == -1 &&
          same_point(out, sentinel) && probe_calls == 0,
          "probe cannot bypass an explicit reversed-oneway edge");
}

static void test_probe_bridges_symmetric_small_gap(void)
{
    VECTORCH left, right, out;
    setup(2, -3000, 700, 9000);
    set_volume(0, 0, 0, 0, 700, 400, 700);
    set_volume(1, 1500, 0, 0, 700, 400, 700); /* 100 mm between X bounds */
    left = world_point(0, 0, 0); right = world_point(1500, 0, 0);
    probe_approve = 1;
    out = (VECTORCH){91, 92, 93};
    check(AccWayRoute_FindWithProbe(&room, &left, &right, 0, &out, probe) == 2 &&
          same_point(out, world_point(1500, 0, 0)),
          "approved probe bridges a 100 mm nearby-volume gap in forward direction");
    out = (VECTORCH){91, 92, 93}; probe_calls = 0;
    check(AccWayRoute_FindWithProbe(&room, &right, &left, 0, &out, probe) == 2 &&
          same_point(out, world_point(0, 0, 0)) && probe_calls > 0,
          "approved probe bridges the same 100 mm gap in reverse direction");
}

static void setup_standing_point_route(void)
{
    setup(3, 6000, 900, -8000);
    set_volume(0, 0, 0, 0, 1500, 600, 1500);
    set_volume(1, 2000, 0, 0, 1500, 600, 1500);
    set_volume(2, 4000, 0, 0, 1500, 600, 1500);
    add_link(0, 1, 0); add_link(1, 2, 0);
}

static void test_blocked_volume_centre_uses_checked_alternate(void)
{
    VECTORCH body, target, out={91,92,93}, expected;
    setup_standing_point_route();
    body=world_point(0,0,0); target=world_point(4000,0,0); expected=world_point(1300,0,0);
    probe_approve=1; standing_probe_policy=2; standing_probe_calls=0;
    standing_probe_allow=expected;
    check(AccWayRoute_FindWithProbe(&room,&body,&target,0,&out,probe)==2 &&
          same_point(out,expected) && standing_probe_calls>1 && standing_probe_calls<=50,
          "blocked next-volume centre selects the only approved body-width-inset standing point");
    standing_probe_policy=0;
}

static void test_failed_alternates_keep_uncertainty_at_centre(void)
{
    VECTORCH body, target, out={91,92,93}, centre;
    setup_standing_point_route();
    body=world_point(0,0,0); target=world_point(4000,0,0); centre=world_point(2000,0,0);
    probe_approve=1; standing_probe_policy=1; standing_probe_calls=0;
    check(AccWayRoute_FindWithProbe(&room,&body,&target,0,&out,probe)==2 &&
          same_point(out,centre) && standing_probe_calls==50,
          "when centre and all sampled alternatives fail, route retains the unverified centre");
    standing_probe_policy=0;
}

static void test_standing_alternates_do_not_cross_height_band(void)
{
    VECTORCH body, target, out={91,92,93}, centre;
    setup(3,6000,900,-8000);
    body=world_point(0,1500,0); target=world_point(4000,0,0); centre=world_point(2000,0,0);
    set_volume(0,0,1500,0,1500,600,1500);
    set_volume(1,2000,0,0,1500,600,1500);
    set_volume(2,4000,0,0,1500,600,1500);
    add_link(0,1,0); add_link(1,2,0);
    probe_approve=1; standing_probe_policy=1; standing_probe_calls=0;
    check(AccWayRoute_FindWithProbe(&room,&body,&target,0,&out,probe)==2 &&
          same_point(out,centre) && standing_probe_calls==0,
          "body height outside next volume band prevents alternate standing-point search");
    standing_probe_policy=0;
}

int main(void)
{
    test_same_volume_direct();
    test_corner_route_and_scratch();
    test_vertical_stacks();
    test_direction_and_species_flags();
    test_large_cyclic_graph();
    test_invalid_graph_and_failures();
    test_probe_connects_touching_volumes_only_when_approved();
    test_probe_cannot_override_restricted_edges();
    test_probe_bridges_symmetric_small_gap();
    test_blocked_volume_centre_uses_checked_alternate();
    test_failed_alternates_keep_uncertainty_at_centre();
    test_standing_alternates_do_not_cross_height_band();
    printf("Wayroute checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
