/*------------------------------- Patrick 21/10/96 ------------------------------
  Source for reading player inputs.
  Note that whilst ReadUserInput() reads raw input data, the functions in this
  file map those inputs onto the player movement structures (defined in Player
  Status).  This is, of course, entirely platform dependant.  Consoles will
  need their own equivalent functions.... 

  -------------------------------------------------------------------------------*/ 
#include "3dc.h"
#include "module.h"
#include "inline.h"

#include "stratdef.h"
#include "dynblock.h"
#include "gamedef.h"
#include "gameplat.h"

#include "bh_types.h"
#include "ourasert.h"
#include "comp_shp.h"

#include "pmove.h"
#include "usr_io.h"
#include "hud.h"
#include "messagehistory.h"
#include "inventry.h"

#include "iofocus.h"

#include "paintball.h"
#include "ahudgadg.hpp"
#include "avp_menus.h"
#include "acc_pad.h"
#include "acc_status.h"
#include "acc_sonar.h"
#include "acc_objectives.h"
#include "acc_route.h"
#include "acc_route_targets.h"
#include "acc_jump_assist.h"
#include "acc_jump_assist_runtime.h"
#include "acc_snap.h"
#include "acc_speech.h"
#include "acc_tracker.h"
#include "acc_bridge.h"
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <string.h>

extern char LevelName[];

static ACC_JUMP_ASSIST_STATE AccJumpAssistState;
static int AccJumpAssistLastAction = -1;
static int AccJumpAssistGateUnlocked;
static int AccJumpAssistChordAOwned;
static unsigned char AccPredatorEquipmentChordOwned[MAX_NUMBER_OF_INPUT_KEYS];
static int AccPredatorEquipmentChordAction;
static int AccPredatorEquipmentChordTriggered;
static int AccAccess_ViewPending;

enum ACC_PREDATOR_EQUIPMENT_ACTION
{
	ACC_PRED_EQ_NONE,
	ACC_PRED_EQ_ZOOM_IN,
	ACC_PRED_EQ_ZOOM_OUT,
	ACC_PRED_EQ_RECALL_DISC,
	ACC_PRED_EQ_MEDICOMP,
	ACC_PRED_EQ_GRAPPLE,
	ACC_PRED_EQ_TAUNT
};

typedef struct acc_predator_equipment_chord
{
	int key;
	int action;
} ACC_PREDATOR_EQUIPMENT_CHORD;

static const ACC_PREDATOR_EQUIPMENT_CHORD AccPredatorEquipmentChords[] =
{
	{KEY_JOYSTICK_BUTTON_6, ACC_PRED_EQ_ZOOM_IN},       /* View + RB */
	{KEY_JOYSTICK_BUTTON_5, ACC_PRED_EQ_ZOOM_OUT},      /* View + LB */
	{KEY_JOYSTICK_BUTTON_3, ACC_PRED_EQ_RECALL_DISC},   /* View + X */
	{KEY_JOYSTICK_BUTTON_2, ACC_PRED_EQ_MEDICOMP},      /* View + B */
	{KEY_JOYSTICK_BUTTON_4, ACC_PRED_EQ_GRAPPLE},      /* View + Y */
	{KEY_JOYSTICK_BUTTON_11, ACC_PRED_EQ_TAUNT}         /* View + L3 */
};

int AccJumpAssist_IsActive(void)
{
	return AccJumpAssistState.phase != ACC_JUMP_ASSIST_PHASE_IDLE;
}

void AccJumpAssist_ResetRuntime(void)
{
	AccJumpAssist_Reset(&AccJumpAssistState);
	AccJumpAssistLastAction = -1;
	AccJumpAssistGateUnlocked = 0;
	AccJumpAssistChordAOwned = 0;
}

extern int InGameMenusAreRunning(void);
extern void AvP_TriggerInGameMenus(void);
extern void Recall_Disc(void);
extern void ShowMultiplayerScores(void);


extern int NormalFrameTime;

FIXED_INPUT_CONFIGURATION FixedInputConfig =
{
	KEY_1,				// Weapon1;
	KEY_2,				// Weapon2;
	KEY_3,				// Weapon3;
	KEY_4,				// Weapon4;
	KEY_5,				// Weapon5;
	KEY_6,				// Weapon6;
	KEY_7,				// Weapon7;
	KEY_8,				// Weapon8;
	KEY_9,				// Weapon9;
	KEY_0,				// Weapon10;
	KEY_ESCAPE,			// PauseGame;

};

PLAYER_INPUT_CONFIGURATION MarineInputPrimaryConfig;
PLAYER_INPUT_CONFIGURATION MarineInputSecondaryConfig;
PLAYER_INPUT_CONFIGURATION PredatorInputPrimaryConfig;
PLAYER_INPUT_CONFIGURATION PredatorInputSecondaryConfig;
PLAYER_INPUT_CONFIGURATION AlienInputPrimaryConfig;
PLAYER_INPUT_CONFIGURATION AlienInputSecondaryConfig;


#if 1 // English
PLAYER_INPUT_CONFIGURATION DefaultMarineInputPrimaryConfig =
{
	KEY_W,				// Forward;
	KEY_S,			    // Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_A,	 		    // StrafeLeft;
	KEY_D,	 		    // StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_G,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

    {KEY_RBRACKET},  	// NextWeapon;
    {KEY_LBRACKET},  	// PreviousWeapon;
    {KEY_BACKSPACE},	// FlashbackWeapon;

    {KEY_SLASH},	  	// ImageIntensifier;
    {KEY_FSTOP},    	// ThrowFlare;
    {KEY_APOSTROPHE},  	// Jetpack;
    {KEY_SEMICOLON},	// Taunt
    {KEY_F1},
    {KEY_F11},
    {KEY_F12},
    {KEY_TAB}
};
PLAYER_INPUT_CONFIGURATION DefaultPredatorInputPrimaryConfig =
{
    KEY_W,              // Forward;
    KEY_S,              // Backward;
    KEY_NUMPAD4,        // Left;
    KEY_NUMPAD6,        // Right;

    KEY_RIGHTALT,       // Strafe;
    KEY_A,              // StrafeLeft;
    KEY_D,              // StrafeRight;

    KEY_Q,              // LookUp;
    KEY_Z,              // LookDown;
    KEY_G,              // CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;
	
    {KEY_RBRACKET},		// NextWeapon;
    {KEY_LBRACKET}, 	// PreviousWeapon;
    {KEY_BACKSPACE},	// FlashbackWeapon;
	
    {KEY_FSTOP},	 	// Cloak;
    {KEY_SLASH},	 	// CycleVisionMode;
    {KEY_PAGEUP},		// ZoomIn;
    {KEY_PAGEDOWN},		// ZoomOut;
    {KEY_APOSTROPHE},  	// GrapplingHook
    {KEY_COMMA},		// RecallDisk
    {KEY_SEMICOLON},	// Taunt
    {KEY_F1},
    KEY_F11,
    KEY_F12,
    KEY_TAB
};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputPrimaryConfig =
{
    KEY_W,              // Forward;
    KEY_S,              // Backward;
    KEY_NUMPAD4,        // Left;
    KEY_NUMPAD6,        // Right;

    KEY_RIGHTALT,       // Strafe;
    KEY_A,              // StrafeLeft;
    KEY_D,              // StrafeRight;

    KEY_Q,              // LookUp;
    KEY_Z,              // LookDown;
    KEY_G,              // CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

    {KEY_SLASH},		// AlternateVision;
    {KEY_FSTOP},		// Taunt;
    {KEY_F1},
    {KEY_F11},
    {KEY_F12},
    {KEY_TAB}
};
#elif 0	// Dutch
PLAYER_INPUT_CONFIGURATION DefaultMarineInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_ASTERISK,		// NextWeapon;
	KEY_DIACRITIC_UMLAUT,// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;

	KEY_MINUS,	  		// ImageIntensifier;
	KEY_FSTOP,    		// ThrowFlare;
	KEY_DIACRITIC_ACUTE,	  	// Jetpack;
	KEY_PLUS,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
PLAYER_INPUT_CONFIGURATION DefaultPredatorInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;
	
	KEY_ASTERISK,		// NextWeapon;
	KEY_DIACRITIC_UMLAUT,// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;
	
	KEY_FSTOP,	 		// Cloak;
	KEY_MINUS,	 		// CycleVisionMode;
	KEY_PAGEUP,			// ZoomIn;
	KEY_PAGEDOWN,		// ZoomOut;
	KEY_DIACRITIC_ACUTE,	  	// GrapplingHook
	KEY_COMMA,			// RecallDisk
	KEY_PLUS,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_MINUS,			// AlternateVision;
	KEY_FSTOP,				// Taunt;
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
#elif 0	// French
PLAYER_INPUT_CONFIGURATION DefaultMarineInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_A, 				// LookUp;
	KEY_W, 				// LookDown;
	KEY_Q,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_DOLLAR,  		// NextWeapon;
	KEY_DIACRITIC_CARET,  		// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;

	KEY_EXCLAMATION,	  		// ImageIntensifier;
	KEY_COLON,    		// ThrowFlare;
	KEY_U_GRAVE,	  	// Jetpack;
	KEY_M,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
PLAYER_INPUT_CONFIGURATION DefaultPredatorInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_A, 				// LookUp;
	KEY_W, 				// LookDown;
	KEY_Q,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;
	
	KEY_DOLLAR,		// NextWeapon;
	KEY_DIACRITIC_CARET, 			// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;
	
	KEY_COLON,	 		// Cloak;
	KEY_EXCLAMATION,	 		// CycleVisionMode;
	KEY_PAGEUP,			// ZoomIn;
	KEY_PAGEDOWN,		// ZoomOut;
	KEY_U_GRAVE,	  	// GrapplingHook
	KEY_SEMICOLON,			// RecallDisk
	KEY_M,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_A, 				// LookUp;
	KEY_W, 				// LookDown;
	KEY_Q,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_EXCLAMATION,			// AlternateVision;
	KEY_SEMICOLON,				// Taunt;
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
#elif 0	 // German
PLAYER_INPUT_CONFIGURATION DefaultMarineInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Y, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_PLUS,  		// NextWeapon;
	KEY_U_UMLAUT,  		// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;

	KEY_MINUS,	  		// ImageIntensifier;
	KEY_FSTOP,    		// ThrowFlare;
	KEY_A_UMLAUT,	  	// Jetpack;
	KEY_O_UMLAUT,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
PLAYER_INPUT_CONFIGURATION DefaultPredatorInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Y, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;
	
	KEY_PLUS,		// NextWeapon;
	KEY_U_UMLAUT, 			// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;
	
	KEY_FSTOP,	 		// Cloak;
	KEY_MINUS,	 		// CycleVisionMode;
	KEY_PAGEUP,			// ZoomIn;
	KEY_PAGEDOWN,		// ZoomOut;
	KEY_A_UMLAUT,	  	// GrapplingHook
	KEY_COMMA,			// RecallDisk
	KEY_O_UMLAUT,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Y, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_MINUS,			// AlternateVision;
	KEY_FSTOP,				// Taunt;
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
#elif 0	// Spanish
PLAYER_INPUT_CONFIGURATION DefaultMarineInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_PLUS,  		// NextWeapon;
	KEY_DIACRITIC_GRAVE,  		// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;

	KEY_MINUS,	  		// ImageIntensifier;
	KEY_FSTOP,    		// ThrowFlare;
	KEY_DIACRITIC_ACUTE,	  	// Jetpack;
	KEY_N_TILDE,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
