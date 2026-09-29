#include "acc_route_targets.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "3dc.h"
#include "stratdef.h"
#include "dynblock.h"
#include "bh_binsw.h"
#include "bh_lnksw.h"
#include "bh_mission.h"
#include "bh_types.h"
#include "bh_ldoor.h"
#include "bh_swdor.h"
#include "bh_plift.h"

/* These are part of the game runtime; keep the resolver on its live object set. */
extern STRATEGYBLOCK *ActiveStBlockList[];
extern int NumActiveStBlocks;


/* Mission request flags are shifted into the high bits by RequestState. */
#define ACC_MISSION_DONT_COMPLETE 0x8
#define ACC_REQUEST_COMPLETES(m) (((m) & 1) != 0 && ((m) & ACC_MISSION_DONT_COMPLETE) == 0)

static int finite_vector(const VECTORCH *v)
{
    return v && isfinite((double)v->vx) && isfinite((double)v->vy) &&
           isfinite((double)v->vz);
}

static int links_satisfied(const LINK_SWITCH_BEHAV_BLOCK *link)
{
    int i;
    if (link->num_linked_switches < 0 ||
        (link->num_linked_switches && !link->lswitch_list)) return 0;
    for (i = 0; i < link->num_linked_switches; ++i) {
        STRATEGYBLOCK *required = link->lswitch_list[i].bswitch;
        if (!required || required->SBflags.please_destroy_me || !required->SBdataptr)
            return 0;
        if (required->I_SBtype == I_BehaviourBinarySwitch) {
            if (!((BINARY_SWITCH_BEHAV_BLOCK *)required->SBdataptr)->state) return 0;
        } else if (required->I_SBtype == I_BehaviourLinkSwitch) {
            if (!((LINK_SWITCH_BEHAV_BLOCK *)required->SBdataptr)->system_state) return 0;
        } else return 0;
    }
    return 1;
}

static int has_direct_objective_target(const STRATEGYBLOCK *sw,
                                       const STRATEGYBLOCK *objective)
{
    int i;
    if (sw->I_SBtype == I_BehaviourBinarySwitch) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if (b->num_targets < 0 || (b->num_targets && (!b->bs_targets || !b->request_messages))) return 0;
        for (i = 0; i < b->num_targets; ++i)
            if (b->bs_targets[i] == objective && ACC_REQUEST_COMPLETES(b->request_messages[i])) return 1;
    } else {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if (l->num_targets < 0 || (l->num_targets && !l->ls_targets)) return 0;
        for (i = 0; i < l->num_targets; ++i)
            if (l->ls_targets[i].sbptr == objective && ACC_REQUEST_COMPLETES(l->ls_targets[i].request_message)) return 1;
    }
    return 0;
}

static int has_direct_door_request(const STRATEGYBLOCK *sw,
                                   const STRATEGYBLOCK *door)
{
    int i;
    if (sw->I_SBtype == I_BehaviourBinarySwitch) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if (b->num_targets < 0 || (b->num_targets && (!b->bs_targets || !b->request_messages))) return 0;
        for (i = 0; i < b->num_targets; ++i)
            if (b->bs_targets[i] == door && (b->request_messages[i] & 1)) return 1;
    } else {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if (l->num_targets < 0 || (l->num_targets && !l->ls_targets)) return 0;
        for (i = 0; i < l->num_targets; ++i)
            if (l->ls_targets[i].sbptr == door && (l->ls_targets[i].request_message & 1)) return 1;
    }
    return 0;
}

static int is_live_door(const STRATEGYBLOCK *door)
{
    int i;
    if (!door || door->SBflags.please_destroy_me || !door->SBdataptr) return 0;
    if (door->I_SBtype != I_BehaviourProximityDoor &&
        door->I_SBtype != I_BehaviourLiftDoor &&
        door->I_SBtype != I_BehaviourSwitchDoor) return 0;
    for (i = 0; i < NumActiveStBlocks; ++i)
        if (ActiveStBlockList[i] == door) return 1;
    return 0;
}

