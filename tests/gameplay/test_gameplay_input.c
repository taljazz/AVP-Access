/* Includes the real input implementation selected by the test runner. */
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <string.h>
#include "input_source.generated.h"

AVP_GAME_DESC AvP;
PAINTBALLMODE PaintBallMode;
JOYINFOEX JoystickData;
JOYCAPS JoystickCaps;
unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
int GotJoystick, GotMouse, MouseVelX, MouseVelY, NormalFrameTime;
int CameraZoomLevel, AccPadTrace;
const int sine[4096], cosine[4096];
static int accepts_controls = 1, menu_active, failures;
static Uint64 mock_ticks;
static PLAYER_STATUS player;
static STRATEGYBLOCK strategy;
static int status_announcements;
static const PLAYER_STATUS *status_player;
static DYNAMICSBLOCK dynamics;
static int tracker_announcements, tracker_yaw;
static const VECTORCH *tracker_player;
static VECTORCH tracker_position;

void AccStatus_AnnounceMarine(const struct player_status *value)
{ ++status_announcements; status_player = value; }
void AccTracker_Announce(const struct vectorch *value, int yaw)
{
    ++tracker_announcements; tracker_player = value;
    tracker_position = *value; tracker_yaw = yaw;
}

OurBool IOFOCUS_AcceptControls(void) { return accepts_controls ? Yes : No; }
void IOFOCUS_Toggle(void) { accepts_controls = !accepts_controls; }
int InGameMenusAreRunning(void) { return menu_active; }
void AvP_TriggerInGameMenus(void) { menu_active = 1; }
Uint64 SDLCALL SDL_GetTicks(void) { return mock_ticks; }
void ThrowAFlare(void) { }
void StartPlayerTaunt(void) { }
void MessageHistory_DisplayPrevious(void) { }
void BringDownConsoleWithSayTypedIn(void) { }
void BringDownConsoleWithSaySpeciesTypedIn(void) { }
void ShowMultiplayerScores(void) { }
void MaintainZoomingLevel(void) { }
void Recall_Disc(void) { }
void PaintBallMode_ChangeSelectedDecalID(int delta) { (void)delta; }
void PaintBallMode_ChangeSize(int delta) { (void)delta; }
void PaintBallMode_Rotate(void) { }
void PaintBallMode_ChangeSubclass(int delta) { (void)delta; }
void PaintBallMode_Randomise(void) { }
void PaintBallMode_AddDecal(void) { }
void PaintBallMode_RemoveDecal(void) { }
void save_preplaced_decals(void) { }
FILE *OpenGameFile(const char *filename, int mode, int type)
{ (void)filename; (void)mode; (void)type; return NULL; }

static void check(int condition, const char *description)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) ++failures;
}

static void neutral_axes(void)
{
    JoystickData.dwXpos = JoystickData.dwYpos = 32768;
    JoystickData.dwUpos = JoystickData.dwVpos = JoystickData.dwRpos = 32768;
    JoystickData.dwPOV = (DWORD)-1;
}

static int no_motion(void)
{
    return !player.Mvt_MotionIncrement && !player.Mvt_SideStepIncrement
        && !player.Mvt_TurnIncrement && !player.Mvt_PitchIncrement;
}

static void setup(void)
{
    memset(&player, 0, sizeof(player)); memset(&strategy, 0, sizeof(strategy));
    memset(KeyboardInput, 0, sizeof(KeyboardInput));
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    memset(&MarineInputPrimaryConfig, KEY_VOID, sizeof(MarineInputPrimaryConfig));
    memset(&MarineInputSecondaryConfig, KEY_VOID, sizeof(MarineInputSecondaryConfig));
    memset(&PredatorInputPrimaryConfig, KEY_VOID, sizeof(PredatorInputPrimaryConfig));
    memset(&PredatorInputSecondaryConfig, KEY_VOID, sizeof(PredatorInputSecondaryConfig));
    memset(&AlienInputPrimaryConfig, KEY_VOID, sizeof(AlienInputPrimaryConfig));
    memset(&AlienInputSecondaryConfig, KEY_VOID, sizeof(AlienInputSecondaryConfig));
    strategy.SBdataptr = &player; player.IsAlive = 1;
    AvP.PlayerType = I_Marine; AvP.LevelCompleted = 0; GotJoystick = 1; GotMouse = 0;
    accepts_controls = 1; menu_active = 0;
    status_announcements = 0; status_player = NULL;
    tracker_announcements = 0; tracker_player = NULL; tracker_yaw = 0;
    memset(&dynamics, 0, sizeof(dynamics));
    memset(&tracker_position, 0, sizeof(tracker_position));
    JoystickControlMethods = DefaultJoystickControlMethods;
    AccPad_ApplyControlMethods(); neutral_axes();
}

