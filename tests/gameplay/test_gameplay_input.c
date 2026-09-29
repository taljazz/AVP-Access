/* Includes the real input implementation selected by the test runner. */
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <string.h>
#include "acc_speech.h"
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
static int pad_present;
int AccPad_IsPresent(void) { return pad_present; }
static Uint64 mock_ticks;
static PLAYER_STATUS player;
static STRATEGYBLOCK strategy;
static MODULE assist_module;
static AIMODULE assist_ai;
static int status_announcements, predator_status_announcements, speech_announcements;
static char last_speech[256];
static const PLAYER_STATUS *status_player;
static DYNAMICSBLOCK dynamics;
static int tracker_announcements, tracker_yaw;
static const VECTORCH *tracker_player;
static VECTORCH tracker_position;

void AccStatus_AnnounceMarine(const struct player_status *value)
{ ++status_announcements; status_player = value; }
void AccStatus_AnnouncePredator(const struct player_status *value)
{ ++predator_status_announcements; status_player = value; }
void AccSpeech_Say(const char *value, int interrupt)
{ (void)interrupt; ++speech_announcements; strncpy(last_speech,value?value:"",sizeof(last_speech)-1); last_speech[sizeof(last_speech)-1]=0; }
void AccTracker_Announce(const struct vectorch *value, int yaw)
{
    ++tracker_announcements; tracker_player = value;
    tracker_position = *value; tracker_yaw = yaw;
}

/* Sonar and objectives share the same dispatcher, so this fixture has to
   satisfy them too. Counted, so the priority order between the four
   accessibility shortcuts can be checked here. */
static int sonar_requests, sonar_updates, objective_announcements;
static int route_toggles, route_updates, route_guard_hits;
static int snap_requests, history_requests;
static int face_yaw_requests, last_face_yaw;
void AccSnap_Request(unsigned int now) { (void)now; ++snap_requests; }
void AccSnap_FaceYaw(int yaw) { ++face_yaw_requests; last_face_yaw=yaw; }
char LevelName[] = "fall";
static int route_enabled=1, opening_gate_result=2;
int AccRoute_IsEnabled(void) { return route_enabled; }
int AccRoute_FallPredatorOpening(const struct vectorch *player, struct vectorch *position,
                                 struct strategyblock **control)
{ (void)player; if(position) *position=(VECTORCH){26790,15167,9780}; if(control) *control=NULL; return opening_gate_result; }
static int loot_cycles, loot_toggles;
static int recall_requests, taunt_requests, zoom_maintains, medicomp_slot;
void AccRoute_CycleLoot(void) { ++loot_cycles; }
void AccRoute_ToggleLoot(void) { ++loot_toggles; }
void AccRoute_Toggle(void) { ++route_toggles; }
void AccRoute_Update(unsigned int nowMs)
{ (void)nowMs; if(AccJumpAssist_IsActive()) { ++route_guard_hits; return; } ++route_updates; }
static int sonar_yaw;
static unsigned int sonar_request_time, sonar_update_time;
static int bridge_cue_depth, sonar_update_in_cue;
static char bridge_cue_source[24];

void AccSonar_Request(const struct vectorch *value, int yaw, unsigned int nowMs)
{ (void)value; ++sonar_requests; sonar_yaw = yaw; sonar_request_time = nowMs; }
void AccSonar_Update(unsigned int nowMs)
{ ++sonar_updates; sonar_update_time = nowMs; if (bridge_cue_depth > 0) sonar_update_in_cue = 1; }
void AccObjectives_Announce(void)
{ ++objective_announcements; }

unsigned int AccBridge_NowMs(void) { return (unsigned int)SDL_GetTicks(); }
void AccBridge_BeginCue(const char *source, int sound)
{
    (void)sound;
    if (bridge_cue_depth++ == 0) {
        strncpy(bridge_cue_source, source ? source : "access", sizeof(bridge_cue_source) - 1);
        bridge_cue_source[sizeof(bridge_cue_source) - 1] = 0;
    }
}
void AccBridge_EndCue(void) { if (bridge_cue_depth > 0) --bridge_cue_depth; }