PLAYER_INPUT_CONFIGURATION DefaultPredatorInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;
	
	KEY_PLUS,		// NextWeapon;
	KEY_DIACRITIC_GRAVE, 			// PreviousWeapon;
	KEY_BACKSPACE,		// FlashbackWeapon;
	
	KEY_FSTOP,	 		// Cloak;
	KEY_MINUS,	 		// CycleVisionMode;
	KEY_PAGEUP,			// ZoomIn;
	KEY_PAGEDOWN,		// ZoomOut;
	KEY_DIACRITIC_ACUTE,	  	// GrapplingHook
	KEY_COMMA,			// RecallDisk
	KEY_N_TILDE,		// Taunt
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputPrimaryConfig =
{
	KEY_UP,				// Forward;
	KEY_DOWN,			// Backward;
	KEY_NUMPAD4, 		// Left;
	KEY_NUMPAD6, 		// Right;

	KEY_RIGHTALT,		// Strafe;
	KEY_LEFT,	 		// StrafeLeft;
	KEY_RIGHT,	 		// StrafeRight;

	KEY_Q, 				// LookUp;
	KEY_Z, 				// LookDown;
	KEY_A,				// CentreView;

	KEY_LEFTSHIFT,		// Walk;
	KEY_RIGHTCTRL, 		// Crouch;
	KEY_RIGHTSHIFT,		// Jump;

	KEY_SPACE,			// Operate;

	KEY_LMOUSE, 		// FirePrimaryWeapon;
	KEY_RMOUSE, 		// FireSecondaryWeapon;

	KEY_MINUS,			// AlternateVision;
	KEY_FSTOP,				// Taunt;
	KEY_F1,
	KEY_F11,
	KEY_F12,
	KEY_TAB,
};
#endif
/* AVP Access: Marine pad preset; primary keyboard/mouse bindings remain. */
PLAYER_INPUT_CONFIGURATION DefaultMarineInputSecondaryConfig =
{
	KEY_VOID,			// Forward;
	KEY_VOID,			// Backward;
	KEY_VOID, 			// Left;
	KEY_VOID, 			// Right;

	KEY_VOID,			// Strafe;
	KEY_VOID,	 		// StrafeLeft;
	KEY_VOID,	 		// StrafeRight;

	KEY_NUMPAD8,		// LookUp;
	KEY_NUMPAD2,		// LookDown;
	KEY_NUMPAD5,		// CentreView;

	KEY_JOYSTICK_BUTTON_11,		// Walk;
	KEY_JOYSTICK_BUTTON_2, 		// Crouch;
	KEY_JOYSTICK_BUTTON_1,			// Jump;

	KEY_JOYSTICK_BUTTON_3,				// Operate;

	KEY_JOYSTICK_BUTTON_8, 		// FirePrimaryWeapon;
	KEY_JOYSTICK_BUTTON_7, 		// FireSecondaryWeapon;

    {KEY_JOYSTICK_BUTTON_4},  	// NextWeapon;
    {KEY_JOYSTICK_BUTTON_5},	// PreviousWeapon;
    {KEY_VOID},			// FlashbackWeapon;

    {KEY_JOYSTICK_BUTTON_13},			// ImageIntensifier;
    {KEY_JOYSTICK_BUTTON_6}, 		// ThrowFlare;
    {KEY_VOID}, 		// Jetpack;
    {KEY_VOID},			// Taunt

    {KEY_JOYSTICK_BUTTON_12},		/* Marine_MessageHistory: R3. AvP has no objectives
				   screen, so replaying messages is the only way back to the
				   mission text -- it must be reachable from the pad. */
    {KEY_VOID},
    {KEY_VOID},
    {KEY_VOID}
};

static const PLAYER_INPUT_CONFIGURATION LegacyMarineInputSecondaryConfig =
{
	KEY_VOID,			// Forward;
	KEY_VOID,			// Backward;
	KEY_VOID, 			// Left;
	KEY_VOID, 			// Right;

	KEY_VOID,			// Strafe;
	KEY_VOID,	 		// StrafeLeft;
	KEY_VOID,	 		// StrafeRight;

	KEY_NUMPAD8,		// LookUp;
	KEY_NUMPAD2,		// LookDown;
	KEY_NUMPAD5,		// CentreView;

	KEY_VOID,		// Walk;
	KEY_VOID, 		// Crouch;
	KEY_MMOUSE,			// Jump;

	KEY_CR,				// Operate;

	KEY_NUMPAD0, 		// FirePrimaryWeapon;
	KEY_NUMPADDEL, 		// FireSecondaryWeapon;

    {KEY_MOUSEWHEELUP},  	// NextWeapon;
    {KEY_MOUSEWHEELDOWN},	// PreviousWeapon;
    {KEY_VOID},			// FlashbackWeapon;

    {KEY_VOID},			// ImageIntensifier;
    {KEY_VOID}, 		// ThrowFlare;
    {KEY_VOID}, 		// Jetpack;
    {KEY_VOID},			// Taunt

    {KEY_VOID},
    {KEY_VOID},
    {KEY_VOID},
    {KEY_VOID}
};

/* Preserve the language-specific primary Predator layout. Upgrade only the
   shipped secondary defaults; customized profile bindings are user data. */
static const PLAYER_INPUT_CONFIGURATION LegacyPredatorInputSecondaryConfig =
{
	KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID,
	KEY_NUMPAD8, KEY_NUMPAD2, KEY_NUMPAD5, KEY_VOID, KEY_VOID, KEY_MMOUSE,
	KEY_CR, KEY_NUMPAD0, KEY_NUMPADDEL, {KEY_VOID}, {KEY_VOID}, {KEY_VOID},
	{KEY_VOID}, {KEY_VOID}, {KEY_MOUSEWHEELUP}, {KEY_MOUSEWHEELDOWN},
	{KEY_VOID}, {KEY_VOID}, {KEY_VOID}, {KEY_VOID}, KEY_VOID, KEY_VOID,
	KEY_VOID, KEY_VOID
};

/* AVP Access: preserve custom bindings while upgrading the old secondary
   defaults. Compare all bytes, including unused slots, and leave the primary
   keyboard/mouse mapping intact. The configured primary default may have been
   loaded from default.cfg. Applying this twice is harmless: the new secondary
   no longer matches the legacy table. */
int AccPad_UpgradeLegacyMarineBindings(
    const PLAYER_INPUT_CONFIGURATION *primary,
    PLAYER_INPUT_CONFIGURATION *secondary)
{
    if (memcmp(primary, &DefaultMarineInputPrimaryConfig, sizeof(*primary)) != 0)
        return 0;

    if (memcmp(secondary, &LegacyMarineInputSecondaryConfig, sizeof(*secondary)) == 0) {
        *secondary = DefaultMarineInputSecondaryConfig;
        return 1;
    }

    /* Narrower second upgrade: the controller preset gained a message-history
       button after some profiles had already taken the preset. Fill only that
       one slot, and only when it is unset and everything else still matches the
       preset exactly -- so a player who has changed anything, or who cleared
       that binding on purpose against a modified layout, is left alone. */
    if (secondary->h.Marine_MessageHistory == KEY_VOID) {
        PLAYER_INPUT_CONFIGURATION probe = *secondary;
        probe.h.Marine_MessageHistory =
            DefaultMarineInputSecondaryConfig.h.Marine_MessageHistory;

        if (memcmp(&probe, &DefaultMarineInputSecondaryConfig, sizeof(probe)) == 0) {
            *secondary = probe;
            return 1;
        }
    }

    return 0;
}

static int AccPredator_PrimaryHasPadBinding(const PLAYER_INPUT_CONFIGURATION *primary)
{
	const unsigned char *keys = (const unsigned char *)primary;
	int i;
	if (!primary) return 1;
	for (i = 0; i < NUMBER_OF_PREDATOR_INPUTS; ++i)
		if (keys[i] >= KEY_JOYSTICK_BUTTON_1 && keys[i] <= KEY_JOYSTICK_BUTTON_16)
			return 1;
	return 0;
}

static int AccPredator_MatchesLegacySecondary(const PLAYER_INPUT_CONFIGURATION *secondary)
{
	/* Only the 30 active Predator slots identify this shipped layout. Older
	   profile writers left the two expansion bytes as zero, while the static
	   table initializer uses KEY_VOID there. */
	return secondary && memcmp(secondary, &LegacyPredatorInputSecondaryConfig,
		NUMBER_OF_PREDATOR_INPUTS) == 0;
}

static void AccPredator_InstallPadSecondary(PLAYER_INPUT_CONFIGURATION *secondary)
{
	unsigned char expansion7 = secondary->ExpansionSpace7;
	unsigned char expansion8 = secondary->ExpansionSpace8;
	*secondary = DefaultPredatorInputSecondaryConfig;
	secondary->ExpansionSpace7 = expansion7;
	secondary->ExpansionSpace8 = expansion8;
}

int AccPad_UpgradeLegacyPredatorBindings(
	const PLAYER_INPUT_CONFIGURATION *primary,
	PLAYER_INPUT_CONFIGURATION *secondary)
{
	if (!primary || !secondary || AccPredator_PrimaryHasPadBinding(primary)) return 0;
	/* A language-specific or user-custom keyboard primary is compatible: pad
	   defaults go in the secondary table. Keep that primary byte-for-byte. */
	if (AccPredator_MatchesLegacySecondary(secondary)) {
		AccPredator_InstallPadSecondary(secondary);
		return 1;
	}

	/* Earlier profiles may already have the pad preset, but not R3 history.
	   Limit this migration to the exact active preset while allowing a keyboard
	   primary; preserve its expansion bytes as profile-owned data. */
	if (secondary->k.Predator_MessageHistory == KEY_VOID) {
		PLAYER_INPUT_CONFIGURATION probe = *secondary;
		probe.k.Predator_MessageHistory =
			DefaultPredatorInputSecondaryConfig.k.Predator_MessageHistory;
		if (memcmp(&probe, &DefaultPredatorInputSecondaryConfig,
			NUMBER_OF_PREDATOR_INPUTS) == 0) {
			secondary->k.Predator_MessageHistory = probe.k.Predator_MessageHistory;
			return 1;
		}
	}

	return 0;
}





PLAYER_INPUT_CONFIGURATION DefaultPredatorInputSecondaryConfig =
{
	KEY_VOID,			// Forward;
	KEY_VOID,			// Backward;
	KEY_VOID, 			// Left;
	KEY_VOID, 			// Right;

	KEY_VOID,			// Strafe;
	KEY_VOID,	 		// StrafeLeft;
	KEY_VOID,	 		// StrafeRight;

	KEY_NUMPAD8,		// LookUp;
	KEY_NUMPAD2,		// LookDown;
	KEY_NUMPAD5,		// CentreView;

	KEY_JOYSTICK_BUTTON_11,		// Walk;
	KEY_JOYSTICK_BUTTON_2, 		// Crouch;
	KEY_JOYSTICK_BUTTON_1,		// Jump;

	KEY_JOYSTICK_BUTTON_3,		// Operate;

	KEY_JOYSTICK_BUTTON_8,		// FirePrimaryWeapon;
	KEY_JOYSTICK_BUTTON_7,		// FireSecondaryWeapon;

    {KEY_JOYSTICK_BUTTON_4},	// NextWeapon;
    {KEY_JOYSTICK_BUTTON_5},	// PreviousWeapon;
    {KEY_VOID},			// FlashbackWeapon;
	
    {KEY_JOYSTICK_BUTTON_13},	// Cloak;
    {KEY_JOYSTICK_BUTTON_6},	// CycleVisionMode;
    {KEY_MOUSEWHEELUP},	// ZoomIn;
    {KEY_MOUSEWHEELDOWN},	// ZoomOut;
    {KEY_VOID},	 		// GrapplingHook;
    {KEY_VOID},			// RecallDisk
    {KEY_VOID},			// Taunt
	
    {KEY_JOYSTICK_BUTTON_12}, /* Predator_MessageHistory: R3 */
    KEY_VOID,
    KEY_VOID,
    KEY_VOID

};

