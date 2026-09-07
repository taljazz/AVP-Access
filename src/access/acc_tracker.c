/* AVP Access: use the game's existing tracker information, not a second scan. */
#include "3dc.h"
#include "psnd.h"
#include "psndplat.h"
#include "acc_tracker.h"
#include "acc_speech.h"
#include "acc_pad.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;

static ACC_TRACKER_CONTACT Contacts[ACC_TRACKER_MAX_CONTACTS];
static int ContactCount, TrackerRange, Ready;

void AccTracker_Reset(void)
{
    Ready = 0;
    ContactCount = 0;
    TrackerRange = 0;
}

void AccTracker_SetContacts(const ACC_TRACKER_CONTACT *contacts, int count, int range)
{
    if (count < 0 || range <= 0 || (count && !contacts)) {
        AccTracker_Reset();
        return;
    }
    if (count > ACC_TRACKER_MAX_CONTACTS) count = ACC_TRACKER_MAX_CONTACTS;
    if (count) memcpy(Contacts, contacts, (size_t)count * sizeof(*contacts));
    ContactCount = count;
    TrackerRange = range;
    Ready = 1;
}

int AccTracker_Format(const struct vectorch *player, int yaw, char *text, size_t size)
{
    const double pi = 3.14159265358979323846;
    double angle, sinYaw, cosYaw, nearest, right = 0.0, forward = 0.0;
    int i, found = 0, written;
    if (!text || !size) return 0;
    text[0] = 0;
    if (!player || !Ready) {
        written = snprintf(text, size, "Motion tracker unavailable.");
    } else {
        angle = (yaw % 4096) * (2.0 * pi / 4096.0);
        sinYaw = sin(angle);
        cosYaw = cos(angle);
        nearest = (double)TrackerRange;
        for (i = 0; i < ContactCount; ++i) {
            /* Convert before subtracting to avoid overflowing large coordinates. */
            double dx = (double)Contacts[i].x - player->vx;
            double dz = (double)Contacts[i].z - player->vz;
            double front = dx * sinYaw + dz * cosYaw;
            double ax = fabs(dx), az = fabs(dz);
            /* Match HUD Fast2dMagnitude, including integer division. Its range
               admits diagonal contacts that Euclidean distance would reject. */
            double distance = ax > az ? ax + floor(az / 3.0) : az + floor(ax / 3.0);
            /* Tracker coverage is the forward half-plane. Allow floating-point
               roundoff at exactly left/right, and retain first contact on ties. */
            if (front < -0.0000001 || distance >= nearest) continue;
            nearest = distance;
            right = dx * cosYaw - dz * sinYaw;
            forward = front;
            found = 1;
        }
        if (!found) {
            written = snprintf(text, size, "No tracker contacts ahead.");
        } else {
            int clock = (int)floor(atan2(right, forward) * 6.0 / pi + 0.5);
            int meters = (int)floor(nearest / 1000.0 + 0.5);
            clock = (clock + 12) % 12;
            if (!clock) clock = 12;
            if (nearest < 1000.0)
                written = snprintf(text, size, "Nearest tracker contact at %d o'clock, within 1 meter.", clock);
            else
                written = snprintf(text, size, "Nearest tracker contact at %d o'clock, about %d %s.",
                                   clock, meters, meters == 1 ? "meter" : "meters");
        }
    }
    text[size - 1] = 0;
    return written >= 0 && (size_t)written < size;
}

void AccTracker_Announce(const struct vectorch *player, int yaw)
{
    char text[192];
    if (!AccTracker_Format(player, yaw, text, sizeof(text))) return;
    if (AccPadTrace) {
        fprintf(stderr, "ACCTRACKER: speech=%d %s\n", AccSpeech_IsAvailable(), text);
        fflush(stderr);
    }
    AccSpeech_Say(text, 1);
}

void AccTracker_PlayContact(int sound, const struct vectorch *position,
                           int range, int *handle, int volume)
{
    SOUND3DDATA data;
    int spatialVolume;
    if (!position || !Global_VDB_Ptr || range <= 0 || range > INT_MAX / 3) {
        /* Keep the old cue when a spatial listener is unavailable. */
        Sound_Play((SOUNDINDEX)sound, "ev", handle, volume);
        return;
    }
    memset(&data, 0, sizeof(data));
    data.position = *position;
    /* The visual tracker supplies horizontal bearing, not contact elevation. */
    data.position.vy = Global_VDB_Ptr->VDB_World.vy;
    data.inner_range = range * 2;
    data.outer_range = range * 3;
    if (volume < VOLUME_MIN) volume = VOLUME_MIN;
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;
    /* 2D playback applies this scaling in the platform layer, whereas 3D does
       not. Apply it once here so changing direction does not raise loudness. */
    spatialVolume = (volume * VOLUME_PLAT2DSCALE) >> 7;
    /* 'm' prevents this new 3D feedback source from altering Marine AI hearing. */
    Sound_Play((SOUNDINDEX)sound, "nevm", &data, handle, spatialVolume);
    if (AccPadTrace) {
        fprintf(stderr, "ACCTRACKER: cue=%d contact=(%d,%d) volume=%d\n",
                sound, position->vx, position->vz, spatialVolume);
        fflush(stderr);
    }
}