static void input_tests(void)
{
    setup();
    ReadPlayerGameInput(&strategy);
    check(no_motion(), "centered controller yields no movement or look");
    JoystickData.dwXpos = 52768; JoystickData.dwYpos = 12768;
    JoystickData.dwUpos = 33768; JoystickData.dwVpos = 31768;
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_MotionIncrement == 40000 && player.Mvt_SideStepIncrement == 40000,
          "left-stick mapped axes request forward movement and right strafe");
    check(player.Mvt_TurnIncrement == 32000 && player.Mvt_PitchIncrement == -32000,
          "right-stick mapped axes preserve default turn and look sensitivity");
    check(player.Mvt_InputRequests.Flags.Rqst_Forward && player.Mvt_InputRequests.Flags.Rqst_SideStepRight &&
          player.Mvt_InputRequests.Flags.Rqst_TurnRight && player.Mvt_InputRequests.Flags.Rqst_LookUp,
          "all axis directions set their matching movement request flags");
    accepts_controls = 0; ReadPlayerGameInput(&strategy);
    check(no_motion(), "console/input-focus ownership suppresses joystick movement and look");
    accepts_controls = 1; menu_active = 1; ReadPlayerGameInput(&strategy);
    check(no_motion(), "open in-game menus suppress joystick movement and look");
    menu_active = 0; ReadPlayerGameInput(&strategy);
    check(player.Mvt_MotionIncrement == 40000 && player.Mvt_TurnIncrement == 32000,
          "gameplay resumes controller movement after closing menus");
    neutral_axes(); ReadPlayerGameInput(&strategy);
    check(no_motion(), "stick release clears movement and look requests");

    MarineInputSecondaryConfig.Jump = KEY_JOYSTICK_BUTTON_1;
    MarineInputSecondaryConfig.Operate = KEY_JOYSTICK_BUTTON_3;
    MarineInputSecondaryConfig.FirePrimaryWeapon = KEY_JOYSTICK_BUTTON_8;
    MarineInputSecondaryConfig.FireSecondaryWeapon = KEY_JOYSTICK_BUTTON_7;
    MarineInputSecondaryConfig.Crouch = KEY_JOYSTICK_BUTTON_2;
    KeyboardInput[KEY_JOYSTICK_BUTTON_1] = KeyboardInput[KEY_JOYSTICK_BUTTON_2] = 1;
    KeyboardInput[KEY_JOYSTICK_BUTTON_3] = KeyboardInput[KEY_JOYSTICK_BUTTON_7] = 1;
    KeyboardInput[KEY_JOYSTICK_BUTTON_8] = 1;
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_Jump && player.Mvt_InputRequests.Flags.Rqst_Operate &&
          player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon && player.Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon &&
          player.Mvt_InputRequests.Flags.Rqst_Crouch, "bound controller actions reach actual gameplay requests");
    menu_active = 1; ReadPlayerGameInput(&strategy);
    check(!player.Mvt_InputRequests.Flags.Rqst_Jump && !player.Mvt_InputRequests.Flags.Rqst_Operate &&
          !player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon && !player.Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon &&
          !player.Mvt_InputRequests.Flags.Rqst_Crouch, "menus block bound controller actions");
    memset(KeyboardInput, 0, sizeof(KeyboardInput)); menu_active = 0;
    ReadPlayerGameInput(&strategy);
    check(!player.Mvt_InputRequests.Flags.Rqst_Jump && !player.Mvt_InputRequests.Flags.Rqst_Operate &&
          !player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon && !player.Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon &&
          !player.Mvt_InputRequests.Flags.Rqst_Crouch, "released buttons clear gameplay actions");
    DebouncedKeyboardInput[KEY_ESCAPE] = 1; ReadPlayerGameInput(&strategy);
    check(menu_active, "Escape/Start press reaches the pause-menu hook");
}