PLAYER_INPUT_CONFIGURATION DefaultAlienInputSecondaryConfig =
{
	KEY_VOID,				// Forward;
	KEY_VOID,			// Backward;
	KEY_VOID, 		// Left;
	KEY_VOID, 		// Right;

	KEY_VOID,		// Strafe;
	KEY_VOID,	 		// StrafeLeft;
	KEY_VOID,	 		// StrafeRight;

	KEY_NUMPAD8,		// LookUp;
	KEY_NUMPAD2,		// LookDown;
	KEY_NUMPAD5,		// CentreView;

	KEY_VOID,			// Walk;
	KEY_VOID, 			// Crouch;
	KEY_MMOUSE,			// Jump;

	KEY_CR,				// Operate;

	KEY_NUMPAD0, 		// FirePrimaryWeapon;
	KEY_NUMPADDEL, 		// FireSecondaryWeapon;
	
    {KEY_VOID}, 		// AlternateVision;
    {KEY_VOID},	 		// Taunt;
    {KEY_VOID},
    {KEY_VOID},
    {KEY_VOID},
    {KEY_VOID}
};


CONTROL_METHODS ControlMethods =
{
	/* analogue stuff */
	DEFAULT_MOUSEX_SENSITIVITY, //unsigned int MouseXSensitivity;
	DEFAULT_MOUSEY_SENSITIVITY, //unsigned int MouseYSensitivity;

	0,//unsigned int VAxisIsMovement :1; // else it's looking
	1,//unsigned int HAxisIsTurning :1; // else it's sidestepping

	0,//unsigned int FlipVerticalAxis :1;
	
	/* general stuff */
	0,//unsigned int AutoCentreOnMovement :1;
};

CONTROL_METHODS DefaultControlMethods =
{
	/* analogue stuff */
	DEFAULT_MOUSEX_SENSITIVITY, //unsigned int MouseXSensitivity;
	DEFAULT_MOUSEY_SENSITIVITY, //unsigned int MouseYSensitivity;

	0,//unsigned int VAxisIsMovement :1; // else it's looking
	1,//unsigned int HAxisIsTurning :1; // else it's sidestepping

	0,//unsigned int FlipVerticalAxis :1;
	
	/* general stuff */
	0,//unsigned int AutoCentreOnMovement :1;
};

JOYSTICK_CONTROL_METHODS JoystickControlMethods =
{
	0,//unsigned int JoystickEnabled;
	
	1,//unsigned int JoystickVAxisIsMovement; // else it's looking
	1,//unsigned int JoystickHAxisIsTurning;  // else it's sidestepping
	0,//unsigned int JoystickFlipVerticalAxis;

	0,//unsigned int JoystickPOVVAxisIsMovement; // else it's looking
	0,//unsigned int JoystickPOVHAxisIsTurning;	 // else it's sidestepping
	0,//unsigned int JoystickPOVFlipVerticalAxis;

	0,//unsigned int JoystickRudderEnabled;		  
	0,//unsigned int JoystickRudderAxisIsTurning; // else it's sidestepping
	
	0,//unsigned int JoystickTrackerBallEnabled;
	0,//unsigned int JoystickTrackerBallFlipVerticalAxis;
	DEFAULT_TRACKERBALL_HORIZONTAL_SENSITIVITY,//unsigned int JoystickTrackerBallHorizontalSensitivity;
	DEFAULT_TRACKERBALL_VERTICAL_SENSITIVITY,//unsigned int JoystickTrackerBallVerticalSensitivity;

};
JOYSTICK_CONTROL_METHODS DefaultJoystickControlMethods =
{
	0,//unsigned int JoystickEnabled;
	
	1,//unsigned int JoystickVAxisIsMovement; // else it's looking
	1,//unsigned int JoystickHAxisIsTurning;  // else it's sidestepping
	0,//unsigned int JoystickFlipVerticalAxis;

	0,//unsigned int JoystickPOVVAxisIsMovement; // else it's looking
	0,//unsigned int JoystickPOVHAxisIsTurning;	 // else it's sidestepping
	0,//unsigned int JoystickPOVFlipVerticalAxis;

	0,//unsigned int JoystickRudderEnabled;		  
	0,//unsigned int JoystickRudderAxisIsTurning; // else it's sidestepping
	
	0,//unsigned int JoystickTrackerBallEnabled;
	0,//unsigned int JoystickTrackerBallFlipVerticalAxis;
	DEFAULT_TRACKERBALL_HORIZONTAL_SENSITIVITY,//unsigned int JoystickTrackerBallHorizontalSensitivity;
	DEFAULT_TRACKERBALL_VERTICAL_SENSITIVITY,//unsigned int JoystickTrackerBallVerticalSensitivity;
};

/* Extern for global keyboard buffer */
extern unsigned char KeyboardInput[];
extern unsigned char DebouncedKeyboardInput[];
extern int GotJoystick;
extern int GotMouse;

/* initialise the player input structure(s) in the player_status block */
void InitPlayerGameInput(STRATEGYBLOCK* sbPtr)
{
	PLAYER_STATUS *playerStatusPtr;

    /* get the player status block ... */
    playerStatusPtr = (PLAYER_STATUS *) (sbPtr->SBdataptr);
    LOCALASSERT(playerStatusPtr);
	

	/* analogue type inputs */
	playerStatusPtr->Mvt_MotionIncrement = 0;
	playerStatusPtr->Mvt_TurnIncrement = 0;
	playerStatusPtr->Mvt_PitchIncrement = 0;
	playerStatusPtr->Mvt_AnalogueTurning = 0;
	playerStatusPtr->Mvt_AnaloguePitching = 0;
	playerStatusPtr->Mvt_SideStepIncrement = 0;

	/* request flags */
	playerStatusPtr->Mvt_InputRequests.Mask = 0;
	playerStatusPtr->Mvt_InputRequests.Mask2 = 0;

	/* KJL 14:23:54 8/7/97 - default to run */
	playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Faster = 1;

}


/* Trace only input configuration and control state, never profile names or text.
   Analog changes are limited to twice a second; releases and action/menu/focus
   transitions are immediate so a short press cannot disappear from the trace. */
static void AccPad_TraceGameInput(const PLAYER_STATUS *player,
    const PLAYER_INPUT_CONFIGURATION *primary,
    const PLAYER_INPUT_CONFIGURATION *secondary)
{
	extern JOYINFOEX JoystickData;
	static PLAYER_INPUT_CONFIGURATION lastPrimary, lastSecondary;
	static int lastSpecies = -1, haveState;
	static int lastLogged[17], previousActions, previousAxes;
	static Uint64 nextReport;
	int state[17], actions, axes, changedBindings, urgent;
	Uint64 now;
	size_t i;

	if (!AccPadTrace) return;
	changedBindings = lastSpecies != (int)AvP.PlayerType
	    || memcmp(&lastPrimary, primary, sizeof(*primary))
	    || memcmp(&lastSecondary, secondary, sizeof(*secondary));
	if (changedBindings) {
		const unsigned char *p = (const unsigned char *)primary;
		const unsigned char *s = (const unsigned char *)secondary;
		fprintf(stderr, "ACCPAD BIND: species=%d primary=[", (int)AvP.PlayerType);
		for (i = 0; i < sizeof(*primary); i++) fprintf(stderr, "%s%u", i ? "," : "", (unsigned int)p[i]);
		fprintf(stderr, "] secondary=[");
		for (i = 0; i < sizeof(*secondary); i++) fprintf(stderr, "%s%u", i ? "," : "", (unsigned int)s[i]);
		fprintf(stderr, "] jump=%u/%u use=%u/%u fire=%u/%u altFire=%u/%u\n",
		    (unsigned int)primary->Jump, (unsigned int)secondary->Jump,
		    (unsigned int)primary->Operate, (unsigned int)secondary->Operate,
		    (unsigned int)primary->FirePrimaryWeapon, (unsigned int)secondary->FirePrimaryWeapon,
		    (unsigned int)primary->FireSecondaryWeapon, (unsigned int)secondary->FireSecondaryWeapon);
		lastPrimary = *primary;
		lastSecondary = *secondary;
		lastSpecies = (int)AvP.PlayerType;
	}
	state[0] = GotJoystick;
	state[1] = IOFOCUS_AcceptControls();
	state[2] = InGameMenusAreRunning();
	state[3] = DebouncedKeyboardInput[FixedInputConfig.PauseGame] != 0;
	state[4] = (int)JoystickData.dwXpos - 32768;
	state[5] = (int)JoystickData.dwYpos - 32768;
	state[6] = (int)JoystickData.dwUpos - 32768;
	state[7] = (int)JoystickData.dwVpos - 32768;
	state[8] = player->Mvt_MotionIncrement;
	state[9] = player->Mvt_SideStepIncrement;
	state[10] = player->Mvt_TurnIncrement;
	state[11] = player->Mvt_PitchIncrement;
	state[12] = player->Mvt_InputRequests.Flags.Rqst_Jump;
	state[13] = player->Mvt_InputRequests.Flags.Rqst_Operate;
	state[14] = player->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon;
	state[15] = player->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon;
	state[16] = player->Mvt_InputRequests.Flags.Rqst_Crouch;
	actions = state[12] | (state[13] << 1) | (state[14] << 2) | (state[15] << 3) | (state[16] << 4);
	axes = (state[4] != 0) | ((state[5] != 0) << 1)
	    | ((state[6] != 0) << 2) | ((state[7] != 0) << 3);
	now = SDL_GetTicks();
	urgent = !haveState || changedBindings || memcmp(state, lastLogged, 4 * sizeof(int))
	    || actions != previousActions || (previousAxes & ~axes);
	if (urgent || (now >= nextReport && memcmp(state, lastLogged, sizeof(state)))) {
		fprintf(stderr, "ACCPAD GAME: species=%d joystick=%d focus=%d menu=%d pause=%d XYUV=(%d,%d,%d,%d) move=%d strafe=%d turn=%d pitch=%d jump=%d use=%d fire=%d altFire=%d crouch=%d\n",
		    (int)AvP.PlayerType, state[0], state[1], state[2], state[3],
		    state[4], state[5], state[6], state[7], state[8], state[9], state[10], state[11],
		    state[12], state[13], state[14], state[15], state[16]);
		fflush(stderr);
		memcpy(lastLogged, state, sizeof(state));
		haveState = 1;
		nextReport = now + 500;
	}
	previousActions = actions;
	previousAxes = axes;
}

/* Readout shortcuts are fallbacks: an explicit player binding always wins.
   Ignore the reserved expansion bytes at the end of the configuration. */
static int AccAccess_KeyIsBound(int key, const PLAYER_INPUT_CONFIGURATION *primary,
    const PLAYER_INPUT_CONFIGURATION *secondary)
{
	const unsigned char *p = (const unsigned char *)primary;
	const unsigned char *s = (const unsigned char *)secondary;
	int i;
	if (AvP.PlayerType == I_Predator) i = NUMBER_OF_PREDATOR_INPUTS;
	else if (AvP.PlayerType == I_Alien) i = NUMBER_OF_ALIEN_INPUTS;
	else i = NUMBER_OF_MARINE_INPUTS;
	{
		int count = i;
		for (i = 0; i < count; i++)
			if (p[i] == key || s[i] == key) return 1;
	}
	return 0;
}

static int AccAccess_FixedKeyIsBound(int key)
{
	const unsigned char *p = (const unsigned char *)&FixedInputConfig;
	size_t i;
	for (i = 0; i < sizeof(FixedInputConfig); ++i)
		if (p[i] == key) return 1;
	return 0;
}

static void AccPredator_ClearChordBinding(PLAYER_INPUT_CONFIGURATION *config, int action, int key)
{
	switch (action) {
		case ACC_PRED_EQ_ZOOM_IN: if (config->e.CycleVisionMode == key) config->e.CycleVisionMode = KEY_VOID; break;
		case ACC_PRED_EQ_ZOOM_OUT: if (config->b.PreviousWeapon == key) config->b.PreviousWeapon = KEY_VOID; break;
		case ACC_PRED_EQ_RECALL_DISC: if (config->Operate == key) config->Operate = KEY_VOID; break;
		case ACC_PRED_EQ_MEDICOMP: if (config->Crouch == key) config->Crouch = KEY_VOID; break;
		case ACC_PRED_EQ_GRAPPLE: if (config->a.NextWeapon == key) config->a.NextWeapon = KEY_VOID; break;
		case ACC_PRED_EQ_TAUNT: if (config->Walk == key) config->Walk = KEY_VOID; break;
		default: break;
	}
}

