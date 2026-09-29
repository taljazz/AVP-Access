/* AVP Access: the engine-free half of the play bridge -- see acc_bridge_core.h. */
#include "fixer.h"
#include "3dc.h"

#include "acc_bridge_core.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ key names -- */

typedef struct {
    const char *name;
    int         key;
} ACC_BRIDGE_KEYNAME;

/* The first entry for a key is its canonical name. */
static const ACC_BRIDGE_KEYNAME KeyNames[] = {
    { "escape", KEY_ESCAPE }, { "esc", KEY_ESCAPE },
    { "up", KEY_UP }, { "down", KEY_DOWN }, { "left", KEY_LEFT }, { "right", KEY_RIGHT },
    { "enter", KEY_CR }, { "return", KEY_CR },
    { "tab", KEY_TAB }, { "insert", KEY_INS }, { "ins", KEY_INS },
    { "delete", KEY_DEL }, { "del", KEY_DEL },
    { "end", KEY_END }, { "home", KEY_HOME },
    { "pageup", KEY_PAGEUP }, { "pagedown", KEY_PAGEDOWN },
    { "backspace", KEY_BACKSPACE },
    { "comma", KEY_COMMA }, { "period", KEY_FSTOP }, { "fstop", KEY_FSTOP },
    { "space", KEY_SPACE },
    { "lshift", KEY_LEFTSHIFT }, { "rshift", KEY_RIGHTSHIFT },
    { "lalt", KEY_LEFTALT }, { "ralt", KEY_RIGHTALT },
    { "lctrl", KEY_LEFTCTRL }, { "rctrl", KEY_RIGHTCTRL },
    { "capslock", KEY_CAPS }, { "numlock", KEY_NUMLOCK }, { "scrolllock", KEY_SCROLLOK },
    { "num0", KEY_NUMPAD0 }, { "num1", KEY_NUMPAD1 }, { "num2", KEY_NUMPAD2 },
    { "num3", KEY_NUMPAD3 }, { "num4", KEY_NUMPAD4 }, { "num5", KEY_NUMPAD5 },
    { "num6", KEY_NUMPAD6 }, { "num7", KEY_NUMPAD7 }, { "num8", KEY_NUMPAD8 },
    { "num9", KEY_NUMPAD9 },
    { "numminus", KEY_NUMPADSUB }, { "numplus", KEY_NUMPADADD },
    { "numdel", KEY_NUMPADDEL }, { "numenter", KEY_NUMPADENTER },
    { "numslash", KEY_NUMPADDIVIDE }, { "numstar", KEY_NUMPADMULTIPLY },
    { "lbracket", KEY_LBRACKET }, { "rbracket", KEY_RBRACKET },
    { "semicolon", KEY_SEMICOLON }, { "apostrophe", KEY_APOSTROPHE },
    { "grave", KEY_GRAVE }, { "backslash", KEY_BACKSLASH }, { "slash", KEY_SLASH },
    { "minus", KEY_MINUS }, { "equals", KEY_EQUALS },
    { "f1", KEY_F1 }, { "f2", KEY_F2 }, { "f3", KEY_F3 }, { "f4", KEY_F4 },
    { "f5", KEY_F5 }, { "f6", KEY_F6 }, { "f7", KEY_F7 }, { "f8", KEY_F8 },
    { "f9", KEY_F9 }, { "f10", KEY_F10 }, { "f11", KEY_F11 }, { "f12", KEY_F12 },
    { "lmouse", KEY_LMOUSE }, { "mmouse", KEY_MMOUSE }, { "rmouse", KEY_RMOUSE },
    { "wheelup", KEY_MOUSEWHEELUP }, { "wheeldown", KEY_MOUSEWHEELDOWN },
    /* Xbox names for acc_pad.c's fixed button layout. */
    { "pad_a", KEY_JOYSTICK_BUTTON_1 }, { "pad_b", KEY_JOYSTICK_BUTTON_2 },
    { "pad_x", KEY_JOYSTICK_BUTTON_3 }, { "pad_y", KEY_JOYSTICK_BUTTON_4 },
    { "pad_lb", KEY_JOYSTICK_BUTTON_5 }, { "pad_rb", KEY_JOYSTICK_BUTTON_6 },
    { "pad_lt", KEY_JOYSTICK_BUTTON_7 }, { "pad_rt", KEY_JOYSTICK_BUTTON_8 },
    { "pad_view", KEY_JOYSTICK_BUTTON_9 }, { "pad_start", KEY_JOYSTICK_BUTTON_10 },
    { "pad_l3", KEY_JOYSTICK_BUTTON_11 }, { "pad_r3", KEY_JOYSTICK_BUTTON_12 },
    { "pad_up", KEY_JOYSTICK_BUTTON_13 }, { "pad_down", KEY_JOYSTICK_BUTTON_14 },
    { "pad_left", KEY_JOYSTICK_BUTTON_15 }, { "pad_right", KEY_JOYSTICK_BUTTON_16 },
};