OurBool IOFOCUS_AcceptControls(void) { return accepts_controls ? Yes : No; }
void IOFOCUS_Toggle(void) { accepts_controls = !accepts_controls; }
int InGameMenusAreRunning(void) { return menu_active; }
void AvP_TriggerInGameMenus(void) { menu_active = 1; }
Uint64 SDLCALL SDL_GetTicks(void) { return mock_ticks; }
void ThrowAFlare(void) { }
void StartPlayerTaunt(void) { ++taunt_requests; }
void MessageHistory_DisplayPrevious(void) { ++history_requests; }
void BringDownConsoleWithSayTypedIn(void) { }
void BringDownConsoleWithSaySpeciesTypedIn(void) { }
void ShowMultiplayerScores(void) { }
void MaintainZoomingLevel(void) { ++zoom_maintains; }
void Recall_Disc(void) { ++recall_requests; }
int SlotForThisWeapon(enum WEAPON_ID id)
{ return id == WEAPON_PRED_MEDICOMP ? medicomp_slot : -1; }
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
    AccAccess_ViewPending=0; pad_present=1;
    memset(AccPredatorEquipmentChordOwned,0,sizeof(AccPredatorEquipmentChordOwned));
    AccPredatorEquipmentChordAction=ACC_PRED_EQ_NONE; AccPredatorEquipmentChordTriggered=0;
    AccJumpAssist_ResetRuntime(); route_enabled=1; opening_gate_result=2;
    route_updates=route_guard_hits=0;
    face_yaw_requests=last_face_yaw=0;
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
    accepts_controls = 1; menu_active = 0; mock_ticks = 0;
    status_announcements = predator_status_announcements = speech_announcements = 0; last_speech[0]=0; status_player = NULL;
    recall_requests=taunt_requests=zoom_maintains=0; medicomp_slot=-1; CameraZoomLevel=0;
    tracker_announcements = 0; tracker_player = NULL; tracker_yaw = 0;
    memset(&dynamics, 0, sizeof(dynamics));
    memset(&tracker_position, 0, sizeof(tracker_position));
    sonar_requests = sonar_updates = objective_announcements = 0;
    sonar_yaw = 0; sonar_request_time = sonar_update_time = 0;
    bridge_cue_depth = sonar_update_in_cue = 0; bridge_cue_source[0] = 0;
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

static void fine_pad_movement(void)
{
    setup(); pad_present = 1;
    JoystickData.dwXpos = 32868;
    JoystickData.dwYpos = 32668;
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_SideStepIncrement == 200 && player.Mvt_MotionIncrement == 200,
          "fine SDL pad movement survives without a second dead zone");
    pad_present = 0;
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_SideStepIncrement == 0 && player.Mvt_MotionIncrement == 0,
          "legacy joystick retains its original dead zone");
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
    check(status_announcements == 0 && !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "Xbox View/Back defers status and consumes its press edge");
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 0, "held View waits silently for possible chord");
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
    check(status_announcements == 1, "held Xbox View/Back does not repeat status");
    setup(); status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9);
    ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !DebouncedKeyboardInput[KEY_H] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9],
          "simultaneous H and View/Back produce one announcement");

    setup(); MarineInputPrimaryConfig.Jump = KEY_H;
    status_press(KEY_H); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
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
    check(!status_announcements && predator_status_announcements == 1,
          "Predator H requests Predator-specific status, not the Marine formatter");
    {
        int slot, preserved=1;
        for(slot=27;slot<=29;++slot) {
            setup(); AvP.PlayerType=I_Predator;
            ((unsigned char *)&PredatorInputSecondaryConfig)[slot]=KEY_H;
            status_press(KEY_H); ReadPlayerGameInput(&strategy);
            if(predator_status_announcements || !DebouncedKeyboardInput[KEY_H]) preserved=0;
        }
        check(preserved,"Predator's final three active binding slots block accessibility hotkeys");
    }
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

    tracker_setup(); AvP.PlayerType=I_Predator; status_press(KEY_T); ReadPlayerGameInput(&strategy);
    check(!tracker_announcements && speech_announcements==1 &&
          strstr(last_speech,"not available") && !DebouncedKeyboardInput[KEY_T],
          "Predator tracker request clearly reports that the Marine tracker is unavailable");

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
        if (gate == 3) {
            if (tracker_announcements || speech_announcements != 1 || DebouncedKeyboardInput[hotkey])
                all_gates_preserved = 0;
        } else if (tracker_announcements || !DebouncedKeyboardInput[hotkey]) all_gates_preserved = 0;
    }
    check(all_gates_preserved,
          "both tracker shortcuts respect menus, focus, life, demos, level end, and give Predator-specific unavailability feedback");
    setup(); status_press(KEY_T); status_press(KEY_JOYSTICK_BUTTON_14); ReadPlayerGameInput(&strategy);
    check(!tracker_announcements && DebouncedKeyboardInput[KEY_T] &&
          DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "missing dynamics safely suppresses tracker without consuming its shortcuts");
    status_press(KEY_H); ReadPlayerGameInput(&strategy);
    check(status_announcements == 1 && !tracker_announcements && !DebouncedKeyboardInput[KEY_T] &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14],
          "status still works without dynamics and consumes simultaneous fallback tracker requests");

    tracker_setup(); status_press(KEY_H);
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