/* Each physical stick direction has an independent contract. The SDL-to-axis
   boundary is covered in tests/controller; this fixture calls real usr_io.c.
   Full engine position/camera integration still needs the live game check. */
static unsigned int direction_flags(void)
{
    return (player.Mvt_InputRequests.Flags.Rqst_Forward ? 1u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_Backward ? 2u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_SideStepLeft ? 4u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_SideStepRight ? 8u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_TurnLeft ? 16u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_TurnRight ? 32u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_LookUp ? 64u : 0u)
        | (player.Mvt_InputRequests.Flags.Rqst_LookDown ? 128u : 0u);
}

static void individual_stick_directions(void)
{
    static const struct {
        const char *name;
        DWORD x, y, u, v;
        int move, strafe, turn, pitch;
        unsigned int flags;
    } cases[] = {
        {"left stick forward",  32768, 16384, 32768, 32768, 32768, 0, 0, 0, 1u},
        {"left stick backward", 32768, 49152, 32768, 32768, -32768, 0, 0, 0, 2u},
        {"left stick left",     16384, 32768, 32768, 32768, 0, -32768, 0, 0, 4u},
        {"left stick right",    49152, 32768, 32768, 32768, 0, 32768, 0, 0, 8u},
        {"right stick left",    32768, 32768, 31744, 32768, 0, 0, -32768, 0, 16u},
        {"right stick right",   32768, 32768, 33792, 32768, 0, 0, 32768, 0, 32u},
        {"right stick up",      32768, 32768, 32768, 31744, 0, 0, 0, -32768, 64u},
        {"right stick down",    32768, 32768, 32768, 33792, 0, 0, 0, 32768, 128u}
    };
    int i;
    char description[160];
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        setup();
        JoystickData.dwXpos = cases[i].x;
        JoystickData.dwYpos = cases[i].y;
        JoystickData.dwUpos = cases[i].u;
        JoystickData.dwVpos = cases[i].v;
        ReadPlayerGameInput(&strategy);
        sprintf(description, "%s produces the expected signed increment with other axes idle", cases[i].name);
        check(player.Mvt_MotionIncrement == cases[i].move &&
              player.Mvt_SideStepIncrement == cases[i].strafe &&
              player.Mvt_TurnIncrement == cases[i].turn &&
              player.Mvt_PitchIncrement == cases[i].pitch, description);
        sprintf(description, "%s sets only its matching direction and analog mode", cases[i].name);
        check(direction_flags() == cases[i].flags &&
              !player.Mvt_InputRequests.Flags.Rqst_Strafe &&
              !!player.Mvt_AnalogueTurning == !!cases[i].turn &&
              !!player.Mvt_AnaloguePitching == !!cases[i].pitch, description);
        neutral_axes();
        ReadPlayerGameInput(&strategy);
        sprintf(description, "%s release clears direction, increments, and analog modes", cases[i].name);
        check(no_motion() && !direction_flags() &&
              !player.Mvt_AnalogueTurning && !player.Mvt_AnaloguePitching, description);
    }

    setup();
    JoystickControlMethods = DefaultJoystickControlMethods;
    JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity = 0;
    JoystickControlMethods.JoystickTrackerBallVerticalSensitivity = 0;
    AccPad_ApplyControlMethods();
    check(JoystickControlMethods.JoystickEnabled &&
          JoystickControlMethods.JoystickVAxisIsMovement &&
          !JoystickControlMethods.JoystickHAxisIsTurning &&
          JoystickControlMethods.JoystickTrackerBallEnabled &&
          !JoystickControlMethods.JoystickFlipVerticalAxis &&
          !JoystickControlMethods.JoystickTrackerBallFlipVerticalAxis,
          "loading legacy defaults restores movement/strafe/look roles with normal vertical directions");
    check(JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity == 32 &&
          JoystickControlMethods.JoystickTrackerBallVerticalSensitivity == 32,
          "missing profile look sensitivities recover usable defaults on both axes");

    JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity = 12;
    JoystickControlMethods.JoystickTrackerBallVerticalSensitivity = 20;
    JoystickControlMethods.JoystickFlipVerticalAxis = 1;
    JoystickControlMethods.JoystickTrackerBallFlipVerticalAxis = 1;
    AccPad_ApplyControlMethods();
    JoystickData.dwYpos = 16384;
    JoystickData.dwUpos = 33792;
    JoystickData.dwVpos = 31744;
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_TurnIncrement == 12288 && player.Mvt_PitchIncrement == 20480,
          "explicit custom look sensitivity and look inversion survive controller setup");
    check(player.Mvt_MotionIncrement == -32768 && direction_flags() == (2u | 32u | 128u),
          "explicit movement and look inversion affect only their vertical directions");
    setup();
}