static int eligible(const STRATEGYBLOCK *sw)
{
    if (!sw || sw->SBflags.please_destroy_me || !sw->SBdataptr) return 0;
    if (sw->I_SBtype == I_BehaviourBinarySwitch) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        return !b->state && b->security_clerance == 0 &&
               b->bs_mode != I_bswitch_time_delay_autoexec;
    }
    if (sw->I_SBtype == I_BehaviourLinkSwitch) {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        return !l->state && l->security_clerance == 0 &&
               !l->switch_always_on &&
               l->ls_mode != I_lswitch_SELFDESTRUCT && links_satisfied(l);
    }
    return 0;
}

static int candidate_position(const STRATEGYBLOCK *sw, VECTORCH *out, int *area)
{
    if (sw->I_SBtype == I_BehaviourBinarySwitch) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if ((b->switch_flags & SwitchFlag_UseTriggerVolume) != 0) {
            const VECTORCH *lo = &b->trigger_volume_min, *hi = &b->trigger_volume_max;
            if (!finite_vector(lo) || !finite_vector(hi) || lo->vx >= hi->vx ||
                lo->vy >= hi->vy || lo->vz >= hi->vz) return 0;
            out->vx = (int)(((double)lo->vx + (double)hi->vx) / 2.0);
            out->vy = (int)(((double)lo->vy + (double)hi->vy) / 2.0);
            out->vz = (int)(((double)lo->vz + (double)hi->vz) / 2.0);
            *area = 1;
            return 1;
        }
    } else {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        if ((l->switch_flags & SwitchFlag_UseTriggerVolume) != 0) {
            const VECTORCH *lo = &l->trigger_volume_min, *hi = &l->trigger_volume_max;
            if (!finite_vector(lo) || !finite_vector(hi) || lo->vx >= hi->vx ||
                lo->vy >= hi->vy || lo->vz >= hi->vz) return 0;
            out->vx = (int)(((double)lo->vx + (double)hi->vx) / 2.0);
            out->vy = (int)(((double)lo->vy + (double)hi->vy) / 2.0);
            out->vz = (int)(((double)lo->vz + (double)hi->vz) / 2.0);
            *area = 1;
            return 1;
        }
    }
    if (sw->shapeIndex < 0 || !sw->DynPtr) return 0;
    out->vx = sw->DynPtr->Position.vx;
    out->vy = sw->DynPtr->Position.vy;
    out->vz = sw->DynPtr->Position.vz;
    *area = 0;
    return finite_vector(out);
}

int AccRoute_FindTarget(void *objective_arg, const struct vectorch *player_arg,
                        struct vectorch *position_arg, int *isArea)
{
    void *objective = objective_arg;
    const VECTORCH *player = (const VECTORCH *)player_arg;
    VECTORCH *position = (VECTORCH *)position_arg;
    double best = DBL_MAX;
    int count = 0, i, best_area = 0;
    if (!objective || !finite_vector(player) || !position || !isArea) return 0;
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sw = ActiveStBlockList[i];
        VECTORCH p;
        double dx, dy, dz, d;
        int area = 0;
        if (!sw || (sw->I_SBtype != I_BehaviourBinarySwitch &&
                    sw->I_SBtype != I_BehaviourLinkSwitch) || !eligible(sw)) continue;
        {
            int k, matches = 0;
            for (k = 0; k < NumActiveStBlocks; ++k) {
                STRATEGYBLOCK *mission = ActiveStBlockList[k];
                MISSION_COMPLETE_BEHAV_BLOCK *mb;
                if (!mission || mission->I_SBtype != I_BehaviourMissionComplete ||
                    mission->SBflags.please_destroy_me || !mission->SBdataptr) continue;
                mb = (MISSION_COMPLETE_BEHAV_BLOCK *)mission->SBdataptr;
                if (mb->mission_objective_ptr == objective &&
                    has_direct_objective_target(sw, mission)) { matches = 1; break; }
            }
            if (!matches) continue;
        }
        if (!candidate_position(sw, &p, &area)) continue;
        ++count;
        dx = (double)p.vx - player->vx;
        dy = (double)p.vy - player->vy;
        dz = (double)p.vz - player->vz;
        d = dx*dx + dy*dy + dz*dz;
        if (d < best) { best = d; *position = p; best_area = area; }
    }
    if (count) *isArea = best_area;
    return count;
}