static void bridge_clock_tests(void)
{
    tracker_setup();
    mock_ticks = 4321;
    status_press(KEY_R);
    ReadPlayerGameInput(&strategy);
    check(sonar_requests == 1 && sonar_request_time == 4321 && sonar_yaw == 3072,
          "sonar shortcut receives the bridge clock value and current heading");
    check(sonar_updates == 1 && sonar_update_time == 4321,
          "scheduled sonar update receives the same bridge clock value");
    check(sonar_update_in_cue && bridge_cue_depth == 0 && !strcmp(bridge_cue_source, "sonar"),
          "sonar update runs inside a balanced bridge cue scope labeled sonar");
    mock_ticks = 9876;
    ReadPlayerGameInput(&strategy);
    check(sonar_updates == 2 && sonar_update_time == 9876,
          "sonar update time advances with the SDL wall clock between input reads");
}

#include "test_marine_preset.c"
#include "test_predator_preset.c"

static void route_input_tests(void)
{
    tracker_setup(); route_toggles = 0;
    status_press(KEY_N); ReadPlayerGameInput(&strategy);
    check(route_toggles == 1 && !DebouncedKeyboardInput[KEY_N], "N toggles guidance once");
    ReadPlayerGameInput(&strategy);
    check(route_toggles == 1, "held N does not repeat toggle");
    tracker_setup(); route_toggles = 0;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_16);
    ReadPlayerGameInput(&strategy);
    check(route_toggles == 1 && !status_announcements && !objective_announcements,
          "View plus right toggles without status or objective speech");
    ReadPlayerGameInput(&strategy);
    check(route_toggles == 1, "held controller chord does not toggle again");
    tracker_setup(); route_toggles = 0;
    MarineInputPrimaryConfig.Jump = KEY_N;
    status_press(KEY_N); ReadPlayerGameInput(&strategy);
    check(!route_toggles && player.Mvt_InputRequests.Flags.Rqst_Jump, "custom N binding wins");
    tracker_setup(); route_toggles = 0;
    MarineInputPrimaryConfig.Jump = KEY_JOYSTICK_BUTTON_9;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_16);
    ReadPlayerGameInput(&strategy);
    check(!route_toggles && player.Mvt_InputRequests.Flags.Rqst_Jump, "custom chord member prevents route toggle");
    tracker_setup(); route_toggles = route_updates = 0;
    menu_active = 1; status_press(KEY_N); ReadPlayerGameInput(&strategy);
    check(!route_toggles && !route_updates, "menus suppress route toggle and updates");
}

