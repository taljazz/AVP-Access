/* AVP Access ------------------------------------------------------------------
  Xbox / SDL gamepad support -- see acc_pad.h.
  ---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "fixer.h"
#include "3dc.h"
#include "platform.h"


#include "acc_pad.h"

/* Declared here rather than by including acc_menu.h, which drags in the whole
   menu element type just for these two. */
extern void AccPad_ApplyControlMethods(void);   /* usr_io.c */
extern int  AccMenu_MenusActive(void);
extern int  AccMenu_BindingActive(void);

/* Engine state this feeds. */
extern JOYINFOEX JoystickData;
extern int GotJoystick;
extern unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
extern unsigned char DebouncedKeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];
extern unsigned char GotAnyKey;
extern int DebouncedGotAnyKey;

static SDL_Gamepad *Pad;
static int PadSubsystemReady;

/* Set by --padtrace; prints why the pad is or is not reaching the engine. */
int AccPadTrace;
static char PadName[128] = "none";

/* Sticks rest a long way off centre on worn hardware, so the dead zone is
   generous; the engine applies its own on top. */
#define ACC_STICK_DEADZONE 8000

/* The trackerball path multiplies by a sensitivity of 32, and was written for a
   small relative delta rather than a full-range stick. Scaling the right stick
   down keeps the resulting turn rate in the same magnitude as the other axes
   instead of spinning the player on the spot. */
#define ACC_LOOK_DIVISOR 16

/* Button order published as KEY_JOYSTICK_BUTTON_1..16, so the key-configuration
   screen offers a predictable Xbox layout. */
static const int PadButtonOrder[] = {
	SDL_GAMEPAD_BUTTON_SOUTH,           /*  1  A          */
	SDL_GAMEPAD_BUTTON_EAST,            /*  2  B          */
	SDL_GAMEPAD_BUTTON_WEST,            /*  3  X          */
	SDL_GAMEPAD_BUTTON_NORTH,           /*  4  Y          */
	SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,   /*  5  LB         */
	SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,  /*  6  RB         */
	-1,                                 /*  7  LT (axis)  */
	-1,                                 /*  8  RT (axis)  */
	SDL_GAMEPAD_BUTTON_BACK,            /*  9  Back       */
	SDL_GAMEPAD_BUTTON_START,           /* 10  Start      */
	SDL_GAMEPAD_BUTTON_LEFT_STICK,      /* 11  L3         */
	SDL_GAMEPAD_BUTTON_RIGHT_STICK,     /* 12  R3         */
	SDL_GAMEPAD_BUTTON_DPAD_UP,         /* 13             */
	SDL_GAMEPAD_BUTTON_DPAD_DOWN,       /* 14             */
	SDL_GAMEPAD_BUTTON_DPAD_LEFT,       /* 15             */
	SDL_GAMEPAD_BUTTON_DPAD_RIGHT       /* 16             */
};
#define ACC_PAD_BUTTONS ((int)(sizeof(PadButtonOrder) / sizeof(PadButtonOrder[0])))

/* Triggers are analogue; treat them as pressed past half travel. */
#define ACC_TRIGGER_THRESHOLD 16000

int AccPad_Init(void)
{
	SDL_JoystickID *ids;
	int count = 0, i;

	if (Pad) return 1;

	if (!PadSubsystemReady && !SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
		if (AccPadTrace) fprintf(stderr, "ACCPAD: gamepad initialization failed: %s\n", SDL_GetError());
		return 0;
	}

	PadSubsystemReady = 1;
	ids = SDL_GetGamepads(&count);
	if (!ids) {
		if (AccPadTrace) fprintf(stderr, "ACCPAD: gamepad enumeration failed: %s\n", SDL_GetError());
		return 0;
	}
	if (AccPadTrace) fprintf(stderr, "ACCPAD: mapped devices=%d\n", count);

	for (i = 0; i < count && !Pad; i++) {
		Pad = SDL_OpenGamepad(ids[i]);
		if (!Pad && AccPadTrace)
			fprintf(stderr, "ACCPAD: open device %u failed: %s\n", (unsigned int)ids[i], SDL_GetError());
	}
	SDL_free(ids);

	if (!Pad) return 0;

	{
		const char *n = SDL_GetGamepadName(Pad);
		if (n && n[0]) {
			strncpy(PadName, n, sizeof(PadName) - 1);
			PadName[sizeof(PadName) - 1] = 0;
		} else {
			strcpy(PadName, "gamepad");
		}
	}

	/* The engine gates all joystick reading on this. */
	GotJoystick = 1;

	return 1;
}