#define KEYNAME_COUNT ((int)(sizeof(KeyNames) / sizeof(KeyNames[0])))

static int EqualsIgnoreCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int AccBridge_KeyFromName(const char *name)
{
    int i;

    if (!name || !name[0]) return -1;

    /* Single letters and digits map straight across. */
    if (!name[1]) {
        int c = tolower((unsigned char)name[0]);
        if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
        if (c >= '0' && c <= '9') return KEY_0 + (c - '0');
    }

    /* joy1..joy16, the raw engine names. */
    if (tolower((unsigned char)name[0]) == 'j' && tolower((unsigned char)name[1]) == 'o'
        && tolower((unsigned char)name[2]) == 'y' && isdigit((unsigned char)name[3])) {
        const char *p = name + 3;
        int n = 0;
        while (isdigit((unsigned char)*p) && n <= 16) n = n * 10 + (*p++ - '0');
        if (!*p && n >= 1 && n <= 16) return KEY_JOYSTICK_BUTTON_1 + (n - 1);
        return -1;
    }

    for (i = 0; i < KEYNAME_COUNT; i++)
        if (EqualsIgnoreCase(name, KeyNames[i].name)) return KeyNames[i].key;

    return -1;
}

const char *AccBridge_KeyName(int key)
{
    static const char *Letters[] = {
        "a","b","c","d","e","f","g","h","i","j","k","l","m",
        "n","o","p","q","r","s","t","u","v","w","x","y","z"
    };
    static const char *Digits[] = { "0","1","2","3","4","5","6","7","8","9" };
    int i;

    if (key >= KEY_A && key <= KEY_Z) return Letters[key - KEY_A];
    if (key >= KEY_0 && key <= KEY_9) return Digits[key - KEY_0];

    for (i = 0; i < KEYNAME_COUNT; i++)
        if (KeyNames[i].key == key) return KeyNames[i].name;

    return NULL;
}

/* -------------------------------------------------------------- parsing -- */

#define MAX_TOKENS 8
#define TOKEN_LEN  48

static int Tokenise(const char *text, char tokens[MAX_TOKENS][TOKEN_LEN])
{
    int count = 0;

    while (*text) {
        int len = 0;

        while (*text && isspace((unsigned char)*text)) text++;
        if (!*text) break;
        if (count == MAX_TOKENS) return -1;

        while (*text && !isspace((unsigned char)*text)) {
            if (len == TOKEN_LEN - 1) return -1;
            tokens[count][len++] = *text++;
        }
        tokens[count][len] = 0;
        count++;
    }
    return count;
}

/* Strict decimal: optional sign, digits only, in int range. */
static int ParseInt(const char *s, int *value)
{
    long v = 0;
    int negative = 0;

    if (*s == '-' || *s == '+') negative = (*s++ == '-');
    if (!isdigit((unsigned char)*s)) return 0;

    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        /* long is 32-bit on Windows: reject before multiplying, otherwise
           a very large duration can wrap into an accepted short command. */
        if (v > (INT_MAX - (*s - '0')) / 10) return 0;
        v = v * 10 + (*s++ - '0');
    }
    *value = negative ? (int)-v : (int)v;
    return 1;
}

static int Fail(ACC_BRIDGE_COMMAND *cmd, const char *message)
{
    snprintf(cmd->error, sizeof(cmd->error), "%s", message);
    return 0;
}

