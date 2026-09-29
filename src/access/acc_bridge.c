/* AVP Access: the play bridge -- see acc_bridge.h. The engine-free parts
   (parsing, runner, PNG, JSON) live in acc_bridge_core.c; this file is the
   glue to files, frames, keys and game state. */
#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "bh_types.h"
#include "avpview.h"
#include "usr_io.h"

#include <SDL3/SDL.h>

#include "acc_bridge.h"
#include "acc_bridge_core.h"
#include "acc_map.h"
#include "acc_speech.h"
#include "acc_status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#define ACC_GETPID() _getpid()
#else
#include <unistd.h>
#define ACC_GETPID() getpid()
#endif

extern unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
extern unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
extern unsigned char GotAnyKey;
extern int DebouncedGotAnyKey;
extern int GlobalFrameCounter;
extern char LevelName[];
extern MODULE *playerPherModule;
extern unsigned char *GetScreenShot24(int *width, int *height);
extern int InGameMenusAreRunning(void);

/* Events kept for the next reply. A reply normally carries a handful; the
   ring only matters if a long real-time stretch passes between commands, and
   events.jsonl has everything regardless. */
#define EVENT_RING 512
#define EVENT_LEN  1536
#define REPLY_MAX  (EVENT_RING * EVENT_LEN + 16384)

/* Screenshots wider than this are halved, to keep them quick to read. */
#define SHOT_MAX_WIDTH 1024

/* How long a reply waits for a frame to be rendered for its screenshot. */
#define SHOT_WAIT_MS 3000

/* Input reads without a simulated frame before a frame-timed command decides
   the frames have stopped (the level ended) and finishes on the clock. */
#define FRAMELESS_READS 10

#define CUE_DEPTH 4

static int  Enabled, Audible, Started;
static char Dir[700];
static PLAYER_STATUS *SurveyStatus;
static int SurveyOriginalImmortal, SurveyImmortalSaved;

static int  StepMode = 1;
static int  InGameplay;
static int  AllSounds;

static ACC_BRIDGE_RUNNER Runner;
static int  FrameGranted, FramelessReads;
static int  Injected[ACC_BRIDGE_MAX_KEYS], InjectedCount;

static unsigned int ClockMs, ClockFixedRemainder;
static Uint64       LastWallMs;

static unsigned long  FlipCount, CaptureFlip;
static unsigned char *Frame;
static int            FrameW, FrameH;

static int                ReplyPending;
static int                ReplyWantsShot;
static ACC_BRIDGE_COMMAND ReplyCmd;
static const char        *ReplyResult;
static char               ReplyError[128];
static char               ReplyDetail[760];
static int                ReplyTurned;
static unsigned long      ReplyAfterFlip;
static Uint64             ReplyDeadline;
static Uint64             ReplyRetryAt;
static char              *ReplyBuffer;

static char          Events[EVENT_RING][EVENT_LEN];
static unsigned long EventCount, EventReported;
static FILE         *EventFile;
static int           ReadyOwned;

static struct {
    char source[24];
    int  sound;
    int  played;
} Cues[CUE_DEPTH];
static int CueDepth;

static void SurveyRestoreImmortality(void)
{
    if (SurveyImmortalSaved && SurveyStatus == PlayerStatusPtr && SurveyStatus)
        SurveyStatus->IsImmortal = SurveyOriginalImmortal;
    SurveyStatus = NULL;
    SurveyImmortalSaved = 0;
}

static void SurveyApplyImmortality(void)
{
    if (!AccBridge_IsSurvey() || !InGameplay || !PlayerStatusPtr) {
        SurveyRestoreImmortality();
        return;
    }
    if (SurveyStatus != PlayerStatusPtr) {
        SurveyRestoreImmortality();
        SurveyStatus = PlayerStatusPtr;
        SurveyOriginalImmortal = PlayerStatusPtr->IsImmortal;
        SurveyImmortalSaved = 1;
    }
    PlayerStatusPtr->IsImmortal = 1;
}

/* ---------------------------------------------------------------- clock -- */

unsigned int AccBridge_NowMs(void)
{
    Uint64 wall = SDL_GetTicks();

    if (!Enabled) return (unsigned int)wall;

    /* Wall time only counts while the bridge is not holding it; held time
       advances in AccBridge_FixedFrameTime instead. */
    if (!(StepMode && InGameplay)) ClockMs += (unsigned int)(wall - LastWallMs);
    LastWallMs = wall;
    return ClockMs;
}

