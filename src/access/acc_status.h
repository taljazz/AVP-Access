/* AVP Access: on-demand spoken Marine status. */
#ifndef ACC_STATUS_H
#define ACC_STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct player_status;

/* Returns 1 for a complete status line. Invalid/unavailable player state or an
   insufficient buffer returns 0. A nonempty buffer is always terminated. */
int AccStatus_FormatMarine(const struct player_status *player, char *text, size_t size);
void AccStatus_AnnounceMarine(const struct player_status *player);

#ifdef __cplusplus
}
#endif
#endif