static void loot_input_tests(void)
{
    tracker_setup(); loot_cycles=loot_toggles=0;
    status_press(KEY_L); ReadPlayerGameInput(&strategy);
    check(loot_cycles==1 && !DebouncedKeyboardInput[KEY_L],"L browses supplies once");
    ReadPlayerGameInput(&strategy); check(loot_cycles==1,"held L does not repeat");
    status_press(KEY_K); ReadPlayerGameInput(&strategy); check(loot_toggles==1,"K toggles supply route");
    tracker_setup(); loot_cycles=loot_toggles=0;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_15);
    ReadPlayerGameInput(&strategy);
    check(loot_cycles==1 && !status_announcements && !sonar_requests,"View left browses without sonar/status");
    ReadPlayerGameInput(&strategy); check(loot_cycles==1,"held View left does not repeat");
    tracker_setup(); loot_cycles=loot_toggles=0;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_14);
    ReadPlayerGameInput(&strategy);
    check(loot_toggles==1 && !status_announcements && !tracker_announcements,"View down toggles without tracker/status");
    tracker_setup(); loot_cycles=loot_toggles=0;
    MarineInputPrimaryConfig.Jump=KEY_L; status_press(KEY_L); ReadPlayerGameInput(&strategy);
    check(!loot_cycles && player.Mvt_InputRequests.Flags.Rqst_Jump,"custom L binding preserved");
    MarineInputPrimaryConfig.Jump=KEY_K; status_press(KEY_K); ReadPlayerGameInput(&strategy);
    check(!loot_toggles && player.Mvt_InputRequests.Flags.Rqst_Jump,"custom K binding preserved");
    tracker_setup(); loot_cycles=loot_toggles=0;
    MarineInputSecondaryConfig.Jump=KEY_JOYSTICK_BUTTON_15;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_15); ReadPlayerGameInput(&strategy);
    check(!loot_cycles && player.Mvt_InputRequests.Flags.Rqst_Jump,"custom chord direction preserved");
    tracker_setup(); loot_cycles=loot_toggles=0; menu_active=1;
    status_press(KEY_L); status_press(KEY_K); ReadPlayerGameInput(&strategy);
    check(!loot_cycles && !loot_toggles,"menu suppresses supply shortcuts");
}

static void snap_input_tests(void)
{
    tracker_setup(); snap_requests=history_requests=0;
    MarineInputSecondaryConfig=DefaultMarineInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(history_requests==1 && !snap_requests,"R3 alone retains history");
    tracker_setup(); snap_requests=history_requests=0;
    MarineInputSecondaryConfig=DefaultMarineInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12);
    ReadPlayerGameInput(&strategy);
    check(snap_requests==1 && !history_requests && !status_announcements,"View R3 snaps without history or status");
    ReadPlayerGameInput(&strategy); check(snap_requests==1 && !history_requests,"held snap chord fires once");
    tracker_setup(); snap_requests=history_requests=0;
    MarineInputSecondaryConfig=DefaultMarineInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_11); ReadPlayerGameInput(&strategy);
    check(!snap_requests && !history_requests,"L3 retains its walk binding without speech or snap");
    tracker_setup(); snap_requests=history_requests=0;
    MarineInputSecondaryConfig.Operate=KEY_JOYSTICK_BUTTON_12;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(!snap_requests && player.Mvt_InputRequests.Flags.Rqst_Operate,"custom R3 gameplay action wins");
    tracker_setup(); snap_requests=history_requests=0; AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(history_requests==1 && !snap_requests,"Predator default R3 replays message history");
    tracker_setup(); snap_requests=history_requests=0; AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(snap_requests==1 && !history_requests,"Predator default R3 history remains available beside View-R3 snap");
    tracker_setup(); snap_requests=0; AvP.PlayerType=I_Predator;
    PredatorInputSecondaryConfig.h.GrapplingHook=KEY_JOYSTICK_BUTTON_12;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(!snap_requests && player.Mvt_InputRequests.Flags.Rqst_GrapplingHook,
          "custom Predator R3 grappling hook takes priority over snap");
    tracker_setup(); snap_requests=0; menu_active=1;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(!snap_requests,"menus block snap");
    tracker_setup(); snap_requests=0; player.IsAlive=0;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_12); ReadPlayerGameInput(&strategy);
    check(!snap_requests,"dead player cannot snap");
}

static void predator_assist_setup(int x, int y, int z);