int AccBridge_FixedFrameTime(void)
{
    if (!Enabled || !StepMode || !InGameplay) return 0;

    AccBridge_NowMs();
    ClockFixedRemainder += (unsigned int)ACC_BRIDGE_STEP_FIXED * 1000u;
    ClockMs += ClockFixedRemainder >> 16;
    ClockFixedRemainder &= 0xFFFF;
    return ACC_BRIDGE_STEP_FIXED;
}

/* -------------------------------------------------------------- helpers -- */

static void PathFor(const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s%s", Dir, name);
}

static int PlayerPose(int *x, int *y, int *z, int *yaw, int *pitch)
{
    DYNAMICSBLOCK *dynamics;

    if (!InGameplay || !Player || !Player->ObStrategyBlock) return 0;
    dynamics = Player->ObStrategyBlock->DynPtr;
    if (!dynamics) return 0;

    if (x) *x = dynamics->Position.vx;
    if (y) *y = dynamics->Position.vy;
    if (z) *z = dynamics->Position.vz;
    if (yaw) *yaw = dynamics->OrientEuler.EulerY & (ACC_BRIDGE_YAW_TURN - 1);
    if (pitch) *pitch = dynamics->OrientEuler.EulerX & (ACC_BRIDGE_YAW_TURN - 1);
    return 1;
}

static const char *SpeciesName(void)
{
    switch (AvP.PlayerType) {
    case I_Marine:   return "marine";
    case I_Predator: return "predator";
    case I_Alien:    return "alien";
    default:         return "unknown";
    }
}

/* The key the player has bound to turning, primary binding first. */
static int TurnKey(int right)
{
    const PLAYER_INPUT_CONFIGURATION *primary, *secondary;
    int key;

    switch (AvP.PlayerType) {
    case I_Predator:
        primary = &PredatorInputPrimaryConfig;
        secondary = &PredatorInputSecondaryConfig;
        break;
    case I_Alien:
        primary = &AlienInputPrimaryConfig;
        secondary = &AlienInputSecondaryConfig;
        break;
    default:
        primary = &MarineInputPrimaryConfig;
        secondary = &MarineInputSecondaryConfig;
        break;
    }

    key = right ? primary->Right : primary->Left;
    if (key == KEY_VOID) key = right ? secondary->Right : secondary->Left;
    return key == KEY_VOID ? -1 : key;
}

/* --------------------------------------------------------------- events -- */

static void AddEvent(const char *type, const char *fields)
{
    char *line = Events[EventCount % EVENT_RING];
    int written;

    written = snprintf(line, EVENT_LEN, "{\"n\":%lu,\"t\":%u,\"frame\":%d,\"type\":\"%s\"%s%s}",
                       EventCount + 1, AccBridge_NowMs(), GlobalFrameCounter, type,
                       (fields && fields[0]) ? "," : "", fields ? fields : "");
    if (written < 0 || written >= EVENT_LEN) {
        /* Fields are sized to fit; if one ever does not, keep the line valid. */
        snprintf(line, EVENT_LEN, "{\"n\":%lu,\"t\":%u,\"frame\":%d,\"type\":\"%s\",\"truncated\":1}",
                 EventCount + 1, AccBridge_NowMs(), GlobalFrameCounter, type);
    }
    EventCount++;

    if (EventFile) {
        fputs(line, EventFile);
        fputc('\n', EventFile);
        fflush(EventFile);
    }
}

static void OnSpeech(const char *text, int interrupt)
{
    char escaped[1200], fields[1300];
    size_t len;

    if (!Enabled) return;

    len = AccBridge_JsonEscape(text, escaped, sizeof(escaped));
    snprintf(fields, sizeof(fields), "\"interrupt\":%d,\"text\":\"%s\"%s",
             interrupt ? 1 : 0, escaped,
             (len + 8 >= sizeof(escaped)) ? ",\"truncated\":1" : "");
    AddEvent("speech", fields);
}

void AccBridge_BeginCue(const char *source, int sound)
{
    if (!Enabled) return;

    if (CueDepth < CUE_DEPTH) {
        const char *name = source ? source : "access";
        if (CueDepth > 0) name = Cues[0].source;
        snprintf(Cues[CueDepth].source, sizeof(Cues[CueDepth].source), "%s", name);
        Cues[CueDepth].sound = sound;
        Cues[CueDepth].played = 0;
    }
    CueDepth++;
}

