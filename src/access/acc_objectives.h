/* AVP Access: spoken mission objectives.
 *
 * AvP has no objectives screen. Each objective's text arrives once, as an
 * on-screen message, and is then gone -- so a player who missed it, or who
 * cannot read it back, has no way to find out what the mission is. This reads
 * the engine's own objective list on demand.
 *
 * One objective per press, cycling, rather than reciting all of them: a level
 * can carry several and a single announcement would be too long to follow.
 * Hidden objectives are excluded by the bridge in missions.cpp -- the level
 * hides them on purpose.
 */
#ifndef ACC_OBJECTIVES_H
#define ACC_OBJECTIVES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented in missions.cpp, where the objective list lives. Index counts
   visible objectives only. */
int AccObjectives_Count(void);
int AccObjectives_Get(int index, int *achieved, int *achievable, int *stringID);

/* Pure: the sentence for one objective. Separated from the engine so the
   wording can be checked without a game. Returns zero if nothing could be said. */
int AccObjectives_Format(int index, int count, int achieved, int achievable,
                         const char *description, char *text, size_t size);

/* Speaks the next objective and advances the cycle. */
void AccObjectives_Announce(void);

/* Returns to the first objective -- level change, or a new mission. */
void AccObjectives_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ACC_OBJECTIVES_H */
