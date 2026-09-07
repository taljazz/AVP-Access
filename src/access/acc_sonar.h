/* AVP Access: on-demand sonar sweep of the space ahead of the player.
 *
 * The motion tracker answers "what is moving near me"; this answers "what shape
 * is the room". It casts a fan of rays through the engine's own line-of-sight
 * routine, then reports the *shape* of the space -- corridor, dead end, opening
 * to one side -- rather than a bare list of wall distances, because knowing you
 * are in a corridor is what you act on.
 *
 * Nine rays analyse the space, but only three things are played: one ping per
 * sector, left then ahead then right, half a second apart. A wall is pitched by
 * distance using the same three tracker tones the player already knows -- near
 * is high, far is low -- while an opening gets the tracker's click, so "you can
 * walk this way" never sounds like a distant wall.
 */
#ifndef ACC_SONAR_H
#define ACC_SONAR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vectorch;

/* 9 rays over 180 degrees: -90 to +90 in 22.5 degree steps. Odd so that one ray
   points straight ahead, which is the direction that matters most. */
#define ACC_SONAR_RAYS 9

/* Millimetres. Room scale rather than the tracker's 30 m: beyond this a sweep
   stops describing a space and starts describing the level. */
#define ACC_SONAR_RANGE 8000

typedef struct {
    int hit;        /* zero when the ray reached full range without striking */
    int distance;   /* millimetres from the player */
    int x, z;       /* world position of the surface, for the spatial cue */
} ACC_SONAR_SAMPLE;

/* Pure: turn a set of samples into the spoken summary. Separated from the
   engine so it can be checked without a game. Returns zero if nothing could be
   said. */
int AccSonar_Format(const ACC_SONAR_SAMPLE *samples, int count,
                    char *text, size_t size);

/* Pure: the bearing of ray `index` in engine units (4096 per turn), relative to
   the player's facing. Negative is left. */
int AccSonar_RayBearing(int index);

/* Casts the fan, schedules the tones, and speaks the summary. Requires the
   player's world position and heading. `nowMs` is the same clock Update uses. */
void AccSonar_Request(const struct vectorch *player, int yaw, unsigned int nowMs);

/* Plays whichever scheduled tones are now due. `nowMs` is passed in rather than
   read here so tests can drive the schedule deterministically. */
void AccSonar_Update(unsigned int nowMs);

/* Drops any sweep in progress -- level change, death, menus. */
void AccSonar_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ACC_SONAR_H */