void AccBridge_EndCue(void)
{
    int level;

    if (!Enabled || CueDepth <= 0) return;

    level = --CueDepth;
    if (level < CUE_DEPTH && Cues[level].sound >= 0 && !Cues[level].played) {
        char fields[96];
        snprintf(fields, sizeof(fields), "\"source\":\"%s\",\"sound\":%d",
                 Cues[level].source, Cues[level].sound);
        AddEvent("cue_dropped", fields);
    }
    /* A sound that played for an inner cue played for the outer one too. */
    if (level > 0 && level < CUE_DEPTH && Cues[level].played && level - 1 < CUE_DEPTH)
        Cues[level - 1].played = 1;
}

void AccBridge_OnSound(int sound, const char *name, const struct vectorch *position,
                       int volume, int pitch, int loop, int innerRange, int outerRange)
{
    char fields[512], where[192], escaped[96], source[48];
    int px, py, pz, yaw, playerPitch, top;

    if (!Enabled) return;

    top = CueDepth - 1;
    if (top >= CUE_DEPTH) top = CUE_DEPTH - 1;
    if (CueDepth > 0) Cues[top].played = 1;
    else if (!AllSounds) return;

    where[0] = 0;
    if (position) {
        if (PlayerPose(&px, &py, &pz, &yaw, &playerPitch)) {
            ACC_BRIDGE_RELATIVE rel;
            AccBridge_Relative(px, pz, yaw, position->vx, position->vz, &rel);
            snprintf(where, sizeof(where),
                     ",\"x\":%d,\"y\":%d,\"z\":%d,\"bearing\":%d,\"clock\":%d,\"distance\":%d",
                     position->vx, position->vy, position->vz, rel.bearing, rel.clock, rel.distance);
        } else {
            snprintf(where, sizeof(where), ",\"x\":%d,\"y\":%d,\"z\":%d",
                     position->vx, position->vy, position->vz);
        }
    }

    source[0] = 0;
    if (CueDepth > 0) snprintf(source, sizeof(source), "\"source\":\"%s\",", Cues[0].source);

    AccBridge_JsonEscape(name ? name : "", escaped, sizeof(escaped));
    snprintf(fields, sizeof(fields),
             "%s\"sound\":%d,\"name\":\"%s\",\"volume\":%d,\"pitch\":%d,\"loop\":%d,\"inner\":%d,\"outer\":%d%s",
             source, sound, escaped, volume, pitch, loop ? 1 : 0, innerRange, outerRange, where);
    AddEvent(CueDepth > 0 ? "cue" : "sound", fields);
}

/* ----------------------------------------------------------------- keys -- */

/* The engine rewrites these from the device every frame, so a held
   injection has to be reasserted each read rather than pressed once. */
static int RecomputedEachFrame(int key)
{
    return (key >= KEY_JOYSTICK_BUTTON_1 && key <= KEY_JOYSTICK_BUTTON_16)
        || key == KEY_LMOUSE || key == KEY_MMOUSE || key == KEY_RMOUSE;
}

static void ApplyKeys(const int *keys, int count)
{
    int i, j;

    for (i = 0; i < InjectedCount;) {
        int wanted = 0;
        for (j = 0; j < count; j++)
            if (keys[j] == Injected[i]) wanted = 1;
        if (wanted) { i++; continue; }

        if (RecomputedEachFrame(Injected[i])) KeyboardInput[Injected[i]] = 0;
        else AccBridge_PlatformKey(Injected[i], 0);
        Injected[i] = Injected[--InjectedCount];
    }

    for (j = 0; j < count; j++) {
        int key = keys[j], held = 0;

        if (key < 0 || key >= MAX_NUMBER_OF_INPUT_KEYS) continue;
        for (i = 0; i < InjectedCount; i++)
            if (Injected[i] == key) held = 1;

        if (RecomputedEachFrame(key)) {
            if (!held && !KeyboardInput[key]) {
                DebouncedKeyboardInput[key] = 1;
                DebouncedGotAnyKey = 1;
            }
            KeyboardInput[key] = 1;
            GotAnyKey = 1;
        } else if (!held) {
            AccBridge_PlatformKey(key, 1);
        }

        if (!held && InjectedCount < ACC_BRIDGE_MAX_KEYS) Injected[InjectedCount++] = key;
    }
}