static void predator_equipment_chord_tests(void)
{
    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==1 && !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode &&
          strstr(last_speech,"Zoom level 1 of 3"), "View+RB zooms in and suppresses its ordinary vision cycle");
    ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==1, "holding one equipment chord does not repeat its action");
    KeyboardInput[KEY_JOYSTICK_BUTTON_6]=0; ReadPlayerGameInput(&strategy);
    status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==2 && !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode,
          "holding View permits another zoom step after RB is released");
    KeyboardInput[KEY_JOYSTICK_BUTTON_6]=0; ReadPlayerGameInput(&strategy);
    status_press(KEY_JOYSTICK_BUTTON_5); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==1 && !player.Mvt_InputRequests.Flags.Rqst_PreviousWeapon,
          "View+LB can follow View+RB without changing weapons");
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
    check(!player.Mvt_InputRequests.Flags.Rqst_PreviousWeapon,
          "releasing View before LB keeps the owned action suppressed");
    KeyboardInput[KEY_JOYSTICK_BUTTON_5]=0; ReadPlayerGameInput(&strategy);

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_3); ReadPlayerGameInput(&strategy);
    check(recall_requests==1 && !player.Mvt_InputRequests.Flags.Rqst_Operate &&
          strstr(last_speech,"Disc recall requested"), "View+X requests disc recall and suppresses operate");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    medicomp_slot=3; player.WeaponSlot[3].Possessed=1;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_2); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_WeaponNo==4 &&
          !player.Mvt_InputRequests.Flags.Rqst_Crouch && strstr(last_speech,"Selecting Predator medicomp"),
          "View+B selects possessed medicomp through normal weapon request and suppresses crouch");
    check(player.SelectedWeaponSlot==0, "medicomp chord does not directly change selected slot");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    player.GrapplingHookEnabled=1;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_4); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_GrapplingHook &&
          !player.Mvt_InputRequests.Flags.Rqst_NextWeapon, "View+Y requests enabled grapple without cycling weapon");
    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_4); ReadPlayerGameInput(&strategy);
    check(!player.Mvt_InputRequests.Flags.Rqst_GrapplingHook &&
          strstr(last_speech,"unavailable with current equipment"), "disabled grapple is reported unavailable");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    player.Mvt_InputRequests.Flags.Rqst_Faster=1;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_11); ReadPlayerGameInput(&strategy);
    check(taunt_requests==1, "View+L3 requests taunt");
    check(player.Mvt_InputRequests.Flags.Rqst_Faster,
          "View+L3 suppresses the ordinary walk modifier and preserves run speed");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    PredatorInputPrimaryConfig.Operate=KEY_JOYSTICK_BUTTON_9;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0 && player.Mvt_InputRequests.Flags.Rqst_Operate,
          "custom View binding wins over equipment chord");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    PredatorInputPrimaryConfig.FirePrimaryWeapon=KEY_JOYSTICK_BUTTON_6;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0 && player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon &&
          player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode,
          "custom chord-button binding is preserved alongside its existing default action");

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    FixedInputConfig.Weapon1=KEY_JOYSTICK_BUTTON_6;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0 && player.Mvt_InputRequests.Flags.Rqst_WeaponNo==1,
          "fixed weapon binding blocks the matching equipment chord");
    FixedInputConfig.Weapon1=KEY_1;

    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6);
    status_press(KEY_JOYSTICK_BUTTON_5); ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0 && !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode &&
          !player.Mvt_InputRequests.Flags.Rqst_PreviousWeapon && strstr(last_speech,"one equipment chord"),
          "simultaneous equipment chords are suppressed instead of firing mixed actions");
    tracker_setup(); AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    menu_active=1; status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6);
    ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0, "equipment chord does not activate in menus");
    menu_active=0; ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0 && !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode,
          "held chord does not leak its ordinary action when leaving menus");
    AvP.PlayerType=I_Marine; ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==0, "Predator equipment chord state resets on species change");

    predator_assist_setup(15750,4741,0);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); status_press(KEY_JOYSTICK_BUTTON_6);
    ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==1 && !AccJumpAssist_IsActive() &&
          !player.Mvt_InputRequests.Flags.Rqst_Jump &&
          !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode &&
          strstr(last_speech,"equipment was requested"),
          "View+A plus View+RB performs equipment only and refuses jump-assist start");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_3);
    ReadPlayerGameInput(&strategy);
    check(recall_requests==1 && !AccJumpAssist_IsActive() &&
          !player.Mvt_InputRequests.Flags.Rqst_Jump &&
          !player.Mvt_InputRequests.Flags.Rqst_Operate &&
          strstr(last_speech,"equipment was requested"),
          "keyboard J plus View+X performs equipment only and refuses jump-assist start");
}

