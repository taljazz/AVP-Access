/* AVP Access: the engine-free half of the play bridge -- see acc_bridge.h.
 *
 * Command parsing, key names, JSON text, listener-relative geometry, PNG
 * encoding and the per-command runner that decides which keys are held on
 * each frame. None of it touches engine or SDL state, so the whole protocol
 * can be checked headless.
 */
#ifndef ACC_BRIDGE_CORE_H
#define ACC_BRIDGE_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keys one command may hold together, e.g. forward+walk+strafe. */
#define ACC_BRIDGE_MAX_KEYS 4

/* The simulation step while the bridge owns time: 1/30 second, in the
   engine's 16.16 fixed point (NormalFrameTime units). */
#define ACC_BRIDGE_STEPS_PER_SECOND 30
#define ACC_BRIDGE_STEP_FIXED       (65536 / ACC_BRIDGE_STEPS_PER_SECOND)

/* Longest single timed command. Long enough to cross a room, short enough
   that a mistyped duration cannot hang a session for minutes. */
#define ACC_BRIDGE_MAX_MS 60000

/* Engine yaw: 4096 units per turn. */
#define ACC_BRIDGE_YAW_TURN 4096

/* A turn is finished when within this many yaw units (about 2 degrees). */
#define ACC_BRIDGE_TURN_TOLERANCE 24

/* A turn gives up after this many frames holding a key without moving. */
#define ACC_BRIDGE_TURN_STALL_FRAMES 10

/* ... or after this long in total. */
#define ACC_BRIDGE_TURN_MAX_FRAMES (15 * ACC_BRIDGE_STEPS_PER_SECOND)
#define ACC_BRIDGE_TURN_MAX_MS     15000

/* A tap is held for two frames and released for one, so the engine sees a
   press edge and then a release before the next command. In wall-clock time
   (menus, loading screens) the same shape is 100 ms down and 60 ms up. */
#define ACC_BRIDGE_TAP_DOWN_FRAMES 2
#define ACC_BRIDGE_TAP_FRAMES      3
#define ACC_BRIDGE_TAP_DOWN_MS     100
#define ACC_BRIDGE_TAP_MS          160

typedef enum {
    ACC_BRIDGE_NONE,
    ACC_BRIDGE_STATE,     /* report, change nothing */
    ACC_BRIDGE_STEP,      /* time advances only when a command asks */
    ACC_BRIDGE_REALTIME,  /* time runs normally */
    ACC_BRIDGE_RUN,       /* let time pass with no input */
    ACC_BRIDGE_HOLD,      /* hold keys for a duration */
    ACC_BRIDGE_TAP,       /* press and release keys */
    ACC_BRIDGE_TURN,      /* turn by an angle using the player's turn keys */
    ACC_BRIDGE_SHOT,      /* capture the screen */
    ACC_BRIDGE_SOUNDS,    /* log every game sound, or accessibility cues only */
    ACC_BRIDGE_QUIT,      /* close the game */
    ACC_BRIDGE_MAP        /* export local map data; does not advance the game */
} ACC_BRIDGE_VERB;

typedef struct {
    unsigned int    seq;
    ACC_BRIDGE_VERB verb;
    int             ms;
    int             keys[ACC_BRIDGE_MAX_KEYS];
    int             keyCount;
    int             degrees;       /* turn: positive is right */
    int             shot;          /* capture a screenshot when finished */
    int             allSounds;     /* sounds: 1 = all, 0 = accessibility cues only */
    char            error[96];
} ACC_BRIDGE_COMMAND;

/* Parses "<seq> <verb> [arguments] [shot]". Returns 1 on success. On failure
   returns 0 with cmd->error set; cmd->seq is still filled in when the
   sequence number itself was readable, so the error can be answered. */
int AccBridge_ParseCommand(const char *text, ACC_BRIDGE_COMMAND *cmd);

const char *AccBridge_VerbName(ACC_BRIDGE_VERB verb);