/* ---------------------------------------------------------------- reply -- */

static int Append(size_t *pos, const char *format, ...)
{
    va_list args;
    int written;

    if (*pos >= REPLY_MAX) return 0;
    va_start(args, format);
    written = vsnprintf(ReplyBuffer + *pos, REPLY_MAX - *pos, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= REPLY_MAX - *pos) return 0;
    *pos += (size_t)written;
    return 1;
}

static int WriteShot(unsigned int seq, char *path, size_t size)
{
    unsigned char *pixels = Frame, *halved = NULL, *png;
    int w = FrameW, h = FrameH;
    size_t pngSize;
    FILE *file;
    char name[32];

    if (!Frame) return 0;

    while (w > SHOT_MAX_WIDTH) {
        int hw, hh;
        unsigned char *next = AccBridge_HalveRgb(pixels, w, h, &hw, &hh);
        if (!next) break;
        free(halved);
        halved = next;
        pixels = next;
        w = hw;
        h = hh;
    }

    png = AccBridge_EncodePng(pixels, w, h, 1, &pngSize);
    free(halved);
    if (!png) return 0;

    snprintf(name, sizeof(name), "shot-%06u.png", seq);
    PathFor(name, path, size);

    file = fopen(path, "wb");
    if (!file) {
        free(png);
        return 0;
    }
    {
        int ok = fwrite(png, 1, pngSize, file) == pngSize;
        if (fclose(file) != 0) ok = 0;
        free(png);
        return ok;
    }
}

