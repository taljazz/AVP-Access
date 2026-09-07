/* AVP Access: on-demand sonar sweep -- see acc_sonar.h. */
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "psnd.h"
#include "psndproj.h"
#include "los.h"

#include "acc_sonar.h"
#include "acc_tracker.h"
#include "acc_speech.h"
#include "acc_pad.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern DISPLAYBLOCK *Player;
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;

/* Milliseconds between pings. Deliberately slow: the point of a ping is that
   you can place it, and tones half a second apart are easy to separate. Only
   the notable features are played, so a sweep is still over in about a second
   rather than the four-plus a ping per ray would cost. */
#define ACC_SONAR_STEP_MS 500

/* Distance bands for the three tracker pitches, in millimetres. */
#define ACC_SONAR_NEAR 2000
#define ACC_SONAR_MID  5000

/* Sector boundaries, as ray indices. Rays 0-2 are left, 3-5 ahead, 6-8 right. */
#define ACC_SONAR_AHEAD_FIRST 3
#define ACC_SONAR_AHEAD_LAST  5

/* One ping per sector: left, ahead, right. */
#define ACC_SONAR_CUES 3

typedef struct {
    int          pending;
    unsigned int dueMs;
    int          sound;
    VECTORCH     position;
} ACC_SONAR_CUE;

static ACC_SONAR_CUE Cues[ACC_SONAR_CUES];
static int CueHandle = SOUND_NOACTIVEINDEX;

int AccSonar_RayBearing(int index)
{
    /* 4096 units per turn, so 22.5 degrees is 256. Ray 4 is dead ahead. */
    return (index - (ACC_SONAR_RAYS / 2)) * 256;
}

/* ------------------------------------------------------------- sectors -- */

/* A sector is open when any ray in it reached full range without striking
   anything: that is a direction you could walk. It is walled when a ray did
   strike, and `nearest` is the closest such ray. Both can be true at once --
   an opening beside a wall is exactly what a doorway looks like. */
static void SummariseSector(const ACC_SONAR_SAMPLE *samples, int count,
                            int first, int last, int *open, int *nearest)
{
    int i;

    *open = 0;
    *nearest = -1;

    for (i = first; i <= last && i < count; i++) {
        if (i < 0) continue;
        if (!samples[i].hit) { *open = 1; continue; }
        if (*nearest < 0 || samples[i].distance < samples[*nearest].distance)
            *nearest = i;
    }
}

/* Names the space from which directions are passable. All eight combinations
   are covered explicitly -- a sweep that cannot describe where it is would be
   worse than one that says nothing. */
static const char *ShapeWord(int aheadOpen, int leftOpen, int rightOpen)
{
    if (aheadOpen && !leftOpen && !rightOpen) return "Corridor ahead";
    if (aheadOpen && leftOpen && rightOpen)   return "Open space";
    if (aheadOpen && leftOpen)                return "Corridor ahead, opening left";
    if (aheadOpen && rightOpen)               return "Corridor ahead, opening right";
    if (!aheadOpen && !leftOpen && !rightOpen) return "Dead end";
    if (leftOpen && rightOpen)                return "Wall ahead, openings left and right";
    if (leftOpen)                             return "Wall ahead, opening left";
    return "Wall ahead, opening right";
}

/* ------------------------------------------------------------- wording -- */

/* Rounds to whole metres, but never reports zero: a surface you are pressed
   against is still there, and "0 metres" would sound like a fault. */
static void DescribeDistance(int mm, char *out, size_t size)
{
    if (mm < 1000) {
        snprintf(out, size, "less than a metre");
        return;
    }
    snprintf(out, size, "%d metres", (mm + 500) / 1000);
}

