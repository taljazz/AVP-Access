/* Tests the actual acc_bridge_core.c: command parsing, key names, JSON text,
 * listener-relative geometry, PNG encoding and the per-command runner. The
 * core touches no engine or SDL state, so nothing is mocked -- the runner is
 * driven with a simulated player whose yaw responds to the turn keys.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixer.h"
#include "3dc.h"
#include "acc_bridge_core.h"

static int assertions, failures;

static void check(int condition, const char *what)
{
    ++assertions;
    if (condition) printf("PASS: %s\n", what);
    else { ++failures; printf("FAIL: %s\n", what); }
}

static int parse(const char *text, ACC_BRIDGE_COMMAND *cmd)
{
    return AccBridge_ParseCommand(text, cmd);
}

/* ------------------------------------------------------------- parsing -- */

static void test_parse_basic(void)
{
    ACC_BRIDGE_COMMAND cmd;

    check(parse("12 state", &cmd) == 1, "a bare command parses");
    check(cmd.seq == 12 && cmd.verb == ACC_BRIDGE_STATE, "sequence and verb are read");
    check(!cmd.shot && !cmd.keyCount, "a bare command carries no options");

    check(parse("1 step", &cmd) && cmd.verb == ACC_BRIDGE_STEP, "step parses");
    check(parse("1 realtime", &cmd) && cmd.verb == ACC_BRIDGE_REALTIME, "realtime parses");
    check(parse("1 shot", &cmd) && cmd.verb == ACC_BRIDGE_SHOT, "shot parses");
    check(parse("1 map", &cmd) && cmd.verb == ACC_BRIDGE_MAP && !cmd.keyCount && !cmd.ms,
          "map parses without arguments");
    check(parse("1 quit", &cmd) && cmd.verb == ACC_BRIDGE_QUIT, "quit parses");
    check(parse("1 sounds all", &cmd) && cmd.verb == ACC_BRIDGE_SOUNDS && cmd.allSounds == 1,
          "sounds all turns every sound on");
    check(parse("1 sounds access", &cmd) && cmd.allSounds == 0,
          "sounds access limits logging to cues");
    check(parse("4000000000 state", &cmd) == 0, "a sequence number past int range is refused");
}

static void test_parse_hold(void)
{
    ACC_BRIDGE_COMMAND cmd;

    check(parse("3 hold w+lshift 1500 shot", &cmd) == 1, "hold with two keys, a duration and shot parses");
    check(cmd.verb == ACC_BRIDGE_HOLD && cmd.ms == 1500 && cmd.shot == 1, "duration and shot are read");
    check(cmd.keyCount == 2 && cmd.keys[0] == KEY_W && cmd.keys[1] == KEY_LEFTSHIFT,
          "combined keys are read in order");

    check(parse("3 hold 500 w", &cmd) == 1 && cmd.ms == 500 && cmd.keys[0] == KEY_W,
          "duration may come before the keys");

    check(parse("3 hold w+w 200", &cmd) == 1 && cmd.keyCount == 1, "a repeated key is held once");

    check(parse("9 run 1000", &cmd) == 1 && cmd.verb == ACC_BRIDGE_RUN && cmd.ms == 1000 && !cmd.keyCount,
          "run takes a duration and no keys");
    check(parse("9 tap enter shot", &cmd) == 1 && cmd.verb == ACC_BRIDGE_TAP && cmd.keys[0] == KEY_CR
          && cmd.shot, "tap takes keys and shot");
}

static void test_parse_turn(void)
{
    ACC_BRIDGE_COMMAND cmd;

    check(parse("5 turn -90", &cmd) == 1 && cmd.degrees == -90, "a negative angle turns left");
    check(parse("5 turn 45 shot", &cmd) == 1 && cmd.degrees == 45 && cmd.shot, "a positive angle turns right");
    check(parse("5 turn left 30", &cmd) == 1 && cmd.degrees == -30, "turn left N is a left turn");
    check(parse("5 turn right 180", &cmd) == 1 && cmd.degrees == 180, "turn right N is a right turn");
    check(parse("5 turn left -30", &cmd) == 0, "a direction with a signed angle is refused");
    check(parse("5 turn 400", &cmd) == 0, "more than a full turn is refused");
    check(parse("5 turn 0", &cmd) == 0, "a zero turn is refused");
    check(parse("5 turn left", &cmd) == 0, "a direction with no angle is refused");
}