static int WriteReply(const char *shotPath, const char *shotError)
{
    char escaped[1200], tmpPath[760], replyPath[760];
    const char *context;
    size_t pos = 0;
    unsigned long first, dropped = 0, i;
    int x, y, z, yaw, pitch, attempt;
    FILE *file;

    if (!ReplyBuffer) ReplyBuffer = (char *)malloc(REPLY_MAX);
    if (!ReplyBuffer) return 0;

    if (InGameplay) context = InGameMenusAreRunning() ? "game_menu" : "gameplay";
    else context = "menus";

    Append(&pos, "{\"seq\":%u,\"verb\":\"%s\",\"result\":\"%s\"",
           ReplyCmd.seq, AccBridge_VerbName(ReplyCmd.verb), ReplyResult ? ReplyResult : "ok");
    if (ReplyError[0]) {
        AccBridge_JsonEscape(ReplyError, escaped, sizeof(escaped));
        Append(&pos, ",\"error\":\"%s\"", escaped);
    }
    if (ReplyDetail[0]) {
        AccBridge_JsonEscape(ReplyDetail, escaped, sizeof(escaped));
        Append(&pos, ",\"detail\":\"%s\"", escaped);
    }
    if (ReplyCmd.verb == ACC_BRIDGE_TURN)
        Append(&pos, ",\"turned\":%d", (ReplyTurned * 360 + (ReplyTurned >= 0 ? 2048 : -2048)) / ACC_BRIDGE_YAW_TURN);

    AccBridge_JsonEscape(LevelName, escaped, sizeof(escaped));
    Append(&pos, ",\n\"mode\":\"%s\",\"context\":\"%s\",\"time_held\":%s,\"t\":%u,\"frame\":%d,\"level\":\"%s\",\"sounds\":\"%s\"",
           StepMode ? "step" : "realtime", context, (StepMode && InGameplay) ? "true" : "false",
           AccBridge_NowMs(), GlobalFrameCounter, escaped, AllSounds ? "all" : "access");

    if (PlayerPose(&x, &y, &z, &yaw, &pitch)) {
        char status[512];
        const char *module = (playerPherModule && playerPherModule->name) ? playerPherModule->name : "";
        DYNAMICSBLOCK *dynamics = Player->ObStrategyBlock->DynPtr;

        status[0] = 0;
        if (AvP.PlayerType == I_Marine && PlayerStatusPtr)
            AccStatus_FormatMarine((const struct player_status *)PlayerStatusPtr, status, sizeof(status));
        else if (AvP.PlayerType == I_Predator && PlayerStatusPtr)
            AccStatus_FormatPredator((const struct player_status *)PlayerStatusPtr, status, sizeof(status));

        Append(&pos, ",\n\"player\":{\"species\":\"%s\",\"alive\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"yaw\":%d,\"heading\":%d,\"pitch\":%d",
               SpeciesName(), PlayerStatusPtr ? (PlayerStatusPtr->IsAlive ? 1 : 0) : 0,
               x, y, z, yaw, (yaw * 360 + 2048) / ACC_BRIDGE_YAW_TURN, pitch);
        AccBridge_JsonEscape(module, escaped, sizeof(escaped));
        Append(&pos, ",\"module\":\"%s\"", escaped);
        AccBridge_JsonEscape(status, escaped, sizeof(escaped));
        Append(&pos, ",\"status\":\"%s\"", escaped);
        if (dynamics) {
            Append(&pos, ",\"grounded\":%s,\"nearly_flat\":%s,\"velocity\":{\"x\":%d,\"y\":%d,\"z\":%d}",
                   dynamics->IsInContactWithFloor ? "true" : "false",
                   dynamics->IsInContactWithNearlyFlatFloor ? "true" : "false",
                   dynamics->LinVelocity.vx, dynamics->LinVelocity.vy, dynamics->LinVelocity.vz);
        } else {
            Append(&pos, ",\"grounded\":null,\"nearly_flat\":null,\"velocity\":null");
        }
        if (Global_VDB_Ptr) {
            Append(&pos, ",\"camera\":{\"x\":%d,\"y\":%d,\"z\":%d}",
                   Global_VDB_Ptr->VDB_World.vx, Global_VDB_Ptr->VDB_World.vy,
                   Global_VDB_Ptr->VDB_World.vz);
        } else {
            Append(&pos, ",\"camera\":null");
        }
        Append(&pos, "}");
    } else {
        Append(&pos, ",\n\"player\":null");
    }

    if (shotPath) {
        AccBridge_JsonEscape(shotPath, escaped, sizeof(escaped));
        Append(&pos, ",\n\"shot\":\"%s\"", escaped);
    } else if (shotError) {
        Append(&pos, ",\n\"shot\":null,\"shot_error\":\"%s\"", shotError);
    }

    first = EventReported;
    if (EventCount - first > EVENT_RING) {
        dropped = EventCount - EVENT_RING - first;
        first = EventCount - EVENT_RING;
    }
    Append(&pos, ",\n\"events_dropped\":%lu,\n\"events\":[", dropped);
    for (i = first; i < EventCount; i++) {
        if (!Append(&pos, "%s\n%s", i == first ? "" : ",", Events[i % EVENT_RING])) break;
    }
    Append(&pos, "]}\n");

    PathFor("reply.tmp", tmpPath, sizeof(tmpPath));
    PathFor("reply.json", replyPath, sizeof(replyPath));

    file = fopen(tmpPath, "wb");
    if (!file) return 0;
    {
        int ok = fwrite(ReplyBuffer, 1, pos, file) == pos;
        if (fclose(file) != 0) ok = 0;
        if (!ok) return 0;
    }

    /* The client may be reading the previous reply at this instant. */
    for (attempt = 0; attempt < 40; attempt++) {
        if (SDL_RenamePath(tmpPath, replyPath)) {
            EventReported = EventCount;
            return 1;
        }
        SDL_Delay(5);
    }
    /* Keep the last complete reply on failure. In particular, never delete it
       just because a client temporarily holds a file handle. */
    return 0;
}

static void QueueReply(const ACC_BRIDGE_COMMAND *cmd, const char *result, const char *error)
{
    ReplyPending = 1;
    ReplyCmd = *cmd;
    ReplyResult = result;
    ReplyWantsShot = (cmd->shot || cmd->verb == ACC_BRIDGE_SHOT) && strcmp(result, "error") != 0;
    snprintf(ReplyError, sizeof(ReplyError), "%s", error ? error : "");
    ReplyDetail[0] = 0;
    ReplyDeadline = SDL_GetTicks() + SHOT_WAIT_MS;
    ReplyRetryAt = 0;

    /* While time is held, the frame on screen is the one captured at the
       last flip; otherwise wait for a fresh one. */
    ReplyAfterFlip = (StepMode && InGameplay && Frame) ? CaptureFlip : FlipCount + 1;
}