void AccPad_Shutdown(void)
{
	if (Pad) {
		SDL_CloseGamepad(Pad);
		Pad = NULL;
	}
	strcpy(PadName, "none");
}

int AccPad_IsPresent(void)   { return Pad != NULL; }
const char *AccPad_Name(void){ return PadName; }

static int DeadZone(int v)
{
	if (v > -ACC_STICK_DEADZONE && v < ACC_STICK_DEADZONE) return 0;
	return v;
}

void AccPad_ReadAxes(void)
{
	int lx, ly, rx, ry;

	if (!Pad) return;

	/* Re-assert the settings a pad needs, every frame. Setting them once at
	   startup is not enough: loading a user profile does
	   `JoystickControlMethods = UserProfilePtr->JoystickControlMethods`, and any
	   profile saved before controller support existed has the right stick
	   disabled and the left stick turning rather than strafing -- which silently
	   undoes the whole configuration the moment a profile loads. Lives in
	   usr_io.c, where the structure is defined. */
	AccPad_ApplyControlMethods();

	SDL_UpdateGamepads();

	lx = DeadZone(SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTX));
	ly = DeadZone(SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTY));
	rx = DeadZone(SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHTX));
	ry = DeadZone(SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHTY));

	/* Left stick: the engine reads dwYpos as forward/back and dwXpos as
	   sidestep. SDL's Y is negative upwards, which is already what the engine
	   wants -- it computes (32768 - dwYpos). */
	JoystickData.dwXpos = (DWORD)(lx + 32768);
	JoystickData.dwYpos = (DWORD)(ly + 32768);

	/* Right stick drives the "trackerball" axes: horizontal turns, vertical
	   looks. Scaled down -- see ACC_LOOK_DIVISOR. */
	JoystickData.dwUpos = (DWORD)((rx / ACC_LOOK_DIVISOR) + 32768);
	JoystickData.dwVpos = (DWORD)((ry / ACC_LOOK_DIVISOR) + 32768);

	/* The rudder axis is unused, and the hat is published as cursor keys
	   instead, so make sure the POV code stays out of the way. */
	JoystickData.dwRpos = 32768;
	JoystickData.dwPOV  = (DWORD)-1;
}

/* Keep the two input sources separate: releasing a controller alias must not
   release the same key still held on the keyboard. */
static unsigned char PadKeyHeld[MAX_NUMBER_OF_INPUT_KEYS];
static unsigned char KeyboardKeyHeld[MAX_NUMBER_OF_INPUT_KEYS];

void AccPad_KeyboardKeyEvent(int key, int pressed)
{
	if (key >= 0 && key < MAX_NUMBER_OF_INPUT_KEYS)
		KeyboardKeyHeld[key] = (unsigned char)(pressed != 0);
}

/* Publish a single combined key state and only generate a fresh press edge.
   Keyboard events are processed before this, so a keyboard release can briefly
   clear KeyboardInput even while the pad still holds the same key. */
static void SetKey(int key, int pressed)
{
	if (key < 0 || key >= MAX_NUMBER_OF_INPUT_KEYS) return;

	if (pressed) {
		GotAnyKey = 1;
		if (!KeyboardInput[key] && !PadKeyHeld[key]) {
			DebouncedKeyboardInput[key] = 1;
			DebouncedGotAnyKey = 1;
		}
		KeyboardInput[key] = 1;
		PadKeyHeld[key] = 1;
	} else if (PadKeyHeld[key]) {
		KeyboardInput[key] = KeyboardKeyHeld[key];
		PadKeyHeld[key] = 0;
	}
}

