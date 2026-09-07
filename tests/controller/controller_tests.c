/* Standalone regression checks for the real acc_pad.c translation unit.
 * SDL is mocked at its documented C boundary; no game/window/hardware opens.
 * Run one scenario per process so module static state starts clean each time.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "fixer.h"
#include "3dc.h"
#include "platform.h"
#include "acc_pad.h"

JOYINFOEX JoystickData;
int GotJoystick;
unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
unsigned char GotAnyKey;
int DebouncedGotAnyKey;

#ifdef TEST_FIXED
extern void AccPad_KeyboardKeyEvent(int key, int pressed);
extern void AccPad_DeviceRemoved(unsigned int instanceID);
#endif

static int menu_active = 1;
static int binding_active;
static int control_methods_calls;
static bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
static Sint16 axes[SDL_GAMEPAD_AXIS_COUNT];
static Uint64 ticks;
static int failures;
static int device_available = 1;
static SDL_JoystickID device_id = 1;
static int subsystem_inits;
static int device_closes;

void AccPad_ApplyControlMethods(void) { ++control_methods_calls; }
int AccMenu_MenusActive(void) { return menu_active; }
int AccMenu_BindingActive(void) { return binding_active; }
void AccMenu_DecayMenusActive(void) { }

bool SDLCALL SDL_InitSubSystem(SDL_InitFlags flags)
{ (void)flags; ++subsystem_inits; return true; }
SDL_JoystickID *SDLCALL SDL_GetGamepads(int *count)
{
    SDL_JoystickID *ids = (SDL_JoystickID *)malloc(2 * sizeof(*ids));
    ids[0] = device_available ? device_id : 0; ids[1] = 0;
    *count = device_available ? 1 : 0; return ids;
}
SDL_Gamepad *SDLCALL SDL_OpenGamepad(SDL_JoystickID id)
{ return device_available ? (SDL_Gamepad *)(uintptr_t)id : NULL; }
void SDLCALL SDL_CloseGamepad(SDL_Gamepad *pad) { (void)pad; ++device_closes; }
SDL_JoystickID SDLCALL SDL_GetGamepadID(SDL_Gamepad *pad)
{ return (SDL_JoystickID)(uintptr_t)pad; }
const char *SDLCALL SDL_GetGamepadName(SDL_Gamepad *pad)
{ (void)pad; return "Regression fixture"; }
const char *SDLCALL SDL_GetError(void) { return "Mock SDL diagnostic"; }
void SDLCALL SDL_UpdateGamepads(void) { }
Sint16 SDLCALL SDL_GetGamepadAxis(SDL_Gamepad *pad, SDL_GamepadAxis axis)
{ (void)pad; return axes[axis]; }
bool SDLCALL SDL_GetGamepadButton(SDL_Gamepad *pad, SDL_GamepadButton button)
{ (void)pad; return buttons[button]; }
void SDLCALL SDL_free(void *ptr) { free(ptr); }
Uint64 SDLCALL SDL_GetTicks(void) { return ticks; }
void SDLCALL SDL_Delay(Uint32 ms) { ticks += ms; }
bool SDLCALL SDL_PollEvent(SDL_Event *event) { (void)event; return false; }

static void check(int condition, const char *description)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) ++failures;
}

static void begin_frame(void)
{
    /* Mirrors the resets in CheckForWindowsMessages before SDL event dispatch. */
    GotAnyKey = 0;
    DebouncedGotAnyKey = 0;
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
}

static void keyboard_event(int key, int pressed)
{
    /* Mirrors handle_keypress's existing state publication and the new hook. */
#ifdef TEST_FIXED
    AccPad_KeyboardKeyEvent(key, pressed);
#endif
    if (pressed && !KeyboardInput[key]) {
        DebouncedKeyboardInput[key] = 1;
        DebouncedGotAnyKey = 1;
    }
    if (pressed) GotAnyKey = 1;
    KeyboardInput[key] = (unsigned char)pressed;
}

static void frame(void) { begin_frame(); AccPad_ReadButtons(); }

static void dpad(void)
{
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true;
    frame();
    check(KeyboardInput[KEY_UP] && DebouncedKeyboardInput[KEY_UP], "D-pad Up publishes menu key and initial edge");
    check(KeyboardInput[KEY_JOYSTICK_BUTTON_13], "D-pad Up also publishes bindable joystick button 13");
    frame();
    check(KeyboardInput[KEY_UP] && !DebouncedKeyboardInput[KEY_UP], "held D-pad retains key without repeated edge");
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = false;
    frame();
    check(!KeyboardInput[KEY_UP] && !KeyboardInput[KEY_JOYSTICK_BUTTON_13], "D-pad release clears both pad contributions");
}

