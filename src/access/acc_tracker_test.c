/* Guided, opt-in listening examples. These are simulated contacts, not enemies. */
#include "3dc.h"
#include "gamedef.h"
#include "psnd.h"
#include "psndplat.h"
#include "acc_tracker.h"
#include "acc_tracker_test.h"
#include "acc_speech.h"
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <string.h>

extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
extern int NormalFrameTime;
extern unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
extern void CheckForWindowsMessages(void);

typedef struct {
    VECTORCH position;
    int yaw;
    SOUNDINDEX tone;
} TRACKER_EXAMPLE;

static const TRACKER_EXAMPLE Examples[] = {
    {{-12000,0,0}, 0, SID_TRACKER_WHEEP},
    {{0,0,12000}, 0, SID_TRACKER_WHEEP},
    {{12000,0,0}, 0, SID_TRACKER_WHEEP},
    {{0,0,5000}, 0, SID_TRACKER_WHEEP_HIGH},
    {{0,0,25000}, 0, SID_TRACKER_WHEEP_LOW},
    /* Same world contact as example 2, with the listener facing right. */
    {{0,0,12000}, 1024, SID_TRACKER_WHEEP}
};

static void ExplainTest(void)
{
    AccSpeech_Say("Tracker listening test. Six simulated contacts. "
        "Press A or Enter for the next example. "
        "Each example speaks, then beeps twice after a short pause. "
        "D-pad Down or T repeats. B or Escape finishes.", 1);
}

static void StopTestCue(int *handle)
{
    if (*handle != SOUND_NOACTIVEINDEX) Sound_Stop(*handle);
    *handle = SOUND_NOACTIVEINDEX;
}

int AccTracker_RunListeningTest(void)
{
    VIEWDESCRIPTORBLOCK view;
    VIEWDESCRIPTORBLOCK *savedView = Global_VDB_Ptr;
    int savedSpecies = AvP.PlayerType, savedFrameTime = NormalFrameTime;
    int example = -1, remainingBeeps = 0, result = 0, handle = SOUND_NOACTIVEINDEX;
    Uint64 nextBeep = 0;

    if (!SoundSys_IsOn() || !GameSounds[SID_TRACKER_WHEEP].loaded
        || !GameSounds[SID_TRACKER_WHEEP_HIGH].loaded
        || !GameSounds[SID_TRACKER_WHEEP_LOW].loaded) {
        fprintf(stderr, "ACCTRACKER TEST: unavailable: sound system or tracker samples missing\n");
        AccSpeech_Say("Tracker listening test unavailable. Game audio or tracker sounds could not load.", 1);
        return 1;
    }

    memset(&view, 0, sizeof(view));
    view.VDB_Mat.mat11 = view.VDB_Mat.mat22 = view.VDB_Mat.mat33 = 65536;
    Global_VDB_Ptr = &view;
    AvP.PlayerType = I_Marine;
    /* Input polling divides mouse deltas by this even without a running level. */
    NormalFrameTime = 65536 / 60;
    AccTracker_Reset();
    SoundSys_ResetFadeLevel();
    SoundSys_Management();
    fprintf(stderr, "ACCTRACKER TEST: ready; simulated contacts only; awaiting next button\n");
    fflush(stderr);
    ExplainTest();

    for (;;) {
        int advance, repeat;
        Uint64 now;
        CheckForWindowsMessages();
        if (DebouncedKeyboardInput[KEY_ESCAPE]
            || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_2]) {
            fprintf(stderr, "ACCTRACKER TEST: cancelled\n");
            break;
        }
        advance = DebouncedKeyboardInput[KEY_CR] || DebouncedKeyboardInput[KEY_SPACE]
            || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1];
        repeat = DebouncedKeyboardInput[KEY_T]
            || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14];
        if (advance || repeat) {
            if (advance) ++example;
            StopTestCue(&handle);
            remainingBeeps = 0;
            if (example >= (int)(sizeof(Examples) / sizeof(Examples[0]))) {
                fprintf(stderr, "ACCTRACKER TEST: all six examples visited; listening confirmation required\n");
                AccSpeech_Say("Tracker listening examples complete.", 1);
                break;
            }
            if (example < 0) {
                ExplainTest();
            } else {
                EULER facing = {0, Examples[example].yaw, 0};
                ACC_TRACKER_CONTACT contact;
                contact.x = Examples[example].position.vx;
                contact.z = Examples[example].position.vz;
                CreateEulerMatrix(&facing, &view.VDB_Mat);
                AccTracker_SetContacts(&contact, 1, 30000);
                SoundSys_Management();
                fprintf(stderr, "ACCTRACKER TEST: example=%d yaw=%d contact=(%d,%d)\n",
                    example + 1, facing.EulerY, contact.x, contact.z);
                AccTracker_Announce(&view.VDB_World, facing.EulerY);
                nextBeep = SDL_GetTicks() + 4500;
                remainingBeeps = 2;
            }
        }
        SoundSys_Management();
        now = SDL_GetTicks();
        if (remainingBeeps && now >= nextBeep) {
            StopTestCue(&handle);
            AccTracker_PlayContact(Examples[example].tone, &Examples[example].position,
                30000, &handle, VOLUME_MAX);
            if (handle == SOUND_NOACTIVEINDEX) {
                fprintf(stderr, "ACCTRACKER TEST: playback failed for example=%d\n", example + 1);
                AccSpeech_Say("The tracker test sound could not play.", 1);
                result = 1;
                break;
            }
            --remainingBeeps;
            nextBeep = now + 1000;
            fprintf(stderr, "ACCTRACKER TEST: example=%d beep=%d handle=%d\n",
                example + 1, 2 - remainingBeeps, handle);
        }
        fflush(stderr);
        SDL_Delay(16);
    }
    StopTestCue(&handle);
    AccTracker_Reset();
    Global_VDB_Ptr = savedView;
    AvP.PlayerType = savedSpecies;
    NormalFrameTime = savedFrameTime;
    fflush(stderr);
    return result;
}