void AccPad_DeviceRemoved(unsigned int instanceID)
{
	int key;
	if (!Pad || SDL_GetGamepadID(Pad) != (SDL_JoystickID)instanceID) return;

	if (AccPadTrace) fprintf(stderr, "ACCPAD: disconnected %s\n", PadName);
	for (key = 0; key < MAX_NUMBER_OF_INPUT_KEYS; key++) SetKey(key, 0);
	AccPad_Shutdown();
	GotJoystick = 0;
	JoystickData.dwXpos = JoystickData.dwYpos = 32768;
	JoystickData.dwUpos = JoystickData.dwVpos = JoystickData.dwRpos = 32768;
	JoystickData.dwPOV = (DWORD)-1;
	/* Keep the SDL subsystem active so connection events can reopen the pad. */
}

void AccPad_ReadButtons(void)
{
	int i;
	int held[ACC_PAD_BUTTONS];
	int lt, rt;
	int menus, binding, lx, ly;

	if (!Pad) return;

	SDL_UpdateGamepads();

	lt = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
	rt = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

	for (i = 0; i < ACC_PAD_BUTTONS; i++) {
		if (PadButtonOrder[i] < 0) {
			held[i] = 0;                     /* trigger slots, filled below */
		} else {
			held[i] = SDL_GetGamepadButton(Pad, (SDL_GamepadButton)PadButtonOrder[i]) ? 1 : 0;
		}
	}
	held[6] = (lt > ACC_TRIGGER_THRESHOLD);  /* slot 7  */
	held[7] = (rt > ACC_TRIGGER_THRESHOLD);  /* slot 8  */

	for (i = 0; i < ACC_PAD_BUTTONS; i++)
		SetKey(KEY_JOYSTICK_BUTTON_1 + i, held[i]);

	/* The rest of the cursor set is only published while a menu is up, so the
	   same buttons stay free for binding during play. Without this the front
	   end could not be driven from the pad at all until the player had somehow
	   navigated to the key-configuration screen to bind it. */
	menus = AccMenu_MenusActive();
	binding = AccMenu_BindingActive();
	lx = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTX);
	ly = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTY);

	{
		int up    = held[12] || (ly < -ACC_STICK_DEADZONE);
		int down  = held[13] || (ly >  ACC_STICK_DEADZONE);
		int left  = held[14] || (lx < -ACC_STICK_DEADZONE);
		int right = held[15] || (lx >  ACC_STICK_DEADZONE);

		SetKey(KEY_UP,    menus && !binding && up);
		SetKey(KEY_DOWN,  menus && !binding && down);
		SetKey(KEY_LEFT,  menus && !binding && left);
		SetKey(KEY_RIGHT, menus && !binding && right);
		SetKey(KEY_CR,    menus && !binding && held[0]);   /* A selects  */
	}

	/* Publish Escape once: holding B must not release/re-press it every frame.
	   Start stays available to cancel binding capture; B remains bindable. */
	SetKey(KEY_ESCAPE, held[9] || (menus && !binding && held[1]));

	/* Compare the actual button/key masks, not just "any button": changing
	   D-pad direction while another button is held must still be visible. */
	if (AccPadTrace) {
		static int lastMenus = -1, lastBinding = -1, lastKeys = -1;
		static unsigned int lastButtons = ~0u;
		unsigned int mask = 0;
		int keys = (KeyboardInput[KEY_UP] ? 1 : 0)
		         | (KeyboardInput[KEY_DOWN] ? 2 : 0)
		         | (KeyboardInput[KEY_LEFT] ? 4 : 0)
		         | (KeyboardInput[KEY_RIGHT] ? 8 : 0)
		         | (KeyboardInput[KEY_CR] ? 16 : 0)
		         | (KeyboardInput[KEY_ESCAPE] ? 32 : 0);

		for (i = 0; i < ACC_PAD_BUTTONS; i++) if (held[i]) mask |= 1u << i;
		if (menus != lastMenus || binding != lastBinding || mask != lastButtons || keys != lastKeys) {
			fprintf(stderr, "ACCPAD: menus=%d binding=%d buttons=%04x left=(%d,%d) KEY_UP=%d KEY_DOWN=%d KEY_CR=%d KEY_ESCAPE=%d anyEdge=%d\n",
			        menus, binding, mask, lx, ly, (int)KeyboardInput[KEY_UP],
			        (int)KeyboardInput[KEY_DOWN], (int)KeyboardInput[KEY_CR],
			        (int)KeyboardInput[KEY_ESCAPE], DebouncedGotAnyKey);
			fflush(stderr);
			lastMenus = menus;
			lastBinding = binding;
			lastButtons = mask;
			lastKeys = keys;
		}
	}

}