static int AccPredator_ChordBindingIsDefault(const ACC_PREDATOR_EQUIPMENT_CHORD *chord,
	const PLAYER_INPUT_CONFIGURATION *primary, const PLAYER_INPUT_CONFIGURATION *secondary)
{
	PLAYER_INPUT_CONFIGURATION p, s;
	unsigned char binding = KEY_VOID;
	if (!chord || !primary || !secondary ||
	    AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary) ||
	    AccAccess_FixedKeyIsBound(KEY_JOYSTICK_BUTTON_9) ||
	    AccAccess_FixedKeyIsBound(chord->key)) return 0;
	switch (chord->action) {
		case ACC_PRED_EQ_ZOOM_IN: binding = secondary->e.CycleVisionMode; break;
		case ACC_PRED_EQ_ZOOM_OUT: binding = secondary->b.PreviousWeapon; break;
		case ACC_PRED_EQ_RECALL_DISC: binding = secondary->Operate; break;
		case ACC_PRED_EQ_MEDICOMP: binding = secondary->Crouch; break;
		case ACC_PRED_EQ_GRAPPLE: binding = secondary->a.NextWeapon; break;
		case ACC_PRED_EQ_TAUNT: binding = secondary->Walk; break;
		default: return 0;
	}
	/* The chord may borrow only the exact stock gamepad binding in the
	   secondary preset. A remap in either preset or another action wins. */
	if (binding != chord->key) return 0;
	p = *primary; s = *secondary;
	AccPredator_ClearChordBinding(&s, chord->action, chord->key);
	return !AccAccess_KeyIsBound(chord->key, &p, &s);
}

static int AccPredator_EquipmentChordMask(PLAYER_STATUS *player,
	const PLAYER_INPUT_CONFIGURATION *primary, const PLAYER_INPUT_CONFIGURATION *secondary,
	PLAYER_INPUT_CONFIGURATION *maskedPrimary, PLAYER_INPUT_CONFIGURATION *maskedSecondary)
{
	int i, candidates = 0, heldCandidates = 0, selected = -1, active = 0, alreadyHeld = 0;
	int canExecute;
	if (!player || !primary || !secondary || !maskedPrimary || !maskedSecondary) return 0;
	if (AvP.PlayerType != I_Predator) {
		memset(AccPredatorEquipmentChordOwned, 0, sizeof(AccPredatorEquipmentChordOwned));
		AccPredatorEquipmentChordAction = ACC_PRED_EQ_NONE;
		return 0;
	}
	*maskedPrimary = *primary;
	*maskedSecondary = *secondary;
	for (i = 0; i < (int)(sizeof(AccPredatorEquipmentChords)/sizeof(AccPredatorEquipmentChords[0])); ++i) {
		const ACC_PREDATOR_EQUIPMENT_CHORD *chord = &AccPredatorEquipmentChords[i];
		if (AccPredatorEquipmentChordOwned[chord->key]) {
			if (!KeyboardInput[chord->key]) AccPredatorEquipmentChordOwned[chord->key] = 0;
			else {
				AccPredator_ClearChordBinding(maskedPrimary, chord->action, chord->key);
				AccPredator_ClearChordBinding(maskedSecondary, chord->action, chord->key);
				alreadyHeld = active = 1;
			}
		}
	}
	if (!KeyboardInput[KEY_JOYSTICK_BUTTON_9])
		AccPredatorEquipmentChordOwned[KEY_JOYSTICK_BUTTON_9] = 0;
	canExecute = IOFOCUS_AcceptControls() && !InGameMenusAreRunning() && player->IsAlive &&
		!player->DemoMode && !AvP.LevelCompleted;
	if (!KeyboardInput[KEY_JOYSTICK_BUTTON_9]) return active;
	for (i = 0; i < (int)(sizeof(AccPredatorEquipmentChords)/sizeof(AccPredatorEquipmentChords[0])); ++i) {
		const ACC_PREDATOR_EQUIPMENT_CHORD *chord = &AccPredatorEquipmentChords[i];
		if (KeyboardInput[chord->key] && AccPredator_ChordBindingIsDefault(chord, primary, secondary)) {
			++heldCandidates;
			if (DebouncedKeyboardInput[chord->key]) { ++candidates; selected = i; }
		}
	}
	if (!heldCandidates) return active;
	AccPredatorEquipmentChordOwned[KEY_JOYSTICK_BUTTON_9] = 1;
	AccAccess_ViewPending = 0;
	DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] = 0;
	for (i = 0; i < (int)(sizeof(AccPredatorEquipmentChords)/sizeof(AccPredatorEquipmentChords[0])); ++i) {
		const ACC_PREDATOR_EQUIPMENT_CHORD *chord = &AccPredatorEquipmentChords[i];
		if (KeyboardInput[chord->key] && AccPredator_ChordBindingIsDefault(chord, primary, secondary)) {
			AccPredatorEquipmentChordOwned[chord->key] = 1;
			DebouncedKeyboardInput[chord->key] = 0;
		}
	}
	for (i = 0; i < (int)(sizeof(AccPredatorEquipmentChords)/sizeof(AccPredatorEquipmentChords[0])); ++i)
		if (AccPredatorEquipmentChordOwned[AccPredatorEquipmentChords[i].key]) {
			AccPredator_ClearChordBinding(maskedPrimary, AccPredatorEquipmentChords[i].action,
				AccPredatorEquipmentChords[i].key);
			AccPredator_ClearChordBinding(maskedSecondary, AccPredatorEquipmentChords[i].action,
				AccPredatorEquipmentChords[i].key);
		}
	AccPredatorEquipmentChordAction = (canExecute && candidates == 1 && heldCandidates == 1 && !alreadyHeld)
		? AccPredatorEquipmentChords[selected].action : -1;
	AccPredatorEquipmentChordTriggered = canExecute && candidates > 0;
	if (canExecute && candidates > 0 && (candidates > 1 || heldCandidates > 1 || alreadyHeld))
		AccSpeech_Say("Use one equipment chord at a time.", 1);
	return 1;
}

static int AccJumpAssist_DefaultAJump(const PLAYER_INPUT_CONFIGURATION *primary,
	const PLAYER_INPUT_CONFIGURATION *secondary)
{
	PLAYER_INPUT_CONFIGURATION p, s;
	if (AvP.PlayerType != I_Predator || !primary || !secondary ||
	    secondary->Jump != KEY_JOYSTICK_BUTTON_1) return 0;
	p = *primary; s = *secondary;
	/* A is eligible only when the default secondary Jump is its sole binding.
	   Every custom action, including a primary Jump remap, takes priority. */
	s.Jump = KEY_VOID;
	return !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_1, &p, &s);
}

static int AccJumpAssist_ManualInput(const PLAYER_STATUS *player)
{
	return (player->Mvt_InputRequests.Mask & ~INPUT_BITMASK_FASTER) != 0 ||
	       player->Mvt_MotionIncrement != 0 || player->Mvt_TurnIncrement != 0 ||
	       player->Mvt_PitchIncrement != 0 || player->Mvt_SideStepIncrement != 0 ||
	       !player->Mvt_InputRequests.Flags.Rqst_Faster;
}

static void AccJumpAssist_SayAction(int action)
{
	const char *text = NULL;
	if (action == AccJumpAssistLastAction) return;
	switch (action) {
		case ACC_JUMP_ASSIST_ALIGN_YAW: text = "Jump assist active. Moving the controls cancels assistance. Aligning."; break;
		case ACC_JUMP_ASSIST_RUN_FORWARD: text = "Running to the launch point."; break;
		case ACC_JUMP_ASSIST_JUMP_ONCE: text = "Jumping."; break;
		case ACC_JUMP_ASSIST_CONTINUE_FLIGHT: text = "Crossing the gap."; break;
		case ACC_JUMP_ASSIST_DONE: text = "Landing confirmed. Jump assist complete."; break;
		case ACC_JUMP_ASSIST_RECOVER: text = "Jump not confirmed. Stop and return to a surveyed staging point."; break;
		case ACC_JUMP_ASSIST_CANCELLED: text = "Jump assist cancelled. Your controls are yours."; break;
		case ACC_JUMP_ASSIST_NOT_ELIGIBLE: text = "Jump assist is unavailable here. Use route guidance at a surveyed launch point."; break;
	}
	AccJumpAssistLastAction = action;
	if (text) AccSpeech_Say(text, 1);
}

/* Consumes only explicit, unbound starts. The returned flag keeps route
   automation from replacing the player's bounded maneuver this frame. */