static void test_parse_errors(void)
{
    ACC_BRIDGE_COMMAND cmd;

    check(parse("", &cmd) == 0 && cmd.error[0], "empty text is an error with a message");
    check(parse("state", &cmd) == 0 && strstr(cmd.error, "sequence"), "a missing sequence number is named");
    check(parse("7", &cmd) == 0 && cmd.seq == 7, "a missing verb still reports the sequence");
    check(parse("7 dance", &cmd) == 0 && cmd.seq == 7 && strstr(cmd.error, "dance"),
          "an unknown verb is named and keeps its sequence");
    check(parse("7 hold w", &cmd) == 0 && strstr(cmd.error, "duration"), "hold without a duration is refused");
    check(parse("7 hold 500", &cmd) == 0 && strstr(cmd.error, "keys"), "hold without keys is refused");
    check(parse("7 run", &cmd) == 0, "run without a duration is refused");
    check(parse("7 run 0", &cmd) == 0, "a zero duration is refused");
    check(parse("7 run 60001", &cmd) == 0, "a duration past the limit is refused");
    check(parse("7 run 60000", &cmd) == 1, "the longest duration is allowed");
    check(parse("7 tap nosuchkey", &cmd) == 0 && strstr(cmd.error, "nosuchkey"), "an unknown key is named");
    check(parse("7 tap a+b+c+d+e", &cmd) == 0 && strstr(cmd.error, "too many"), "a fifth key is refused");
    check(parse("7 tap a++b", &cmd) == 0, "an empty key between pluses is refused");
    check(parse("7 tap a+", &cmd) == 0, "a trailing empty key is refused");
    check(parse("7 run 4294967396", &cmd) == 0, "overflow cannot wrap a duration to 100 ms");
    check(parse("4294967297 state", &cmd) == 0, "overflow cannot wrap a sequence to one");
    check(parse("7 turn -4294967386", &cmd) == 0, "overflow cannot wrap a turn to minus 90 degrees");
    check(parse("7 state extra", &cmd) == 0 && strstr(cmd.error, "extra"), "an unexpected argument is named");
    check(parse("7 run 100 200", &cmd) == 0, "a second duration is refused");
    check(parse("7 sounds loud", &cmd) == 0, "an unknown sounds setting is refused");
    check(parse("7 shot shot", &cmd) == 0, "shot does not take a shot option");
    check(parse("7 map extra", &cmd) == 0 && strstr(cmd.error, "extra"),
          "map refuses arguments");
    check(parse("7 tap a b c d e f g h i", &cmd) == 0 && cmd.seq == 7 && strstr(cmd.error, "too long"),
          "too many words are refused with a correlatable sequence");
    check(parse(NULL, &cmd) == 0, "a NULL command is refused");
}

static void test_parse_whitespace(void)
{
    ACC_BRIDGE_COMMAND cmd;

    check(parse("  7\ttap  ENTER \r\n", &cmd) == 1, "tabs, repeated spaces and CRLF are tolerated");
    check(cmd.seq == 7 && cmd.keys[0] == KEY_CR, "verbs and key names ignore case");
    check(parse("8 HOLD W 100 SHOT", &cmd) == 1 && cmd.shot && cmd.keys[0] == KEY_W,
          "upper-case commands parse");
}

/* ----------------------------------------------------------------- keys -- */

