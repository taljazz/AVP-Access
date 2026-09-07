/* Tests the actual acc_sonar.c against real engine types. The line-of-sight
 * routine, speech and Sound_Play are captured at their public boundaries, so a
 * synthetic room can be described ray by ray without a game, a level, an OpenAL
 * context or a speech backend.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "fixer.h"
#include "3dc.h"
#include "psnd.h"
#include "acc_speech.h"
#include "acc_sonar.h"

/* --- engine globals the module reaches for ----------------------------- */

DISPLAYBLOCK *Player;
VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
int AccPadTrace;

VECTORCH LOS_Point;
int LOS_Lambda;
DISPLAYBLOCK *LOS_ObjectHitPtr;
VECTORCH LOS_ObjectNormal;

/* --- the synthetic room ------------------------------------------------ */

/* Distance the mocked raycast should report for each ray, in millimetres.
   ACC_SONAR_RANGE or more means the ray found nothing. */
static int rayDistance[ACC_SONAR_RAYS];
static int rayIndex;
static int rayCalls;
static int lastDirX[ACC_SONAR_RAYS], lastDirZ[ACC_SONAR_RAYS];

void FindPolygonInLineOfSight(VECTORCH *direction, VECTORCH *position,
                              int useOnScreenBlockList, DISPLAYBLOCK *ignore)
{
    int d;

    (void)position; (void)useOnScreenBlockList; (void)ignore;

    if (rayIndex < ACC_SONAR_RAYS) {
        lastDirX[rayIndex] = direction->vx;
        lastDirZ[rayIndex] = direction->vz;
    }

    d = (rayIndex < ACC_SONAR_RAYS) ? rayDistance[rayIndex] : ACC_SONAR_RANGE;
    ++rayIndex;
    ++rayCalls;

    if (d >= ACC_SONAR_RANGE) return;      /* leave LOS_Lambda at full range */

    LOS_Lambda = d;
    LOS_ObjectHitPtr = NULL;
    /* Put the surface somewhere distinguishable so cue positions can be
       checked; the exact geometry does not matter to the module. */
    LOS_Point.vx = 1000 + rayIndex;
    LOS_Point.vy = 0;
    LOS_Point.vz = 2000 + rayIndex;
}

/* --- captured outputs -------------------------------------------------- */

static char spoken[512];
static int speech_calls, speech_interrupt;

int AccSpeech_IsAvailable(void) { return 1; }

void AccSpeech_Say(const char *message, int interrupt)
{
    ++speech_calls;
    speech_interrupt = interrupt;
    strncpy(spoken, message ? message : "", sizeof(spoken) - 1);
    spoken[sizeof(spoken) - 1] = 0;
}

static int cue_calls;
static int cue_sound[ACC_SONAR_RAYS];
static int cue_x[ACC_SONAR_RAYS];
static int stop_calls;

void AccTracker_PlayContact(int sound, const struct vectorch *position,
                            int range, int *handle, int volume)
{
    (void)range; (void)volume;
    /* Sound_Play fills in the caller's handle through its 'e' option; mirror
       that, or a reset has nothing to stop and the check proves nothing. */
    if (handle) *handle = 42;
    if (cue_calls < ACC_SONAR_RAYS) {
        cue_sound[cue_calls] = sound;
        cue_x[cue_calls] = position ? position->vx : 0;
    }
    ++cue_calls;
}

void Sound_Stop(int handle) { (void)handle; ++stop_calls; }

/* --- harness ----------------------------------------------------------- */

static int assertions, failures;