static void keyboard_idle(void)
{
    begin_frame(); keyboard_event(KEY_UP, 1); AccPad_ReadButtons();
    check(KeyboardInput[KEY_UP], "idle pad preserves physical keyboard Up on press frame");
    frame();
    check(KeyboardInput[KEY_UP], "idle pad preserves held physical keyboard Up");
}

static void any_key(void)
{
    menu_active = 0; /* Same no-menu state as the loading-screen wait. */
    buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
    frame();
    check(KeyboardInput[KEY_JOYSTICK_BUTTON_1], "A reaches engine as joystick button 1 outside menus");
    check(DebouncedGotAnyKey, "A supplies the any-key edge needed to leave loading screen");
    frame();
    check(!DebouncedGotAnyKey, "held A does not supply a second any-key edge");
    buttons[SDL_GAMEPAD_BUTTON_SOUTH] = false; frame();
    buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true; frame();
    check(DebouncedGotAnyKey, "A supplies a new any-key edge after release and re-press");
}

static void back_hold(void)
{
    int i;
    buttons[SDL_GAMEPAD_BUTTON_EAST] = true;
    frame();
    check(DebouncedKeyboardInput[KEY_ESCAPE], "B supplies first Escape edge");
    for (i = 0; i < 3; ++i) {
        frame();
        check(KeyboardInput[KEY_ESCAPE] && !DebouncedKeyboardInput[KEY_ESCAPE], "held B retains Escape without repeated back action");
    }
    buttons[SDL_GAMEPAD_BUTTON_EAST] = false; frame();
    check(!KeyboardInput[KEY_ESCAPE], "B release clears Escape");
}

static void shared_key(void)
{
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true;
    begin_frame(); keyboard_event(KEY_UP, 1); AccPad_ReadButtons();
    check(KeyboardInput[KEY_UP], "keyboard and pad both hold Up");
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = false; frame();
    check(KeyboardInput[KEY_UP], "pad release retains physical keyboard Up");
    begin_frame(); keyboard_event(KEY_UP, 0); AccPad_ReadButtons();
    check(!KeyboardInput[KEY_UP], "Up releases when both sources are released");
}

static void keyboard_release(void)
{
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true;
    begin_frame(); keyboard_event(KEY_UP, 1); AccPad_ReadButtons();
    frame();
    check(KeyboardInput[KEY_UP] && !DebouncedKeyboardInput[KEY_UP], "both sources hold Up without an extra key edge");
    begin_frame(); keyboard_event(KEY_UP, 0); AccPad_ReadButtons();
    check(KeyboardInput[KEY_UP], "physical-key release retains pad-held Up");
    check(!DebouncedKeyboardInput[KEY_UP], "physical-key release does not create a new pad key edge");
    check(!DebouncedGotAnyKey, "physical-key release does not create any-key edge");
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = false; frame();
    check(!KeyboardInput[KEY_UP], "final pad release clears Up");
}

static void combined_back(void)
{
    buttons[SDL_GAMEPAD_BUTTON_START] = true; frame();
    check(DebouncedKeyboardInput[KEY_ESCAPE], "Start supplies initial Escape edge");
    buttons[SDL_GAMEPAD_BUTTON_EAST] = true; frame();
    check(KeyboardInput[KEY_ESCAPE] && !DebouncedKeyboardInput[KEY_ESCAPE], "adding B to held Start does not repeat Escape");
    buttons[SDL_GAMEPAD_BUTTON_START] = false; frame();
    check(KeyboardInput[KEY_ESCAPE] && !DebouncedKeyboardInput[KEY_ESCAPE], "releasing Start while B holds does not repeat Escape");
    frame();
    check(KeyboardInput[KEY_ESCAPE] && !DebouncedKeyboardInput[KEY_ESCAPE], "remaining held B does not repeat Escape");
    buttons[SDL_GAMEPAD_BUTTON_EAST] = false; frame();
    check(!KeyboardInput[KEY_ESCAPE], "Escape clears when Start and B are released");
}

static void bindings(void)
{
    const SDL_GamepadButton pad_buttons[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };
    const int slots[] = {1, 2, 13, 14, 15, 16};
    int i;
    binding_active = 1;
    for (i = 0; i < 6; ++i) {
        buttons[pad_buttons[i]] = true; frame();
        check(DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1 + slots[i] - 1], "binding capture receives raw controller button edge");
        check(!KeyboardInput[KEY_CR] && !KeyboardInput[KEY_ESCAPE] &&
              !KeyboardInput[KEY_UP] && !KeyboardInput[KEY_DOWN] &&
              !KeyboardInput[KEY_LEFT] && !KeyboardInput[KEY_RIGHT],
              "binding capture suppresses conflicting menu aliases");
        buttons[pad_buttons[i]] = false; frame();
    }
}