static void test_keys(void)
{
    int k, roundTrips = 1;

    check(AccBridge_KeyFromName("a") == KEY_A && AccBridge_KeyFromName("Z") == KEY_Z, "letters map");
    check(AccBridge_KeyFromName("0") == KEY_0 && AccBridge_KeyFromName("9") == KEY_9, "digits map");
    check(AccBridge_KeyFromName("enter") == KEY_CR && AccBridge_KeyFromName("return") == KEY_CR,
          "enter has an alias");
    check(AccBridge_KeyFromName("esc") == KEY_ESCAPE, "esc is escape");
    check(AccBridge_KeyFromName("num4") == KEY_NUMPAD4, "numeric keypad keys map");
    check(AccBridge_KeyFromName("lmouse") == KEY_LMOUSE, "mouse buttons map");
    check(AccBridge_KeyFromName("f12") == KEY_F12, "function keys map");
    check(AccBridge_KeyFromName("joy1") == KEY_JOYSTICK_BUTTON_1
          && AccBridge_KeyFromName("joy16") == KEY_JOYSTICK_BUTTON_16, "raw joystick buttons map");
    check(AccBridge_KeyFromName("joy0") == -1 && AccBridge_KeyFromName("joy17") == -1
          && AccBridge_KeyFromName("joy1x") == -1, "joystick numbers outside 1-16 are refused");
    check(AccBridge_KeyFromName("pad_r3") == KEY_JOYSTICK_BUTTON_12, "R3 is joystick button 12");
    check(AccBridge_KeyFromName("pad_right") == KEY_JOYSTICK_BUTTON_16, "D-pad right is button 16");
    check(AccBridge_KeyFromName("pad_view") == KEY_JOYSTICK_BUTTON_9, "View is button 9");
    check(AccBridge_KeyFromName("") == -1 && AccBridge_KeyFromName(NULL) == -1, "empty names are refused");
    check(AccBridge_KeyFromName("ab") == -1, "two letters are not a key");

    for (k = 0; k < MAX_NUMBER_OF_INPUT_KEYS; k++) {
        const char *name = AccBridge_KeyName(k);
        if (name && AccBridge_KeyFromName(name) != k) roundTrips = 0;
    }
    check(roundTrips, "every named key maps back to itself");
    check(AccBridge_KeyName(KEY_CR) && !strcmp(AccBridge_KeyName(KEY_CR), "enter"),
          "the first alias is the canonical name");
    check(AccBridge_KeyName(KEY_VOID) == NULL, "an unbound slot has no name");
}

/* ----------------------------------------------------------------- JSON -- */

static void test_json(void)
{
    char out[64];
    char narrow[8];
    size_t n;

    n = AccBridge_JsonEscape("say \"hi\"\\ok", out, sizeof(out));
    check(!strcmp(out, "say \\\"hi\\\"\\\\ok") && n == strlen(out), "quotes and backslashes are escaped");

    AccBridge_JsonEscape("a\nb\tc\rd", out, sizeof(out));
    check(!strcmp(out, "a\\nb\\tc\\rd"), "newlines, tabs and returns are escaped");

    AccBridge_JsonEscape("\x01" "x", out, sizeof(out));
    check(!strcmp(out, "\\u0001x"), "other control characters use \\u escapes");

    AccBridge_JsonEscape("caf\xe9", out, sizeof(out));
    check(!strcmp(out, "caf\\u00e9"), "8-bit game text is written as Latin-1 escapes");

    n = AccBridge_JsonEscape("abc\"", narrow, 5);
    check(!strcmp(narrow, "abc") && n == 3, "an escape that does not fit is left out whole");

    n = AccBridge_JsonEscape("abcdefghijkl", narrow, sizeof(narrow));
    check(n == 7 && narrow[7] == 0, "a long string is truncated and terminated");

    check(AccBridge_JsonEscape(NULL, out, sizeof(out)) == 0 && out[0] == 0, "NULL writes an empty string");
}

/* ------------------------------------------------------------- geometry -- */

static void test_relative(void)
{
    ACC_BRIDGE_RELATIVE r;

    AccBridge_Relative(0, 0, 0, 0, 5000, &r);
    check(r.bearing == 0 && r.clock == 12 && r.distance == 5000, "a point along +Z is ahead at yaw 0");

    AccBridge_Relative(0, 0, 0, 3000, 0, &r);
    check(r.bearing == 90 && r.clock == 3, "+X is to the right at yaw 0");

    AccBridge_Relative(0, 0, 0, -3000, 0, &r);
    check(r.bearing == -90 && r.clock == 9, "-X is to the left at yaw 0");

    AccBridge_Relative(0, 0, 0, 0, -2000, &r);
    check((r.bearing == 180 || r.bearing == -180) && r.clock == 6, "-Z is behind");

    AccBridge_Relative(0, 0, 1024, 3000, 0, &r);
    check(r.bearing == 0 && r.clock == 12, "facing +X, a point along +X is ahead");

    AccBridge_Relative(1000, 1000, 1024, 1000, 4000, &r);
    check(r.bearing == -90 && r.distance == 3000, "the listener's own position is subtracted");

    AccBridge_Relative(0, 0, -3072, 3000, 0, &r);
    check(r.bearing == 0, "a negative yaw wraps");

    AccBridge_Relative(0, 0, 0, 3000, 3000, &r);
    check(r.bearing == 45 && r.distance == 4243, "diagonals use true distance");
}