/* ----------------------------------------------------------- diagnostic -- */

static const char *PadButtonNames[] = {
	"A", "B", "X", "Y", "LB", "RB", "LT", "RT",
	"Back", "Start", "L3", "R3", "DUp", "DDown", "DLeft", "DRight"
};

void AccPad_SelfTest(int seconds)
{
	Uint64 end;
	int lastHeld[ACC_PAD_BUTTONS];
	int i;

	if (!Pad) {
		fprintf(stderr, "AVP Access: padtest -- no gamepad opened\n");
		return;
	}

	fprintf(stderr, "AVP Access: padtest on \"%s\" for %d seconds.\n", PadName, seconds);
	fprintf(stderr, "AVP Access: press buttons and move the sticks now.\n");
	fflush(stderr);

	for (i = 0; i < ACC_PAD_BUTTONS; i++) lastHeld[i] = 0;

	end = SDL_GetTicks() + (Uint64)seconds * 1000;

	while (SDL_GetTicks() < end) {
		SDL_Event ev;
		int lx, ly, rx, ry, lt, rt;

		/* Pump events: without this SDL never notices the device at all. */
		while (SDL_PollEvent(&ev)) { }

		SDL_UpdateGamepads();

		for (i = 0; i < ACC_PAD_BUTTONS; i++) {
			int held;

			if (PadButtonOrder[i] < 0) {
				int axis = (i == 6) ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
				                    : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
				held = SDL_GetGamepadAxis(Pad, (SDL_GamepadAxis)axis) > ACC_TRIGGER_THRESHOLD;
			} else {
				held = SDL_GetGamepadButton(Pad, (SDL_GamepadButton)PadButtonOrder[i]) ? 1 : 0;
			}

			if (held != lastHeld[i]) {
				fprintf(stderr, "AVP Access: padtest button %s %s (engine key %d)\n",
				        PadButtonNames[i], held ? "down" : "up",
				        KEY_JOYSTICK_BUTTON_1 + i);
				fflush(stderr);
				lastHeld[i] = held;
			}
		}

		lx = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTX);
		ly = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFTY);
		rx = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHTX);
		ry = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHTY);
		lt = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
		rt = SDL_GetGamepadAxis(Pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

		if (DeadZone(lx) || DeadZone(ly) || DeadZone(rx) || DeadZone(ry) ||
		    lt > ACC_TRIGGER_THRESHOLD || rt > ACC_TRIGGER_THRESHOLD) {
			static Uint64 nextAxisReport;
			if (SDL_GetTicks() >= nextAxisReport) {
				fprintf(stderr, "AVP Access: padtest axes L(%d,%d) R(%d,%d) LT%d RT%d\n",
				        lx, ly, rx, ry, lt, rt);
				fflush(stderr);
				nextAxisReport = SDL_GetTicks() + 250;
			}
		}

		SDL_Delay(10);
	}

	fprintf(stderr, "AVP Access: padtest finished.\n");
	fflush(stderr);
}