static void reconnect(void)
{
#ifdef TEST_FIXED
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true;
    buttons[SDL_GAMEPAD_BUTTON_EAST] = true;
    axes[SDL_GAMEPAD_AXIS_LEFTX] = 18000;
    axes[SDL_GAMEPAD_AXIS_RIGHTY] = -20000;
    begin_frame(); keyboard_event(KEY_UP, 1);
    AccPad_ReadButtons(); AccPad_ReadAxes();
    check(JoystickData.dwXpos != 32768 && JoystickData.dwVpos != 32768, "fixture has live movement and look axes before disconnect");
    AccPad_DeviceRemoved(99);
    check(AccPad_IsPresent() && device_closes == 0, "unrelated device removal leaves active controller open");
    device_available = 0;
    AccPad_DeviceRemoved(1);
    check(!AccPad_IsPresent() && !GotJoystick && device_closes == 1, "active removal closes pad and disables joystick input");
    check(KeyboardInput[KEY_UP], "disconnect retains Up held by physical keyboard");
    check(!KeyboardInput[KEY_JOYSTICK_BUTTON_13] && !KeyboardInput[KEY_ESCAPE], "disconnect clears raw controller key and Escape alias");
    check(JoystickData.dwXpos == 32768 && JoystickData.dwYpos == 32768 &&
          JoystickData.dwUpos == 32768 && JoystickData.dwVpos == 32768 &&
          JoystickData.dwRpos == 32768 && JoystickData.dwPOV == (DWORD)-1,
          "disconnect centers all controller axes and clears POV");
    check(!AccPad_Init() && subsystem_inits == 1, "absent-device rescan does not reinitialize SDL subsystem");
    begin_frame(); keyboard_event(KEY_UP, 0); AccPad_ReadButtons();
    check(!KeyboardInput[KEY_UP], "keyboard releases normally with controller disconnected");
    memset(buttons, 0, sizeof(buttons)); memset(axes, 0, sizeof(axes));
    device_available = 1; device_id = 2;
    check(AccPad_Init() && GotJoystick && subsystem_inits == 1, "controller reopens under its new instance ID without extra SDL init");
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true; frame();
    check(KeyboardInput[KEY_UP] && DebouncedKeyboardInput[KEY_UP] && DebouncedGotAnyKey,
          "reconnected pad generates a clean new menu press");
#else
    printf("SKIP: baseline has no device-removal API\n");
#endif
}

static int no_menu_navigation(void)
{
    return !KeyboardInput[KEY_CR] && !KeyboardInput[KEY_UP] &&
           !KeyboardInput[KEY_DOWN] && !KeyboardInput[KEY_LEFT] &&
           !KeyboardInput[KEY_RIGHT] && !DebouncedKeyboardInput[KEY_CR] &&
           !DebouncedKeyboardInput[KEY_UP] && !DebouncedKeyboardInput[KEY_DOWN] &&
           !DebouncedKeyboardInput[KEY_LEFT] && !DebouncedKeyboardInput[KEY_RIGHT];
}

static int raw_buttons_match(unsigned int held_mask, unsigned int edge_mask)
{
    int slot;
    for (slot = 0; slot < 16; ++slot) {
        unsigned int bit = 1u << slot;
        if (!!KeyboardInput[KEY_JOYSTICK_BUTTON_1 + slot] != !!(held_mask & bit) ||
            !!DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1 + slot] != !!(edge_mask & bit))
            return 0;
    }
    return 1;
}