static void test_frames(void)
{
    check(AccBridge_FramesForMs(1) == 1, "any duration is at least one frame");
    check(AccBridge_FramesForMs(33) == 1, "33 ms is one frame");
    check(AccBridge_FramesForMs(34) == 2, "34 ms rounds up to two");
    check(AccBridge_FramesForMs(1000) == 30, "one second is thirty frames");
    check(AccBridge_FramesForMs(1500) == 45, "a second and a half is forty-five");
    check(AccBridge_FramesForMs(0) == 1 && AccBridge_FramesForMs(-5) == 1, "nonsense still yields a frame");
}

/* ------------------------------------------------------------------ PNG -- */

static unsigned long ReadBE32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16)
         | ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static unsigned long TestCrc(const unsigned char *data, size_t len)
{
    unsigned long c = 0xFFFFFFFFUL;
    size_t i;
    int k;
    for (i = 0; i < len; i++) {
        c ^= data[i];
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
    }
    return c ^ 0xFFFFFFFFUL;
}

/* Walks the chunks, checks every CRC, and inflates the stored blocks into
   `raw`. Returns the raw length, or -1 on any structural fault. */
static long DecodeStored(const unsigned char *png, size_t size, int *w, int *h,
                         unsigned char *raw, size_t rawCap, int *crcOk, int *adlerOk)
{
    static const unsigned char Sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    size_t pos = 8;
    long rawLen = 0;
    int sawEnd = 0;

    *crcOk = 1;
    *adlerOk = 0;
    if (size < 8 || memcmp(png, Sig, 8)) return -1;

    while (pos + 12 <= size) {
        unsigned long len = ReadBE32(png + pos);
        const unsigned char *type = png + pos + 4;
        const unsigned char *data = png + pos + 8;

        if (pos + 12 + len > size) return -1;
        if (TestCrc(type, len + 4) != ReadBE32(data + len)) *crcOk = 0;

        if (!memcmp(type, "IHDR", 4)) {
            *w = (int)ReadBE32(data);
            *h = (int)ReadBE32(data + 4);
            if (data[8] != 8 || data[9] != 2) return -1;
        } else if (!memcmp(type, "IDAT", 4)) {
            size_t z = 2;
            unsigned long a = 1, b = 0;
            int final = 0;
            if (len < 6 || data[0] != 0x78 || (data[0] * 256 + data[1]) % 31) return -1;
            while (!final) {
                unsigned blen, nlen, i;
                if (z + 5 > len) return -1;
                final = data[z] & 1;
                if ((data[z] >> 1) & 3) return -1;       /* stored blocks only */
                blen = data[z + 1] | (data[z + 2] << 8);
                nlen = data[z + 3] | (data[z + 4] << 8);
                if ((blen ^ 0xFFFF) != nlen) return -1;
                z += 5;
                if (z + blen > len || (size_t)rawLen + blen > rawCap) return -1;
                for (i = 0; i < blen; i++) {
                    raw[rawLen++] = data[z + i];
                    a = (a + data[z + i]) % 65521UL;
                    b = (b + a) % 65521UL;
                }
                z += blen;
            }
            *adlerOk = (z + 4 == len) && ReadBE32(data + z) == ((b << 16) | a);
        } else if (!memcmp(type, "IEND", 4)) {
            sawEnd = 1;
        }
        pos += 12 + len;
    }
    return sawEnd && pos == size ? rawLen : -1;
}

