/* Tests the actual acc_tracker.c against real engine types. Speech and
 * Sound_Play are captured at their public boundaries; no game, device,
 * OpenAL context, or speech backend is opened.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "fixer.h"
#include "3dc.h"
#include "psnd.h"
#include "acc_speech.h"
#include "acc_tracker.h"

VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
int AccPadTrace;

static VIEWDESCRIPTORBLOCK view;
static VECTORCH player;
static char text[512], spoken[512];
static int assertions, failures;
static int speech_calls, last_interrupt;
static int sound_calls, sound_volume, sound_has_position, sound_unknown_option;
static SOUNDINDEX sound_id;
static SOUND3DDATA sound_data;
static int *sound_handle;
static char sound_format[32];

int AccSpeech_IsAvailable(void) { return 1; }

void AccSpeech_Say(const char *message, int interrupt)
{
    ++speech_calls;
    last_interrupt = interrupt;
    strncpy(spoken, message ? message : "", sizeof(spoken) - 1);
    spoken[sizeof(spoken) - 1] = 0;
}

void Sound_Play(SOUNDINDEX id, char *format, ...)
{
    va_list args;
    const char *option;
    ++sound_calls;
    sound_id = id;
    sound_volume = -999;
    sound_has_position = sound_unknown_option = 0;
    sound_handle = NULL;
    memset(&sound_data, 0xa5, sizeof(sound_data));
    strncpy(sound_format, format ? format : "", sizeof(sound_format) - 1);
    sound_format[sizeof(sound_format) - 1] = 0;
    va_start(args, format);
    for (option = sound_format; *option; ++option) {
        switch (*option) {
        case 'e': sound_handle = va_arg(args, int *); break;
        case 'n': {
            SOUND3DDATA *data = va_arg(args, SOUND3DDATA *);
            if (data) { sound_data = *data; sound_has_position = 1; }
            break;
        }
        case 'v': sound_volume = va_arg(args, int); break;
        case 'm': break;
        default: sound_unknown_option = 1; break;
        }
    }
    va_end(args);
    if (sound_handle) *sound_handle = 700 + sound_calls;
}

static void check(int condition, const char *description)
{
    ++assertions;
    printf("%s: %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) {
        ++failures;
        printf("  Status: %s\n", text);
        printf("  Sound: %s id=%d volume=%d positioned=%d\n", sound_format,
               (int)sound_id, sound_volume, sound_has_position);
    }
}

static int status_is(const char *expected, int yaw)
{
    return AccTracker_Format(&player, yaw, text, sizeof(text)) == 1 && !strcmp(text, expected);
}

static int contact_is(int clock_hour, int meters, int yaw)
{
    char expected[160];
    sprintf(expected, "Nearest tracker contact at %d o'clock, about %d meters.", clock_hour, meters);
    return status_is(expected, yaw);
}

static void fixture(void)
{
    memset(&view, 0, sizeof(view));
    memset(&player, 0, sizeof(player));
    memset(text, 0, sizeof(text));
    memset(spoken, 0, sizeof(spoken));
    memset(sound_format, 0, sizeof(sound_format));
    Global_VDB_Ptr = &view;
    view.VDB_World.vy = -3456;
    speech_calls = last_interrupt = sound_calls = AccPadTrace = 0;
    AccTracker_Reset();
}

static void availability(void)
{
    ACC_TRACKER_CONTACT contact = {0, 12000};
    check(status_is("Motion tracker unavailable.", 0), "reset tracker is unavailable before the HUD publishes contacts");
    AccTracker_SetContacts(NULL, 0, 30000);
    check(status_is("No tracker contacts ahead.", 0), "a valid empty HUD snapshot is ready and has no contacts");
    AccTracker_SetContacts(&contact, 1, 30000);
    check(contact_is(12, 12, 0), "a published HUD contact becomes available");
    check(AccTracker_Format(NULL, 0, text, sizeof(text)) == 1 && !strcmp(text, "Motion tracker unavailable."),
          "missing player coordinates produce an unavailable status");
    AccTracker_Reset();
    check(status_is("Motion tracker unavailable.", 0), "reset discards an earlier contact instead of reading stale level data");
    AccTracker_SetContacts(NULL, 1, 30000);
    check(status_is("Motion tracker unavailable.", 0), "positive count with no contact array is unavailable");
    AccTracker_SetContacts(&contact, -1, 30000);
    check(status_is("Motion tracker unavailable.", 0), "negative contact count is unavailable");
    AccTracker_SetContacts(&contact, 1, 0);
    check(status_is("Motion tracker unavailable.", 0), "zero tracker range is unavailable");
    AccTracker_SetContacts(&contact, 1, -1);
    check(status_is("Motion tracker unavailable.", 0), "negative tracker range is unavailable");
    AccTracker_SetContacts(&contact, 0, 30000);
    check(status_is("No tracker contacts ahead.", 0), "a nonnull empty snapshot is also a valid empty tracker");
}

static void snapshot(void)
{
    ACC_TRACKER_CONTACT contacts[ACC_TRACKER_MAX_CONTACTS + 1];
    int i;
    contacts[0].x = 9600; contacts[0].z = 7200;
    AccTracker_SetContacts(contacts, 1, 30000);
    contacts[0].x = -8000; contacts[0].z = 6000;
    check(contact_is(2, 12, 0), "tracker value-copies a HUD contact rather than retaining a mutable caller pointer");
    AccTracker_SetContacts(contacts, 1, 30000);
    check(contact_is(10, 10, 0), "the next snapshot replaces the previously copied contact");
    AccTracker_SetContacts(NULL, 0, 30000);
    check(status_is("No tracker contacts ahead.", 0), "an empty snapshot removes contacts from the previous sweep");
    for (i = 0; i < ACC_TRACKER_MAX_CONTACTS; ++i) { contacts[i].x = 0; contacts[i].z = 12000; }
    contacts[ACC_TRACKER_MAX_CONTACTS].x = 0;
    contacts[ACC_TRACKER_MAX_CONTACTS].z = 2000;
    AccTracker_SetContacts(contacts, ACC_TRACKER_MAX_CONTACTS + 1, 30000);
    check(contact_is(12, 12, 0), "counts above the HUD capacity are capped and do not expose an eleventh contact");
    check(ACC_TRACKER_MAX_CONTACTS == 10, "tracker cache capacity matches the HUD's ten visible blips");
}

static void bearings(void)
{
    static const struct { int x, z, hour, meters; } cases[] = {
        {-12000, 0, 9, 12}, {-9600, 7200, 10, 12}, {-6000, 10392, 11, 12},
        {0, 12000, 12, 12}, {6000, 10392, 1, 12}, {9600, 7200, 2, 12}, {12000, 0, 3, 12}
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ACC_TRACKER_CONTACT contact = {cases[i].x, cases[i].z};
        char description[160];
        AccTracker_SetContacts(&contact, 1, 30000);
        sprintf(description, "front contact (%d,%d) is described at %d o'clock and %d meters",
                cases[i].x, cases[i].z, cases[i].hour, cases[i].meters);
        check(contact_is(cases[i].hour, cases[i].meters, 0), description);
    }
}

static void rotations(void)
{
    static const struct { int x, z, yaw, hour; } cases[] = {
        {0, 12000, 0, 12}, {12000, 0, 1024, 12}, {0, -12000, 2048, 12}, {-12000, 0, 3072, 12},
        {0, -12000, 1024, 3}, {0, 12000, 1024, 9}, {-12000, 0, 2048, 3}, {12000, 0, 2048, 9},
        {0, 12000, 3072, 3}, {0, -12000, 3072, 9}, {0, 12000, 4096, 12}, {-12000, 0, -1024, 12},
        {0, 12000, 8192, 12}
    };
    size_t i;
    player.vx = 27000; player.vy = 999999; player.vz = -19000;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ACC_TRACKER_CONTACT contact = {player.vx + cases[i].x, player.vz + cases[i].z};
        char description[160];
        AccTracker_SetContacts(&contact, 1, 30000);
        sprintf(description, "yaw %d rotates relative contact (%d,%d) to %d o'clock including exact sideways contacts",
                cases[i].yaw, cases[i].x, cases[i].z, cases[i].hour);
        check(contact_is(cases[i].hour, 12, cases[i].yaw), description);
    }
    {
        ACC_TRACKER_CONTACT contact = {27000, -7000};
        AccTracker_SetContacts(&contact, 1, 30000);
        player.vz = -15000;
        check(contact_is(12, 8, 0), "distance is recomputed from current player position against the copied world contact");
    }
}

static void selection(void)
{
    ACC_TRACKER_CONTACT contacts[] = {{0, -1000}, {0, 35000}, {0, 18000}, {9600, 7200}};
    AccTracker_SetContacts(contacts, 4, 30000);
    check(contact_is(2, 12, 0), "nearest selection ignores a closer contact behind the player and one outside range");
    contacts[0].z = -1000; contacts[1].x = 0; contacts[1].z = 30000;
    AccTracker_SetContacts(contacts, 2, 30000);
    check(status_is("No tracker contacts ahead.", 0), "contacts behind or exactly at tracker range are excluded like the HUD");
    contacts[0].x = 0; contacts[0].z = 29999;
    AccTracker_SetContacts(contacts, 1, 30000);
    check(contact_is(12, 30, 0), "a contact just inside tracker range remains visible");
    contacts[0].x = 24000; contacts[0].z = 24000;
    AccTracker_SetContacts(contacts, 1, 30000);
    check(status_is("No tracker contacts ahead.", 0), "range uses radial distance rather than accepting the whole bounding square");
    contacts[0].x = 0; contacts[0].z = 29500;
    contacts[1].x = 22000; contacts[1].z = 22000;
    AccTracker_SetContacts(contacts, 2, 30000);
    check(contact_is(2, 29, 0), "HUD distance metric includes the diagonal contact and selects it ahead of a 29.5-meter straight contact");
    contacts[0].x = 6000; contacts[0].z = 8000;
    contacts[1].x = -6000; contacts[1].z = 8000;
    AccTracker_SetContacts(contacts, 2, 30000);
    check(contact_is(1, 10, 0), "equal-distance contacts keep the first HUD contact as a stable tie-breaker");
    contacts[0].x = -6000; contacts[1].x = 6000;
    AccTracker_SetContacts(contacts, 2, 30000);
    check(contact_is(11, 10, 0), "reordering tied contacts changes the winner to the new first contact");
    contacts[0].x = INT_MAX; contacts[0].z = INT_MAX;
    player.vx = INT_MIN; player.vz = INT_MIN;
    AccTracker_SetContacts(contacts, 1, 30000);
    check(status_is("No tracker contacts ahead.", 0), "extreme world coordinates cannot overflow into a false nearby contact");
}

static void nearby(void)
{
    ACC_TRACKER_CONTACT contact = {0, 0};
    AccTracker_SetContacts(&contact, 1, 30000);
    check(status_is("Nearest tracker contact at 12 o'clock, within 1 meter.", 0),
          "an overlapping contact has a safe bearing and is within one meter");
    contact.z = 999;
    AccTracker_SetContacts(&contact, 1, 30000);
    check(status_is("Nearest tracker contact at 12 o'clock, within 1 meter.", 0),
          "sub-meter distance is spoken as within one meter");
    contact.z = 12000;
    AccTracker_SetContacts(&contact, 1, 30000);
    check(contact_is(12, 12, 0), "world units are converted to meters for a known twelve-meter separation");
}

static void announcement(void)
{
    ACC_TRACKER_CONTACT contact = {9600, 7200};
    AccTracker_SetContacts(&contact, 1, 30000);
    check(contact_is(2, 12, 0) && speech_calls == 0, "formatting tracker status never speaks by itself");
    AccTracker_Announce(&player, 0);
    check(speech_calls == 1 && last_interrupt == 1 && !strcmp(spoken, text),
          "one requested announcement speaks the formatted contact once and interrupts prior speech");
    AccTracker_Announce(&player, 0);
    check(speech_calls == 2 && last_interrupt == 1 && !strcmp(spoken, text),
          "an unchanged tracker report can be requested and spoken again");
    AccTracker_Reset();
    AccTracker_Announce(&player, 0);
    check(speech_calls == 3 && last_interrupt == 1 && !strcmp(spoken, "Motion tracker unavailable."),
          "a request after reset reports unavailable rather than an old contact");
    AccTracker_SetContacts(NULL, 0, 30000);
    AccTracker_Announce(&player, 0);
    check(speech_calls == 4 && !strcmp(spoken, "No tracker contacts ahead."),
          "an empty ready tracker has an explicit spoken response");
    check(sound_calls == 0, "spoken requests do not manufacture extra tracker pings");
}

static void spatial_sound(void)
{
    static const SOUNDINDEX ids[] = {SID_TRACKER_WHEEP_HIGH, SID_TRACKER_WHEEP, SID_TRACKER_WHEEP_LOW};
    VECTORCH position = {12000, 76543, -9000};
    VECTORCH original = position;
    size_t i;
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        int handle = -1;
        AccTracker_PlayContact(ids[i], &position, 30000, &handle, 127);
        check(sound_calls == (int)i + 1 && sound_id == ids[i] && sound_handle == &handle && handle == 700 + sound_calls,
              "contact sound preserves its sample ID and the caller's external handle");
        check(!strcmp(sound_format, "nevm") && !sound_unknown_option && sound_has_position,
              "contact uses positional sound with an external handle, volume, and Marine-ignore flag");
        check(sound_data.position.vx == 12000 && sound_data.position.vz == -9000 &&
              sound_data.position.vy == -3456 && !memcmp(&position, &original, sizeof(position)),
              "sound copies contact X/Z, flattens height to the view, and preserves caller coordinates");
        check(sound_data.velocity.vx == 0 && sound_data.velocity.vy == 0 && sound_data.velocity.vz == 0 &&
              sound_data.inner_range == 60000 && sound_data.outer_range == 90000,
              "stationary ping has zero velocity and two/three times tracker range");
        check(sound_volume == 95, "full tracker volume retains the old effective two-dimensional loudness");
    }
    view.VDB_World.vy = 22222;
    AccTracker_PlayContact(SID_TRACKER_WHEEP, &position, 1000, NULL, 64);
    check(sound_handle == NULL && sound_data.position.vy == 22222 &&
          sound_data.inner_range == 2000 && sound_data.outer_range == 3000 && sound_volume == 48,
          "optional null sound handle and updated view/range/volume are passed correctly");
    check(speech_calls == 0, "positional tracker pings never trigger speech");
}

static void volumes(void)
{
    /* Golden outputs for every user volume 0..127, preserving the engine's
       original 2D 96/128 scale after switching these pings to 3D. */
    static const int expected[] = {
        0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9, 10, 11,
        12, 12, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 21, 22, 23,
        24, 24, 25, 26, 27, 27, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35,
        36, 36, 37, 38, 39, 39, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47,
        48, 48, 49, 50, 51, 51, 52, 53, 54, 54, 55, 56, 57, 57, 58, 59,
        60, 60, 61, 62, 63, 63, 64, 65, 66, 66, 67, 68, 69, 69, 70, 71,
        72, 72, 73, 74, 75, 75, 76, 77, 78, 78, 79, 80, 81, 81, 82, 83,
        84, 84, 85, 86, 87, 87, 88, 89, 90, 90, 91, 92, 93, 93, 94, 95
    };
    VECTORCH position = {0, 100000, 12000};
    int volume, handle = -1;
    for (volume = 0; volume <= 127; ++volume) {
        char description[128];
        AccTracker_PlayContact(SID_TRACKER_WHEEP, &position, 30000, &handle, volume);
        sprintf(description, "tracker volume %d produces calibrated positional volume %d", volume, expected[volume]);
        check(sound_volume == expected[volume] && !strcmp(sound_format, "nevm") && sound_handle == &handle,
              description);
    }
    check(sound_calls == 128, "each volume input emits exactly one contact sound");
}