static int ParseKeys(const char *spec, ACC_BRIDGE_COMMAND *cmd)
{
    char part[TOKEN_LEN];
    const char *p = spec;

    while (*p) {
        int len = 0, key, i, duplicate = 0;

        while (*p && *p != '+') {
            if (len == TOKEN_LEN - 1) return Fail(cmd, "key name too long");
            part[len++] = *p++;
        }
        part[len] = 0;
        if (*p == '+') {
            p++;
            if (!*p) return Fail(cmd, "empty key name");
        }

        if (!len) return Fail(cmd, "empty key name");

        key = AccBridge_KeyFromName(part);
        if (key < 0) {
            snprintf(cmd->error, sizeof(cmd->error), "unknown key '%s'", part);
            return 0;
        }

        for (i = 0; i < cmd->keyCount; i++)
            if (cmd->keys[i] == key) duplicate = 1;
        if (duplicate) continue;

        if (cmd->keyCount == ACC_BRIDGE_MAX_KEYS) return Fail(cmd, "too many keys (at most 4)");
        cmd->keys[cmd->keyCount++] = key;
    }
    return 1;
}

static const struct { const char *name; ACC_BRIDGE_VERB verb; } Verbs[] = {
    { "state", ACC_BRIDGE_STATE },   { "step", ACC_BRIDGE_STEP },
    { "realtime", ACC_BRIDGE_REALTIME }, { "run", ACC_BRIDGE_RUN },
    { "hold", ACC_BRIDGE_HOLD },     { "tap", ACC_BRIDGE_TAP },
    { "turn", ACC_BRIDGE_TURN },     { "shot", ACC_BRIDGE_SHOT },
    { "sounds", ACC_BRIDGE_SOUNDS }, { "quit", ACC_BRIDGE_QUIT },
    { "map", ACC_BRIDGE_MAP },
};

const char *AccBridge_VerbName(ACC_BRIDGE_VERB verb)
{
    size_t i;
    for (i = 0; i < sizeof(Verbs) / sizeof(Verbs[0]); i++)
        if (Verbs[i].verb == verb) return Verbs[i].name;
    return "none";
}