static void test_png_small(void)
{
    /* 3x2, stored bottom-up as glReadPixels returns it. Bottom row first:
       red, green, blue; top row: white, black, grey. */
    const unsigned char rgb[18] = {
        255,0,0,  0,255,0,  0,0,255,
        255,255,255,  0,0,0,  128,128,128
    };
    unsigned char raw[64];
    size_t size;
    unsigned char *png = AccBridge_EncodePng(rgb, 3, 2, 1, &size);
    int w = 0, h = 0, crcOk, adlerOk;
    long rawLen;
    FILE *f;

    check(png != NULL && size > 0, "a small image encodes");
    if (!png) return;

    rawLen = DecodeStored(png, size, &w, &h, raw, sizeof(raw), &crcOk, &adlerOk);
    check(rawLen == 2 * (1 + 9), "the image data has a filter byte per row");
    check(w == 3 && h == 2, "the header carries the size");
    check(crcOk, "every chunk CRC is correct");
    check(adlerOk, "the zlib checksum is correct");
    check(raw[0] == 0 && raw[10] == 0, "rows use no filter");
    check(raw[1] == 255 && raw[2] == 255 && raw[3] == 255, "the top row comes first (flipped from bottom-up)");
    check(raw[11] == 255 && raw[12] == 0 && raw[13] == 0, "the bottom row comes last");

    /* Left for the PowerShell decode check in run_tests.bat. */
    f = fopen("bridge_small.png", "wb");
    if (f) {
        fwrite(png, 1, size, f);
        fclose(f);
    }
    free(png);

    png = AccBridge_EncodePng(rgb, 3, 2, 0, &size);
    if (png) {
        DecodeStored(png, size, &w, &h, raw, sizeof(raw), &crcOk, &adlerOk);
        check(raw[1] == 255 && raw[2] == 0 && raw[3] == 0, "top-down input is written in its own order");
        free(png);
    }

    check(AccBridge_EncodePng(NULL, 3, 2, 1, &size) == NULL && size == 0, "no pixels encodes nothing");
    check(AccBridge_EncodePng(rgb, 0, 2, 1, &size) == NULL, "a zero width encodes nothing");
}

static void test_png_large(void)
{
    /* 300x200 is 180,200 bytes of image data: three stored blocks. */
    const int w = 300, h = 200;
    unsigned char *rgb = (unsigned char *)malloc((size_t)w * h * 3);
    unsigned char *raw = (unsigned char *)malloc((size_t)(w * 3 + 1) * h + 16);
    unsigned char *png;
    size_t size;
    int dw, dh, crcOk, adlerOk, x, y, match = 1;
    long rawLen;

    if (!rgb || !raw) { check(0, "allocate test image"); free(rgb); free(raw); return; }

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            unsigned char *p = rgb + ((size_t)y * w + x) * 3;
            p[0] = (unsigned char)x; p[1] = (unsigned char)y; p[2] = (unsigned char)(x ^ y);
        }

    png = AccBridge_EncodePng(rgb, w, h, 0, &size);
    check(png != NULL, "a large image encodes");
    if (png) {
        rawLen = DecodeStored(png, size, &dw, &dh, raw, (size_t)(w * 3 + 1) * h + 16, &crcOk, &adlerOk);
        check(rawLen == (long)(w * 3 + 1) * h, "image data spanning several blocks is complete");
        check(crcOk && adlerOk, "checksums hold across blocks");
        for (y = 0; y < h && match; y++)
            for (x = 0; x < w; x++) {
                const unsigned char *d = raw + (size_t)y * (w * 3 + 1) + 1 + x * 3;
                if (d[0] != (unsigned char)x || d[1] != (unsigned char)y || d[2] != (unsigned char)(x ^ y)) {
                    match = 0;
                    break;
                }
            }
        check(match, "every pixel survives block boundaries");
        free(png);
    }
    free(rgb);
    free(raw);
}

static void test_halve(void)
{
    const unsigned char rgb[4 * 3 * 3] = {
        0,0,0,     4,4,4,     100,0,0,   100,0,0,
        8,8,8,     12,12,12,  100,0,0,   101,2,0,
        9,9,9,     9,9,9,     9,9,9,     9,9,9       /* odd row, dropped */
    };
    int w, h;
    unsigned char *out = AccBridge_HalveRgb(rgb, 4, 3, &w, &h);

    check(out != NULL && w == 2 && h == 1, "4x3 halves to 2x1, dropping the odd row");
    if (out) {
        check(out[0] == 6 && out[1] == 6 && out[2] == 6, "each pixel is the rounded mean of its block");
        check(out[3] == 100 && out[4] == 1 && out[5] == 0, "means round half up");
        free(out);
    }
    check(AccBridge_HalveRgb(rgb, 1, 3, &w, &h) == NULL && w == 0, "a one-pixel-wide image cannot halve");
}

/* --------------------------------------------------------------- runner -- */

#define LEFT_KEY  KEY_NUMPAD4
#define RIGHT_KEY KEY_NUMPAD6

static int contains(const int *keys, int n, int key)
{
    int i;
    for (i = 0; i < n; i++) if (keys[i] == key) return 1;
    return 0;
}