int AccRoute_FallPredatorOpening(const struct vectorch *player_arg,
                                 struct vectorch *position_arg,
                                 struct strategyblock **control)
{
    const VECTORCH *player = (const VECTORCH *)player_arg;
    VECTORCH switchPosition;
    STRATEGYBLOCK *selected = NULL;
    int i, j;
    if (!finite_vector(player) || !position_arg || !control) return 0;
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sw = ActiveStBlockList[i];
        int area = 0;
        int opensLockedProximityDoor = 0;
        if (!sw || sw->I_SBtype != I_BehaviourBinarySwitch ||
            sw->SBflags.please_destroy_me || !sw->SBdataptr || !sw->DynPtr ||
            !candidate_position(sw, &switchPosition, &area)) continue;

        /* This is the authored first facility switch in fall. The coordinate
           anchor is intentionally narrow; the live link and door state below
           keep unrelated switches from matching. Allow the small z movement
           in the retail switch's activation animation. */
        if (abs(switchPosition.vx - 26790) > 700 ||
            abs(switchPosition.vy - 15167) > 700 ||
            abs(switchPosition.vz - 9780) > 700) continue;

        {
            BINARY_SWITCH_BEHAV_BLOCK *b = (BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
            if (b->num_targets < 0 || (b->num_targets && (!b->bs_targets || !b->request_messages)))
                continue;
            for (j = 0; j < b->num_targets; ++j) {
                STRATEGYBLOCK *door = b->bs_targets[j];
                if ((b->request_messages[j] & 1) && is_live_door(door) &&
                    door->I_SBtype == I_BehaviourProximityDoor) {
                    selected = sw;
                    opensLockedProximityDoor =
                        ((PROXDOOR_BEHAV_BLOCK *)door->SBdataptr)->door_locked != 0;
                    break;
                }
            }
        }
        if (!selected) continue;
        if (opensLockedProximityDoor) {
            if (!eligible(selected)) return 0;
            *position_arg = switchPosition;
            *control = selected;
            return 1;
        }
        /* Once the first control is active, lead through the exact start-side
           portal into gate02. This is a target point only; route traversal
           across the directed room edge remains separately verified. */
        *position_arg = (VECTORCH){40915, 4475, 21312};
        *control = NULL;
        return 2;
    }
    return 0;
}

static int fall_level_name(const char *level_name)
{
    static const char expected[] = "fall";
    int i;
    if (!level_name) return 0;
    for (i = 0; expected[i]; ++i)
        if (!level_name[i] || tolower((unsigned char)level_name[i]) != expected[i]) return 0;
    return level_name[i] == 0;
}

int AccRoute_FallPredatorGate05Control(const char *level_name, int is_predator,
                                       int source_room, int blocked_room,
                                       STRATEGYBLOCK *door,
                                       VECTORCH *position,
                                       STRATEGYBLOCK **control)
{
    STRATEGYBLOCK *match = NULL;
    int i, matches = 0;
    if (!fall_level_name(level_name) || !is_predator || source_room != 76 ||
        blocked_room != 77 || !position || !control || !is_live_door(door) ||
        door->I_SBtype != I_BehaviourProximityDoor ||
        !((PROXDOOR_BEHAV_BLOCK *)door->SBdataptr)->door_locked) return 0;

    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sw = ActiveStBlockList[i];
        VECTORCH p;
        int area = 0;
        if (!sw || sw->I_SBtype != I_BehaviourBinarySwitch || !eligible(sw) ||
            !candidate_position(sw, &p, &area) ||
            area != 0 ||
            abs(p.vx - 209310) > 500 || abs(p.vy - 11271) > 500 ||
            abs(p.vz - 145229) > 500 || !has_direct_door_request(sw, door)) continue;
        match = sw;
        ++matches;
    }
    /* The direct live link and exact switch signature must be unique. The
       approach pose was observed to permit Interact from the reachable side. */
    if (matches != 1) return 0;
    *position = (VECTORCH){206976, 12533, 144370};
    *control = match;
    return 1;
}