int AccBridge_ParseCommand(const char *text, ACC_BRIDGE_COMMAND *cmd)
{
    char tokens[MAX_TOKENS][TOKEN_LEN];
    int count, i, value, haveMs = 0, haveKeys = 0, haveDegrees = 0, direction = 0;
    size_t v;

    if (!cmd) return 0;
    memset(cmd, 0, sizeof(*cmd));

    if (!text) return Fail(cmd, "empty command");

    /* Preserve a valid sequence even when tokenization rejects later input.
       Clients need to correlate an error, not time out waiting for their id. */
    {
        char sequenceText[TOKEN_LEN];
        const char *p = text;
        size_t n = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && !isspace((unsigned char)*p) && n + 1 < sizeof(sequenceText))
            sequenceText[n++] = *p++;
        sequenceText[n] = 0;
        if ((!*p || isspace((unsigned char)*p)) && ParseInt(sequenceText, &value) && value > 0)
            cmd->seq = (unsigned int)value;
    }

    count = Tokenise(text, tokens);
    if (count < 0) return Fail(cmd, "command too long");
    if (count == 0) return Fail(cmd, "empty command");

    if (!ParseInt(tokens[0], &value) || value <= 0)
        return Fail(cmd, "missing sequence number");
    cmd->seq = (unsigned int)value;

    if (count < 2) return Fail(cmd, "missing command");

    for (v = 0; v < sizeof(Verbs) / sizeof(Verbs[0]); v++)
        if (EqualsIgnoreCase(tokens[1], Verbs[v].name)) cmd->verb = Verbs[v].verb;
    if (cmd->verb == ACC_BRIDGE_NONE) {
        snprintf(cmd->error, sizeof(cmd->error), "unknown command '%s'", tokens[1]);
        return 0;
    }

    /* Arguments are recognised by shape, so "hold w 500" and "hold 500 w"
       both work -- one less thing to get wrong mid-session. */
    for (i = 2; i < count; i++) {
        const char *t = tokens[i];
        int timed = cmd->verb == ACC_BRIDGE_RUN || cmd->verb == ACC_BRIDGE_HOLD
                 || cmd->verb == ACC_BRIDGE_TAP || cmd->verb == ACC_BRIDGE_TURN;

        if (timed && EqualsIgnoreCase(t, "shot")) {
            cmd->shot = 1;
        } else if (cmd->verb == ACC_BRIDGE_SOUNDS && EqualsIgnoreCase(t, "all") && i == 2) {
            cmd->allSounds = 1;
            haveKeys = 1;
        } else if (cmd->verb == ACC_BRIDGE_SOUNDS && EqualsIgnoreCase(t, "access") && i == 2) {
            cmd->allSounds = 0;
            haveKeys = 1;
        } else if (cmd->verb == ACC_BRIDGE_TURN && !haveDegrees && !direction
                   && (EqualsIgnoreCase(t, "left") || EqualsIgnoreCase(t, "right"))) {
            direction = EqualsIgnoreCase(t, "left") ? -1 : 1;
        } else if (cmd->verb == ACC_BRIDGE_TURN && !haveDegrees && ParseInt(t, &value)) {
            cmd->degrees = value;
            haveDegrees = 1;
        } else if ((cmd->verb == ACC_BRIDGE_RUN || cmd->verb == ACC_BRIDGE_HOLD)
                   && !haveMs && ParseInt(t, &value)) {
            if (value < 1 || value > ACC_BRIDGE_MAX_MS)
                return Fail(cmd, "duration must be 1 to 60000 ms");
            cmd->ms = value;
            haveMs = 1;
        } else if ((cmd->verb == ACC_BRIDGE_HOLD || cmd->verb == ACC_BRIDGE_TAP) && !haveKeys) {
            if (!ParseKeys(t, cmd)) return 0;
            haveKeys = 1;
        } else {
            snprintf(cmd->error, sizeof(cmd->error), "unexpected '%s'", t);
            return 0;
        }
    }

    switch (cmd->verb) {
    case ACC_BRIDGE_RUN:
        if (!haveMs) return Fail(cmd, "run needs a duration in ms");
        break;
    case ACC_BRIDGE_HOLD:
        if (!haveKeys) return Fail(cmd, "hold needs keys, e.g. hold w 500");
        if (!haveMs) return Fail(cmd, "hold needs a duration in ms");
        break;
    case ACC_BRIDGE_TAP:
        if (!haveKeys) return Fail(cmd, "tap needs keys, e.g. tap enter");
        break;
    case ACC_BRIDGE_TURN:
        if (!haveDegrees) return Fail(cmd, "turn needs degrees, e.g. turn right 90");
        if (direction) {
            if (cmd->degrees < 0) return Fail(cmd, "give either a direction or a signed angle");
            cmd->degrees *= direction;
        }
        if (cmd->degrees < -360 || cmd->degrees > 360 || cmd->degrees == 0)
            return Fail(cmd, "turn angle must be 1 to 360 degrees");
        break;
    case ACC_BRIDGE_SOUNDS:
        if (!haveKeys) return Fail(cmd, "sounds needs 'all' or 'access'");
        break;
    default:
        break;
    }

    return 1;
}

/* ----------------------------------------------------------------- JSON -- */

size_t AccBridge_JsonEscape(const char *in, char *out, size_t size)
{
    size_t n = 0;

    if (!out || !size) return 0;
    out[0] = 0;
    if (!in) return 0;

    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        char piece[8];
        size_t len;

        switch (c) {
        case '"':  strcpy(piece, "\\\""); break;
        case '\\': strcpy(piece, "\\\\"); break;
        case '\n': strcpy(piece, "\\n");  break;
        case '\r': strcpy(piece, "\\r");  break;
        case '\t': strcpy(piece, "\\t");  break;
        default:
            if (c < 0x20 || c >= 0x7F) snprintf(piece, sizeof(piece), "\\u%04x", c);
            else { piece[0] = (char)c; piece[1] = 0; }
            break;
        }

        len = strlen(piece);
        if (n + len >= size) break;          /* whole escapes only */
        memcpy(out + n, piece, len);
        n += len;
    }
    out[n] = 0;
    return n;
}

/* ------------------------------------------------------------- geometry -- */

void AccBridge_Relative(int px, int pz, int yaw, int sx, int sz, ACC_BRIDGE_RELATIVE *out)
{
    const double pi = 3.14159265358979323846;
    double angle, dx, dz, front, right, degrees;
    int clock;

    if (!out) return;

    /* Same frame as acc_tracker.c: yaw 0 faces +Z, 1024 faces +X. */
    angle = (((yaw % ACC_BRIDGE_YAW_TURN) + ACC_BRIDGE_YAW_TURN) % ACC_BRIDGE_YAW_TURN)
          * (2.0 * pi / ACC_BRIDGE_YAW_TURN);
    dx = (double)sx - px;
    dz = (double)sz - pz;
    front = dx * sin(angle) + dz * cos(angle);
    right = dx * cos(angle) - dz * sin(angle);

    degrees = atan2(right, front) * 180.0 / pi;
    out->bearing = (int)floor(degrees + 0.5);
    if (out->bearing <= -180) out->bearing += 360;
    if (out->bearing > 180) out->bearing -= 360;

    clock = (int)floor(degrees / 30.0 + 0.5);
    clock = ((clock % 12) + 12) % 12;
    out->clock = clock ? clock : 12;

    out->distance = (int)floor(sqrt(dx * dx + dz * dz) + 0.5);
}