static void start(ACC_BRIDGE_RUNNER *r, const char *text, int byFrames, unsigned now, int yaw)
{
    ACC_BRIDGE_COMMAND cmd;
    if (!AccBridge_ParseCommand(text, &cmd)) printf("setup parse failed: %s\n", cmd.error);
    AccBridgeRunner_Start(r, &cmd, byFrames, now, yaw, LEFT_KEY, RIGHT_KEY);
}

static void test_runner_hold_frames(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[ACC_BRIDGE_MAX_KEYS], n, frames = 0, allHeld = 1;

    start(&r, "1 hold w+lshift 100", 1, 0, 0);
    check(r.framesNeeded == 3, "100 ms is three frames");

    while (!AccBridgeRunner_Done(&r, 0, 0) && frames < 100) {
        n = AccBridgeRunner_Keys(&r, 0, keys, ACC_BRIDGE_MAX_KEYS);
        if (n != 2 || !contains(keys, n, KEY_W) || !contains(keys, n, KEY_LEFTSHIFT)) allHeld = 0;
        frames++;
    }
    check(frames == 3, "the hold lasts exactly its frames");
    check(allHeld, "both keys are held on every frame");
    check(!strcmp(r.result, "ok"), "a finished hold reports ok");
}

static void test_runner_hold_clock(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[ACC_BRIDGE_MAX_KEYS];

    start(&r, "1 hold space 500", 0, 1000, 0);
    check(!AccBridgeRunner_Done(&r, 1000, 0), "a clock hold is not done when it starts");
    check(AccBridgeRunner_Keys(&r, 1200, keys, 4) == 1 && keys[0] == KEY_SPACE, "the key is held mid-way");
    check(!AccBridgeRunner_Done(&r, 1499, 0), "not done a millisecond early");
    check(AccBridgeRunner_Done(&r, 1500, 0), "done at its duration");
    check(r.framesDone == 0, "clock time counts no frames");
}

static void test_runner_tap(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[ACC_BRIDGE_MAX_KEYS];

    start(&r, "1 tap r", 1, 0, 0);
    check(!AccBridgeRunner_Done(&r, 0, 0), "a tap starts not done");
    check(AccBridgeRunner_Keys(&r, 0, keys, 4) == 1 && keys[0] == KEY_R, "frame 1: pressed");
    check(!AccBridgeRunner_Done(&r, 0, 0), "a tap is not done after one frame");
    check(AccBridgeRunner_Keys(&r, 0, keys, 4) == 1, "frame 2: still pressed");
    check(!AccBridgeRunner_Done(&r, 0, 0), "not done after two frames");
    check(AccBridgeRunner_Keys(&r, 0, keys, 4) == 0, "frame 3: released, so the engine sees the release");
    check(AccBridgeRunner_Done(&r, 0, 0), "done after three frames");

    start(&r, "2 tap enter", 0, 5000, 0);
    check(AccBridgeRunner_Keys(&r, 5000, keys, 4) == 1, "a clock tap presses at once");
    check(AccBridgeRunner_Keys(&r, 5099, keys, 4) == 1, "and holds for 100 ms");
    check(AccBridgeRunner_Keys(&r, 5100, keys, 4) == 0, "then releases");
    check(!AccBridgeRunner_Done(&r, 5159, 0) && AccBridgeRunner_Done(&r, 5160, 0),
          "and finishes 60 ms after the release");
}

static void test_runner_run(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[ACC_BRIDGE_MAX_KEYS], frames = 0, anyKey = 0;

    start(&r, "1 run 200", 1, 0, 0);
    while (!AccBridgeRunner_Done(&r, 0, 0) && frames < 100) {
        if (AccBridgeRunner_Keys(&r, 0, keys, 4)) anyKey = 1;
        frames++;
    }
    check(frames == 6 && !anyKey, "run lets frames pass with nothing held");
}

/* A player whose yaw follows the turn keys: `speed` units per frame while a
   key is held, and `coast` more on the frame after release. */