static int AccJumpAssist_HandleInput(STRATEGYBLOCK *strategy, PLAYER_STATUS *player,
	const PLAYER_INPUT_CONFIGURATION *primary, const PLAYER_INPUT_CONFIGURATION *secondary,
	int snapRequest)
{
	DYNAMICSBLOCK *d = strategy ? strategy->DynPtr : NULL;
	int active = AccJumpAssist_IsActive();
	int keyboardStart, gamepadStart, start, gateUnlocked = 0, room = -1, gateResult = 0;
	int assistFrame = 0;
	ACC_JUMP_ASSIST_INPUT input;
	ACC_JUMP_ASSIST_OUTPUT output;
	VECTORCH gatePosition;
	STRATEGYBLOCK *gateControl = NULL;
	keyboardStart = AvP.PlayerType == I_Predator && DebouncedKeyboardInput[KEY_J] &&
		!AccAccess_KeyIsBound(KEY_J, primary, secondary);
	gamepadStart = AvP.PlayerType == I_Predator && KeyboardInput[KEY_JOYSTICK_BUTTON_9] &&
		DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1] &&
		!AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary) &&
		AccJumpAssist_DefaultAJump(primary, secondary);
	start = keyboardStart || gamepadStart;
	if (keyboardStart) DebouncedKeyboardInput[KEY_J] = 0;
	if (gamepadStart)
		DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_1] = 0;
	if (!active && !start) return 0;
	memset(&input, 0, sizeof(input));
	input.level_name = LevelName;
	input.is_predator = AvP.PlayerType == I_Predator;
	input.room_index = strategy && strategy->containingModule && strategy->containingModule->m_aimodule
		? strategy->containingModule->m_aimodule->m_index : -1;
	if (d) {
		input.grounded = d->IsInContactWithFloor;
		input.foot_x = d->Position.vx; input.foot_y = d->Position.vy; input.foot_z = d->Position.vz;
		input.yaw = d->OrientEuler.EulerY;
		input.velocity_x = d->LinVelocity.vx; input.velocity_z = d->LinVelocity.vz;
	}
	input.now_ms = AccBridge_NowMs();
	if (start && !active && !snapRequest && !AccJumpAssist_ManualInput(player) &&
	    !AccPredatorEquipmentChordTriggered) {
		if (IOFOCUS_AcceptControls() && !InGameMenusAreRunning() && player->IsAlive &&
		    !player->DemoMode && !AvP.LevelCompleted && AvP.Network == I_No_Network &&
		    AccRoute_IsEnabled() && d && input.room_index == 94 &&
		    !strcmp(LevelName, "fall")) {
			gateResult = AccRoute_FallPredatorOpening(&d->Position, &gatePosition, &gateControl);
			gateUnlocked = gateResult == 2;
			if (gateUnlocked) AccJumpAssistGateUnlocked = 1;
		}
		input.gate_unlocked = gateUnlocked;
		if (gateUnlocked) input.explicit_start = 1;
	} else if (start && active) {
		input.cancel = 1;
	} else if (active) {
		input.gate_unlocked = AccJumpAssistGateUnlocked;
		input.cancel = !player->IsAlive || player->DemoMode || AvP.LevelCompleted ||
			AvP.Network != I_No_Network || !IOFOCUS_AcceptControls() ||
			InGameMenusAreRunning() || !AccRoute_IsEnabled() || !d ||
			AccJumpAssist_ManualInput(player) || snapRequest || AccPredatorEquipmentChordTriggered;
	}
	if (start && !active && snapRequest) {
		AccSpeech_Say("Jump assist not started because snap was requested.", 1);
		return 0;
	}
	if (start && !active && AccPredatorEquipmentChordTriggered) {
		AccSpeech_Say("Jump assist not started because equipment was requested.", 1);
		return 0;
	}
	if (start && !active && AccJumpAssist_ManualInput(player)) {
		AccJumpAssist_SayAction(ACC_JUMP_ASSIST_NOT_ELIGIBLE);
		return 0;
	}
	if (start && !active && !input.explicit_start) {
		if (!AccRoute_IsEnabled()) AccSpeech_Say("Turn route guidance on before starting jump assist.", 1);
		else AccJumpAssist_SayAction(ACC_JUMP_ASSIST_NOT_ELIGIBLE);
		return 0;
	}
	assistFrame = active || input.explicit_start;
	{
		int phaseBefore = AccJumpAssistState.phase;
	if (!AccJumpAssist_Update(&input, &AccJumpAssistState, &output)) return assistFrame;
	if (AccPadTrace && (output.action == ACC_JUMP_ASSIST_RECOVER ||
	    phaseBefore == ACC_JUMP_ASSIST_PHASE_JUMP_PENDING ||
	    (phaseBefore == ACC_JUMP_ASSIST_PHASE_FLIGHT && input.grounded))) {
		fprintf(stderr, "ACCJUMP TRACE: t=%u phase_start=%u phase=%d->%d action=%d failure=%d pos=(%d,%d,%d) grounded=%d room=%d yaw=%d vel=(%d,%d) gate=%d\n",
			input.now_ms, AccJumpAssistState.phase_started_ms, phaseBefore,
			AccJumpAssistState.phase, output.action,
			output.failure_reason, input.foot_x, input.foot_y, input.foot_z,
			input.grounded, input.room_index, input.yaw, input.velocity_x,
			input.velocity_z, input.gate_unlocked);
		fflush(stderr);
	}
	AccJumpAssist_SayAction(output.action);
	if (output.action == ACC_JUMP_ASSIST_ALIGN_YAW) {
		AccSnap_FaceYaw(output.desired_yaw);
	} else if (output.action == ACC_JUMP_ASSIST_RUN_FORWARD ||
		   output.action == ACC_JUMP_ASSIST_JUMP_ONCE ||
		   output.action == ACC_JUMP_ASSIST_WAIT_TAKEOFF ||
		   output.action == ACC_JUMP_ASSIST_CONTINUE_FLIGHT) {
		player->Mvt_InputRequests.Flags.Rqst_Forward = 1;
		player->Mvt_InputRequests.Flags.Rqst_Faster = 1;
		player->Mvt_MotionIncrement = ONE_FIXED;
		if (output.action == ACC_JUMP_ASSIST_JUMP_ONCE)
			player->Mvt_InputRequests.Flags.Rqst_Jump = 1;
	}
	if (output.action == ACC_JUMP_ASSIST_DONE || output.action == ACC_JUMP_ASSIST_CANCELLED ||
	    output.action == ACC_JUMP_ASSIST_RECOVER || output.action == ACC_JUMP_ASSIST_NOT_ELIGIBLE)
		AccJumpAssistLastAction = -1, AccJumpAssistGateUnlocked = 0;
	return assistFrame;
	}
}

static int AccAccess_ViewPending;
static void AccAccess_CheckRequests(const PLAYER_STATUS *player, const DYNAMICSBLOCK *dynamics,
    const PLAYER_INPUT_CONFIGURATION *primary,
    const PLAYER_INPUT_CONFIGURATION *secondary)
{
	int statusKeyboard, statusGamepad, trackerKeyboard, trackerGamepad;
	int sonarKeyboard, sonarGamepad, objectivesKeyboard, objectivesGamepad;
	int wantTracker, wantSonar, wantObjectives;
	int routeKeyboard, routeGamepad;
	int lootKeyboard, lootGamepad, lootGoKeyboard, lootGoGamepad;
	int viewEdge, key;
	if ((AvP.PlayerType != I_Marine && AvP.PlayerType != I_Predator) || !player->IsAlive || player->DemoMode
	    || AvP.LevelCompleted || !IOFOCUS_AcceptControls() || InGameMenusAreRunning()) {
	    AccAccess_ViewPending=0; return;
	}
	statusKeyboard = DebouncedKeyboardInput[KEY_H]
	    && !AccAccess_KeyIsBound(KEY_H, primary, secondary);
	viewEdge = DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary);
	/* View is a modifier. Announce status only after a solo press is released,
	   never while the player is still choosing the second chord button. */
	if(viewEdge) AccAccess_ViewPending=1;
	if(AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9,primary,secondary) || !AccPad_IsPresent())
	    AccAccess_ViewPending=0;
	for(key=KEY_JOYSTICK_BUTTON_1;key<=KEY_JOYSTICK_BUTTON_16;++key)
	    if(key!=KEY_JOYSTICK_BUTTON_9 && (KeyboardInput[key] || DebouncedKeyboardInput[key]))
	        AccAccess_ViewPending=0;
	if(statusKeyboard) AccAccess_ViewPending=0;
	statusGamepad=AccAccess_ViewPending && !KeyboardInput[KEY_JOYSTICK_BUTTON_9];
	if(!KeyboardInput[KEY_JOYSTICK_BUTTON_9]) AccAccess_ViewPending=0;
	trackerKeyboard = DebouncedKeyboardInput[KEY_T]
	    && !AccAccess_KeyIsBound(KEY_T, primary, secondary);
	trackerGamepad = DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14]
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_14, primary, secondary);
	sonarKeyboard = DebouncedKeyboardInput[KEY_R]
	    && !AccAccess_KeyIsBound(KEY_R, primary, secondary);
	sonarGamepad = DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_15]
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_15, primary, secondary);
	objectivesKeyboard = DebouncedKeyboardInput[KEY_O]
	    && !AccAccess_KeyIsBound(KEY_O, primary, secondary);
	objectivesGamepad = DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_16]
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_16, primary, secondary);
	routeKeyboard = DebouncedKeyboardInput[KEY_N]
	    && !AccAccess_KeyIsBound(KEY_N, primary, secondary);
	routeGamepad = KeyboardInput[KEY_JOYSTICK_BUTTON_9]
	    && KeyboardInput[KEY_JOYSTICK_BUTTON_16]
	    && (DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]
	        || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_16])
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary)
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_16, primary, secondary);
	if (routeGamepad) {
		statusGamepad = objectivesGamepad = 0;
		DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] = 0;
		DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_16] = 0;
	}
	lootKeyboard = DebouncedKeyboardInput[KEY_L] && !AccAccess_KeyIsBound(KEY_L, primary, secondary);
	lootGoKeyboard = DebouncedKeyboardInput[KEY_K] && !AccAccess_KeyIsBound(KEY_K, primary, secondary);
	lootGamepad = KeyboardInput[KEY_JOYSTICK_BUTTON_9] && KeyboardInput[KEY_JOYSTICK_BUTTON_15]
	    && (DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_15])
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary)
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_15, primary, secondary);
	lootGoGamepad = KeyboardInput[KEY_JOYSTICK_BUTTON_9] && KeyboardInput[KEY_JOYSTICK_BUTTON_14]
	    && (DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14])
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primary, secondary)
	    && !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_14, primary, secondary);
	if(lootGamepad) {
	    statusGamepad=sonarGamepad=0;
	    DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]=DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_15]=0;
	}
	if(lootGoGamepad) {
	    statusGamepad=trackerGamepad=0;
	    DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]=DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14]=0;
	}
	/* Keep the edge available through chord detection, then consume even when
	   solo status is deferred. Custom View actions retain their original edge. */
	if(viewEdge) DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]=0;

	/* Both the tracker and the sonar need the player's position and heading;
	   the objective list does not. */
	wantTracker = (trackerKeyboard || trackerGamepad) && dynamics != NULL;
	wantSonar = (sonarKeyboard || sonarGamepad) && dynamics != NULL;
	wantObjectives = objectivesKeyboard || objectivesGamepad;
	if (!statusKeyboard && !statusGamepad && !wantTracker && !wantSonar
	    && !wantObjectives && !routeKeyboard && !routeGamepad
	    && !lootKeyboard && !lootGamepad && !lootGoKeyboard && !lootGoGamepad) return;

	/* Consume only our unbound shortcuts so another read in this same frame
	   cannot repeat the announcement. Leave custom gameplay bindings intact. */
	if (statusKeyboard) DebouncedKeyboardInput[KEY_H] = 0;
	if (statusGamepad) DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9] = 0;
	if (trackerKeyboard) DebouncedKeyboardInput[KEY_T] = 0;
	if (trackerGamepad) DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_14] = 0;
	if (sonarKeyboard) DebouncedKeyboardInput[KEY_R] = 0;
	if (sonarGamepad) DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_15] = 0;
	if (objectivesKeyboard) DebouncedKeyboardInput[KEY_O] = 0;
	if (objectivesGamepad) DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_16] = 0;
	if (routeKeyboard) DebouncedKeyboardInput[KEY_N] = 0;
	if (lootKeyboard) DebouncedKeyboardInput[KEY_L] = 0;
	if (lootGoKeyboard) DebouncedKeyboardInput[KEY_K] = 0;
	/* One request per frame. The guidance toggle has priority so it can always
	   be stopped; then your own condition, then what is hunting
	   you, then the shape of the room, then the mission. Every edge is consumed
	   above regardless, so a losing request cannot fire again next frame. */
	if (routeKeyboard || routeGamepad) AccRoute_Toggle();
	else if (lootGoKeyboard || lootGoGamepad) AccRoute_ToggleLoot();
	else if (lootKeyboard || lootGamepad) AccRoute_CycleLoot();
	else if (statusKeyboard || statusGamepad) {
		if (AvP.PlayerType == I_Predator) AccStatus_AnnouncePredator(player);
		else AccStatus_AnnounceMarine(player);
	}
	else if (wantTracker && AvP.PlayerType == I_Predator)
		AccSpeech_Say("The Marine motion tracker is not available to the Predator. Use Predator vision modes and targeting guidance.", 1);
	else if (wantTracker)
		AccTracker_Announce(&dynamics->Position, dynamics->OrientEuler.EulerY);
	else if (wantSonar)
		AccSonar_Request(&dynamics->Position, dynamics->OrientEuler.EulerY,
		                 AccBridge_NowMs());
	else if (wantObjectives)
		AccObjectives_Announce();
}

/* This function maps raw inputs onto the players movement attributes in
   the player_status block.  It is called from the ExecuteFreeMovement
   function.
   NB Currently, only keyboard input is supported. */