static void predator_assist_setup(int x, int y, int z)
{
    setup();
    AvP.PlayerType=I_Predator;
    PredatorInputPrimaryConfig=DefaultPredatorInputPrimaryConfig;
    PredatorInputSecondaryConfig=DefaultPredatorInputSecondaryConfig;
    memset(&assist_module,0,sizeof(assist_module)); memset(&assist_ai,0,sizeof(assist_ai));
    assist_module.m_aimodule=&assist_ai; assist_ai.m_index=94;
    strategy.containingModule=&assist_module; strategy.DynPtr=&dynamics;
    dynamics.Position=(VECTORCH){x,y,z}; dynamics.IsInContactWithFloor=1;
}

static int prepare_saved_predator_profile(void)
{
    PLAYER_INPUT_CONFIGURATION primary;
    setup();
    AvP.PlayerType=I_Predator;
    primary=DefaultPredatorInputPrimaryConfig;
    primary.Forward=KEY_UP;
    PredatorInputPrimaryConfig=primary;
    PredatorInputSecondaryConfig=legacy_predator_secondary;
    PredatorInputSecondaryConfig.ExpansionSpace7=0;
    PredatorInputSecondaryConfig.ExpansionSpace8=0;
    return AccPad_UpgradeLegacyPredatorBindings(&PredatorInputPrimaryConfig,
                                                &PredatorInputSecondaryConfig);
}

static void predator_saved_profile_pad_migration_test(void)
{
    PLAYER_INPUT_CONFIGURATION primary, before;
    setup();
    AvP.PlayerType=I_Predator;
    primary=DefaultPredatorInputPrimaryConfig;
    primary.Forward=KEY_UP; /* language/user keyboard layout retained in profile */
    before=primary;
    PredatorInputPrimaryConfig=primary;
    PredatorInputSecondaryConfig=legacy_predator_secondary;
    PredatorInputSecondaryConfig.ExpansionSpace7=0;
    PredatorInputSecondaryConfig.ExpansionSpace8=0;
    check(AccPad_UpgradeLegacyPredatorBindings(&PredatorInputPrimaryConfig,
          &PredatorInputSecondaryConfig)==1,
          "saved-profile-shaped legacy Predator secondary bindings gain the pad preset");
    check(!memcmp(&PredatorInputPrimaryConfig,&before,sizeof(before)) &&
          PredatorInputSecondaryConfig.ExpansionSpace7==0 && PredatorInputSecondaryConfig.ExpansionSpace8==0,
          "profile migration preserves custom keyboard primary and zero expansion bytes");
    status_press(KEY_JOYSTICK_BUTTON_1);
    ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_Jump,
          "migrated saved Predator profile dispatches Xbox A through the gameplay mapper");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables RT/LT test fixture");
    status_press(KEY_JOYSTICK_BUTTON_8); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon,
          "migrated saved Predator profile maps RT to primary fire");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables LT test fixture");
    status_press(KEY_JOYSTICK_BUTTON_7); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon,
          "migrated saved Predator profile maps LT to secondary fire");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables Y test fixture");
    status_press(KEY_JOYSTICK_BUTTON_4); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_NextWeapon,
          "migrated saved Predator profile maps Y to next weapon");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables LB test fixture");
    status_press(KEY_JOYSTICK_BUTTON_5); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_PreviousWeapon,
          "migrated saved Predator profile maps LB to previous weapon");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables RB test fixture");
    status_press(KEY_JOYSTICK_BUTTON_6); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode,
          "migrated saved Predator profile maps RB to cycle vision");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables D-pad Up test fixture");
    status_press(KEY_JOYSTICK_BUTTON_13); ReadPlayerGameInput(&strategy);
    check(player.Mvt_InputRequests.Flags.Rqst_ChangeVision,
          "migrated saved Predator profile maps D-pad Up to cloak");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables View+RB fixture");
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_6);
    ReadPlayerGameInput(&strategy);
    check(CameraZoomLevel==1 && !player.Mvt_InputRequests.Flags.Rqst_CycleVisionMode,
          "migrated saved Predator profile maps View+RB to zoom and consumes vision cycle");

    check(prepare_saved_predator_profile()==1,"saved Predator profile migration enables View+X fixture");
    status_press(KEY_JOYSTICK_BUTTON_9); status_press(KEY_JOYSTICK_BUTTON_3);
    ReadPlayerGameInput(&strategy);
    check(recall_requests==1 && !player.Mvt_InputRequests.Flags.Rqst_Operate,
          "migrated saved Predator profile maps View+X to recall and consumes operate");
}