static void FlushReplyIfReady(void)
{
    char shotPath[760];
    const char *shotError = NULL;

    if (!ReplyPending || SDL_GetTicks() < ReplyRetryAt) return;

    shotPath[0] = 0;

    if (ReplyWantsShot) {
        if (!Frame || CaptureFlip < ReplyAfterFlip) {
            if (StepMode && InGameplay && !Frame)
                shotError = "no frame captured yet; send run 50 shot";
            else if (SDL_GetTicks() < ReplyDeadline)
                return;
            else
                shotError = "no frame was rendered in time";
        } else if (!WriteShot(ReplyCmd.seq, shotPath, sizeof(shotPath))) {
            shotError = "could not write the screenshot";
        }
    }

    if (WriteReply(shotPath[0] ? shotPath : NULL, shotError)) {
        ReplyPending = 0;
    } else {
        fprintf(stderr, "AVP Access: bridge could not publish reply %u; retaining it for retry.\n", ReplyCmd.seq);
        fflush(stderr);
        ReplyRetryAt = SDL_GetTicks() + 1000;
    }
}

/* ------------------------------------------------------------- commands -- */

static void Finish(void)
{
    ApplyKeys(NULL, 0);
    ReplyTurned = Runner.turned;
    QueueReply(&Runner.cmd, Runner.result, NULL);
    Runner.active = 0;
    FrameGranted = 0;
}

static void StartCommand(const ACC_BRIDGE_COMMAND *cmd)
{
    char fields[64];
    int yaw = 0, left = -1, right = -1;

    switch (cmd->verb) {
    case ACC_BRIDGE_STATE:
    case ACC_BRIDGE_SHOT:
        QueueReply(cmd, "ok", NULL);
        return;
    case ACC_BRIDGE_STEP:
        AccBridge_NowMs();     /* include elapsed real time before holding it */
        StepMode = 1;
        AddEvent("mode", "\"mode\":\"step\"");
        QueueReply(cmd, "ok", NULL);
        return;
    case ACC_BRIDGE_REALTIME:
        AccBridge_NowMs();     /* start counting wall time from now */
        StepMode = 0;
        AddEvent("mode", "\"mode\":\"realtime\"");
        QueueReply(cmd, "ok", NULL);
        return;
    case ACC_BRIDGE_SOUNDS:
        AllSounds = cmd->allSounds;
        QueueReply(cmd, "ok", NULL);
        return;
    case ACC_BRIDGE_MAP:
    {
        char path[760], error[128];

        if (!PlayerPose(NULL, NULL, NULL, NULL, NULL)) {
            QueueReply(cmd, "error", "map only works during gameplay with a valid player pose");
            return;
        }
        PathFor("map.json", path, sizeof(path));
        error[0] = 0;
        if (!AccMap_Export(path, error, sizeof(error))) {
            QueueReply(cmd, "error", error[0] ? error : "map export failed");
            return;
        }
        QueueReply(cmd, "ok", NULL);
        snprintf(ReplyDetail, sizeof(ReplyDetail), "map.json");
        return;
    }
    case ACC_BRIDGE_QUIT:
        QueueReply(cmd, "ok", NULL);
        FlushReplyIfReady();
        exit(ReplyPending ? EXIT_FAILURE : EXIT_SUCCESS);
    default:
        break;
    }

    if (cmd->verb == ACC_BRIDGE_TURN) {
        if (!PlayerPose(NULL, NULL, NULL, &yaw, NULL)) {
            QueueReply(cmd, "error", "turn only works during gameplay");
            return;
        }
        left = TurnKey(0);
        right = TurnKey(1);
        if (left < 0 || right < 0) {
            QueueReply(cmd, "error", "no turn left/right keys are bound");
            return;
        }
    }

    AccBridgeRunner_Start(&Runner, cmd, StepMode && InGameplay, AccBridge_NowMs(), yaw, left, right);
    FrameGranted = 0;
    FramelessReads = 0;

    snprintf(fields, sizeof(fields), "\"seq\":%u,\"verb\":\"%s\"", cmd->seq, AccBridge_VerbName(cmd->verb));
    AddEvent("command", fields);
}

static void PollCommand(void)
{
    char path[760], text[512];
    ACC_BRIDGE_COMMAND cmd;
    size_t len;
    void *data;

    if (!Started || ReplyPending || Runner.active) return;

    PathFor("command.txt", path, sizeof(path));
    if (!SDL_GetPathInfo(path, NULL)) return;

    data = SDL_LoadFile(path, &len);
    if (!data) return;      /* not readable yet; try on the next poll */
    /* Do not execute until the command has been consumed. Otherwise a failed
       delete could repeat a movement command indefinitely. */
    if (!SDL_RemovePath(path)) {
        SDL_free(data);
        return;
    }

    if (len >= sizeof(text) || memchr(data, 0, len)) {
        /* Parsing only a prefix can turn invalid input into a valid action. */
        memset(&cmd, 0, sizeof(cmd));
        SDL_free(data);
        QueueReply(&cmd, "error", "command too long or contains a null byte");
        return;
    }
    memcpy(text, data, len);
    text[len] = 0;
    SDL_free(data);

    if (!AccBridge_ParseCommand(text, &cmd)) {
        QueueReply(&cmd, "error", cmd.error);
        return;
    }
    StartCommand(&cmd);
}

