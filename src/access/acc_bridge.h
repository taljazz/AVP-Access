/* AVP Access: the play bridge.
 *
 * Lets a program outside the game -- in practice an AI assistant developing
 * this mod -- play it: send a command, get back what happened. Started with
 * --bridge, and inert otherwise.
 *
 * The protocol is files in one directory (default: "bridge" beside avp.exe),
 * so any process can drive it and every exchange can be inspected:
 *
 *   command.txt   "<seq> <verb> [arguments]", written by the client
 *   reply.json    the answer to the last command, carrying its seq
 *   events.jsonl  every spoken line and accessibility sound, timestamped
 *   shot-N.png    screenshots requested with "shot"
 *   ready.json    written at startup: process id and directory
 *
 * Write commands atomically (temporary file, then rename to command.txt) and
 * match reply.json by sequence number. tools/bridge.ps1 handles this and guards
 * against concurrent clients and resubmitting after a timeout.
 *
 * Time. In step mode (the default) gameplay advances only while a command is
 * running, in fixed 1/30 s steps, and stops when it finishes -- so the player
 * cannot be hurt while the client is thinking. Menus and loading screens are
 * never frozen; commands there run against the wall clock. "realtime" lets
 * the game run freely.
 *
 * Input goes through the same path as a real keyboard, so it exercises the
 * player's actual bindings and every accessibility shortcut.
 *
 * Output. Speech requests and tagged accessibility sound starts (tracker
 * contact tones and scan clicks, sonar) are logged with simulation time, and, for positioned sounds, where
 * the sound was relative to the player. By default speech does not reach the
 * screen reader and the game is muted, so a session does not talk over the
 * person at the computer; --bridge-audible keeps both. Events do not measure
 * audible onset, sound completion, or FFmpeg voiceover playback.
 */
#ifndef ACC_BRIDGE_H
#define ACC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

struct vectorch;

/* From the command line. `dir` may be NULL for the default. */
void AccBridge_Enable(const char *dir, int audible);
int  AccBridge_IsActive(void);
/* Opt-in isolated survey mode: bridge active and AVP_BRIDGE_SURVEY exactly "1". */
int  AccBridge_IsSurvey(void);

/* Whether game audio should be silenced (bridge active and not audible). */
int  AccBridge_Muted(void);

/* Opens the bridge directory once SDL is up. */
void AccBridge_Start(void);

/* The gameplay loop brackets itself with these, so the bridge knows when
   time is its to hold. */
void AccBridge_EnterGameplay(void);
void AccBridge_LeaveGameplay(void);

/* Top of each gameplay frame. In step mode, blocks until a command wants a
   frame, answering commands that need none meanwhile. */
void AccBridge_WaitForFrame(void);

/* End of each input read (CheckForWindowsMessages), after the keyboard,
   mouse and pad have been read: applies held keys and finishes commands that
   run against the clock. */
void AccBridge_Service(void);

/* FrameCounterHandler: the fixed frame time to use, or 0 for real time. */
int  AccBridge_FixedFrameTime(void);

/* Immediately before a buffer swap: captures the frame when one is needed. */
void AccBridge_OnFlip(void);

/* The accessibility clock in milliseconds. Real time normally; while the
   bridge holds time, it advances only with simulated frames, so timed cues
   (the sonar's half-second pings) keep their spacing in game time. */
unsigned int AccBridge_NowMs(void);

/* Sounds started between these are accessibility cues. `source` names the
   feature ("tracker", "sonar"); a nested call keeps the outer source, so a
   feature can label the shared tracker playback it goes through. When
   `sound` is not negative and nothing played by EndCue, the cue is logged as
   dropped -- a silent cue is otherwise indistinguishable from no cue. */
void AccBridge_BeginCue(const char *source, int sound);
void AccBridge_EndCue(void);

/* Sound_Play started a sound. `position` is NULL for a sound without one. */
void AccBridge_OnSound(int sound, const char *name, const struct vectorch *position,
                       int volume, int pitch, int loop, int innerRange, int outerRange);

/* Implemented by the platform layer (main.c): press or release a key exactly
   as the keyboard handler would. */
void AccBridge_PlatformKey(int key, int press);

#ifdef __cplusplus
}
#endif

#endif /* ACC_BRIDGE_H */
