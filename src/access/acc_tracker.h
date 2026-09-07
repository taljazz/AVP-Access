/* AVP Access: directional audio and speech for existing motion-tracker blips. */
#ifndef ACC_TRACKER_H
#define ACC_TRACKER_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

struct vectorch;
#define ACC_TRACKER_MAX_CONTACTS 10
typedef struct { int x, z; } ACC_TRACKER_CONTACT;

/* Copy values from the HUD, never retain pointers into an entity/world. */
void AccTracker_Reset(void);
void AccTracker_SetContacts(const ACC_TRACKER_CONTACT *contacts, int count, int range);
int AccTracker_Format(const struct vectorch *player, int yaw, char *text, size_t size);
void AccTracker_Announce(const struct vectorch *player, int yaw);
void AccTracker_PlayContact(int sound, const struct vectorch *position,
                           int range, int *handle, int volume);

/* Engine-side hook in hud.c: stop the cue and clear scan/blip state on reset. */
void AccTracker_ResetHUD(void);

#ifdef __cplusplus
}
#endif
#endif