/* ------------------------------------------------------------ lifecycle -- */

static void BridgeShutdown(void)
{
    char path[760];
    if (EventFile) { fclose(EventFile); EventFile = NULL; }
    if (ReadyOwned) {
        PathFor("ready.json", path, sizeof(path));
        SDL_RemovePath(path);
        ReadyOwned = 0;
    }
}

static void StartupFailure(const char *operation)
{
    fprintf(stderr, "AVP Access: bridge startup failed (%s) in %s\n", operation, Dir);
    fflush(stderr);
    BridgeShutdown();
    exit(EXIT_FAILURE);
}

void AccBridge_Enable(const char *dir, int audible)
{
    Enabled = 1;
    Audible = audible ? 1 : 0;
    LastWallMs = SDL_GetTicks();

    if (dir && dir[0]) {
        size_t n;
        if (strlen(dir) >= sizeof(Dir) - 2) StartupFailure("directory path too long");
        snprintf(Dir, sizeof(Dir) - 1, "%s", dir);
        n = strlen(Dir);
        if (n && Dir[n - 1] != '/' && Dir[n - 1] != '\\') {
            Dir[n] = '/';
            Dir[n + 1] = 0;
        }
    }

    AccSpeech_SetObserver(OnSpeech, !Audible);
}

int AccBridge_IsActive(void) { return Enabled; }
int AccBridge_IsSurvey(void)
{
    const char *value = getenv("AVP_BRIDGE_SURVEY");
    return Enabled && value && !strcmp(value, "1");
}
int AccBridge_Muted(void)    { return Enabled && !Audible; }

void AccBridge_Start(void)
{
    char path[760], readyPath[760];
    FILE *file;
    unsigned long i;

    if (!Enabled || Started) return;

    if (!Dir[0]) {
        /* Beside the executable, using the base path's own separator. */
        const char *base = SDL_GetBasePath();
        size_t n = base ? strlen(base) : 0;
        if (n) snprintf(Dir, sizeof(Dir), "%sbridge%c", base, base[n - 1]);
        else snprintf(Dir, sizeof(Dir), "bridge/");
    }

    if (!SDL_CreateDirectory(Dir)) {
        StartupFailure("create directory");
    }

    PathFor("ready.json", path, sizeof(path));
    if (SDL_GetPathInfo(path, NULL) && !SDL_RemovePath(path)) StartupFailure("remove stale ready file");
    PathFor("command.txt", path, sizeof(path));
    if (SDL_GetPathInfo(path, NULL) && !SDL_RemovePath(path)) StartupFailure("remove stale command");
    PathFor("reply.json", path, sizeof(path));
    if (SDL_GetPathInfo(path, NULL) && !SDL_RemovePath(path)) StartupFailure("remove stale reply");

    PathFor("events.jsonl", path, sizeof(path));
    EventFile = fopen(path, "wb");
    if (!EventFile) StartupFailure("open event log");
    if (EventFile) {
        for (i = EventCount > EVENT_RING ? EventCount - EVENT_RING : 0; i < EventCount; i++) {
            fputs(Events[i % EVENT_RING], EventFile);
            fputc('\n', EventFile);
        }
        if (fflush(EventFile) != 0 || ferror(EventFile)) StartupFailure("write event log");
    }

    PathFor("ready.tmp", path, sizeof(path));
    PathFor("ready.json", readyPath, sizeof(readyPath));
    file = fopen(path, "wb");
    if (!file) StartupFailure("open ready file");
    if (file) {
        char escaped[1400];
        int ok;
        AccBridge_JsonEscape(Dir, escaped, sizeof(escaped));
        ok = fprintf(file, "{\"pid\":%d,\"dir\":\"%s\",\"audible\":%d}\n", (int)ACC_GETPID(), escaped, Audible) > 0;
        if (fclose(file) != 0) ok = 0;
        if (!ok || !SDL_RenamePath(path, readyPath)) StartupFailure("publish ready file");
    }

    ReadyOwned = 1;
    atexit(BridgeShutdown);
    Started = 1;
    fprintf(stderr, "AVP Access: bridge ready in %s (%s)\n", Dir, Audible ? "audible" : "muted");
    if (AccBridge_IsSurvey()) {
        fprintf(stderr, "AVP Access: SURVEY MODE enabled: isolated profile only; player is immortal during bridge gameplay. Damage and combat observations are invalid.\n");
        fflush(stderr);
    }
}