static int simulate_turn(ACC_BRIDGE_RUNNER *r, int *yaw, int speed, int coast, int maxFrames)
{
    int keys[ACC_BRIDGE_MAX_KEYS], n, frames = 0, lastDir = 0;

    while (!AccBridgeRunner_Done(r, (unsigned)frames * 33, *yaw) && frames < maxFrames) {
        int dir;
        n = AccBridgeRunner_Keys(r, (unsigned)frames * 33, keys, 4);
        dir = contains(keys, n, RIGHT_KEY) ? 1 : contains(keys, n, LEFT_KEY) ? -1 : 0;
        if (dir) *yaw += dir * speed;
        else if (lastDir) *yaw += lastDir * coast;
        *yaw = ((*yaw % 4096) + 4096) % 4096;
        lastDir = dir;
        frames++;
    }
    return frames;
}

static int yaw_error(int yaw, int target)
{
    int d = ((yaw - target) % 4096 + 4096) % 4096;
    if (d > 2048) d -= 4096;
    return d < 0 ? -d : d;
}

static void test_runner_turn(void)
{
    ACC_BRIDGE_RUNNER r;
    int yaw = 0, frames;

    start(&r, "1 turn right 90", 1, 0, yaw);
    check(r.target == 1024, "90 degrees is a quarter of 4096");
    frames = simulate_turn(&r, &yaw, 60, 30, 600);
    check(!strcmp(r.result, "ok"), "a right turn finishes ok");
    check(yaw_error(yaw, 1024) <= ACC_BRIDGE_TURN_TOLERANCE + 30, "it stops within tolerance plus one coast");
    check(frames < 40, "and does not dither");

    yaw = 100;
    start(&r, "2 turn left 45", 1, 0, yaw);
    simulate_turn(&r, &yaw, 60, 30, 600);
    check(!strcmp(r.result, "ok"), "a left turn across yaw zero finishes ok");
    check(yaw_error(yaw, (100 - 512 + 4096) % 4096) <= ACC_BRIDGE_TURN_TOLERANCE + 30,
          "wrapping past zero is followed correctly");

    /* 200 units a frame can only land on multiples of 200; the best it can
       do is within half a step, and it must not dither trying for better. */
    yaw = 0;
    start(&r, "3 turn 180", 1, 0, yaw);
    frames = simulate_turn(&r, &yaw, 200, 0, 600);
    check(!strcmp(r.result, "ok") && yaw_error(yaw, 2048) <= 100,
          "a fast half turn lands within half a step");
    check(frames < 20, "and stops instead of chasing the remainder");

    /* Momentum that carries several frames after release. */
    yaw = 0;
    start(&r, "5 turn right 90", 1, 0, yaw);
    {
        int keys[4], n, f = 0, velocity = 0;
        while (!AccBridgeRunner_Done(&r, 0, yaw) && f < 600) {
            n = AccBridgeRunner_Keys(&r, 0, keys, 4);
            if (contains(keys, n, RIGHT_KEY)) velocity = 50;
            else if (contains(keys, n, LEFT_KEY)) velocity = -50;
            else velocity /= 2;                       /* 25, 12, 6, 3, 1, 0 */
            yaw = ((yaw + velocity) % 4096 + 4096) % 4096;
            f++;
        }
        check(!strcmp(r.result, "ok"), "a turn with lingering momentum finishes ok");
        check(yaw_error(yaw, 1024) <= 50, "and waits for the view to settle before finishing");
        check(f < 60, "without oscillating");
    }

    yaw = 0;
    start(&r, "4 turn 360", 1, 0, yaw);
    simulate_turn(&r, &yaw, 100, 0, 600);
    check(!strcmp(r.result, "ok") && r.turned >= 4096 - ACC_BRIDGE_TURN_TOLERANCE,
          "a full turn is counted, not mistaken for no turn");
}

static void test_runner_turn_failures(void)
{
    ACC_BRIDGE_RUNNER r;
    int yaw = 0, keys[4], frames = 0;

    /* The player cannot turn: a menu has input, or the turn key does nothing. */
    start(&r, "1 turn right 90", 1, 0, yaw);
    while (!AccBridgeRunner_Done(&r, 0, yaw) && frames < 100) {
        AccBridgeRunner_Keys(&r, 0, keys, 4);
        frames++;
    }
    check(!strcmp(r.result, "stalled"), "a turn that never moves is reported stalled");
    check(frames == ACC_BRIDGE_TURN_STALL_FRAMES, "after the stall limit, not before");

    /* The keys turn the other way. */
    yaw = 0;
    frames = 0;
    start(&r, "2 turn right 90", 1, 0, yaw);
    while (!AccBridgeRunner_Done(&r, 0, yaw) && frames < 200) {
        int n = AccBridgeRunner_Keys(&r, 0, keys, 4);
        if (contains(keys, n, RIGHT_KEY)) yaw = ((yaw - 60) % 4096 + 4096) % 4096;
        frames++;
    }
    check(!strcmp(r.result, "wrong_direction"), "turning away from the target is caught");
    check(frames < 20, "before spinning far");

    /* Far too slow to finish. */
    yaw = 0;
    start(&r, "3 turn right 90", 1, 0, yaw);
    simulate_turn(&r, &yaw, 1, 0, 1000);
    check(!strcmp(r.result, "timeout"), "a turn that cannot finish in time times out");
    check(r.framesDone == ACC_BRIDGE_TURN_MAX_FRAMES, "at the frame limit");
}