static void check(int condition, const char *what)
{
    ++assertions;
    if (condition) {
        printf("PASS: %s\n", what);
    } else {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

static void reset_world(int fill)
{
    int i;
    for (i = 0; i < ACC_SONAR_RAYS; i++) rayDistance[i] = fill;
    AccSonar_Reset();          /* clear the module before counting */
    rayIndex = rayCalls = 0;
    cue_calls = stop_calls = speech_calls = 0;
    spoken[0] = 0;
    rayIndex = 0;   /* Reset may not cast, but keep the counter honest */
}

static void sweep(unsigned int now)
{
    VECTORCH player;
    memset(&player, 0, sizeof(player));
    rayIndex = 0;
    AccSonar_Request(&player, 0, now);
}

/* --- cases ------------------------------------------------------------- */

static void test_bearings(void)
{
    check(AccSonar_RayBearing(4) == 0, "middle ray points straight ahead");
    check(AccSonar_RayBearing(0) == -1024, "first ray is 90 degrees left");
    check(AccSonar_RayBearing(ACC_SONAR_RAYS - 1) == 1024, "last ray is 90 degrees right");
    check(AccSonar_RayBearing(5) - AccSonar_RayBearing(4) == 256, "rays are 22.5 degrees apart");
}

static void test_open_space(void)
{
    reset_world(ACC_SONAR_RANGE);
    sweep(0);

    check(rayCalls == ACC_SONAR_RAYS, "one ray cast per fan position");
    check(strstr(spoken, "Open space") != NULL, "nothing struck reads as open space");
    check(strstr(spoken, "Wall") == NULL, "open space reports no wall distances");
    check(speech_interrupt == 1, "the summary interrupts older speech");
}

static void test_corridor(void)
{
    reset_world(ACC_SONAR_RANGE);
    /* Walls to both sides, nothing ahead: the shape of a corridor. */
    rayDistance[0] = rayDistance[1] = rayDistance[2] = 2000;
    rayDistance[6] = rayDistance[7] = rayDistance[8] = 2000;
    sweep(0);

    check(strncmp(spoken, "Corridor ahead.", 15) == 0,
          "walls both sides and open ahead is named a corridor");
    check(strstr(spoken, "2 metres left") != NULL, "corridor reports the left wall");
    check(strstr(spoken, "2 metres right") != NULL, "corridor reports the right wall");
    check(strstr(spoken, "Walls") != NULL, "two walls are announced in the plural");
}

static void test_dead_end(void)
{
    reset_world(1500);
    sweep(0);

    check(strncmp(spoken, "Dead end.", 9) == 0,
          "no opening in any sector is a dead end");
}

static void test_doorway(void)
{
    reset_world(ACC_SONAR_RANGE);
    /* Wall ahead and right, a gap on the left. */
    rayDistance[3] = rayDistance[4] = rayDistance[5] = 3000;
    rayDistance[6] = rayDistance[7] = rayDistance[8] = 2000;
    sweep(0);

    check(strstr(spoken, "Wall ahead, opening left") != NULL,
          "a gap beside a wall is named as an opening");
}

static void test_side_opening(void)
{
    reset_world(ACC_SONAR_RANGE);
    /* Open ahead and to the left, wall on the right: a junction. */
    rayDistance[6] = rayDistance[7] = rayDistance[8] = 2000;
    sweep(0);

    check(strstr(spoken, "Corridor ahead, opening left") != NULL,
          "an open side off a corridor is named");
}

static void test_wall_ahead(void)
{
    reset_world(ACC_SONAR_RANGE);
    rayDistance[3] = 4000;
    rayDistance[4] = 3000;
    rayDistance[5] = 4000;
    sweep(0);

    check(strstr(spoken, "3 metres ahead") != NULL,
          "nearest hit in a sector is the one reported");
    check(strstr(spoken, "Wall ") != NULL, "a single wall is announced in the singular");
}

static void test_sub_metre(void)
{
    reset_world(ACC_SONAR_RANGE);
    rayDistance[4] = 400;
    sweep(0);

    check(strstr(spoken, "less than a metre") != NULL,
          "a surface under a metre is not rounded to zero");
}

static void test_rounding(void)
{
    reset_world(ACC_SONAR_RANGE);
    rayDistance[4] = 2500;
    sweep(0);
    check(strstr(spoken, "3 metres ahead") != NULL, "distance rounds to nearest metre");

    reset_world(ACC_SONAR_RANGE);
    rayDistance[4] = 2400;
    sweep(0);
    check(strstr(spoken, "2 metres ahead") != NULL, "distance rounds down below the half");
}

static void test_distance_bands(void)
{
    reset_world(ACC_SONAR_RANGE);
    rayDistance[0] = rayDistance[1] = rayDistance[2] = 1000;   /* near, left  */
    rayDistance[3] = rayDistance[4] = rayDistance[5] = 3000;   /* mid, ahead  */
    rayDistance[6] = rayDistance[7] = rayDistance[8] = 7000;   /* far, right  */
    sweep(0);
    AccSonar_Update(10000);

    check(cue_calls == 3, "one ping per sector");
    check(cue_sound[0] == SID_TRACKER_WHEEP_HIGH, "a near wall uses the high tone");
    check(cue_sound[1] == SID_TRACKER_WHEEP, "a mid wall uses the middle tone");
    check(cue_sound[2] == SID_TRACKER_WHEEP_LOW, "a far wall uses the low tone");
}

static void test_opening_click(void)
{
    reset_world(ACC_SONAR_RANGE);
    rayDistance[0] = rayDistance[1] = rayDistance[2] = 2000;  /* wall left */
    sweep(0);
    AccSonar_Update(10000);

    check(cue_calls == 3, "an opening is pinged as well as a wall");
    check(cue_sound[0] != SID_TRACKER_CLICK, "the walled sector keeps a wall tone");
    check(cue_sound[1] == SID_TRACKER_CLICK, "an opening ahead is a click, not a tone");
    check(cue_sound[2] == SID_TRACKER_CLICK, "an opening to the side is a click");
}

static void test_schedule(void)
{
    reset_world(2000);       /* walls everywhere, so all three sectors ping */
    sweep(1000);

    AccSonar_Update(1000);
    check(cue_calls == 1, "the left ping plays first");

    AccSonar_Update(1000 + 500);
    check(cue_calls == 2, "the ahead ping follows half a second later");

    AccSonar_Update(1000 + 1000);
    check(cue_calls == 3, "the right ping completes the sweep");

    AccSonar_Update(1000 + 9000);
    check(cue_calls == 3, "no ping plays twice");
}

static void test_reset(void)
{
    reset_world(2000);
    sweep(0);

    /* Let the first tone actually play, so there is a sounding cue to stop.
       Resetting before anything played would prove nothing. */
    AccSonar_Update(0);
    check(cue_calls == 1, "a tone played before the reset");

    AccSonar_Reset();
    AccSonar_Update(100000);

    check(cue_calls == 1, "reset cancels the tones still pending");
    check(stop_calls >= 1, "reset stops the cue that is still sounding");
}

static void test_null_player(void)
{
    reset_world(2000);
    rayIndex = 0;
    AccSonar_Request(NULL, 0, 0);

    check(rayCalls == 0, "a missing player casts no rays");
    check(speech_calls == 0, "a missing player says nothing");
}

int main(int argc, char **argv)
{
    const char *which = (argc > 1) ? argv[1] : "";

    if (!strcmp(which, "bearings"))        test_bearings();
    else if (!strcmp(which, "open"))       test_open_space();
    else if (!strcmp(which, "dead_end"))   test_dead_end();
    else if (!strcmp(which, "doorway"))    test_doorway();
    else if (!strcmp(which, "side"))       test_side_opening();
    else if (!strcmp(which, "click"))      test_opening_click();
    else if (!strcmp(which, "corridor"))   test_corridor();
    else if (!strcmp(which, "wall_ahead")) test_wall_ahead();
    else if (!strcmp(which, "sub_metre"))  test_sub_metre();
    else if (!strcmp(which, "rounding"))   test_rounding();
    else if (!strcmp(which, "bands"))      test_distance_bands();
    else if (!strcmp(which, "schedule"))   test_schedule();
    else if (!strcmp(which, "reset"))      test_reset();
    else if (!strcmp(which, "null"))       test_null_player();
    else {
        printf("unknown case: %s\n", which);
        return 2;
    }

    printf("%s: %d assertions, %d failed\n", which, assertions, failures);
    return failures ? 1 : 0;
}
