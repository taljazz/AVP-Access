#include <stdio.h>
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
#include "acc_route_targets.h"

#define maxstblocks 1000
STRATEGYBLOCK *ActiveStBlockList[maxstblocks];
int NumActiveStBlocks;

static int checks, failures;
#define CHECK(c, m) do { ++checks; if (!(c)) { ++failures; printf("FAIL: %s\n", m); } } while (0)

static STRATEGYBLOCK obj, sw[5];
static MISSION_COMPLETE_BEHAV_BLOCK mission;
static BINARY_SWITCH_BEHAV_BLOCK binary[5];
static LINK_SWITCH_BEHAV_BLOCK link_data;
static VECTORCH area_lo = {100, 200, 300}, area_hi = {300, 400, 500};
static DYNAMICSBLOCK dyn[5];
static STRATEGYBLOCK *targets[5];
static int requests[5];
static STRATEGYBLOCK door;
static PROXDOOR_BEHAV_BLOCK proxdoor;
static LIFT_DOOR_BEHAV_BLOCK liftdoor;
static SWITCH_DOOR_BEHAV_BLOCK switchdoor;
static STRATEGYBLOCK finalPlatform, duplicatePlatform;
static PLATFORMLIFT_BEHAVIOUR_BLOCK finalLiftData, duplicateLiftData;
static DYNAMICSBLOCK finalPlatformDyn, duplicatePlatformDyn;

static void reset(void)
{
    int i;
    memset(&obj, 0, sizeof(obj)); memset(&mission, 0, sizeof(mission));
    memset(sw, 0, sizeof(sw)); memset(binary, 0, sizeof(binary));
    memset(dyn, 0, sizeof(dyn)); memset(targets, 0, sizeof(targets));
    memset(requests, 0, sizeof(requests)); memset(ActiveStBlockList, 0, sizeof(ActiveStBlockList));
    memset(&door, 0, sizeof(door)); memset(&proxdoor, 0, sizeof(proxdoor));
    memset(&liftdoor, 0, sizeof(liftdoor)); memset(&switchdoor, 0, sizeof(switchdoor));
    memset(&finalPlatform, 0, sizeof(finalPlatform)); memset(&duplicatePlatform, 0, sizeof(duplicatePlatform));
    memset(&finalLiftData, 0, sizeof(finalLiftData)); memset(&duplicateLiftData, 0, sizeof(duplicateLiftData));
    memset(&finalPlatformDyn, 0, sizeof(finalPlatformDyn)); memset(&duplicatePlatformDyn, 0, sizeof(duplicatePlatformDyn));
    obj.I_SBtype = I_BehaviourMissionComplete; obj.SBdataptr = &mission;
    mission.mission_objective_ptr = (void *)0x1234;
    for (i=0;i<5;i++) {
        sw[i].I_SBtype = I_BehaviourBinarySwitch; sw[i].SBdataptr = &binary[i];
        sw[i].shapeIndex = 1; sw[i].DynPtr = &dyn[i];
        binary[i].num_targets = 1; binary[i].bs_targets = &targets[i];
        binary[i].request_messages = &requests[i]; binary[i].bs_mode = I_bswitch_wait;
        targets[i] = &obj; requests[i] = 1; ActiveStBlockList[i] = &sw[i];
    }
    ActiveStBlockList[5] = &obj; NumActiveStBlocks = 6;
}

static void setup_door(AVP_BEHAVIOUR_TYPE type)
{
    door.I_SBtype = type;
    door.SBdataptr = type == I_BehaviourProximityDoor ? (void *)&proxdoor :
                     type == I_BehaviourLiftDoor ? (void *)&liftdoor : (void *)&switchdoor;
    switch (type) {
    case I_BehaviourProximityDoor: proxdoor.bhvr_type = type; break;
    case I_BehaviourLiftDoor: liftdoor.bhvr_type = type; break;
    case I_BehaviourSwitchDoor: switchdoor.myBehaviourType = type; break;
    }
    ActiveStBlockList[6] = &door;
    NumActiveStBlocks = 7;
    for (int i = 0; i < 5; ++i) { targets[i] = &door; requests[i] = 1; }
}