/* Key names are case-insensitive: letters, digits, "enter", "space",
   "lshift", "f1", "num4", "lmouse", "joy12", "pad_a", ... Returns -1 for an
   unknown name. */
int AccBridge_KeyFromName(const char *name);

/* The canonical name for a key, or NULL. */
const char *AccBridge_KeyName(int key);

/* Writes `in` as the inside of a JSON string. Game text is 8-bit, so bytes
   above 0x7F are written as \u00XX. Never splits an escape; always
   terminates a nonempty buffer. Returns the length written. */
size_t AccBridge_JsonEscape(const char *in, char *out, size_t size);

typedef struct {
    int bearing;     /* degrees, -180..180, positive to the right */
    int clock;       /* 1..12, 12 ahead, 3 right */
    int distance;    /* horizontal millimetres */
} ACC_BRIDGE_RELATIVE;

/* Where a world point lies relative to a listener at (px, pz) facing yaw. */
void AccBridge_Relative(int px, int pz, int yaw, int sx, int sz, ACC_BRIDGE_RELATIVE *out);

/* Simulation frames needed to cover `ms`, at least one. */
int AccBridge_FramesForMs(int ms);

/* A complete PNG of 8-bit RGB pixels, rows bottom-up if `bottomUp` (as
   glReadPixels returns them). Uncompressed deflate, so no zlib is needed.
   malloc'd; free() it. NULL on bad arguments or allocation failure. */
unsigned char *AccBridge_EncodePng(const unsigned char *rgb, int width, int height,
                                   int bottomUp, size_t *size);

/* Halves an RGB image in both directions by averaging 2x2 blocks. An odd last
   row or column is dropped. malloc'd; NULL on bad arguments. */
unsigned char *AccBridge_HalveRgb(const unsigned char *rgb, int width, int height,
                                  int *outWidth, int *outHeight);

/* ---------------------------------------------------------------- runner -- */

/* Runs one timed command. It is either counted in simulation frames (when
   the bridge owns time during gameplay) or in clock milliseconds (menus,
   loading screens, real-time mode).

   Per frame, in order: AccBridgeRunner_Done() sees the result of the frame
   just simulated; if not done, AccBridgeRunner_Keys() says what to hold for
   the next one. */
typedef struct {
    int                active;
    ACC_BRIDGE_COMMAND cmd;
    int                byFrames;
    int                framesNeeded;
    int                framesDone;
    unsigned int       startMs;
    unsigned int       endMs;
    /* turn */
    int                target;       /* yaw units, signed */
    int                turned;       /* yaw units moved so far, signed */
    int                lastYaw;
    int                lastDelta;
    int                heldStep;     /* largest movement in one frame with a key held */
    int                coastRun;     /* movement since the key was released */
    int                coast;        /* largest such run: how far a release carries */
    int                stallFrames;
    int                turnKey;      /* key held for the last frame, or -1 */
    int                leftKey;
    int                rightKey;
    const char        *result;       /* "ok", "stalled", "timeout", "wrong_direction" */
} ACC_BRIDGE_RUNNER;

void AccBridgeRunner_Start(ACC_BRIDGE_RUNNER *r, const ACC_BRIDGE_COMMAND *cmd,
                           int byFrames, unsigned int nowMs, int yaw,
                           int leftKey, int rightKey);

/* Returns 1 once the command has finished; r->result says how. */
int AccBridgeRunner_Done(ACC_BRIDGE_RUNNER *r, unsigned int nowMs, int yaw);

/* Keys to hold now. Counts a frame when frame-timed. Returns the count. */
int AccBridgeRunner_Keys(ACC_BRIDGE_RUNNER *r, unsigned int nowMs, int *keys, int max);

/* Frames stopped arriving (the level ended mid-command): finish the rest of
   the command against the clock instead. */
void AccBridgeRunner_SwitchToClock(ACC_BRIDGE_RUNNER *r, unsigned int nowMs);

#ifdef __cplusplus
}
#endif

#endif /* ACC_BRIDGE_CORE_H */