int AccBridge_FramesForMs(int ms)
{
    int frames;
    if (ms <= 0) return 1;
    frames = (int)(((long long)ms * ACC_BRIDGE_STEPS_PER_SECOND + 999) / 1000);
    return frames < 1 ? 1 : frames;
}

/* ------------------------------------------------------------------ PNG -- */

static unsigned long CrcTable[256];
static int CrcReady;

static void MakeCrcTable(void)
{
    unsigned long c;
    int n, k;
    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        CrcTable[n] = c;
    }
    CrcReady = 1;
}

static unsigned long Crc(const unsigned char *data, size_t len)
{
    unsigned long c = 0xFFFFFFFFUL;
    size_t i;
    if (!CrcReady) MakeCrcTable();
    for (i = 0; i < len; i++) c = CrcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return (c ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}

static void PutBE32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* Writes one chunk at `p`: length, type, data (already in place after the
   type), CRC. Returns the bytes written. */
static size_t FinishChunk(unsigned char *p, const char *type, size_t dataLen)
{
    PutBE32(p, (unsigned long)dataLen);
    memcpy(p + 4, type, 4);
    PutBE32(p + 8 + dataLen, Crc(p + 4, dataLen + 4));
    return dataLen + 12;
}

unsigned char *AccBridge_EncodePng(const unsigned char *rgb, int width, int height,
                                   int bottomUp, size_t *size)
{
    static const unsigned char Signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    const size_t maxBlock = 65535;
    size_t rowLen, rawLen, blocks, zlibLen, total, pos;
    unsigned long adlerA = 1, adlerB = 0;
    unsigned char *png, *z;
    int y;

    if (size) *size = 0;
    if (!rgb || width <= 0 || height <= 0 || width > 16384 || height > 16384) return NULL;

    rowLen = (size_t)width * 3 + 1;                 /* filter byte + pixels */
    rawLen = rowLen * (size_t)height;
    blocks = (rawLen + maxBlock - 1) / maxBlock;
    zlibLen = 2 + rawLen + blocks * 5 + 4;
    total = 8 + (12 + 13) + (12 + zlibLen) + 12;

    png = (unsigned char *)malloc(total);
    if (!png) return NULL;

    memcpy(png, Signature, 8);
    pos = 8;

    /* IHDR: 8-bit truecolour, no interlace. */
    PutBE32(png + pos + 8, (unsigned long)width);
    PutBE32(png + pos + 12, (unsigned long)height);
    png[pos + 16] = 8;
    png[pos + 17] = 2;
    png[pos + 18] = 0;
    png[pos + 19] = 0;
    png[pos + 20] = 0;
    pos += FinishChunk(png + pos, "IHDR", 13);

    /* IDAT: a zlib stream of stored (uncompressed) deflate blocks. */
    z = png + pos + 8;
    z[0] = 0x78;
    z[1] = 0x01;
    {
        size_t zpos = 2, written = 0, inBlock = 0, blockStart = 0;

        for (y = 0; y < height; y++) {
            int srcRow = bottomUp ? (height - 1 - y) : y;
            const unsigned char *row = rgb + (size_t)srcRow * (size_t)width * 3;
            size_t i;

            for (i = 0; i < rowLen; i++) {
                unsigned char byte = i ? row[i - 1] : 0;

                if (inBlock == 0) {
                    size_t remaining = rawLen - written;
                    size_t len = remaining < maxBlock ? remaining : maxBlock;
                    blockStart = zpos;
                    z[zpos++] = (unsigned char)(remaining <= maxBlock ? 1 : 0);
                    z[zpos++] = (unsigned char)(len & 0xFF);
                    z[zpos++] = (unsigned char)(len >> 8);
                    z[zpos++] = (unsigned char)(~len & 0xFF);
                    z[zpos++] = (unsigned char)((~len >> 8) & 0xFF);
                    inBlock = len;
                }
                z[zpos++] = byte;
                inBlock--;
                written++;

                adlerA = (adlerA + byte) % 65521UL;
                adlerB = (adlerB + adlerA) % 65521UL;
            }
        }
        (void)blockStart;
        PutBE32(z + zpos, (adlerB << 16) | adlerA);
        zpos += 4;
        pos += FinishChunk(png + pos, "IDAT", zpos);
    }

    pos += FinishChunk(png + pos, "IEND", 0);

    if (size) *size = pos;
    return png;
}

unsigned char *AccBridge_HalveRgb(const unsigned char *rgb, int width, int height,
                                  int *outWidth, int *outHeight)
{
    unsigned char *out;
    int w, h, x, y, c;

    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;
    if (!rgb || width < 2 || height < 2) return NULL;

    w = width / 2;
    h = height / 2;
    out = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    if (!out) return NULL;

    for (y = 0; y < h; y++) {
        const unsigned char *r0 = rgb + (size_t)(y * 2) * (size_t)width * 3;
        const unsigned char *r1 = r0 + (size_t)width * 3;
        unsigned char *d = out + (size_t)y * (size_t)w * 3;
        for (x = 0; x < w; x++) {
            for (c = 0; c < 3; c++) {
                int sum = r0[x * 6 + c] + r0[x * 6 + 3 + c] + r1[x * 6 + c] + r1[x * 6 + 3 + c];
                d[x * 3 + c] = (unsigned char)((sum + 2) / 4);
            }
        }
    }

    if (outWidth) *outWidth = w;
    if (outHeight) *outHeight = h;
    return out;
}

/* --------------------------------------------------------------- runner -- */

static int WrapYaw(int delta)
{
    delta %= ACC_BRIDGE_YAW_TURN;
    if (delta >= ACC_BRIDGE_YAW_TURN / 2) delta -= ACC_BRIDGE_YAW_TURN;
    if (delta < -ACC_BRIDGE_YAW_TURN / 2) delta += ACC_BRIDGE_YAW_TURN;
    return delta;
}

void AccBridgeRunner_Start(ACC_BRIDGE_RUNNER *r, const ACC_BRIDGE_COMMAND *cmd,
                           int byFrames, unsigned int nowMs, int yaw,
                           int leftKey, int rightKey)
{
    if (!r || !cmd) return;

    memset(r, 0, sizeof(*r));
    r->active = 1;
    r->cmd = *cmd;
    r->byFrames = byFrames ? 1 : 0;
    r->startMs = nowMs;
    r->turnKey = -1;
    r->leftKey = leftKey;
    r->rightKey = rightKey;
    r->lastYaw = yaw;
    r->result = "ok";

    switch (cmd->verb) {
    case ACC_BRIDGE_RUN:
    case ACC_BRIDGE_HOLD:
        r->framesNeeded = AccBridge_FramesForMs(cmd->ms);
        r->endMs = nowMs + (unsigned int)cmd->ms;
        break;
    case ACC_BRIDGE_TAP:
        r->framesNeeded = ACC_BRIDGE_TAP_FRAMES;
        r->endMs = nowMs + ACC_BRIDGE_TAP_MS;
        break;
    case ACC_BRIDGE_TURN:
        /* Rounded to the nearest yaw unit. */
        r->target = (int)floor(cmd->degrees * (double)ACC_BRIDGE_YAW_TURN / 360.0 + 0.5);
        r->framesNeeded = ACC_BRIDGE_TURN_MAX_FRAMES;
        r->endMs = nowMs + ACC_BRIDGE_TURN_MAX_MS;
        break;
    default:
        /* Not a timed command: finishes immediately. */
        r->framesNeeded = 0;
        r->endMs = nowMs;
        break;
    }
}

/* Half of what one more frame of turning would move, once that is known.
   Closer than this, another press cannot get nearer the target. */
static int HalfPressTravel(const ACC_BRIDGE_RUNNER *r)
{
    return r->heldStep ? (r->heldStep + r->coast) / 2 : 0;
}

static int TurnDone(ACC_BRIDGE_RUNNER *r, unsigned int nowMs, int yaw)
{
    int delta = WrapYaw(yaw - r->lastYaw);
    int remaining, settled;

    r->lastYaw = yaw;
    r->turned += delta;
    r->lastDelta = delta;

    /* Learn the player's turn rate rather than assume one: how far a frame
       with the key held moves, and how far a release carries on. */
    if (r->turnKey >= 0) {
        if (abs(delta) > r->heldStep) r->heldStep = abs(delta);
        r->coastRun = 0;
    } else {
        r->coastRun += abs(delta);
        if (r->coastRun > r->coast) r->coast = r->coastRun;
    }

    if (r->turnKey >= 0 && delta == 0) r->stallFrames++;
    else if (delta != 0) r->stallFrames = 0;

    remaining = r->target - r->turned;
    settled = r->turnKey < 0 && delta == 0;

    if (abs(remaining) <= ACC_BRIDGE_TURN_TOLERANCE && r->turnKey < 0) {
        r->result = "ok";
        return 1;
    }
    /* Stopped, and a further press would overshoot by more than it gains:
       this is as close as the turn rate allows. */
    if (settled && abs(remaining) <= HalfPressTravel(r)) {
        r->result = "ok";
        return 1;
    }
    /* Moving away from the target: the turn keys are the other way round
       from what was assumed. Stop rather than spin. */
    if (abs(remaining) > abs(r->target) + ACC_BRIDGE_YAW_TURN / 8) {
        r->result = "wrong_direction";
        return 1;
    }
    if (r->stallFrames >= ACC_BRIDGE_TURN_STALL_FRAMES) {
        r->result = "stalled";
        return 1;
    }
    if (r->byFrames ? (r->framesDone >= r->framesNeeded) : (nowMs >= r->endMs)) {
        r->result = "timeout";
        return 1;
    }
    return 0;
}

int AccBridgeRunner_Done(ACC_BRIDGE_RUNNER *r, unsigned int nowMs, int yaw)
{
    if (!r || !r->active) return 1;

    if (r->cmd.verb == ACC_BRIDGE_TURN) return TurnDone(r, nowMs, yaw);

    if (r->byFrames) return r->framesDone >= r->framesNeeded;
    return nowMs >= r->endMs;
}

int AccBridgeRunner_Keys(ACC_BRIDGE_RUNNER *r, unsigned int nowMs, int *keys, int max)
{
    int n = 0, i, down = 0;

    if (!r || !r->active || !keys || max <= 0) return 0;

    switch (r->cmd.verb) {
    case ACC_BRIDGE_HOLD:
        down = 1;
        break;
    case ACC_BRIDGE_TAP:
        down = r->byFrames ? (r->framesDone < ACC_BRIDGE_TAP_DOWN_FRAMES)
                           : (nowMs < r->startMs + ACC_BRIDGE_TAP_DOWN_MS);
        break;
    case ACC_BRIDGE_TURN: {
        int remaining = r->target - r->turned;
        r->turnKey = -1;
        /* Press only while a press brings the view nearer the target;
           otherwise let any momentum settle. */
        if (abs(remaining) > ACC_BRIDGE_TURN_TOLERANCE
            && abs(remaining) > HalfPressTravel(r)) {
            r->turnKey = remaining > 0 ? r->rightKey : r->leftKey;
        }
        if (r->turnKey >= 0) keys[n++] = r->turnKey;
        break;
    }
    default:
        break;
    }

    if (down)
        for (i = 0; i < r->cmd.keyCount && n < max; i++) keys[n++] = r->cmd.keys[i];

    if (r->byFrames) r->framesDone++;
    return n;
}

void AccBridgeRunner_SwitchToClock(ACC_BRIDGE_RUNNER *r, unsigned int nowMs)
{
    int left;

    if (!r || !r->active || !r->byFrames) return;

    left = r->framesNeeded - r->framesDone;
    if (left < 0) left = 0;

    r->byFrames = 0;
    r->startMs = nowMs;
    r->endMs = nowMs + (unsigned int)((left * 1000 + ACC_BRIDGE_STEPS_PER_SECOND - 1)
                                      / ACC_BRIDGE_STEPS_PER_SECOND);
    /* A tap already past its press keeps its keys up. */
    if (r->cmd.verb == ACC_BRIDGE_TAP && r->framesDone >= ACC_BRIDGE_TAP_DOWN_FRAMES)
        r->startMs = nowMs - ACC_BRIDGE_TAP_DOWN_MS;
}