static void gameplay_axes(void)
{
    static const struct {
        SDL_GamepadAxis axis;
        const char *name;
    } inputs[] = {
        {SDL_GAMEPAD_AXIS_LEFTX, "left stick horizontal / engine X"},
        {SDL_GAMEPAD_AXIS_LEFTY, "left stick vertical / engine Y"},
        {SDL_GAMEPAD_AXIS_RIGHTX, "right stick horizontal / engine U"},
        {SDL_GAMEPAD_AXIS_RIGHTY, "right stick vertical / engine V"}
    };
    /* Explicit boundary fixtures also catch direction, scaling, and cross-axis
       regressions. Expected values are the engine's unsigned centered axes. */
    static const struct {
        Sint16 input;
        DWORD movement, look;
        const char *name;
    } samples[] = {
        {0,      32768, 32768, "neutral"},
        {7999,   32768, 32768, "inside positive dead zone"},
        {-7999,  32768, 32768, "inside negative dead zone"},
        {8000,   40768, 33268, "positive dead-zone boundary"},
        {-8000,  24768, 32268, "negative dead-zone boundary"},
        {16384,  49152, 33792, "positive half deflection"},
        {-16384, 16384, 31744, "negative half deflection"},
        {32767,  65535, 34815, "positive full deflection"},
        {-32768,     0, 30720, "negative full deflection"},
        {0,      32768, 32768, "release to neutral"}
    };
    int axis, sample;
    char description[192];
    menu_active = 0;
    for (axis = 0; axis < 4; ++axis) {
        for (sample = 0; sample < (int)(sizeof(samples) / sizeof(samples[0])); ++sample) {
            DWORD expected[] = {32768, 32768, 32768, 32768};
            memset(axes, 0, sizeof(axes));
            axes[inputs[axis].axis] = samples[sample].input;
            expected[axis] = axis < 2 ? samples[sample].movement : samples[sample].look;
            AccPad_ReadAxes();
            frame();
            sprintf(description, "%s: %s maps to the expected axis and leaves other axes centered",
                    inputs[axis].name, samples[sample].name);
            check(JoystickData.dwXpos == expected[0] && JoystickData.dwYpos == expected[1] &&
                  JoystickData.dwUpos == expected[2] && JoystickData.dwVpos == expected[3] &&
                  JoystickData.dwRpos == 32768 && JoystickData.dwPOV == (DWORD)-1,
                  description);
            check(no_menu_navigation() && !KeyboardInput[KEY_ESCAPE] && raw_buttons_match(0, 0),
                  "gameplay stick motion emits axes without menu aliases or button presses");
        }
    }
    check(control_methods_calls == 40, "each axis read reapplies controller methods after possible profile load");
}

static void gameplay_triggers(void)
{
    static const struct {
        SDL_GamepadAxis axis;
        int slot;
        const char *name;
    } triggers[] = {
        {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 8, "RT"},
        {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 7, "LT"}
    };
    static const struct {
        Sint16 value;
        int held, edge;
        const char *name;
    } samples[] = {
        {0,     0, 0, "neutral"},
        {15999, 0, 0, "below threshold"},
        {16000, 0, 0, "at threshold"},
        {16001, 1, 1, "first press above threshold"},
        {32767, 1, 0, "held at full travel"},
        {16001, 1, 0, "held just above threshold"},
        {16000, 0, 0, "release at threshold"},
        {15999, 0, 0, "held below threshold"},
        {0,     0, 0, "fully released"},
        {32767, 1, 1, "re-press after release"},
        {0,     0, 0, "final release"}
    };
    int trigger, sample;
    char description[192];
    menu_active = 0;
    for (trigger = 0; trigger < 2; ++trigger) {
        unsigned int bit = 1u << (triggers[trigger].slot - 1);
        for (sample = 0; sample < (int)(sizeof(samples) / sizeof(samples[0])); ++sample) {
            axes[triggers[trigger].axis] = samples[sample].value;
            frame();
            sprintf(description, "%s %s publishes only button %d with the expected held state and press edge",
                    triggers[trigger].name, samples[sample].name, triggers[trigger].slot);
            check(raw_buttons_match(samples[sample].held ? bit : 0, samples[sample].edge ? bit : 0),
                  description);
            check(!!GotAnyKey == samples[sample].held && !!DebouncedGotAnyKey == samples[sample].edge &&
                  no_menu_navigation() && !KeyboardInput[KEY_ESCAPE],
                  "trigger any-key state follows hold/press and does not emit menu aliases");
        }
    }
    axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767; frame();
    check(raw_buttons_match(0x80u, 0x80u), "RT initial press has its own edge");
    axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 32767; frame();
    check(raw_buttons_match(0xc0u, 0x40u), "pressing LT while RT is held adds only the LT edge");
    axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0; frame();
    check(raw_buttons_match(0x40u, 0), "releasing RT preserves held LT without a new edge");
    axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767; frame();
    check(raw_buttons_match(0xc0u, 0x80u), "re-pressing RT while LT is held adds only the RT edge");
    axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 0;
    axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0; frame();
    check(raw_buttons_match(0, 0) && !DebouncedGotAnyKey, "releasing both triggers clears their inputs without press edges");
}