static void trace_scenario(void)
{
    setup(); AccPadTrace = 1;
    MarineInputSecondaryConfig.Jump = KEY_JOYSTICK_BUTTON_1;
    MarineInputSecondaryConfig.Crouch = KEY_JOYSTICK_BUTTON_2;
    mock_ticks = 0; ReadPlayerGameInput(&strategy);
    mock_ticks = 100; JoystickData.dwXpos = 52768; ReadPlayerGameInput(&strategy);
    mock_ticks = 200; JoystickData.dwXpos = 53768; ReadPlayerGameInput(&strategy);
    mock_ticks = 500; JoystickData.dwXpos = 54768; ReadPlayerGameInput(&strategy);
    mock_ticks = 600; neutral_axes(); ReadPlayerGameInput(&strategy);
    mock_ticks = 650; JoystickData.dwXpos = 52768; ReadPlayerGameInput(&strategy);
    mock_ticks = 700; neutral_axes(); ReadPlayerGameInput(&strategy);
    mock_ticks = 750; KeyboardInput[KEY_JOYSTICK_BUTTON_1] = 1; ReadPlayerGameInput(&strategy);
    mock_ticks = 800; KeyboardInput[KEY_JOYSTICK_BUTTON_1] = 0; ReadPlayerGameInput(&strategy);
    mock_ticks = 825; KeyboardInput[KEY_JOYSTICK_BUTTON_2] = 1; ReadPlayerGameInput(&strategy);
    mock_ticks = 840; KeyboardInput[KEY_JOYSTICK_BUTTON_2] = 0; ReadPlayerGameInput(&strategy);
    mock_ticks = 850; accepts_controls = 0; ReadPlayerGameInput(&strategy);
    mock_ticks = 900; accepts_controls = 1; ReadPlayerGameInput(&strategy);
    mock_ticks = 950; menu_active = 1; ReadPlayerGameInput(&strategy);
    mock_ticks = 1000; menu_active = 0; ReadPlayerGameInput(&strategy);
    mock_ticks = 1100; MarineInputSecondaryConfig.Jump = KEY_JOYSTICK_BUTTON_4; ReadPlayerGameInput(&strategy);
    mock_ticks = 1200; AvP.PlayerType = I_Predator; ReadPlayerGameInput(&strategy);
    mock_ticks = 1300; ReadPlayerGameInput(&strategy);
}

static void status_press(int key)
{
    KeyboardInput[key] = 1;
    DebouncedKeyboardInput[key] = 1;
}