static int active_strategy(const STRATEGYBLOCK *candidate)
{
    int i;
    for (i = 0; i < NumActiveStBlocks; ++i)
        if (ActiveStBlockList[i] == candidate) return 1;
    return 0;
}

static int final_shaft_platform(const STRATEGYBLOCK *s)
{
    const PLATFORMLIFT_BEHAVIOUR_BLOCK *lift;
    const VECTORCH *p;
    if (!s || s->I_SBtype != I_BehaviourPlatform ||
        s->SBflags.please_destroy_me || !s->SBdataptr || !s->DynPtr ||
        !active_strategy(s)) return 0;
    lift = (const PLATFORMLIFT_BEHAVIOUR_BLOCK *)s->SBdataptr;
    p = &s->DynPtr->Position;
    return abs(p->vx - 187235) <= 500 && abs(p->vz - 70510) <= 500 &&
           abs(lift->upHeight - (-2076)) <= 250 &&
           abs(lift->downHeight - 25226) <= 250;
}

static int final_shaft_request(const STRATEGYBLOCK *sw,
                               const STRATEGYBLOCK *platform)
{
    const BINARY_SWITCH_BEHAV_BLOCK *b;
    int i;
    if (!sw || sw->I_SBtype != I_BehaviourBinarySwitch ||
        sw->SBflags.please_destroy_me || !sw->SBdataptr) return 0;
    b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
    if (b->num_targets < 0 || (b->num_targets && (!b->bs_targets || !b->request_messages))) return 0;
    for (i = 0; i < b->num_targets; ++i)
        if (b->bs_targets[i] == platform && (b->request_messages[i] & 1)) return 1;
    return 0;
}

int AccRoute_FallPredatorFinalLift(const char *level_name, int is_predator,
                                   int room_index, struct vectorch *position_arg,
                                   struct strategyblock **control_arg,
                                   struct strategyblock **platform_arg)
{
    STRATEGYBLOCK *platform = NULL, *control = NULL;
    VECTORCH switch_position, result_position;
    int i, platform_count = 0, control_count = 0;
    if (!fall_level_name(level_name) || !is_predator || room_index != 85 ||
        !position_arg || !control_arg || !platform_arg) return 0;

    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *s = ActiveStBlockList[i];
        if (!final_shaft_platform(s)) continue;
        platform = s;
        ++platform_count;
    }
    /* Duplicate matching lift signatures are ambiguous; do not guess. */
    if (platform_count != 1) return 0;

    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sw = ActiveStBlockList[i];
        int area = 0;
        if (!sw || sw->I_SBtype != I_BehaviourBinarySwitch ||
            !candidate_position(sw, &switch_position, &area) ||
            abs(switch_position.vx - 190216) > 700 ||
            abs(switch_position.vy - 26856) > 700 ||
            abs(switch_position.vz - 63594) > 700 ||
            !final_shaft_request(sw, platform)) continue;
        control = sw;
        result_position = switch_position;
        ++control_count;
    }
    /* The authored switch-to-platform request must be unique and live. */
    if (control_count != 1) return 0;

    if (((PLATFORMLIFT_BEHAVIOUR_BLOCK *)platform->SBdataptr)->Enabled) {
        result_position = platform->DynPtr->Position;
        *position_arg = result_position;
        *control_arg = NULL;
        *platform_arg = platform;
        return 2;
    }
    if (!eligible(control)) return 0;
    *position_arg = result_position;
    *control_arg = control;
    *platform_arg = platform;
    return 1;
}

int AccRoute_DoorControl(STRATEGYBLOCK *door, int index, VECTORCH *position,
                         int *isArea, STRATEGYBLOCK **control)
{
    int count = 0, i;
    if (!is_live_door(door) || index < 0 || !position || !isArea || !control) return 0;
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sw = ActiveStBlockList[i];
        VECTORCH p;
        int area = 0;
        if (!sw || (sw->I_SBtype != I_BehaviourBinarySwitch &&
                    sw->I_SBtype != I_BehaviourLinkSwitch) || !eligible(sw) ||
            !has_direct_door_request(sw, door) || !candidate_position(sw, &p, &area)) continue;
        if (count == index) {
            *position = p;
            *isArea = area;
            *control = sw;
            return 1;
        }
        ++count;
    }
    return 0;
}