void ReadPlayerGameInput(STRATEGYBLOCK* sbPtr)
{
	int snapRequest=0;
	int suppressDefaultAJump=0;
	int jumpAssistFrame=0;
	int equipmentChordActive=0;
	PLAYER_INPUT_CONFIGURATION *primaryInput;
	PLAYER_INPUT_CONFIGURATION *secondaryInput;
	PLAYER_INPUT_CONFIGURATION maskedPrimary, maskedSecondary;
	PLAYER_STATUS *playerStatusPtr;
	AccPredatorEquipmentChordTriggered = 0;

    /* get the player status block ... */
    playerStatusPtr = (PLAYER_STATUS *) (sbPtr->SBdataptr);
    LOCALASSERT(playerStatusPtr);
	
	/* start off by initialising the inputs */
	InitPlayerGameInput(sbPtr);

	switch (AvP.PlayerType)
	{
		default:
		case I_Marine:
		{
			primaryInput = &MarineInputPrimaryConfig;
			secondaryInput = &MarineInputSecondaryConfig;
			break;
		}
		case I_Predator:
		{
			primaryInput = &PredatorInputPrimaryConfig;
			secondaryInput = &PredatorInputSecondaryConfig;
			break;
		}
	case I_Alien:
		{
			primaryInput = &AlienInputPrimaryConfig;
			secondaryInput = &AlienInputSecondaryConfig;
			break;
		}
	}
	equipmentChordActive = AccPredator_EquipmentChordMask(playerStatusPtr,
		primaryInput, secondaryInput, &maskedPrimary, &maskedSecondary);
	if (AvP.PlayerType == I_Predator) {
		if (equipmentChordActive) {
			primaryInput = &maskedPrimary;
			secondaryInput = &maskedSecondary;
			if (AccPredatorEquipmentChordTriggered) {
				switch (AccPredatorEquipmentChordAction) {
					case ACC_PRED_EQ_ZOOM_IN:
					{
						extern int CameraZoomLevel;
						int previous = CameraZoomLevel;
						if (CameraZoomLevel < 3) ++CameraZoomLevel;
						if (CameraZoomLevel != previous) {
							if (CameraZoomLevel == 0) AccSpeech_Say("Normal view.", 1);
						else {
							char text[48];
							snprintf(text, sizeof(text), "Zoom level %d of 3.", CameraZoomLevel);
							AccSpeech_Say(text, 1);
						}
						}
						break;
					}
					case ACC_PRED_EQ_ZOOM_OUT:
					{
						extern int CameraZoomLevel;
						int previous = CameraZoomLevel;
						if (CameraZoomLevel > 0) --CameraZoomLevel;
						if (CameraZoomLevel != previous) {
							if (CameraZoomLevel == 0) AccSpeech_Say("Normal view.", 1);
							else {
							char text[48];
							snprintf(text, sizeof(text), "Zoom level %d of 3.", CameraZoomLevel);
							AccSpeech_Say(text, 1);
						}
						}
						break;
					}
					case ACC_PRED_EQ_RECALL_DISC:
						Recall_Disc();
						AccSpeech_Say("Disc recall requested.", 1);
						break;
					case ACC_PRED_EQ_MEDICOMP:
					{
						int slot = SlotForThisWeapon(WEAPON_PRED_MEDICOMP);
						if (slot >= 0 && playerStatusPtr->WeaponSlot[slot].Possessed == 1) {
							playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = slot + 1;
							AccSpeech_Say("Selecting Predator medicomp.", 1);
						} else AccSpeech_Say("Predator medicomp is unavailable.", 1);
						break;
					}
					case ACC_PRED_EQ_GRAPPLE:
#if !(PREDATOR_DEMO||DEATHMATCH_DEMO)
						if (playerStatusPtr->GrapplingHookEnabled) {
							playerStatusPtr->Mvt_InputRequests.Flags.Rqst_GrapplingHook = 1;
							AccSpeech_Say("Grappling hook requested.", 1);
						} else AccSpeech_Say("Grappling hook unavailable with current equipment.", 1);
#else
						AccSpeech_Say("Grappling hook is unavailable in this edition.", 1);
#endif
						break;
					case ACC_PRED_EQ_TAUNT:
						if (playerStatusPtr->tauntTimer) AccSpeech_Say("Taunt already active.", 1);
						else { StartPlayerTaunt(); AccSpeech_Say("Taunt started.", 1); }
						break;
					default: break;
				}
			}
		}
	}
	/* Reserve View+A before the bound Jump mapping runs. The handler consumes
	   the edge later, whether it starts or safely rejects the request. */
	if (!KeyboardInput[KEY_JOYSTICK_BUTTON_9] && !KeyboardInput[KEY_JOYSTICK_BUTTON_1])
		AccJumpAssistChordAOwned = 0;
	if (AvP.PlayerType == I_Predator && KeyboardInput[KEY_JOYSTICK_BUTTON_9] &&
	    KeyboardInput[KEY_JOYSTICK_BUTTON_1] &&
	    !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primaryInput, secondaryInput) &&
	    AccJumpAssist_DefaultAJump(primaryInput, secondaryInput))
		AccJumpAssistChordAOwned = 1;
	if (AvP.PlayerType == I_Predator && KeyboardInput[KEY_JOYSTICK_BUTTON_1] &&
	    AccJumpAssistChordAOwned &&
	    !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9, primaryInput, secondaryInput) &&
	    AccJumpAssist_DefaultAJump(primaryInput, secondaryInput))
		suppressDefaultAJump = 1;

	/* Claim View+R3 before the ordinary message-history action sees R3.
	   Only the history binding is allowed to share R3; custom actions win. */
	if((AvP.PlayerType==I_Marine || AvP.PlayerType==I_Predator) && AvP.Network==I_No_Network && playerStatusPtr->IsAlive &&
	   !playerStatusPtr->DemoMode && !AvP.LevelCompleted && IOFOCUS_AcceptControls() &&
	   !InGameMenusAreRunning() && KeyboardInput[KEY_JOYSTICK_BUTTON_9] &&
	   DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_12]) {
	    PLAYER_INPUT_CONFIGURATION p=*primaryInput,s=*secondaryInput;
	    if(AvP.PlayerType==I_Marine) {
	        if(p.h.Marine_MessageHistory==KEY_JOYSTICK_BUTTON_12) p.h.Marine_MessageHistory=KEY_VOID;
	        if(s.h.Marine_MessageHistory==KEY_JOYSTICK_BUTTON_12) s.h.Marine_MessageHistory=KEY_VOID;
	    } else {
	        if(p.k.Predator_MessageHistory==KEY_JOYSTICK_BUTTON_12) p.k.Predator_MessageHistory=KEY_VOID;
	        if(s.k.Predator_MessageHistory==KEY_JOYSTICK_BUTTON_12) s.k.Predator_MessageHistory=KEY_VOID;
	    }
	    if(!AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_9,&p,&s) &&
	       !AccAccess_KeyIsBound(KEY_JOYSTICK_BUTTON_12,&p,&s)) {
	        snapRequest=1;
	        DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_9]=DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_12]=0;
	    }
	}
	if ( IOFOCUS_AcceptControls() && !InGameMenusAreRunning())
	{
		/* now do forward,backward,left,right,up and down 
		   IMPORTANT:  The request flag and the movement 
		   increment must BOTH be set!
		*/
		if(KeyboardInput[primaryInput->Forward]
		 ||KeyboardInput[secondaryInput->Forward])
		{
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward = 1;
			playerStatusPtr->Mvt_MotionIncrement = ONE_FIXED;
		}	
		if(KeyboardInput[primaryInput->Backward]
		 ||KeyboardInput[secondaryInput->Backward])
		{
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward = 1;
			playerStatusPtr->Mvt_MotionIncrement = -ONE_FIXED;
		}
		if(KeyboardInput[primaryInput->Left]
		 ||KeyboardInput[secondaryInput->Left])
		{
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
			playerStatusPtr->Mvt_TurnIncrement = -ONE_FIXED;
		}
		if(KeyboardInput[primaryInput->Right]
		 ||KeyboardInput[secondaryInput->Right])
		{
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
			playerStatusPtr->Mvt_TurnIncrement = ONE_FIXED;
		}

		if(KeyboardInput[primaryInput->StrafeLeft]
		 ||KeyboardInput[secondaryInput->StrafeLeft])
		{
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
			playerStatusPtr->Mvt_SideStepIncrement = -ONE_FIXED;
		}
		if(KeyboardInput[primaryInput->StrafeRight]
		 ||KeyboardInput[secondaryInput->StrafeRight])
		{	
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
			playerStatusPtr->Mvt_SideStepIncrement = ONE_FIXED;
		}
		if(KeyboardInput[primaryInput->Walk]
		 ||KeyboardInput[secondaryInput->Walk])
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Faster = 0;
		
		if(KeyboardInput[primaryInput->Strafe]
		 ||KeyboardInput[secondaryInput->Strafe])
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Strafe = 1;

		if(KeyboardInput[primaryInput->Crouch]
		 ||KeyboardInput[secondaryInput->Crouch])
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Crouch = 1;
		
		if(KeyboardInput[primaryInput->Jump]
		 ||(KeyboardInput[secondaryInput->Jump] &&
		    !(suppressDefaultAJump && secondaryInput->Jump == KEY_JOYSTICK_BUTTON_1)))
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Jump = 1;

		if(KeyboardInput[primaryInput->Operate]
		 ||KeyboardInput[secondaryInput->Operate])
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Operate = 1;

		/* check for character specific abilities */
		if (playerStatusPtr->IsAlive)
		switch (AvP.PlayerType)
		{
			case I_Marine:
			{
				if(KeyboardInput[primaryInput->d.ImageIntensifier]
				 ||KeyboardInput[secondaryInput->d.ImageIntensifier])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_ChangeVision = 1;

				if(DebouncedKeyboardInput[primaryInput->e.ThrowFlare]
				 ||DebouncedKeyboardInput[secondaryInput->e.ThrowFlare])
					ThrowAFlare();

				#if !(MARINE_DEMO||DEATHMATCH_DEMO)
				if(KeyboardInput[primaryInput->f.Jetpack]
				 ||KeyboardInput[secondaryInput->f.Jetpack])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Jetpack = 1;
				#endif
				
				if(KeyboardInput[primaryInput->g.MarineTaunt]
				 ||KeyboardInput[secondaryInput->g.MarineTaunt])
					StartPlayerTaunt();
				
				if(DebouncedKeyboardInput[primaryInput->h.Marine_MessageHistory]
				 ||DebouncedKeyboardInput[secondaryInput->h.Marine_MessageHistory])
					MessageHistory_DisplayPrevious();
					
				if(DebouncedKeyboardInput[primaryInput->i.Marine_Say]
				 ||DebouncedKeyboardInput[secondaryInput->i.Marine_Say])
					BringDownConsoleWithSayTypedIn();

				if(DebouncedKeyboardInput[primaryInput->j.Marine_SpeciesSay]
				 ||DebouncedKeyboardInput[secondaryInput->j.Marine_SpeciesSay])
					BringDownConsoleWithSaySpeciesTypedIn();

				if(KeyboardInput[primaryInput->k.Marine_ShowScores]
				 ||KeyboardInput[secondaryInput->k.Marine_ShowScores])
					ShowMultiplayerScores();
					

				break;
			}
			case I_Predator:
			{
				extern int CameraZoomLevel;
				
				if(KeyboardInput[primaryInput->d.Cloak]
				 ||KeyboardInput[secondaryInput->d.Cloak])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_ChangeVision = 1;
				
				if(DebouncedKeyboardInput[primaryInput->e.CycleVisionMode]
				 ||DebouncedKeyboardInput[secondaryInput->e.CycleVisionMode])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_CycleVisionMode = 1;

				#if !(PREDATOR_DEMO||DEATHMATCH_DEMO)
				if(DebouncedKeyboardInput[primaryInput->h.GrapplingHook]
				 ||DebouncedKeyboardInput[secondaryInput->h.GrapplingHook])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_GrapplingHook = 1;
				#endif

				if(DebouncedKeyboardInput[primaryInput->f.ZoomIn]
				 ||DebouncedKeyboardInput[secondaryInput->f.ZoomIn])
				{
					if (CameraZoomLevel<3) CameraZoomLevel++;
				}
				if(DebouncedKeyboardInput[primaryInput->g.ZoomOut]
				 ||DebouncedKeyboardInput[secondaryInput->g.ZoomOut])
				{
					if (CameraZoomLevel>0) CameraZoomLevel--;
				}
				
				MaintainZoomingLevel();
				
				if(KeyboardInput[primaryInput->j.PredatorTaunt]
				 ||KeyboardInput[secondaryInput->j.PredatorTaunt])
					StartPlayerTaunt();

				if(KeyboardInput[primaryInput->i.RecallDisc]
				 ||KeyboardInput[secondaryInput->i.RecallDisc])
					Recall_Disc();
					
				if(DebouncedKeyboardInput[primaryInput->k.Predator_MessageHistory]
				 ||DebouncedKeyboardInput[secondaryInput->k.Predator_MessageHistory])
					MessageHistory_DisplayPrevious();
					
				if(DebouncedKeyboardInput[primaryInput->Predator_Say]
				 ||DebouncedKeyboardInput[secondaryInput->Predator_Say])
					BringDownConsoleWithSayTypedIn();

				if(DebouncedKeyboardInput[primaryInput->Predator_SpeciesSay]
				 ||DebouncedKeyboardInput[secondaryInput->Predator_SpeciesSay])
					BringDownConsoleWithSaySpeciesTypedIn();

				if(KeyboardInput[primaryInput->Predator_ShowScores]
				 ||KeyboardInput[secondaryInput->Predator_ShowScores])
					ShowMultiplayerScores();

				break;
			}

			case I_Alien:
			{
				if(KeyboardInput[primaryInput->a.AlternateVision]
				 ||KeyboardInput[secondaryInput->a.AlternateVision])
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_ChangeVision = 1;

				if(KeyboardInput[primaryInput->b.Taunt]
				 ||KeyboardInput[secondaryInput->b.Taunt])
					StartPlayerTaunt();
	
				if(DebouncedKeyboardInput[primaryInput->c.Alien_MessageHistory]
				 ||DebouncedKeyboardInput[secondaryInput->c.Alien_MessageHistory])
					MessageHistory_DisplayPrevious();
					
				if(DebouncedKeyboardInput[primaryInput->d.Alien_Say]
				 ||DebouncedKeyboardInput[secondaryInput->d.Alien_Say])
					BringDownConsoleWithSayTypedIn();

				if(DebouncedKeyboardInput[primaryInput->e.Alien_SpeciesSay]
				 ||DebouncedKeyboardInput[secondaryInput->e.Alien_SpeciesSay])
					BringDownConsoleWithSaySpeciesTypedIn();

				if(KeyboardInput[primaryInput->f.Alien_ShowScores]
				 ||KeyboardInput[secondaryInput->f.Alien_ShowScores])
					ShowMultiplayerScores();

				break;
			}
		}
		
		if(DebouncedKeyboardInput[FixedInputConfig.PauseGame])
			AvP_TriggerInGameMenus();
	//		playerStatusPtr->Mvt_InputRequests.Flags.Rqst_QuitGame = 1;
//			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_PauseGame = 1;

		if(!PaintBallMode.IsOn)
		{
			if(KeyboardInput[primaryInput->FirePrimaryWeapon]
			 ||KeyboardInput[secondaryInput->FirePrimaryWeapon])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			
			if(KeyboardInput[primaryInput->LookUp]
			 ||KeyboardInput[secondaryInput->LookUp])
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookUp = 1;
				playerStatusPtr->Mvt_PitchIncrement = -ONE_FIXED;
			}
			else if(KeyboardInput[primaryInput->LookDown]
			 ||KeyboardInput[secondaryInput->LookDown])
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookDown = 1;
				playerStatusPtr->Mvt_PitchIncrement = ONE_FIXED;
			}
  			
			if(KeyboardInput[primaryInput->CentreView]
			 ||KeyboardInput[secondaryInput->CentreView])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_CentreView = 1;

			if(KeyboardInput[primaryInput->a.NextWeapon]
			 ||KeyboardInput[secondaryInput->a.NextWeapon])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
			
			if(KeyboardInput[primaryInput->b.PreviousWeapon]
			 ||KeyboardInput[secondaryInput->b.PreviousWeapon])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_PreviousWeapon = 1;

			if(DebouncedKeyboardInput[primaryInput->c.FlashbackWeapon]
			 ||DebouncedKeyboardInput[secondaryInput->c.FlashbackWeapon])
			{
				if (playerStatusPtr->PreviouslySelectedWeaponSlot!=playerStatusPtr->SelectedWeaponSlot)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = playerStatusPtr->PreviouslySelectedWeaponSlot+1;
				}
			}
			
			if(KeyboardInput[primaryInput->FireSecondaryWeapon]
			 ||KeyboardInput[secondaryInput->FireSecondaryWeapon])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
			
			/* fixed controls */
			if(KeyboardInput[FixedInputConfig.Weapon1])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 1;
			
			#if !PREDATOR_DEMO
		  	if(KeyboardInput[FixedInputConfig.Weapon2])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 2;
			#else
		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon2])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 2;
			#endif
			if(KeyboardInput[FixedInputConfig.Weapon3])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 3;
			
			#if !(MARINE_DEMO)
			if(KeyboardInput[FixedInputConfig.Weapon4])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 4;
			#else
			if(DebouncedKeyboardInput[FixedInputConfig.Weapon4])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 4;
			#endif
			
			#if !(PREDATOR_DEMO||MARINE_DEMO)
			if(KeyboardInput[FixedInputConfig.Weapon5])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 5;
			#else
		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon5])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 5;
			#endif

			#if !(MARINE_DEMO)
			if(KeyboardInput[FixedInputConfig.Weapon6])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 6;
			#else
			if(DebouncedKeyboardInput[FixedInputConfig.Weapon6])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 6;
			#endif
			
			if(KeyboardInput[FixedInputConfig.Weapon7])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 7;
			
			if(KeyboardInput[FixedInputConfig.Weapon8])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 8;
			
			if(KeyboardInput[FixedInputConfig.Weapon9])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 9;
			
			if(KeyboardInput[FixedInputConfig.Weapon10])
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_WeaponNo = 10;
			
		 
		}
		#if !(PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO||DEATHMATCH_DEMO)
		else // Cool - paintball mode				`
		{
			if(DebouncedKeyboardInput[primaryInput->a.NextWeapon]
			 ||DebouncedKeyboardInput[secondaryInput->a.NextWeapon])
			{
				PaintBallMode_ChangeSelectedDecalID(+1);
			}
			
			if(DebouncedKeyboardInput[primaryInput->b.PreviousWeapon]
			 ||DebouncedKeyboardInput[secondaryInput->b.PreviousWeapon])
			{
				PaintBallMode_ChangeSelectedDecalID(-1);
			}
				
			if(KeyboardInput[primaryInput->LookUp]
			 ||KeyboardInput[secondaryInput->LookUp])
			{
				PaintBallMode_ChangeSize(+1);
			}
			
			if(KeyboardInput[primaryInput->LookDown]
			 ||KeyboardInput[secondaryInput->LookDown])
			{
				PaintBallMode_ChangeSize(-1);
			}
			
			if(KeyboardInput[primaryInput->CentreView]
			 ||KeyboardInput[secondaryInput->CentreView])
			{
				PaintBallMode_Rotate();				
			}
						  
			if(DebouncedKeyboardInput[FixedInputConfig.Weapon1])
			{
				PaintBallMode_ChangeSubclass(+1);
			}
			
		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon2])
			{
				PaintBallMode_ChangeSubclass(-1);
			}

		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon3])
			{
				PaintBallMode.DecalIsInverted = ~PaintBallMode.DecalIsInverted;
			}

		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon4])
			{
				PaintBallMode_Randomise();
			}

		  	if(DebouncedKeyboardInput[FixedInputConfig.Weapon10])
			{
				extern void save_preplaced_decals();
				save_preplaced_decals();
			}
	

			if(DebouncedKeyboardInput[primaryInput->FirePrimaryWeapon]
			 ||DebouncedKeyboardInput[secondaryInput->FirePrimaryWeapon])
			{
				PaintBallMode_AddDecal();
			}
			if(DebouncedKeyboardInput[primaryInput->FireSecondaryWeapon]
			 ||DebouncedKeyboardInput[secondaryInput->FireSecondaryWeapon])
			{
				PaintBallMode_RemoveDecal();
			}
			
		}
		#endif
	}
	/* end of block conditional on input focus */


	/* KJL 10:16:49 04/29/97 - mouse control */




	if (GotMouse)
	{
		extern int MouseVelX;
		extern int MouseVelY;


		if(ControlMethods.HAxisIsTurning)
		{
			if(MouseVelX<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = ((int)MouseVelX)*ControlMethods.MouseXSensitivity;
			   
			}
			else if(MouseVelX>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = ((int)MouseVelX)*ControlMethods.MouseXSensitivity;
			}

			/* KJL 17:36:37 9/9/97 - cap values if strafing */
		   	if(playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Strafe)
			{
		   		if(playerStatusPtr->Mvt_TurnIncrement < -ONE_FIXED)
		   			playerStatusPtr->Mvt_TurnIncrement = -ONE_FIXED;
		   		if(playerStatusPtr->Mvt_TurnIncrement > ONE_FIXED)
		   			playerStatusPtr->Mvt_TurnIncrement = ONE_FIXED;
			}
			
		}
		else // it's sidestep
		{
			if(MouseVelX<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
				playerStatusPtr->Mvt_SideStepIncrement = ((int)MouseVelX)*ControlMethods.MouseXSensitivity;
			}
			else if(MouseVelX>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
				playerStatusPtr->Mvt_SideStepIncrement = ((int)MouseVelX)*ControlMethods.MouseXSensitivity;
			}
	   		
	   		if(playerStatusPtr->Mvt_SideStepIncrement < -ONE_FIXED)
	   			playerStatusPtr->Mvt_SideStepIncrement = -ONE_FIXED;
	   		if(playerStatusPtr->Mvt_SideStepIncrement > ONE_FIXED)
	   			playerStatusPtr->Mvt_SideStepIncrement = ONE_FIXED;
			

		}

		if(ControlMethods.VAxisIsMovement)
		{
			
			if(MouseVelY<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward = 1;
			 	playerStatusPtr->Mvt_MotionIncrement = -((int)MouseVelY)*ControlMethods.MouseYSensitivity;
			}
			else if(MouseVelY>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward = 1;
			 	playerStatusPtr->Mvt_MotionIncrement = -((int)MouseVelY)*ControlMethods.MouseYSensitivity;
			}
	   	
	   		if(playerStatusPtr->Mvt_MotionIncrement < -ONE_FIXED)
	   			playerStatusPtr->Mvt_MotionIncrement = -ONE_FIXED;
	   		if(playerStatusPtr->Mvt_MotionIncrement > ONE_FIXED)
	   			playerStatusPtr->Mvt_MotionIncrement = ONE_FIXED;
		}
		else // it's looking
		{
			int newMouseVelY;

			if (ControlMethods.FlipVerticalAxis) newMouseVelY = -MouseVelY;
			else newMouseVelY = MouseVelY;

			if(newMouseVelY<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookUp = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = ((int)newMouseVelY)*ControlMethods.MouseYSensitivity;
			}
			else if(newMouseVelY>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookDown = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = ((int)newMouseVelY)*ControlMethods.MouseYSensitivity;
			}
		}	
								 
	}
	
	/* KJL 18:27:34 04/29/97 - joystick control */
	if (GotJoystick && IOFOCUS_AcceptControls() && !InGameMenusAreRunning())
	{
		/* SDL pads already have a rescaled dead zone. Applying the legacy
		   threshold again would erase fine movement and reintroduce a jump. */
		const int JOYSTICK_DEAD_ZONE = AccPad_IsPresent() ? 0 : 12000;
		extern JOYINFOEX JoystickData;
		extern JOYCAPS JoystickCaps;
		
		
		int yAxis = (32768-JoystickData.dwYpos)*2;
		int xAxis = (JoystickData.dwXpos-32768)*2;
		
		if(JoystickControlMethods.JoystickVAxisIsMovement)
		{
			if(JoystickControlMethods.JoystickFlipVerticalAxis) yAxis=-yAxis;

			if(yAxis>JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward = 1;
				playerStatusPtr->Mvt_MotionIncrement = yAxis;
			}	
			else if(yAxis<-JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward = 1;
				playerStatusPtr->Mvt_MotionIncrement = yAxis;
			}
		}
		else // looking up/down
		{
			if(!JoystickControlMethods.JoystickFlipVerticalAxis) yAxis=-yAxis;

			if(yAxis>JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookDown = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = yAxis;
			}
			else if(yAxis<-JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookUp = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = yAxis;
			}
		}

		if (JoystickControlMethods.JoystickHAxisIsTurning)
		{
			if(xAxis<-JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = xAxis;
			}
			else if(xAxis>JOYSTICK_DEAD_ZONE)
			{			  
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = xAxis;
			}
		}
		else // strafing
		{
			if(xAxis<-JOYSTICK_DEAD_ZONE)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
				playerStatusPtr->Mvt_SideStepIncrement = xAxis;
			}
			else if(xAxis>JOYSTICK_DEAD_ZONE)
			{			  
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
				playerStatusPtr->Mvt_SideStepIncrement = xAxis;
			}
		}
		
		/* check for rudder */
		if ((JoystickCaps.wCaps & JOYCAPS_HASR) && JoystickControlMethods.JoystickRudderEnabled)
		{
			int rAxis = (JoystickData.dwRpos-32768)*2;
			if (JoystickControlMethods.JoystickRudderAxisIsTurning)
			{
				if(rAxis>JOYSTICK_DEAD_ZONE)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = rAxis;
				}
				else if(rAxis<-JOYSTICK_DEAD_ZONE)
				{			  
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = rAxis;
				}
			}
			else
			{
				if(rAxis>JOYSTICK_DEAD_ZONE)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Strafe = 1;
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = rAxis;
				}	
				else if(rAxis<-JOYSTICK_DEAD_ZONE)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Strafe = 1;
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = rAxis;
				}	
			}
		}

		/* check joystick buttons */
		#if 0 
		if(JoystickData.dwButtons & JOY_BUTTON1)
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
		else if(JoystickData.dwButtons & JOY_BUTTON2)
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
		else if(JoystickData.dwButtons & JOY_BUTTON3)
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
		else if(JoystickData.dwButtons & JOY_BUTTON4)
			playerStatusPtr->Mvt_InputRequests.Flags.Rqst_PreviousWeapon = 1;
		#endif

		/* Point Of View Hat */
		if (JoystickData.dwPOV<36000)
		{
			int theta = ((JoystickData.dwPOV * 4096) /36000);
			int verticalAxis = GetCos(theta);
			int horizontalAxis = GetSin(theta);

			if (JoystickControlMethods.JoystickPOVFlipVerticalAxis)
			{
				verticalAxis = -verticalAxis;
			}

			if (JoystickControlMethods.JoystickPOVVAxisIsMovement)
			{
				if(verticalAxis>0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward = 1;
					playerStatusPtr->Mvt_MotionIncrement = verticalAxis;
				}								  
				else if(verticalAxis<0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward = 1;
					playerStatusPtr->Mvt_MotionIncrement = verticalAxis;
				}
			}
			else
			{
				if(verticalAxis>0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookUp = 1;
					playerStatusPtr->Mvt_PitchIncrement -= verticalAxis;
				}								  
				else if(verticalAxis<0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookDown = 1;
					playerStatusPtr->Mvt_PitchIncrement -= verticalAxis;
				}
			}
			if (JoystickControlMethods.JoystickPOVHAxisIsTurning)
			{
				if(horizontalAxis>0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = horizontalAxis;
				}
				else if(horizontalAxis<0)
				{			  
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
					playerStatusPtr->Mvt_AnalogueTurning = 1;
					playerStatusPtr->Mvt_TurnIncrement = horizontalAxis;
				}
			}
			else // strafing
			{
				if(horizontalAxis>0)
				{
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight = 1;
					playerStatusPtr->Mvt_SideStepIncrement = horizontalAxis;
				}
				else if(horizontalAxis<0)
				{			  
					playerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft = 1;
					playerStatusPtr->Mvt_SideStepIncrement = horizontalAxis;
				}
			}
		}
		if (JoystickControlMethods.JoystickTrackerBallEnabled)
		{
			int trackerballH = JoystickData.dwUpos - 32768;
			int trackerballV = JoystickData.dwVpos - 32768;

			if (JoystickControlMethods.JoystickTrackerBallFlipVerticalAxis)
			{
				trackerballV = -trackerballV;
			}

			if(trackerballH<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnLeft = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = trackerballH*JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity;
			   
			}
			else if(trackerballH>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
				playerStatusPtr->Mvt_AnalogueTurning = 1;
				playerStatusPtr->Mvt_TurnIncrement = trackerballH*JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity;

			}
			if(trackerballV<0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookUp = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = trackerballV*JoystickControlMethods.JoystickTrackerBallVerticalSensitivity;
			}
			else if(trackerballV>0)
			{
				playerStatusPtr->Mvt_InputRequests.Flags.Rqst_LookDown = 1;
				playerStatusPtr->Mvt_AnaloguePitching = 1;
				playerStatusPtr->Mvt_PitchIncrement = trackerballV*JoystickControlMethods.JoystickTrackerBallVerticalSensitivity;;
			}

		}

		#if 0
		textprint("%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
			JoystickData.dwXpos,
			JoystickData.dwYpos,
			JoystickData.dwZpos,
			JoystickData.dwRpos,
			JoystickData.dwUpos,
			JoystickData.dwVpos,
			JoystickData.dwButtons,
			JoystickData.dwPOV);
		#endif
	}

	/* KJL 10:55:22 10/9/97 - HUD transparency */
	#if 0
	{
		extern signed int HUDTranslucencyLevel;

		if (KeyboardInput[KEY_F1])
		{
			HUDTranslucencyLevel-=NormalFrameTime>>9;
			if (HUDTranslucencyLevel<0) HUDTranslucencyLevel=0;
		}
		else if (KeyboardInput[KEY_F2])
		{
			HUDTranslucencyLevel+=NormalFrameTime>>9;
			if (HUDTranslucencyLevel>255) HUDTranslucencyLevel=255;
		}
	}
	#endif
	/* KJL 10:55:32 10/9/97 - screen size */
	#if 0
	if(KeyboardInput[KEY_F3])
		MakeViewingWindowLarger();
	else if(KeyboardInput[KEY_F4])
		MakeViewingWindowSmaller();
	#endif
	#if 0
	if (DebouncedKeyboardInput[KEY_F3])
	{
		MessageHistory_DisplayPrevious();
	}
	#endif
	if (DebouncedKeyboardInput[KEY_GRAVE]) IOFOCUS_Toggle();
	AccAccess_CheckRequests(playerStatusPtr, sbPtr->DynPtr, primaryInput, secondaryInput);
	jumpAssistFrame=AccJumpAssist_HandleInput(sbPtr, playerStatusPtr, primaryInput, secondaryInput, snapRequest);
	/* Plays the sweep tones a sonar request scheduled, spread over time so the
	   fan is heard moving left to right rather than as one chord. The bridge
	   clock is real time except while a bridge session holds time, when the
	   pings must stay half a second apart in game time. */
	AccBridge_BeginCue("sonar", -1);
	AccSonar_Update(AccBridge_NowMs());
	AccBridge_EndCue();
	if ((!jumpAssistFrame || AccJumpAssist_IsActive()) &&
	    (AvP.PlayerType == I_Marine || AvP.PlayerType == I_Predator) && playerStatusPtr->IsAlive && !playerStatusPtr->DemoMode
	    && !AvP.LevelCompleted && IOFOCUS_AcceptControls() && !InGameMenusAreRunning())
		AccRoute_Update(AccBridge_NowMs());
	if(snapRequest && IOFOCUS_AcceptControls() && !InGameMenusAreRunning())
		AccSnap_Request(AccBridge_NowMs());
	AccPad_TraceGameInput(playerStatusPtr, primaryInput, secondaryInput);
}

