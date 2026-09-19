/* Real bridge integration with actual SDL filesystem operations. Only engine
   state, speech and screenshot/input boundaries are mocked. Including the source
   lets failure checks inspect pending state without adding production test APIs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <process.h>
#include <windows.h>
static jmp_buf exit_target;
static int exit_status;
static void test_exit(int status) { exit_status = status; longjmp(exit_target, 1); }
#define exit test_exit
#include "../../src/access/acc_bridge.c"
#undef exit

AVP_GAME_DESC AvP;
DISPLAYBLOCK *Player;
PLAYER_STATUS *PlayerStatusPtr;
MODULE *playerPherModule;
int GlobalFrameCounter;
char LevelName[40] = "fixture";
unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
unsigned char GotAnyKey;
int DebouncedGotAnyKey;
PLAYER_INPUT_CONFIGURATION MarineInputPrimaryConfig, MarineInputSecondaryConfig;
PLAYER_INPUT_CONFIGURATION AlienInputPrimaryConfig, AlienInputSecondaryConfig;
PLAYER_INPUT_CONFIGURATION PredatorInputPrimaryConfig, PredatorInputSecondaryConfig;
static int presses, releases, failures, checks;
void AccSpeech_SetObserver(ACC_SPEECH_OBSERVER observer, int suppress) { (void)observer; (void)suppress; }
int InGameMenusAreRunning(void) { return 0; }
int AccStatus_FormatMarine(const struct player_status *p, char *text, size_t size)
{ (void)p; if (size) text[0] = 0; return 0; }
unsigned char *GetScreenShot24(int *width, int *height)
{ (void)width; (void)height; return NULL; }
void AccBridge_PlatformKey(int key, int press)
{ KeyboardInput[key] = (unsigned char)press; if (press) presses++; else releases++; }

static void check(int condition, const char *description)
{ checks++; if (!condition) { failures++; printf("FAIL: %s\n", description); } }
static void write_bytes(const char *name, const char *data, size_t size)
{
    char path[760]; FILE *file;
    PathFor(name, path, sizeof(path)); file = fopen(path, "wb");
    if (!file) { perror(path); exit(2); }
    if (fwrite(data, 1, size, file) != size || fclose(file)) exit(2);
}
static void command(const char *text) { write_bytes("command.txt", text, strlen(text)); }
static int exists(const char *name)
{ char path[760]; PathFor(name, path, sizeof(path)); return SDL_GetPathInfo(path, NULL); }
static void remove_path(const char *name)
{ char path[760]; PathFor(name, path, sizeof(path)); SDL_RemovePath(path); }
static void directory(const char *name)
{ char path[760]; PathFor(name, path, sizeof(path)); if (!SDL_CreateDirectory(path)) exit(2); }
static int reply_contains(const char *text)
{
    char path[760]; void *data; int found;
    PathFor("reply.json", path, sizeof(path)); data = SDL_LoadFile(path, NULL);
    if (!data) return 0;
    found = strstr((const char *)data, text) != NULL; SDL_free(data); return found;
}

static void reply_retry(void)
{
    AccBridge_Start();
    OnSpeech("event retained across failed publication", 1);
    command("1 hold w 10"); AccBridge_Service();
    check(Runner.active && presses == 1, "hold starts through engine input boundary");
    directory("reply.tmp");
    Runner.endMs = AccBridge_NowMs(); AccBridge_Service();
    check(!Runner.active && releases == 1 && !KeyboardInput[KEY_W], "held key releases even when reply write fails");
    check(ReplyPending && EventReported == 0 && !exists("reply.json"), "failed write retains pending reply and undelivered events");
    command("2 tap a"); AccBridge_Service();
    check(exists("command.txt") && presses == 1, "next command waits for pending response");
    remove_path("reply.tmp"); ReplyRetryAt = 0; AccBridge_Service();
    check(!ReplyPending && reply_contains("\"seq\":1") && reply_contains("event retained"), "retry publishes original reply with retained events");
    check(EventReported == EventCount, "events count delivered only after publication");
    AccBridge_Service();
    check(Runner.active && Runner.cmd.seq == 2 && presses == 2, "queued command starts once after recovery");
    ApplyKeys(NULL, 0); Runner.active = 0;
}

static void invalid_commands(void)
{
    char oversized[600];
    const char binary[] = "4 tap w\0ignored";
    AccBridge_Start();
    memset(oversized, ' ', sizeof(oversized)); memcpy(oversized, "3 tap w", 7);
    write_bytes("command.txt", oversized, sizeof(oversized)); AccBridge_Service();
    check(!Runner.active && !presses && reply_contains("\"result\":\"error\""), "oversized valid prefix cannot execute");
    write_bytes("command.txt", binary, sizeof(binary)-1); AccBridge_Service();
    check(!Runner.active && !presses && reply_contains("null byte"), "embedded null cannot hide trailing command data");
    command("5 state"); AccBridge_Service();
    check(reply_contains("\"seq\":5") && reply_contains("\"result\":\"ok\""), "valid command still works after rejected input");
}

static void locked_command(void)
{
    char path[760]; HANDLE lock;
    AccBridge_Start(); command("6 tap w"); PathFor("command.txt", path, sizeof(path));
    lock = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "fixture locks command against deletion");
    AccBridge_Service();
    check(!Runner.active && !presses && exists("command.txt"), "undeletable command is not executed");
    CloseHandle(lock); AccBridge_Service();
    check(Runner.active && presses == 1 && !exists("command.txt"), "unlocked command executes once");
    ApplyKeys(NULL, 0); Runner.active = 0;
}

static void rename_retry(void)
{
    char path[760]; HANDLE lock;
    AccBridge_Start(); command("7 state"); AccBridge_Service();
    PathFor("reply.json", path, sizeof(path));
    lock = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "fixture locks previous reply against replacement");
    OnSpeech("after lock", 0); command("8 state"); AccBridge_Service();
    check(ReplyPending && reply_contains("\"seq\":7"), "publication failure preserves previous complete reply");
    CloseHandle(lock); ReplyRetryAt = 0; AccBridge_Service();
    check(!ReplyPending && reply_contains("\"seq\":8") && reply_contains("after lock"), "unlock recovers without losing reply events");
}

static void startup_failure(const char *which)
{
    directory(!strcmp(which, "startup_log") ? "events.jsonl" : "ready.tmp");
    if (!setjmp(exit_target)) { AccBridge_Start(); check(0, "startup should fail"); }
    else check(exit_status == EXIT_FAILURE, "unwritable startup file returns failure");
    check(!Started && !exists("ready.json"), "failed startup never publishes readiness");
}

static void held_close(void)
{
    SDL_Event event;
    int key = KEY_W;
    AccBridge_Start();
    check(SDL_Init(SDL_INIT_EVENTS), "SDL event queue initialized without a window");
    memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_QUIT;
    check(SDL_PushEvent(&event), "close event queued while time is held");
    InGameplay = 1; ApplyKeys(&key, 1);
    if (!setjmp(exit_target)) { AccBridge_WaitForFrame(); check(0, "held close must exit"); }
    else check(exit_status == EXIT_SUCCESS, "held loop honors queued close");
    check(releases == 1 && !KeyboardInput[KEY_W] && !Runner.active, "close releases injected input without running a frame");
    SDL_Quit();
}

int main(int argc, char **argv)
{
    char root[760];
    if (argc != 3) return 2;
    snprintf(root, sizeof(root), "%s", argv[2]);
    AccBridge_Enable(root, 0);
    if (!SDL_CreateDirectory(root)) return 2;
    if (!strcmp(argv[1], "client_server")) {
        /* Headless helper named avp.exe by the client test. This runs the real
           filesystem protocol, with no game, window, retail data or audio. */
        if (setjmp(exit_target)) { BridgeShutdown(); return exit_status; }
        AccBridge_Start();
        for (;;) { AccBridge_Service(); SDL_Delay(1); }
    }
    if (!strcmp(argv[1], "reply_retry")) reply_retry();
    else if (!strcmp(argv[1], "invalid")) invalid_commands();
    else if (!strcmp(argv[1], "locked_command")) locked_command();
    else if (!strcmp(argv[1], "rename_retry")) rename_retry();
    else if (!strcmp(argv[1], "held_close")) held_close();
    else if (!strncmp(argv[1], "startup_", 8)) startup_failure(argv[1]);
    else return 2;
    BridgeShutdown();
    check(!exists("ready.json"), "shutdown removes readiness marker");
    remove_path("events.jsonl"); remove_path("ready.tmp"); remove_path("reply.tmp");
    remove_path("reply.json"); remove_path("command.txt"); SDL_RemovePath(root);
    printf("%s: %d checks, %d failures\n", argv[1], checks, failures);
    return failures ? 1 : 0;
}