#define ACC_TRACE_MAX_DEPTH 4
#define ACC_TRACE_MAX_NODES 128

static STRATEGYBLOCK *trace_seen[ACC_TRACE_MAX_NODES];
static int trace_seen_count;

static int trace_has_seen(const STRATEGYBLOCK *sw)
{
    int i;
    for (i = 0; i < trace_seen_count; ++i)
        if (trace_seen[i] == sw) return 1;
    if (trace_seen_count < ACC_TRACE_MAX_NODES)
        trace_seen[trace_seen_count++] = (STRATEGYBLOCK *)sw;
    return 0;
}

static const char *trace_type(const STRATEGYBLOCK *sw)
{
    if (!sw) return "null";
    if (sw->I_SBtype == I_BehaviourBinarySwitch) return "binary";
    if (sw->I_SBtype == I_BehaviourLinkSwitch) return "link";
    return "other";
}

static void trace_switch_status(const STRATEGYBLOCK *sw, const char *indent)
{
    VECTORCH p;
    int area = 0, has_position;
    if (!sw) return;
    has_position = sw->SBdataptr && candidate_position(sw, &p, &area);
    if (sw->I_SBtype == I_BehaviourBinarySwitch && sw->SBdataptr) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        fprintf(stderr, "%sSW %p type=binary destroyed=%d state=%d systemstate=n/a security=%d mode=%d switch_flags=%u shape=%d position=%s",
                indent, (const void *)sw, sw->SBflags.please_destroy_me, b->state,
                b->security_clerance, b->bs_mode, (unsigned)b->switch_flags,
                sw->shapeIndex, has_position ? "valid" : "invalid");
        if (has_position) fprintf(stderr, " area=%d xyz=%d,%d,%d", area, p.vx, p.vy, p.vz);
        fprintf(stderr, " eligible=%d\n", eligible(sw));
    } else if (sw->I_SBtype == I_BehaviourLinkSwitch && sw->SBdataptr) {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sw->SBdataptr;
        int i;
        fprintf(stderr, "%sSW %p type=link destroyed=%d state=%d systemstate=%d security=%d mode=%d switch_flags=%u shape=%d position=%s",
                indent, (const void *)sw, sw->SBflags.please_destroy_me, l->state,
                l->system_state, l->security_clerance, l->ls_mode,
                (unsigned)l->switch_flags, sw->shapeIndex,
                has_position ? "valid" : "invalid");
        if (has_position) fprintf(stderr, " area=%d xyz=%d,%d,%d", area, p.vx, p.vy, p.vz);
        fprintf(stderr, " eligible=%d prereq_count=%d\n", eligible(sw), l->num_linked_switches);
        if (l->num_linked_switches < 0 || (l->num_linked_switches && !l->lswitch_list)) {
            fprintf(stderr, "%s  prerequisites=invalid-list\n", indent);
        } else {
            for (i = 0; i < l->num_linked_switches; ++i) {
                STRATEGYBLOCK *req = l->lswitch_list[i].bswitch;
                if (!req) fprintf(stderr, "%s  prerequisite[%d]=null\n", indent, i);
                else if (req->I_SBtype == I_BehaviourBinarySwitch && req->SBdataptr)
                    fprintf(stderr, "%s  prerequisite[%d]=%p type=binary destroyed=%d state=%d\n",
                            indent, i, (void *)req, req->SBflags.please_destroy_me,
                            ((BINARY_SWITCH_BEHAV_BLOCK *)req->SBdataptr)->state);
                else if (req->I_SBtype == I_BehaviourLinkSwitch && req->SBdataptr)
                    fprintf(stderr, "%s  prerequisite[%d]=%p type=link destroyed=%d state=%d systemstate=%d\n",
                            indent, i, (void *)req, req->SBflags.please_destroy_me,
                            ((LINK_SWITCH_BEHAV_BLOCK *)req->SBdataptr)->state,
                            ((LINK_SWITCH_BEHAV_BLOCK *)req->SBdataptr)->system_state);
                else fprintf(stderr, "%s  prerequisite[%d]=%p type=%d destroyed=%d data=%s\n",
                             indent, i, (void *)req, (int)req->I_SBtype,
                             req->SBflags.please_destroy_me, req->SBdataptr ? "yes" : "null");
            }
        }
    } else {
        fprintf(stderr, "%sSW %p type=%s destroyed=%d data=%s shape=%d position=invalid eligible=0\n",
                indent, (const void *)sw, trace_type(sw), sw->SBflags.please_destroy_me,
                sw->SBdataptr ? "yes" : "null", sw->shapeIndex);
    }
}