void LoadKeyConfiguration(void)
{
	#if ALIEN_DEMO
	LoadAKeyConfiguration("alienavpkey.cfg");
	#else
	LoadAKeyConfiguration("avpkey.cfg");
	#endif

}

void SaveKeyConfiguration(void)
{
	#if ALIEN_DEMO
	SaveAKeyConfiguration("alienavpkey.cfg");
	#else
	SaveAKeyConfiguration("avpkey.cfg");
	#endif
}

void LoadAKeyConfiguration(char* Filename)
{
	#if 0
	FILE* file=fopen(Filename,"rb");
	if(!file)
	{
		MarineInputPrimaryConfig = DefaultMarineInputPrimaryConfig;
		MarineInputSecondaryConfig = DefaultMarineInputSecondaryConfig;
		PredatorInputPrimaryConfig = DefaultPredatorInputPrimaryConfig;
		PredatorInputSecondaryConfig = DefaultPredatorInputSecondaryConfig;
		AlienInputPrimaryConfig = DefaultAlienInputPrimaryConfig;
		AlienInputSecondaryConfig = DefaultAlienInputSecondaryConfig;
		return;
	}
	fread(&MarineInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&MarineInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&PredatorInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&PredatorInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&AlienInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&AlienInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);

	fread(&ControlMethods,sizeof(CONTROL_METHODS),1,file);
	fread(&JoystickControlMethods,sizeof(JOYSTICK_CONTROL_METHODS),1,file);
	
	fclose(file);
	#endif
}