static void status_input_tests(void)
{
    int key, table, index, all_custom_preserved = 1;
    setup(); status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && status_player == &player,
          "H requests Marine status using the current player");
    check(!DebouncedKeyboardInput[KEY_H], "handled status shortcut consumes its press edge");
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1, "repeated input read in the same frame does not repeat status");
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1, "holding H across frames does not repeat status");
    KeyboardInput[KEY_H] = 0; ReadPlayerGameInput(&strategy);
    status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(status_announcements == 2, "release and fresh H press requests status again");

    setup(); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "Xbox View/Back requests status and consumes its press edge");
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1, "held Xbox View/Back does not repeat status");
    setup(); status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9);
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !DebouncedKeyboardInput[KEY_H] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "simultaneous H and View/Back produce one announcement");

    setup(); MarineInputPrimaryConfig.Jump = KEY_H;
    status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && player.Mvt_InputRequests.Flags.Rqst_Jump &&
          DebouncedKeyboardInput[KEY_H] && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "custom H binding survives while unbound View/Back still requests status");
    setup(); MarineInputSecondaryConfig.Operate = KEY_JOYSTICK_BUTTON_9;
    status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && player.Mvt_InputRequests.Flags.Rqst_Operate &&
          !DebouncedKeyboardInput[KEY_H] && DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "custom View/Back binding survives while unbound H still requests status");

    for (key = 0; key < 2; ++key) for (table = 0; table < 2; ++table)
    for (index = 0; index < NUMBER_OF_MARINE_INPUTS; ++index) {
        int hotkey = key ? KEY_JOYSTICK_BUTTON_9 : KEY_H;
        PLAYER_INPUT_CONFIGURATION *config;
        setup(); config = table ? &MarineInputSecondaryConfig : &MarineInputPrimaryConfig;
        ((unsigned char *)config)[index] = (unsigned char)hotkey;
        status_press(hotkey); ReadPlayerGameInput(&strategy);
        if (status_announcements || !DebouncedKeyboardInput[hotkey]) all_custom_preserved = 0;
    }
    check(all_custom_preserved, "all 108 hotkey/table/active-binding combinations preserve custom bindings");
    setup();
    ((unsigned char *)&MarineInputPrimaryConfig)[27] = KEY_H;
    ((unsigned char *)&MarineInputSecondaryConfig)[31] = KEY_JOYSTICK_BUTTON_9;
    status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1, "inactive expansion bytes do not claim status shortcuts");

    setup(); menu_active = 1; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements && DebouncedKeyboardInput[KEY_H], "menus suppress status without consuming the shortcut");
    setup(); accepts_controls = 0; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "console/input focus suppresses status");
    setup(); player.IsAlive = 0; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "dead players receive no status announcement");
    setup(); AvP.PlayerType = I_Predator; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "Predator input does not trigger Marine status");
    setup(); AvP.PlayerType = I_Alien; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "Alien input does not trigger Marine status");
    setup(); player.DemoMode = 1; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "demo playback suppresses status");
    setup(); AvP.LevelCompleted = 1; status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(!status_announcements, "completed levels suppress status");
    setup(); status_press(KEY_H); status_press(KEY_ESCAPE); ReadPlayerGameInput(&strategy);
    check(menu_active && !status_announcements, "opening pause in the same frame suppresses status");
    setup(); status_press(KEY_H); status_press(KEY_GRAVE); ReadPlayerGameInput(&strategy);
    check(!accepts_controls && !status_announcements, "opening the console in the same frame suppresses status");
    setup();
}

static void tracker_setup(void)
{
    setup(); strategy.DynPtr = &dynamics;
    dynamics.Position.vx = 1234; dynamics.Position.vy = -5678; dynamics.Position.vz = 9012;
    dynamics.OrientEuler.EulerY = 3072;
}