static void sound_fallback(void)
{
    VECTORCH position = {2000, 5000, 12000};
    int handle = -1;
    int i;
    static const int invalid_ranges[] = {0, -1, INT_MAX / 3 + 1, INT_MAX};
    Global_VDB_Ptr = NULL;
    AccTracker_PlayContact(SID_TRACKER_WHEEP_HIGH, &position, 30000, &handle, 100);
    check(!strcmp(sound_format, "ev") && !sound_has_position && sound_id == SID_TRACKER_WHEEP_HIGH &&
          sound_handle == &handle && sound_volume == 100, "missing view falls back to the original 2D sound and unscaled volume");
    Global_VDB_Ptr = &view;
    AccTracker_PlayContact(SID_TRACKER_WHEEP_LOW, NULL, 30000, &handle, 64);
    check(!strcmp(sound_format, "ev") && !sound_has_position && sound_id == SID_TRACKER_WHEEP_LOW &&
          sound_handle == &handle && sound_volume == 64, "missing contact position preserves the original 2D behavior");
    for (i = 0; i < (int)(sizeof(invalid_ranges) / sizeof(invalid_ranges[0])); ++i) {
        AccTracker_PlayContact(SID_TRACKER_WHEEP, &position, invalid_ranges[i], &handle, 127);
        check(!strcmp(sound_format, "ev") && !sound_has_position && sound_volume == 127 && sound_handle == &handle,
              "invalid or multiplication-overflowing range falls back without corrupting volume or handle");
    }
    AccTracker_PlayContact(SID_TRACKER_WHEEP, &position, INT_MAX / 3, &handle, 127);
    check(!strcmp(sound_format, "nevm") && sound_has_position &&
          sound_data.inner_range == 1431655764 && sound_data.outer_range == 2147483646,
          "largest safe positional range multiplies without overflowing");
}