static void jump_assist_input_tests(void)
{
    route_updates=0;
    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    check(AccJumpAssist_IsActive() && face_yaw_requests==1 && !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "unbound J starts assist at the surveyed Predator staging point without jumping");
    check(!DebouncedKeyboardInput[KEY_J] && route_updates==0 && route_guard_hits==1,
          "assist start consumes J and pauses route automation for its active frame");

    predator_assist_setup(15750,4741,0);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1],
          "View+A starts assist and consumes the default Jump edge before normal jump mapping");
    dynamics.OrientEuler.EulerY=last_face_yaw; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "holding the recognized View+A chord across frames never leaks a normal Jump request");

    predator_assist_setup(15750,4741,0); route_enabled=0;
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump &&
          !DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1] && strstr(last_speech,"route guidance"),
          "recognized View+A request is consumed and rejected when route guidance is off");

    predator_assist_setup(15750,4741,0); opening_gate_result=1;
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "unlocked-gate check rejects a locked first route gate");

    predator_assist_setup(15750,4741,0);
    PredatorInputPrimaryConfig.Operate=KEY_JOYSTICK_BUTTON_1;
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Operate &&
          player.Mvt_InputRequests.Flags.Rqst_Jump,
          "custom A action prevents assist and keeps the existing bound gameplay mapping");

    predator_assist_setup(15750,4741,0);
    PredatorInputPrimaryConfig.Operate=KEY_JOYSTICK_BUTTON_9;
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Operate &&
          player.Mvt_InputRequests.Flags.Rqst_Jump,
          "custom View binding wins and preserves ordinary A gameplay input");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Jump,
          "solo A keeps its ordinary default Predator Jump behavior");

    predator_assist_setup(25000,5000,8000);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1;
    status_press(KEY_JOYSTICK_BUTTON_1); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "View+A at an unsurveyed position is consumed without an accidental jump");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    dynamics.OrientEuler.EulerY=last_face_yaw;
    mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Forward &&
          player.Mvt_MotionIncrement==ONE_FIXED && player.Mvt_InputRequests.Flags.Rqst_Faster,
          "aligned assist uses normal forward movement requests for run-up");
    KeyboardInput[KEY_W]=1; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Forward &&
          player.Mvt_MotionIncrement==ONE_FIXED,
          "manual movement cancels assist while preserving the user's mapped movement");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    status_press(KEY_J); mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Forward &&
          !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "pressing the explicit start again cancels with no sticky movement or jump");

    predator_assist_setup(15750,4741,0);
    PredatorInputPrimaryConfig.j.PredatorTaunt=KEY_J;
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive(), "custom J binding prevents keyboard assist fallback");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_Q]=1; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_LookUp,
          "manual look input cancels assist and remains mapped");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_2]=1; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_Crouch,
          "manual crouch cancels assist and remains mapped");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_8]=1; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && player.Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon,
          "manual fire cancels assist and remains mapped");

    predator_assist_setup(15750,4741,0);
    KeyboardInput[KEY_JOYSTICK_BUTTON_11]=1; status_press(KEY_J); ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Faster,
          "holding the bound Walk control rejects assist start rather than overriding run speed");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_11]=1; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Faster,
          "manual Walk cancels active assist and preserves the chosen pace");

    predator_assist_setup(15750,4741,0);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1; status_press(KEY_JOYSTICK_BUTTON_1);
    ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1; status_press(KEY_JOYSTICK_BUTTON_1);
    mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Jump,
          "repeating View+A cancels assist without becoming an ordinary Jump");

    predator_assist_setup(15750,4741,0); snap_requests=0;
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    KeyboardInput[KEY_JOYSTICK_BUTTON_9]=1; status_press(KEY_JOYSTICK_BUTTON_12);
    mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && snap_requests==1 && !player.Mvt_InputRequests.Flags.Rqst_Forward,
          "an explicit View+R3 snap request cancels assist before changing facing");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    route_enabled=0; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive(), "turning route guidance off cancels active assist");

    predator_assist_setup(15750,4741,0);
    status_press(KEY_J); ReadPlayerGameInput(&strategy);
    accepts_controls=0; mock_ticks+=33; ReadPlayerGameInput(&strategy);
    check(!AccJumpAssist_IsActive() && !player.Mvt_InputRequests.Flags.Rqst_Forward,
          "focus loss cancels assist and clears only assist-owned input");
}