static void tracker_input_tests(void)
{
    int key, table, index, gate, all_custom_preserved = 1, all_gates_preserved = 1;
    tracker_setup(); status_press(KEY_T); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && tracker_player == &dynamics.Position &&
          tracker_position.vx == 1234 && tracker_position.vy == -5678 &&
          tracker_position.vz == 9012 && tracker_yaw == 3072 && !status_announcements,
          "T requests tracker using the current world position and heading");
    check(!DebouncedKeyboardInput[KEY_T], "handled tracker shortcut consumes its press edge");
    ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1, "repeated input read in the same frame does not repeat tracker");
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1, "holding T across frames does not repeat tracker");
    KeyboardInput[KEY_T] = 0; ReadPlayerGameInput(&strategy);
    dynamics.Position.vx = -321; dynamics.OrientEuler.EulerY = 1024;
    status_press(KEY_T); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 2 && tracker_position.vx == -321 && tracker_yaw == 1024,
          "fresh T press requests tracker with the latest position and heading");

    tracker_setup(); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "Xbox D-pad Down requests tracker and consumes its press edge");
    ReadPlayerGameInput(&strategy);
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1, "held Xbox D-pad Down does not repeat tracker");
    tracker_setup(); status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14);
    ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && !DebouncedKeyboardInput[KEY_T] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "simultaneous T and D-pad Down produce one tracker announcement");

    tracker_setup(); MarineInputPrimaryConfig.Jump = KEY_T;
    status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && player.Mvt_InputRequests.Flags.Rqst_Jump &&
          DebouncedKeyboardInput[KEY_T] && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "custom T binding survives while unbound D-pad Down requests tracker");
    tracker_setup(); MarineInputSecondaryConfig.Operate = KEY_JOYSTICK_BUTTON_14;
    status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && player.Mvt_InputRequests.Flags.Rqst_Operate &&
          !DebouncedKeyboardInput[KEY_T] && DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "custom D-pad Down binding survives while unbound T requests tracker");
    for (key = 0; key < 2; ++key) for (table = 0; table < 2; ++table)
    for (index = 0; index < NUMBER_OF_MARINE_INPUTS; ++index) {
        int hotkey = key ? KEY_JOYSTICK_BUTTON_14 : KEY_T;
        PLAYER_INPUT_CONFIGURATION *config;
        tracker_setup(); config = table ? &MarineInputSecondaryConfig : &MarineInputPrimaryConfig;
        ((unsigned char *)config)[index] = (unsigned char)hotkey;
        status_press(hotkey); ReadPlayerGameInput(&strategy);
        if (tracker_announcements || !DebouncedKeyboardInput[hotkey]) all_custom_preserved = 0;
    }
    check(all_custom_preserved, "all 108 tracker hotkey/table/active-binding combinations preserve custom bindings");
    tracker_setup();
    ((unsigned char *)&MarineInputPrimaryConfig)[27] = KEY_T;
    ((unsigned char *)&MarineInputSecondaryConfig)[31] = KEY_JOYSTICK_BUTTON_14;
    status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1, "inactive expansion bytes do not claim tracker shortcuts");

    for (key = 0; key < 2; ++key) for (gate = 0; gate < 9; ++gate) {
        int hotkey = key ? KEY_JOYSTICK_BUTTON_14 : KEY_T;
        tracker_setup(); status_press(hotkey);
        switch (gate) {
            case 0: menu_active = 1; break;
            case 1: accepts_controls = 0; break;
            case 2: player.IsAlive = 0; break;
            case 3: AvP.PlayerType = I_Predator; break;
            case 4: AvP.PlayerType = I_Alien; break;
            case 5: player.DemoMode = 1; break;
            case 6: AvP.LevelCompleted = 1; break;
            case 7: status_press(KEY_ESCAPE); break;
            case 8: status_press(KEY_GRAVE); break;
        }
        ReadPlayerGameInput(&strategy);
        if (tracker_announcements || !DebouncedKeyboardInput[hotkey]) all_gates_preserved = 0;
    }
    check(all_gates_preserved,
          "both tracker shortcuts respect menus, focus, life, species, demos, level end and same-frame UI transitions");
    setup(); status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(!tracker_announcements && DebouncedKeyboardInput[KEY_T] &&
          DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "missing dynamics safely suppresses tracker without consuming its shortcuts");
    status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !tracker_announcements && !DebouncedKeyboardInput[KEY_T] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "status still works without dynamics and consumes simultaneous fallback tracker requests");

    tracker_setup(); status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9);
    status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !tracker_announcements && !DebouncedKeyboardInput[KEY_H] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] && !DebouncedKeyboardInput[KEY_T] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "status wins simultaneous status/tracker presses and consumes every eligible edge");
    ReadPlayerGameInput(&strategy);
    memset(DebouncedKeyboardInput, 0, sizeof(DebouncedKeyboardInput));
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !tracker_announcements,
          "simultaneous presses leave no tracker announcement queued for another read or held frame");
    KeyboardInput[KEY_JOYSTICK_BUTTON_14] = 0; ReadPlayerGameInput(&strategy);
    status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1, "a fresh tracker press works after a status-priority read");
    tracker_setup(); MarineInputPrimaryConfig.Jump = KEY_T;
    status_press(KEY_H); status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14);
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !tracker_announcements && player.Mvt_InputRequests.Flags.Rqst_Jump &&
          DebouncedKeyboardInput[KEY_T] && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "status priority consumes only fallback tracker edges and preserves custom tracker actions");
    tracker_setup(); MarineInputPrimaryConfig.Jump = KEY_H;
    status_press(KEY_H); status_press(KEY_T); ReadPlayerGameInput(&strategy);
    check(tracker_announcements == 1 && !status_announcements && player.Mvt_InputRequests.Flags.Rqst_Jump &&
          DebouncedKeyboardInput[KEY_H] && !DebouncedKeyboardInput[KEY_T],
          "a custom-bound status key does not suppress an eligible tracker request");
    setup();
}

#include "test_marine_preset.c"

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "trace")) trace_scenario();
    else {
        input_tests();
        individual_stick_directions();
        status_input_tests();
        tracker_input_tests();
        failures += RunMarinePresetTests();
    }
    printf("gameplay input: %d failed assertion(s)\n", failures);
    return failures ? 1 : 0;
}
