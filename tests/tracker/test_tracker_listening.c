/* Drive the actual optional listening-test module with deterministic input and
 * time. Real engine headers are used; no game window, device or saves are opened. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "fixer.h"
#include "3dc.h"
#include "gamedef.h"
#include "psnd.h"
#include "psndplat.h"
#include "acc_speech.h"
#include "acc_tracker.h"
#include "acc_tracker_test.h"

VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
AVP_GAME_DESC AvP;
int NormalFrameTime;
int AccPadTrace;
SOUNDSAMPLEDATA GameSounds[SID_MAXIMUM];
unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS], DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];

typedef struct { Uint64 time; int key, key2; } INPUT_EVENT;
typedef struct { Uint64 time; int id, yaw, x, y, z, range, volume; } CUE;
typedef struct { Uint64 time; int yaw, x, z, range; } REPORT;
static INPUT_EVENT events[64];
static CUE cues[128];
static REPORT reports[64];
static VIEWDESCRIPTORBLOCK saved_view;
static ACC_TRACKER_CONTACT published;
static int event_count, next_event, cue_count, report_count, announcements;
static int contact_count, published_range, reset_count, stop_count, management_count;
static int publication_count;
static int sound_on, current_yaw, checks, failures, cancelled, play_after_cancel;
static int invalid_listener, invalid_species, invalid_time, fail_playback;
static int *live_handle;
static Uint64 ticks, live_until;
static char last_speech[768];

static void check(int condition, const char *label)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL: %s\n", label); }
}

Uint64 SDLCALL SDL_GetTicks(void) { return ticks; }
void SDLCALL SDL_Delay(Uint32 ms) { ticks += ms; }

void CheckForWindowsMessages(void)
{
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    if (next_event < event_count && ticks >= events[next_event].time) {
        INPUT_EVENT *e = &events[next_event++];
        DebouncedKeyboardInput[e->key] = 1;
        KeyboardInput[e->key] = 1;
        if (e->key2) DebouncedKeyboardInput[e->key2] = 1;
        if (e->key == KEY_ESCAPE || e->key == KEY_JOYSTICK_BUTTON_2)
            cancelled = 1;
    }
    if (ticks > 120000) {
        check(0, "diagnostic completed within scripted time");
        exit(2);
    }
}

int SoundSys_IsOn(void) { return sound_on; }
void SoundSys_ResetFadeLevel(void) { }
void SoundSys_Management(void)
{
    ++management_count;
    if (!Global_VDB_Ptr) invalid_listener=1;
    if (AvP.PlayerType != I_Marine) invalid_species=1;
    if (NormalFrameTime <= 0) invalid_time=1;
    if (live_handle && ticks >= live_until) {
        *live_handle = SOUND_NOACTIVEINDEX;
        live_handle = NULL;
    }
}

void Sound_Stop(int handle)
{
    check(handle != SOUND_NOACTIVEINDEX, "only a live cue handle is stopped");
    ++stop_count;
    if (live_handle) *live_handle = SOUND_NOACTIVEINDEX;
    live_handle = NULL;
}

void CreateEulerMatrix(EULER *e, MATRIXCH *m)
{
    memset(m, 0, sizeof(*m));
    current_yaw = e->EulerY;
    check(e->EulerX == 0 && e->EulerZ == 0, "test listener remains level");
    m->mat22 = ONE_FIXED;
    if (e->EulerY == 0) m->mat11 = m->mat33 = ONE_FIXED;
    else if (e->EulerY == 1024) { m->mat13 = ONE_FIXED; m->mat31 = -ONE_FIXED; }
    else check(0, "listening stages use an expected listener heading");
}

void AccSpeech_Say(const char *message, int interrupt)
{
    (void)interrupt;
    ++announcements;
    strncpy(last_speech, message ? message : "", sizeof(last_speech) - 1);
    last_speech[sizeof(last_speech) - 1] = 0;
}
int AccSpeech_IsAvailable(void) { return 1; }

void AccTracker_SetContacts(const ACC_TRACKER_CONTACT *contacts, int count, int range)
{
    ++publication_count;
    check(count == 1 && contacts != NULL, "one explicitly simulated contact is published");
    if (contacts) published = contacts[0];
    contact_count = count;
    published_range = range;
}

void AccTracker_Reset(void) { ++reset_count; contact_count = 0; }

void AccTracker_Announce(const struct vectorch *player, int yaw)
{
    REPORT *r;
    check(report_count < 64, "bounded number of reports");
    if (report_count >= 64) exit(2);
    r = &reports[report_count++];
    r->time = ticks; r->yaw = yaw;
    r->x = published.x; r->z = published.z; r->range = published_range;
    check(contact_count == 1, "speech uses the published simulated contact");
    check(player && player->vx == 0 && player->vy == 0 && player->vz == 0,
          "speech and audio use an origin listener");
    check(yaw == current_yaw, "speech and audio use the same heading");
}

void AccTracker_PlayContact(int id, const struct vectorch *position,
                           int range, int *handle, int volume)
{
    CUE *c;
    check(cue_count < 128, "bounded number of beeps");
    if (cue_count >= 128) exit(2);
    c = &cues[cue_count++];
    c->time=ticks; c->id=id; c->yaw=current_yaw;
    c->x=position->vx; c->y=position->vy; c->z=position->vz;
    c->range=range; c->volume=volume;
    check(!live_handle, "next cue does not orphan an earlier live handle");
    check(Global_VDB_Ptr && Global_VDB_Ptr != &saved_view, "real cue adapter receives test listener");
    if (cancelled) ++play_after_cancel;
    if (fail_playback) return;
    *handle = 100 + cue_count;
    live_handle = handle;
    live_until = ticks + 377;
}

static void fixture(void)
{
    memset(events, 0, sizeof(events)); memset(cues, 0, sizeof(cues));
    memset(reports, 0, sizeof(reports)); memset(GameSounds, 0, sizeof(GameSounds));
    memset(KeyboardInput, 0, sizeof(KeyboardInput));
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    memset(&saved_view, 0xa5, sizeof(saved_view));
    Global_VDB_Ptr = &saved_view;
    AvP.PlayerType = I_Predator; NormalFrameTime = 4321;
    ticks=live_until=0; live_handle=NULL; sound_on=1;
    event_count=next_event=cue_count=report_count=announcements=0;
    contact_count=published_range=reset_count=stop_count=management_count=0;
    publication_count=0;
    current_yaw=cancelled=play_after_cancel=0;
    invalid_listener=invalid_species=invalid_time=fail_playback=0;
    last_speech[0]=0;
    GameSounds[SID_TRACKER_WHEEP].loaded=1;
    GameSounds[SID_TRACKER_WHEEP_HIGH].loaded=1;
    GameSounds[SID_TRACKER_WHEEP_LOW].loaded=1;
}

static void input(Uint64 when, int key)
{
    events[event_count].time=when; events[event_count++].key=key;
}

static void restored(void)
{
    check(Global_VDB_Ptr == &saved_view, "original listener pointer restored");
    check(AvP.PlayerType == I_Predator, "original species restored");
    check(NormalFrameTime == 4321, "original frame time restored");
    check(!live_handle, "no diagnostic cue remains live");
    check(!play_after_cancel, "cancellation prevents any later beep");
    check(!invalid_listener && !invalid_species && !invalid_time,
          "all audio updates have a valid Marine listener and nonzero frame time");
}

static void intro(void)
{
    fixture(); input(7000,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"intro can be cancelled");
    check(!cue_count && !report_count,"intro waits for deliberate advance without autoplay");
    check(announcements>0,"intro is spoken"); restored();
    fixture(); input(100,KEY_T); input(200,KEY_JOYSTICK_BUTTON_14); input(300,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"intro repeats then cancels");
    check(announcements>=3 && !report_count && !cue_count,"repeat shortcuts repeat introduction without selecting a contact");
    restored();
}

static void timing(void)
{
    fixture(); input(100,KEY_JOYSTICK_BUTTON_1); input(7000,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"first stage finishes on cancel");
    check(report_count==1,"holding advance without fresh edge selects exactly one stage");
    check(cue_count==2,"each selection schedules exactly two beeps");
    if (cue_count==2 && report_count==1) {
        check(cues[0].time-reports[0].time>=4500 && cues[0].time-reports[0].time<4520,"first cue waits 4.5 seconds for speech");
        check(cues[1].time-cues[0].time>=1000 && cues[1].time-cues[0].time<1020,"second cue follows one second after first");
    }
    check(reset_count>0 && !contact_count,"exit invalidates the simulated contact"); restored();
}

static void repeat(void)
{
    int i;
    fixture(); input(100,KEY_CR); input(6000,KEY_T); input(12000,KEY_JOYSTICK_BUTTON_14); input(18000,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"both repeat shortcuts complete");
    check(report_count==1 && publication_count==1 && announcements==1,
          "repeat does not restart speech or republish the simulated contact");
    check(cue_count==6,"both repeat shortcuts replay two beeps after the original pair");
    if(cue_count==6) {
        check(cues[2].time>=6000 && cues[2].time<6016,"T repeat plays its first beep immediately");
        check(cues[4].time>=12000 && cues[4].time<12016,"D-pad Down repeat plays its first beep immediately");
        for(i=2;i<=4;i+=2)
            check(cues[i+1].time-cues[i].time>=1000 && cues[i+1].time-cues[i].time<1016,
                  "each repeated pair has its second beep one second later");
        for(i=0;i<6;++i)
            check(cues[i].id==SID_TRACKER_WHEEP && cues[i].x==-12000 && cues[i].z==0 && cues[i].yaw==0,
                  "repeat keeps the current example location, heading and tone");
    }
    restored();

    fixture(); input(100,KEY_CR);
    for(i=0;i<4;++i) input(200+(Uint64)i*100,KEY_JOYSTICK_BUTTON_14);
    input(2500,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"rapid D-pad repeats complete");
    check(report_count==1 && publication_count==1 && announcements==1,
          "rapid repeats retain the first contact and do not interrupt with more speech");
    check(cue_count==5,"each rapid repeat plays immediately, then the last pair finishes");
    if(cue_count==5) {
        for(i=0;i<4;++i)
            check(cues[i].time>=200+(Uint64)i*100 && cues[i].time<216+(Uint64)i*100,
                  "repeated D-pad presses cannot postpone every beep");
        check(cues[4].time-cues[3].time>=1000 && cues[4].time-cues[3].time<1016,
              "last rapid repeat retains its delayed second beep");
    }
    check(stop_count>=3,"rapid repeats stop the earlier live cue before replaying");
    restored();

    fixture(); input(100,KEY_JOYSTICK_BUTTON_1); input(200,KEY_JOYSTICK_BUTTON_1);
    events[1].key2=KEY_JOYSTICK_BUTTON_14; input(400,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"simultaneous advance and repeat can be cancelled");
    check(report_count==2 && publication_count==2 && cue_count==0,
          "advance wins simultaneous repeat and keeps its speech delay");
    restored();
}

static void advance_cancel(void)
{
    fixture(); input(100,KEY_SPACE); input(4700,KEY_JOYSTICK_BUTTON_1); input(4900,KEY_JOYSTICK_BUTTON_2);
    check(AccTracker_RunListeningTest()==0,"advance and B cancellation succeed");
    check(report_count==2 && cue_count==1,"advancing cancels pending old beeps and abort cancels new beeps");
    check(stop_count==1,"advancing stops a cue that is still playing"); restored();
    fixture(); input(100,KEY_JOYSTICK_BUTTON_1); input(4600,KEY_ESCAPE);
    check(AccTracker_RunListeningTest()==0,"cancel on first cue deadline succeeds");
    check(cue_count==0,"cancel input wins over a due beep"); restored();
}

static void stages(void)
{
    static const int xs[6]={-12000,0,12000,0,0,0};
    static const int zs[6]={0,12000,0,5000,25000,12000};
    static const int sounds[6]={SID_TRACKER_WHEEP,SID_TRACKER_WHEEP,SID_TRACKER_WHEEP,SID_TRACKER_WHEEP_HIGH,SID_TRACKER_WHEEP_LOW,SID_TRACKER_WHEEP};
    int i,j;
    fixture(); for(i=0;i<7;++i) input(100+(Uint64)i*6500,KEY_JOYSTICK_BUTTON_1);
    check(AccTracker_RunListeningTest()==0,"last advance completes six-stage diagnostic");
    check(report_count==6 && cue_count==12,"all six stages each announce and play twice");
    for(i=0;i<6 && i<report_count;++i) {
        check(reports[i].x==xs[i] && reports[i].z==zs[i],"stage speech uses expected location");
        check(reports[i].yaw==(i==5?1024:0) && reports[i].range==30000,"stage speech uses expected heading and range");
        for(j=0;j<2 && 2*i+j<cue_count;++j) {
            CUE *c=&cues[2*i+j];
            check(c->id==sounds[i] && c->x==xs[i] && c->y==0 && c->z==zs[i],"stage uses expected real cue and world location");
            check(c->yaw==(i==5?1024:0) && c->range==30000 && c->volume==127,"cue preserves heading, gameplay range and volume");
        }
    }
    check(reset_count>0 && !contact_count,"normal completion clears simulated cache"); restored();
}

static void unavailable(void)
{
    const int ids[3]={SID_TRACKER_WHEEP,SID_TRACKER_WHEEP_HIGH,SID_TRACKER_WHEEP_LOW};
    int i;
    for(i=-1;i<3;++i) {
        fixture(); if(i<0) sound_on=0; else GameSounds[ids[i]].loaded=0;
        check(AccTracker_RunListeningTest()==1,"unavailable audio returns failure");
        check(announcements>0 && last_speech[0],"unavailable audio has spoken explanation");
        check(!cue_count && !report_count,"unavailable audio never starts a test stage"); restored();
    }
    fixture(); fail_playback=1; input(100,KEY_CR);
    check(AccTracker_RunListeningTest()==1,"failed cue playback returns failure");
    check(cue_count==1 && report_count==1,"failed cue playback stops further examples");
    check(announcements>1 && last_speech[0],"failed cue playback has spoken explanation");
    check(reset_count>0 && !contact_count,"failed playback clears simulated cache"); restored();
}

int main(void)
{
    intro(); timing(); repeat(); advance_cancel(); stages(); unavailable();
    printf("Listening diagnostic: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