void SaveAKeyConfiguration(char* Filename)
{
	#if 0
	FILE* file=fopen(Filename,"wb");
	if(!file) return;

	fwrite(&MarineInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&MarineInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&PredatorInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&PredatorInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&AlienInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&AlienInputSecondaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);

	fwrite(&ControlMethods,sizeof(CONTROL_METHODS),1,file);
	fwrite(&JoystickControlMethods,sizeof(JOYSTICK_CONTROL_METHODS),1,file);

	fclose(file);
	#endif
}

void SaveDefaultPrimaryConfigs(void)
{
	FILE *file = OpenGameFile("default.cfg", FILEMODE_WRITEONLY, FILETYPE_CONFIG);
	if(!file) return;

	fwrite(&DefaultMarineInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&DefaultPredatorInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fwrite(&DefaultAlienInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);

	fclose(file);
}
void LoadDefaultPrimaryConfigs(void)
{
	FILE *file = OpenGameFile("default.cfg", FILEMODE_READONLY, FILETYPE_CONFIG);
	if(!file) return;

	fread(&DefaultMarineInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&DefaultPredatorInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);
	fread(&DefaultAlienInputPrimaryConfig,sizeof(PLAYER_INPUT_CONFIGURATION),1,file);

	fclose(file);
}

/* AVP Access ------------------------------------------------------------------
  Re-assert the control settings a gamepad needs. Called every frame from
  acc_pad.c rather than once at startup, because loading a user profile does
  `JoystickControlMethods = UserProfilePtr->JoystickControlMethods` -- so any
  profile saved before controller support existed silently disables the right
  stick and puts turning back on the left one.

  Lives here because JOYSTICK_CONTROL_METHODS is defined in this translation
  unit's headers, which pull in types the standalone pad module does not have.
  ---------------------------------------------------------------------------*/
void AccPad_ApplyControlMethods(void)
{
	JoystickControlMethods.JoystickEnabled            = 1;
	JoystickControlMethods.JoystickVAxisIsMovement    = 1;
	JoystickControlMethods.JoystickHAxisIsTurning     = 0;   /* left stick strafes */
	JoystickControlMethods.JoystickTrackerBallEnabled = 1;   /* right stick looks  */

	if (JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity == 0)
		JoystickControlMethods.JoystickTrackerBallHorizontalSensitivity =
			DEFAULT_TRACKERBALL_HORIZONTAL_SENSITIVITY;

	if (JoystickControlMethods.JoystickTrackerBallVerticalSensitivity == 0)
		JoystickControlMethods.JoystickTrackerBallVerticalSensitivity =
			DEFAULT_TRACKERBALL_VERTICAL_SENSITIVITY;
}