static void buffers(void)
{
    struct { unsigned char prefix[16]; char data[256]; unsigned char suffix[16]; } guarded;
    static const size_t sizes[] = {0, 1, 2, 16, 64, 256};
    ACC_TRACKER_CONTACT contact = {9600, 7200};
    size_t i, j;
    AccTracker_SetContacts(&contact, 1, 30000);
    check(AccTracker_Format(&player, 0, NULL, 0) == 0 && AccTracker_Format(&player, 0, NULL, 128) == 0,
          "null output buffers are rejected safely");
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        int guards_ok = 1, result;
        memset(&guarded, 0xa5, sizeof(guarded));
        result = AccTracker_Format(&player, 0, guarded.data, sizes[i]);
        for (j = 0; j < sizeof(guarded.prefix); ++j) if (guarded.prefix[j] != 0xa5) guards_ok = 0;
        for (j = sizes[i]; j < sizeof(guarded.data); ++j) if ((unsigned char)guarded.data[j] != 0xa5) guards_ok = 0;
        for (j = 0; j < sizeof(guarded.suffix); ++j) if (guarded.suffix[j] != 0xa5) guards_ok = 0;
        check(guards_ok, "formatting respects the exact buffer capacity and surrounding canaries");
        check(result == (sizes[i] >= 64), "formatter returns success only when the whole tracker sentence fits");
        if (sizes[i]) check(memchr(guarded.data, 0, sizes[i]) != NULL, "truncated tracker sentences remain terminated");
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: tracker_tests CASE\n"); return 2; }
    fixture();
    if (!strcmp(argv[1], "availability")) availability();
    else if (!strcmp(argv[1], "snapshot")) snapshot();
    else if (!strcmp(argv[1], "bearings")) bearings();
    else if (!strcmp(argv[1], "rotations")) rotations();
    else if (!strcmp(argv[1], "selection")) selection();
    else if (!strcmp(argv[1], "nearby")) nearby();
    else if (!strcmp(argv[1], "announcement")) announcement();
    else if (!strcmp(argv[1], "spatial_sound")) spatial_sound();
    else if (!strcmp(argv[1], "volumes")) volumes();
    else if (!strcmp(argv[1], "sound_fallback")) sound_fallback();
    else if (!strcmp(argv[1], "buffers")) buffers();
    else { fprintf(stderr, "Unknown case: %s\n", argv[1]); return 2; }
    printf("%s: %d assertions, %d failed\n", argv[1], assertions, failures);
    return failures ? 1 : 0;
}