static void test_runner_switch_clock(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[4], i;

    start(&r, "1 hold w 1000", 1, 0, 0);
    for (i = 0; i < 10; i++) AccBridgeRunner_Keys(&r, 0, keys, 4);

    AccBridgeRunner_SwitchToClock(&r, 5000);
    check(!r.byFrames, "the runner now counts the clock");
    check(r.endMs == 5000 + 667, "the twenty remaining frames become 667 ms");
    check(AccBridgeRunner_Keys(&r, 5100, keys, 4) == 1, "the key stays held");
    check(!AccBridgeRunner_Done(&r, 5666, 0) && AccBridgeRunner_Done(&r, 5667, 0),
          "and the hold ends when the remaining time is up");

    start(&r, "2 tap enter", 1, 0, 0);
    AccBridgeRunner_Keys(&r, 0, keys, 4);
    AccBridgeRunner_Keys(&r, 0, keys, 4);
    AccBridgeRunner_SwitchToClock(&r, 9000);
    check(AccBridgeRunner_Keys(&r, 9000, keys, 4) == 0, "a tap already pressed is not pressed again");

    start(&r, "3 run 100", 0, 0, 0);
    AccBridgeRunner_SwitchToClock(&r, 50);
    check(r.endMs == 100, "switching a clock runner changes nothing");
}

static void test_runner_inactive(void)
{
    ACC_BRIDGE_RUNNER r;
    int keys[4];

    memset(&r, 0, sizeof(r));
    check(AccBridgeRunner_Done(&r, 0, 0) == 1, "an idle runner is done");
    check(AccBridgeRunner_Keys(&r, 0, keys, 4) == 0, "an idle runner holds nothing");
    check(AccBridgeRunner_Done(NULL, 0, 0) == 1 && AccBridgeRunner_Keys(NULL, 0, keys, 4) == 0,
          "a NULL runner is harmless");
}

int main(int argc, char **argv)
{
    const char *which = (argc > 1) ? argv[1] : "";

    if (!strcmp(which, "parse_basic"))           test_parse_basic();
    else if (!strcmp(which, "parse_hold"))       test_parse_hold();
    else if (!strcmp(which, "parse_turn"))       test_parse_turn();
    else if (!strcmp(which, "parse_errors"))     test_parse_errors();
    else if (!strcmp(which, "parse_whitespace")) test_parse_whitespace();
    else if (!strcmp(which, "keys"))             test_keys();
    else if (!strcmp(which, "json"))             test_json();
    else if (!strcmp(which, "relative"))         test_relative();
    else if (!strcmp(which, "frames"))           test_frames();
    else if (!strcmp(which, "png_small"))        test_png_small();
    else if (!strcmp(which, "png_large"))        test_png_large();
    else if (!strcmp(which, "halve"))            test_halve();
    else if (!strcmp(which, "hold_frames"))      test_runner_hold_frames();
    else if (!strcmp(which, "hold_clock"))       test_runner_hold_clock();
    else if (!strcmp(which, "tap"))              test_runner_tap();
    else if (!strcmp(which, "run"))              test_runner_run();
    else if (!strcmp(which, "turn"))             test_runner_turn();
    else if (!strcmp(which, "turn_failures"))    test_runner_turn_failures();
    else if (!strcmp(which, "switch_clock"))     test_runner_switch_clock();
    else if (!strcmp(which, "inactive"))         test_runner_inactive();
    else { printf("unknown case: %s\n", which); return 2; }

    printf("%s: %d assertions, %d failed\n", which, assertions, failures);
    return failures ? 1 : 0;
}