static int trace_request_to(const STRATEGYBLOCK *sender, const STRATEGYBLOCK *target)
{
    int i;
    if (!sender || !sender->SBdataptr) return 0;
    if (sender->I_SBtype == I_BehaviourBinarySwitch) {
        const BINARY_SWITCH_BEHAV_BLOCK *b = (const BINARY_SWITCH_BEHAV_BLOCK *)sender->SBdataptr;
        if (b->num_targets < 0 || (b->num_targets && (!b->bs_targets || !b->request_messages))) return 0;
        for (i = 0; i < b->num_targets; ++i)
            if (b->bs_targets[i] == target) {
                fprintf(stderr, "    edge request=%d lowbit=%d\n", b->request_messages[i], b->request_messages[i] & 1);
                return 1;
            }
    } else if (sender->I_SBtype == I_BehaviourLinkSwitch) {
        const LINK_SWITCH_BEHAV_BLOCK *l = (const LINK_SWITCH_BEHAV_BLOCK *)sender->SBdataptr;
        if (l->num_targets < 0 || (l->num_targets && !l->ls_targets)) return 0;
        for (i = 0; i < l->num_targets; ++i)
            if (l->ls_targets[i].sbptr == target) {
                fprintf(stderr, "    edge request=%d lowbit=%d\n", l->ls_targets[i].request_message,
                        l->ls_targets[i].request_message & 1);
                return 1;
            }
    }
    return 0;
}

static void trace_upstream(STRATEGYBLOCK *target, int depth)
{
    int i;
    char indent[16];
    int spaces = depth * 2;
    if (depth > ACC_TRACE_MAX_DEPTH || !target) return;
    while (spaces-- > 0) indent[spaces] = ' ';
    indent[depth * 2] = '\0';
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sender = ActiveStBlockList[i];
        if (!sender || (sender->I_SBtype != I_BehaviourBinarySwitch &&
                        sender->I_SBtype != I_BehaviourLinkSwitch) ||
            !trace_request_to(sender, target)) continue;
        trace_switch_status(sender, indent);
        if (depth < ACC_TRACE_MAX_DEPTH) {
            if (trace_has_seen(sender)) {
                fprintf(stderr, "%s  cycle=%p\n", indent, (void *)sender);
            } else {
                trace_upstream(sender, depth + 1);
            }
        }
    }
}

void AccRoute_TraceDoor(STRATEGYBLOCK *door)
{
    int i;
    if (!door) {
        fprintf(stderr, "AccRoute_TraceDoor door=null\n");
        return;
    }
    fprintf(stderr, "AccRoute_TraceDoor door=%p type=%d destroyed=%d data=%s\n",
            (void *)door, (int)door->I_SBtype, door->SBflags.please_destroy_me,
            door->SBdataptr ? "yes" : "null");
    trace_seen_count = 0;
    for (i = 0; i < NumActiveStBlocks; ++i) {
        STRATEGYBLOCK *sender = ActiveStBlockList[i];
        if (!sender || (sender->I_SBtype != I_BehaviourBinarySwitch &&
                        sender->I_SBtype != I_BehaviourLinkSwitch) ||
            !trace_request_to(sender, door)) continue;
        trace_switch_status(sender, "");
        if (!trace_has_seen(sender)) trace_upstream(sender, 1);
    }
}