int AccSonar_Format(const ACC_SONAR_SAMPLE *samples, int count,
                    char *text, size_t size)
{
    int aheadOpen, leftOpen, rightOpen;
    int aheadWall, leftWall, rightWall;
    char detail[160], one[48];
    int walls = 0;

    if (!samples || !text || size == 0 || count <= 0) return 0;

    SummariseSector(samples, count, ACC_SONAR_AHEAD_FIRST, ACC_SONAR_AHEAD_LAST,
                    &aheadOpen, &aheadWall);
    SummariseSector(samples, count, 0, ACC_SONAR_AHEAD_FIRST - 1,
                    &leftOpen, &leftWall);
    SummariseSector(samples, count, ACC_SONAR_AHEAD_LAST + 1, count - 1,
                    &rightOpen, &rightWall);

    /* Distances follow the shape, so the player hears what the space is before
       hearing the numbers that qualify it. */
    detail[0] = 0;
    if (aheadWall >= 0) {
        DescribeDistance(samples[aheadWall].distance, one, sizeof(one));
        snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                 "%s%s ahead", walls++ ? ", " : "", one);
    }
    if (leftWall >= 0) {
        DescribeDistance(samples[leftWall].distance, one, sizeof(one));
        snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                 "%s%s left", walls++ ? ", " : "", one);
    }
    if (rightWall >= 0) {
        DescribeDistance(samples[rightWall].distance, one, sizeof(one));
        snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                 "%s%s right", walls++ ? ", " : "", one);
    }

    if (walls == 0)
        snprintf(text, size, "%s.", ShapeWord(aheadOpen, leftOpen, rightOpen));
    else
        snprintf(text, size, "%s. %s %s.",
                 ShapeWord(aheadOpen, leftOpen, rightOpen),
                 walls > 1 ? "Walls" : "Wall", detail);

    return 1;
}

/* ------------------------------------------------------------ scheduling -- */

void AccSonar_Reset(void)
{
    int i;

    if (CueHandle != SOUND_NOACTIVEINDEX) {
        Sound_Stop(CueHandle);
        CueHandle = SOUND_NOACTIVEINDEX;
    }
    for (i = 0; i < ACC_SONAR_CUES; i++) Cues[i].pending = 0;
}

static int SoundForDistance(int mm)
{
    if (mm < ACC_SONAR_NEAR) return SID_TRACKER_WHEEP_HIGH;
    if (mm < ACC_SONAR_MID)  return SID_TRACKER_WHEEP;
    return SID_TRACKER_WHEEP_LOW;
}

void AccSonar_Update(unsigned int nowMs)
{
    int i;

    for (i = 0; i < ACC_SONAR_CUES; i++) {
        if (!Cues[i].pending) continue;
        /* Unsigned difference, so the wrap of a 32-bit millisecond clock does
           not strand a cue for 49 days. */
        if ((int)(nowMs - Cues[i].dueMs) < 0) continue;

        Cues[i].pending = 0;
        AccTracker_PlayContact(Cues[i].sound, &Cues[i].position,
                               ACC_SONAR_RANGE, &CueHandle, VOLUME_MAX);
    }
}

/* ---------------------------------------------------------------- sweep -- */

/* Unit direction for a bearing in engine units. Zero faces +Z, a quarter turn
   (1024) faces +X. */
static void BearingToVector(int bearing, VECTORCH *out)
{
    double radians = (bearing % 4096) * (2.0 * 3.14159265358979323846 / 4096.0);

    out->vx = (int)(sin(radians) * ONE_FIXED);
    out->vy = 0;
    out->vz = (int)(cos(radians) * ONE_FIXED);
}

/* One ray. Returns non-zero when a surface was struck inside range. */
static int CastRay(const VECTORCH *origin, int bearing, ACC_SONAR_SAMPLE *out)
{
    VECTORCH direction, from;

    BearingToVector(bearing, &direction);

    /* FindPolygonInLineOfSight is documented to modify the vectors it is given,
       so both are throwaway copies. */
    from = *origin;

    LOS_ObjectHitPtr = (DISPLAYBLOCK *)0;
    LOS_Lambda = ACC_SONAR_RANGE;
    FindPolygonInLineOfSight(&direction, &from, 0, Player);

    memset(out, 0, sizeof(*out));

    /* Test the distance rather than LOS_ObjectHitPtr: world geometry shortens
       the ray without ever setting an object pointer, and walls are the whole
       point of this sweep. */
    if (LOS_Lambda >= ACC_SONAR_RANGE) return 0;

    out->hit = 1;
    out->distance = LOS_Lambda;
    out->x = LOS_Point.vx;
    out->z = LOS_Point.vz;
    return 1;
}