static int find(VECTORCH player, VECTORCH *out, int *area)
{ return AccRoute_FindTarget((void *)0x1234, &player, out, area); }

int main(void)
{
    VECTORCH player = {0,0,0}, out = {-9,-9,-9};
    int area = -1, n;
    reset();
    binary[0].switch_flags = SwitchFlag_UseTriggerVolume;
    binary[0].trigger_volume_min = area_lo; binary[0].trigger_volume_max = area_hi;
    dyn[1].Position.vx = dyn[2].Position.vx = dyn[3].Position.vx = dyn[4].Position.vx = 1000;
    n = find(player, &out, &area);
    CHECK(n == 5 && out.vx == 200 && out.vy == 300 && out.vz == 400 && area == 1,
          "area switch reports center of world-coordinate bounds");
    binary[0].trigger_volume_max.vx = binary[0].trigger_volume_min.vx;
    CHECK(find(player, &out, &area) == 4, "degenerate area bounds are rejected");

    reset();
    CHECK(find(player, &out, &area) == 5, "all direct active switch edges are counted");
    CHECK(AccRoute_FindTarget((void *)0x9999, &player, &out, &area) == 0,
          "objective pointer must match a live MissionComplete strategy");

    reset(); binary[0].bs_mode = I_bswitch_timer;
    CHECK(find(player, &out, &area) == 5,
          "ordinary binary timer mode remains usable through player request");

    reset(); requests[0] = 9; requests[1] = 0;
    CHECK(find(player, &out, &area) == 3, "DontComplete and off requests are rejected");

    reset(); binary[0].security_clerance = 1; binary[1].bs_mode = I_bswitch_time_delay_autoexec;
    sw[2].SBflags.please_destroy_me = 1; binary[3].state = 1;
    CHECK(find(player, &out, &area) == 1, "security, autoexec, destroyed, and active switches are rejected");

    reset(); sw[0].shapeIndex = -1; sw[1].DynPtr = NULL;
    CHECK(find(player, &out, &area) == 3, "unplaced or shape-less physical switches are rejected");

    reset(); dyn[0].Position.vx = 50; dyn[1].Position.vx = 10; dyn[2].Position.vx = 25;
    dyn[3].Position.vx = dyn[4].Position.vx = 1000;
    n = find(player, &out, &area);
    CHECK(n == 5 && out.vx == 10 && area == 0, "nearest physical switch is selected by distance");

    reset();
    sw[0].I_SBtype = I_BehaviourLinkSwitch; sw[0].SBdataptr = &link_data;
    memset(&link_data, 0, sizeof(link_data));
    link_data.num_targets = 1;
    /* Layout differs; use the real link-target record for its request edge. */
    {
        LINK_SWITCH_TARGET lt;
        STRATEGYBLOCK prerequisite;
        LINK_SWITCH_BEHAV_BLOCK prerequisite_data;
        LSWITCH_ITEM required_link;
        memset(&lt, 0, sizeof(lt)); lt.sbptr = &obj; lt.request_message = 1;
        memset(&prerequisite, 0, sizeof(prerequisite));
        memset(&prerequisite_data, 0, sizeof(prerequisite_data));
        link_data.ls_targets = &lt; link_data.ls_mode = I_lswitch_wait;
        CHECK(find(player, &out, &area) == 5, "direct link-switch target request is accepted");
        link_data.ls_mode = I_lswitch_timer;
        CHECK(find(player, &out, &area) == 5,
              "ordinary link timer mode remains usable through player request");
        link_data.ls_mode = I_lswitch_wait;
        lt.request_message = 9;
        CHECK(find(player, &out, &area) == 4, "link-switch DontComplete request is rejected");
        lt.request_message = 1;
        link_data.num_linked_switches = 1;
        required_link.bswitch = &prerequisite;
        link_data.lswitch_list = &required_link;
        CHECK(find(player, &out, &area) == 4, "link switches with missing unmet dependencies are rejected");
        prerequisite.I_SBtype = I_BehaviourLinkSwitch;
        prerequisite.SBdataptr = &prerequisite_data;
        prerequisite_data.system_state = 1;
        prerequisite_data.state = 0;
        CHECK(find(player, &out, &area) == 5,
              "link prerequisite uses the engine system_state latch");
        link_data.num_linked_switches = 0;
        link_data.switch_always_on = 1;
        CHECK(find(player, &out, &area) == 4, "always-on logical switches are rejected");
    }

    {
        int types[3] = { I_BehaviourProximityDoor, I_BehaviourLiftDoor, I_BehaviourSwitchDoor };
        int t;
        for (t = 0; t < 3; ++t) {
            STRATEGYBLOCK *control = NULL;
            reset(); setup_door((AVP_BEHAVIOUR_TYPE)types[t]);
            requests[0] = 9; /* high bits are irrelevant to door requests */
            sw[1].SBflags.please_destroy_me = 1;
            binary[2].state = 1;
            binary[3].switch_flags = SwitchFlag_UseTriggerVolume;
            binary[3].trigger_volume_min = area_lo; binary[3].trigger_volume_max = area_hi;
            sw[4].shapeIndex = -1; /* trigger volume still gives a valid position below */
            binary[4].switch_flags = SwitchFlag_UseTriggerVolume;
            binary[4].trigger_volume_min = area_lo; binary[4].trigger_volume_max = area_hi;
            n = AccRoute_DoorControl(&door, 0, &out, &area, &control);
            CHECK(n == 1 && control == &sw[0] && out.vx == dyn[0].Position.vx && area == 0,
                  "door control accepts low request bit despite high bits and returns first eligible switch");
            control = NULL;
            n = AccRoute_DoorControl(&door, 1, &out, &area, &control);
            CHECK(n == 1 && control == &sw[3] && area == 1 && out.vx == 200,
                  "door control returns indexed area candidate and its center");
            CHECK(AccRoute_DoorControl(&door, 2, &out, &area, &control) == 1 && control == &sw[4],
                  "shape-less switch with valid trigger area is eligible");
            control = (STRATEGYBLOCK *)0x1234;
            out = (VECTORCH){-7, -8, -9}; area = 77;
            CHECK(AccRoute_DoorControl(&door, 3, &out, &area, &control) == 0 &&
                  control == (STRATEGYBLOCK *)0x1234 && out.vx == -7 && out.vy == -8 && out.vz == -9 && area == 77,
                  "first index beyond candidates fails without changing outputs");
            CHECK(AccRoute_DoorControl(&door, INT_MAX, &out, &area, &control) == 0 &&
                  control == (STRATEGYBLOCK *)0x1234 && out.vx == -7 && out.vy == -8 && out.vz == -9 && area == 77,
                  "very large index fails without changing outputs");
            targets[0] = &obj;
            CHECK(AccRoute_DoorControl(&door, 0, &out, &area, &control) == 1,
                  "wrong receiver edge is excluded");
            targets[0] = &door; requests[0] = 8;
            CHECK(AccRoute_DoorControl(&door, 0, &out, &area, &control) == 1,
                  "door request without low bit is excluded");
            binary[3].trigger_volume_max.vx = binary[3].trigger_volume_min.vx;
            CHECK(AccRoute_DoorControl(&door, 0, &out, &area, &control) == 1,
                  "invalid trigger bounds exclude that candidate");
            CHECK(AccRoute_DoorControl(&door, -1, &out, &area, &control) == 0 &&
                  AccRoute_DoorControl(&door, 0, NULL, &area, &control) == 0 &&
                  AccRoute_DoorControl(&door, 0, &out, NULL, &control) == 0 &&
                  AccRoute_DoorControl(&door, 0, &out, &area, NULL) == 0,
                  "index and output pointer guards reject invalid calls");
            {
                STRATEGYBLOCK dead = door, wrong = door, inactive = door;
                dead.SBflags.please_destroy_me = 1;
                wrong.I_SBtype = I_BehaviourMissionComplete;
                inactive.I_SBtype = I_BehaviourProximityDoor;
                CHECK(AccRoute_DoorControl(&dead, 0, &out, &area, &control) == 0 &&
                      AccRoute_DoorControl(&wrong, 0, &out, &area, &control) == 0,
                      "dead and unsupported receivers are rejected");
                NumActiveStBlocks = 6;
                CHECK(AccRoute_DoorControl(&inactive, 0, &out, &area, &control) == 0,
                      "door receiver must be in active strategy list");
            }
        }
    }
    reset();
    setup_door(I_BehaviourProximityDoor);
    dyn[0].Position = (VECTORCH){26790,15167,9780};
    targets[0] = &door; requests[0] = 1;
    proxdoor.door_locked = 1;
    out = (VECTORCH){-1,-2,-3};
    {
        STRATEGYBLOCK *control = NULL;
        CHECK(AccRoute_FallPredatorOpening(&player, &out, &control) == 1 &&
              control == &sw[0] && out.vx == 26790 && out.vy == 15167 && out.vz == 9780,
              "fall opening identifies the live eligible facility switch by position and locked-door request");
        binary[0].security_clerance = 1;
        out = (VECTORCH){-1,-2,-3}; control = (STRATEGYBLOCK *)0x1234;
        CHECK(AccRoute_FallPredatorOpening(&player, &out, &control) == 0 &&
              out.vx == -1 && out.vy == -2 && out.vz == -3 && control == (STRATEGYBLOCK *)0x1234,
              "locked facility door with an ineligible switch does not advance guidance to the gate");
        binary[0].security_clerance = 0;
        proxdoor.door_locked = 0; binary[0].state = 1;
        CHECK(AccRoute_FallPredatorOpening(&player, &out, &control) == 2 &&
              control == NULL && out.vx == 40915 && out.vy == 4475 && out.vz == 21312,
              "unlocked facility door advances to its surveyed start-side gate approach");
        dyn[0].Position.vx += 1000;
        CHECK(AccRoute_FallPredatorOpening(&player, &out, &control) == 0,
              "opening guidance is anchored narrowly to the surveyed switch location");
    }

    reset();
    setup_door(I_BehaviourProximityDoor);
    dyn[0].Position = (VECTORCH){209310,11271,145250};
    proxdoor.door_locked = 1;
    out = (VECTORCH){-1,-2,-3};
    {
        STRATEGYBLOCK *control = NULL;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 1 &&
              control == &sw[0] && out.vx == 206976 && out.vy == 12533 && out.vz == 144370,
              "Fall Predator resolves the live gate05 switch to its verified front-side interaction pose");
        out = (VECTORCH){-1,-2,-3}; control = (STRATEGYBLOCK *)0x1234;
        CHECK(AccRoute_FallPredatorGate05Control("derelict",1,76,77,&door,&out,&control) == 0 &&
              AccRoute_FallPredatorGate05Control("fall",0,76,77,&door,&out,&control) == 0 &&
              AccRoute_FallPredatorGate05Control("fall",1,75,77,&door,&out,&control) == 0 &&
              AccRoute_FallPredatorGate05Control("fall",1,76,78,&door,&out,&control) == 0 &&
              out.vx == -1 && out.vy == -2 && out.vz == -3 && control == (STRATEGYBLOCK *)0x1234,
              "gate05 exception fails closed outside Fall/Predator/source76/boundary77 scope");
        proxdoor.door_locked = 0;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 0,
              "opened gate05 no longer selects its special locked-door control route");
        proxdoor.door_locked = 1;
        binary[0].security_clerance = 1;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 0,
              "ineligible gate05 switch is rejected");
        binary[0].security_clerance = 0;
        dyn[0].Position.vx += 1000;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 0,
              "unrelated switch position does not match gate05 signature");
        dyn[0].Position.vx -= 1000;
        targets[0] = &obj;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 0,
              "gate05 switch must request its direct proximity-door target");
        targets[0] = &door;
        dyn[1].Position = dyn[0].Position;
        CHECK(AccRoute_FallPredatorGate05Control("fall",1,76,77,&door,&out,&control) == 0,
              "ambiguous duplicate gate05 switch signatures fail closed");
    }

    reset();
    dyn[0].Position = (VECTORCH){190216,26856,63594};
    finalPlatform.I_SBtype = I_BehaviourPlatform;
    finalPlatform.SBdataptr = &finalLiftData;
    finalPlatform.DynPtr = &finalPlatformDyn;
    finalPlatform.shapeIndex = 1;
    finalPlatformDyn.Position = (VECTORCH){187235,-2076,70510};
    finalLiftData.upHeight = -2076;
    finalLiftData.downHeight = 25226;
    targets[0] = &finalPlatform;
    requests[0] = 1;
    ActiveStBlockList[6] = &finalPlatform;
    NumActiveStBlocks = 7;
    out = (VECTORCH){-1,-2,-3};
    {
        STRATEGYBLOCK *control = NULL, *platform = NULL;
        int liftMatch = AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform);
        CHECK(liftMatch == 1 &&
              control == &sw[0] && platform == &finalPlatform &&
              out.vx == 190216 && out.vy == 26856 && out.vz == 63594,
              "fall Predator room 85 finds the eligible linked final-lift switch while disabled");
        out = (VECTORCH){-1,-2,-3}; control = (STRATEGYBLOCK *)0x1234; platform = (STRATEGYBLOCK *)0x5678;
        CHECK(AccRoute_FallPredatorFinalLift("derelict", 1, 85, &out, &control, &platform) == 0 &&
              AccRoute_FallPredatorFinalLift("fall", 0, 85, &out, &control, &platform) == 0 &&
              AccRoute_FallPredatorFinalLift("fall", 1, 87, &out, &control, &platform) == 0 &&
              out.vx == -1 && out.vy == -2 && out.vz == -3 &&
              control == (STRATEGYBLOCK *)0x1234 && platform == (STRATEGYBLOCK *)0x5678,
              "final-lift matcher fails closed outside exact level, species, or source-room scope");
        requests[0] = 8;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "final-lift edge requires the request low bit");
        requests[0] = 1; targets[0] = &obj;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "unlinked switch does not match the shaft");
        targets[0] = &finalPlatform; finalLiftData.downHeight += 1000;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "unrelated platform limits do not match the shaft signature");
        finalLiftData.downHeight = 25226; finalPlatformDyn.Position.vx += 1000;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "unrelated platform position does not match the shaft signature");
        finalPlatformDyn.Position.vx = 187235;
        binary[0].security_clerance = 1;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "disabled lift does not guide to an ineligible switch");
        binary[0].security_clerance = 0;
        duplicatePlatform = finalPlatform; duplicatePlatformDyn = finalPlatformDyn;
        duplicateLiftData = finalLiftData; duplicatePlatform.SBdataptr = &duplicateLiftData;
        duplicatePlatform.DynPtr = &duplicatePlatformDyn;
        ActiveStBlockList[7] = &duplicatePlatform; NumActiveStBlocks = 8;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "ambiguous duplicate platform signatures fail closed");
        ActiveStBlockList[7] = NULL; NumActiveStBlocks = 7;
        finalLiftData.Enabled = 1; binary[0].state = 1;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 2 &&
              control == NULL && platform == &finalPlatform && out.vx == 187235 && out.vy == -2076 && out.vz == 70510,
              "enabled linked lift is reported as a platform handoff, not a switch target");
        finalLiftData.Enabled = 0; binary[0].state = 0; binary[0].num_targets = 1; binary[0].bs_targets = NULL;
        CHECK(AccRoute_FallPredatorFinalLift("fall", 1, 85, &out, &control, &platform) == 0,
              "malformed missing target list fails closed");
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