static void gameplay_buttons(void)
{
    static const struct {
        SDL_GamepadButton button;
        int slot;
        const char *name;
    } inputs[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, 1, "A"},
        {SDL_GAMEPAD_BUTTON_EAST, 2, "B"},
        {SDL_GAMEPAD_BUTTON_WEST, 3, "X"},
        {SDL_GAMEPAD_BUTTON_NORTH, 4, "Y"},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 5, "LB"},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 6, "RB"},
        {SDL_GAMEPAD_BUTTON_BACK, 9, "Back"},
        {SDL_GAMEPAD_BUTTON_START, 10, "Start"},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, 11, "L3"},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 12, "R3"},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, 13, "D-pad Up"},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 14, "D-pad Down"},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 15, "D-pad Left"},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 16, "D-pad Right"}
    };
    int input;
    char description[192];
    menu_active = 0;
    for (input = 0; input < (int)(sizeof(inputs) / sizeof(inputs[0])); ++input) {
        unsigned int bit = 1u << (inputs[input].slot - 1);
        int pause_button = inputs[input].button == SDL_GAMEPAD_BUTTON_START;
        buttons[inputs[input].button] = true; frame();
        sprintf(description, "%s gameplay press publishes only bindable button %d and its initial edge",
                inputs[input].name, inputs[input].slot);
        check(raw_buttons_match(bit, bit) && GotAnyKey && DebouncedGotAnyKey, description);
        check(no_menu_navigation() && !!KeyboardInput[KEY_ESCAPE] == pause_button &&
              !!DebouncedKeyboardInput[KEY_ESCAPE] == pause_button,
              "gameplay suppresses menu aliases except Start's deliberate pause action");
        frame();
        check(raw_buttons_match(bit, 0) && GotAnyKey && !DebouncedGotAnyKey &&
              no_menu_navigation() && !!KeyboardInput[KEY_ESCAPE] == pause_button &&
              !DebouncedKeyboardInput[KEY_ESCAPE],
              "held gameplay button remains bindable without repeat or navigation edges");
        buttons[inputs[input].button] = false; frame();
        check(raw_buttons_match(0, 0) && !GotAnyKey && !DebouncedGotAnyKey &&
              no_menu_navigation() && !KeyboardInput[KEY_ESCAPE],
              "gameplay button release clears its input and any pause alias without a press edge");
    }
}

static void gameplay_transition(void)
{
    buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
    buttons[SDL_GAMEPAD_BUTTON_EAST] = true;
    buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = true;
    axes[SDL_GAMEPAD_AXIS_LEFTX] = 32767;
    frame();
    check(KeyboardInput[KEY_CR] && KeyboardInput[KEY_ESCAPE] &&
          KeyboardInput[KEY_UP] && KeyboardInput[KEY_RIGHT],
          "menu fixture starts with active A/B/D-pad/stick aliases");
    menu_active = 0; frame();
    check(raw_buttons_match(0x1003u, 0) && no_menu_navigation() && !KeyboardInput[KEY_ESCAPE],
          "entering gameplay clears stale menu aliases while retaining held bindable inputs");
    check(!DebouncedGotAnyKey, "entering gameplay with held buttons does not invent a new press edge");
    memset(buttons, 0, sizeof(buttons)); memset(axes, 0, sizeof(axes)); frame();
    check(raw_buttons_match(0, 0) && no_menu_navigation() && !KeyboardInput[KEY_ESCAPE],
          "inputs release normally after the menu-to-gameplay transition");
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: controller_tests CASE\n"); return 2; }
    if (!AccPad_Init()) { fprintf(stderr, "Mock pad initialization failed\n"); return 2; }
    if (!strcmp(argv[1], "dpad")) dpad();
    else if (!strcmp(argv[1], "keyboard_idle")) keyboard_idle();
    else if (!strcmp(argv[1], "any_key")) any_key();
    else if (!strcmp(argv[1], "back_hold")) back_hold();
    else if (!strcmp(argv[1], "shared_key")) shared_key();
    else if (!strcmp(argv[1], "keyboard_release")) keyboard_release();
    else if (!strcmp(argv[1], "combined_back")) combined_back();
    else if (!strcmp(argv[1], "bindings")) bindings();
    else if (!strcmp(argv[1], "reconnect")) reconnect();
    else if (!strcmp(argv[1], "gameplay_axes")) gameplay_axes();
    else if (!strcmp(argv[1], "gameplay_triggers")) gameplay_triggers();
    else if (!strcmp(argv[1], "gameplay_buttons")) gameplay_buttons();
    else if (!strcmp(argv[1], "gameplay_transition")) gameplay_transition();
    else { fprintf(stderr, "Unknown case: %s\n", argv[1]); return 2; }
    AccPad_Shutdown();
    printf("%s: %d failed assertion(s)\n", argv[1], failures);
    return failures ? 1 : 0;
}