/* Schedules one sector's ping. A wall is pitched by distance; an opening gets
   the tracker's click instead, so "you can walk this way" never sounds like a
   distant wall. */
static void ScheduleSector(int slot, const ACC_SONAR_SAMPLE *samples,
                           int nearest, int open, int bearing,
                           const VECTORCH *origin, int yaw, unsigned int dueMs)
{
    VECTORCH direction;

    if (nearest < 0 && !open) return;

    Cues[slot].pending = 1;
    Cues[slot].dueMs = dueMs;

    if (nearest >= 0) {
        Cues[slot].sound = SoundForDistance(samples[nearest].distance);
        Cues[slot].position.vx = samples[nearest].x;
        Cues[slot].position.vz = samples[nearest].z;
    } else {
        /* Place the opening's click out at the edge of range along the sector,
           so it is heard in the direction you could travel. */
        BearingToVector(yaw + bearing, &direction);
        Cues[slot].sound = SID_TRACKER_CLICK;
        Cues[slot].position.vx = origin->vx + (int)(((double)direction.vx / ONE_FIXED) * ACC_SONAR_RANGE);
        Cues[slot].position.vz = origin->vz + (int)(((double)direction.vz / ONE_FIXED) * ACC_SONAR_RANGE);
    }
    Cues[slot].position.vy = origin->vy;
}

void AccSonar_Request(const struct vectorch *player, int yaw, unsigned int nowMs)
{
    ACC_SONAR_SAMPLE samples[ACC_SONAR_RAYS];
    VECTORCH origin;
    char text[224];
    int aheadOpen, leftOpen, rightOpen;
    int aheadWall, leftWall, rightWall;
    int i;

    if (!player) return;

    AccSonar_Reset();

    origin = *(const VECTORCH *)player;
    /* Cast at eye height so the sweep describes walls rather than the floor or
       the step the player happens to be standing on. */
    if (Global_VDB_Ptr) origin.vy = Global_VDB_Ptr->VDB_World.vy;

    for (i = 0; i < ACC_SONAR_RAYS; i++)
        CastRay(&origin, yaw + AccSonar_RayBearing(i), &samples[i]);

    SummariseSector(samples, ACC_SONAR_RAYS, 0, ACC_SONAR_AHEAD_FIRST - 1,
                    &leftOpen, &leftWall);
    SummariseSector(samples, ACC_SONAR_RAYS, ACC_SONAR_AHEAD_FIRST,
                    ACC_SONAR_AHEAD_LAST, &aheadOpen, &aheadWall);
    SummariseSector(samples, ACC_SONAR_RAYS, ACC_SONAR_AHEAD_LAST + 1,
                    ACC_SONAR_RAYS - 1, &rightOpen, &rightWall);

    /* Left, ahead, right, so the sweep is still heard as a movement across the
       space even though only three things are played. */
    ScheduleSector(0, samples, leftWall,  leftOpen,  AccSonar_RayBearing(1),
                   &origin, yaw, nowMs);
    ScheduleSector(1, samples, aheadWall, aheadOpen, AccSonar_RayBearing(4),
                   &origin, yaw, nowMs + ACC_SONAR_STEP_MS);
    ScheduleSector(2, samples, rightWall, rightOpen, AccSonar_RayBearing(7),
                   &origin, yaw, nowMs + 2 * ACC_SONAR_STEP_MS);

    if (AccSonar_Format(samples, ACC_SONAR_RAYS, text, sizeof(text)))
        AccSpeech_Say(text, 1);

    if (AccPadTrace) {
        fprintf(stderr,
                "ACCSONAR: yaw=%d open(l/a/r)=%d/%d/%d wall(l/a/r)=%d/%d/%d text=%s\n",
                yaw, leftOpen, aheadOpen, rightOpen,
                leftWall >= 0 ? samples[leftWall].distance : -1,
                aheadWall >= 0 ? samples[aheadWall].distance : -1,
                rightWall >= 0 ? samples[rightWall].distance : -1, text);
        fflush(stderr);
    }
}