static void view_modifier_tests(void)
{
    int keys[]={KEY_JOYSTICK_BUTTON_12,KEY_JOYSTICK_BUTTON_14,KEY_JOYSTICK_BUTTON_15,KEY_JOYSTICK_BUTTON_16};
    int i;
    for(i=0;i<4;++i) {
        tracker_setup(); snap_requests=loot_cycles=loot_toggles=route_toggles=history_requests=0;
        MarineInputSecondaryConfig=DefaultMarineInputSecondaryConfig;
        status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
        mock_ticks+=5000; ReadPlayerGameInput(&strategy);
        check(!status_announcements,"View held alone stays silent even before a delayed chord");
        status_press(keys[i]); ReadPlayerGameInput(&strategy);
        check(snap_requests+loot_cycles+loot_toggles+route_toggles==1 && !status_announcements && !history_requests,
              "delayed View chord performs only its requested action");
        KeyboardInput[keys[i]]=0; DebouncedKeyboardInput[keys[i]]=0; ReadPlayerGameInput(&strategy);
        status_press(keys[i]); ReadPlayerGameInput(&strategy);
        check(snap_requests+loot_cycles+loot_toggles+route_toggles==2 && !status_announcements,
              "repeated chord while holding View stays free of status spam");
        KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
        KeyboardInput[keys[i]]=0; DebouncedKeyboardInput[keys[i]]=0; ReadPlayerGameInput(&strategy);
        check(!status_announcements,"releasing a used modifier never announces status");
        status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
        KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
        check(status_announcements==1,"fresh solo View release still announces status after a chord");
    }
    tracker_setup(); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    menu_active=1; ReadPlayerGameInput(&strategy); KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0;
    menu_active=0; ReadPlayerGameInput(&strategy); check(!status_announcements,"pause cancels pending View announcement");
    tracker_setup(); status_press(KEY_JOYSTICK_BUTTON_9); ReadPlayerGameInput(&strategy);
    pad_present=0; KeyboardInput[KEY_JOYSTICK_BUTTON_9]=0; ReadPlayerGameInput(&strategy);
    check(!status_announcements,"disconnect does not resemble solo View release");
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "trace")) trace_scenario();
    else {
        input_tests();
        individual_stick_directions();
        fine_pad_movement();
        status_input_tests();
        tracker_input_tests();
        bridge_clock_tests();
        route_input_tests();
        loot_input_tests();
        snap_input_tests();
        predator_equipment_chord_tests();
        predator_saved_profile_pad_migration_test();
        jump_assist_input_tests();
        view_modifier_tests();
        failures += RunMarinePresetTests();
        failures += RunPredatorPresetTests();
    }
    printf("gameplay input: %d failed assertion(s)\n", failures);
    return failures ? 1 : 0;
}