void AccBridge_EnterGameplay(void)
{
    char escaped[128], fields[160];

    if (!Enabled) return;

    AccBridge_NowMs();
    InGameplay = 1;
    SurveyApplyImmortality();
    free(Frame);
    Frame = NULL;

    AccBridge_JsonEscape(LevelName, escaped, sizeof(escaped));
    snprintf(fields, sizeof(fields), "\"level\":\"%s\"", escaped);
    AddEvent("gameplay_started", fields);
}

void AccBridge_LeaveGameplay(void)
{
    if (!Enabled) return;

    AccBridge_NowMs();
    InGameplay = 0;
    SurveyApplyImmortality();
    if (Runner.active && Runner.byFrames) AccBridgeRunner_SwitchToClock(&Runner, AccBridge_NowMs());
    AddEvent("gameplay_ended", NULL);
}

void AccBridge_WaitForFrame(void)
{
    if (!Enabled || !Started) return;
    SurveyApplyImmortality();

    for (;;) {
        int yaw = 0;

        if (!(StepMode && InGameplay)) return;

        if (Runner.active && Runner.byFrames) {
            PlayerPose(NULL, NULL, NULL, &yaw, NULL);
            if (AccBridgeRunner_Done(&Runner, AccBridge_NowMs(), yaw)) Finish();
        }

        FlushReplyIfReady();
        PollCommand();

        if (Runner.active) {
            FrameGranted = 1;
            return;
        }

        /* Pumping alone leaves a close request queued forever while time is
           held. Honor it without consuming gameplay input or advancing time. */
        SDL_PumpEvents();
        if (SDL_HasEvent(SDL_EVENT_QUIT) || SDL_HasEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED)) {
            ApplyKeys(NULL, 0);
            exit(EXIT_SUCCESS);
        }
        SDL_Delay(2);
    }
}

void AccBridge_Service(void)
{
    int keys[ACC_BRIDGE_MAX_KEYS], count;
    unsigned int now;

    if (!Enabled || !Started) return;

    SurveyApplyImmortality();

    now = AccBridge_NowMs();

    if (!(StepMode && InGameplay)) PollCommand();

    if (Runner.active && Runner.byFrames && !FrameGranted) {
        if (++FramelessReads > FRAMELESS_READS) AccBridgeRunner_SwitchToClock(&Runner, now);
    }

    if (Runner.active) {
        if (Runner.byFrames) {
            if (FrameGranted) {
                FrameGranted = 0;
                FramelessReads = 0;
                count = AccBridgeRunner_Keys(&Runner, now, keys, ACC_BRIDGE_MAX_KEYS);
                ApplyKeys(keys, count);
            }
        } else {
            int yaw = 0;
            PlayerPose(NULL, NULL, NULL, &yaw, NULL);
            if (AccBridgeRunner_Done(&Runner, now, yaw)) {
                Finish();
            } else {
                count = AccBridgeRunner_Keys(&Runner, now, keys, ACC_BRIDGE_MAX_KEYS);
                ApplyKeys(keys, count);
            }
        }
    } else if (InjectedCount) {
        ApplyKeys(NULL, 0);
    }

    FlushReplyIfReady();
}

void AccBridge_OnFlip(void)
{
    int needed, w, h;
    unsigned char *pixels;

    if (!Enabled) return;

    FlipCount++;

    /* While time is held every simulated frame is kept, because once time
       stops there is no later frame to capture. Otherwise only on request. */
    needed = (StepMode && InGameplay) || (ReplyPending && ReplyWantsShot);
    if (!needed) return;

    pixels = GetScreenShot24(&w, &h);
    if (!pixels) return;

    free(Frame);
    Frame = pixels;
    FrameW = w;
    FrameH = h;
    CaptureFlip = FlipCount;
}
