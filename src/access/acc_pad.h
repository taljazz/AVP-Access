/* AVP Access ------------------------------------------------------------------
  Xbox / SDL gamepad support.

  The port only ever opened a raw SDL_Joystick, whose axis and button numbering
  differs between controllers, and `WantJoystick` defaulted to 0 while the only
  switch (-j) also set 0 -- so joystick support could never actually be turned
  on. This uses SDL3's gamepad layer instead, which applies a per-device mapping
  and gives a controller-independent layout, so an Xbox pad is correct with no
  configuration.

  Two things are fed from one pad read:
    * the analogue axes, written into the JOYINFOEX the engine already consumes,
      so the existing movement and look code drives the player;
    * the buttons, published both as the engine's bindable KEY_JOYSTICK_BUTTON_n
      keys and, while a menu is up, as the cursor keys and Enter/Escape -- so the
      front end can be driven from the pad without binding anything first.
  ---------------------------------------------------------------------------*/
#ifndef ACC_PAD_H
#define ACC_PAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the first attached gamepad. Returns 1 if one was found. */
int  AccPad_Init(void);
void AccPad_Shutdown(void);

int  AccPad_IsPresent(void);
const char *AccPad_Name(void);      /* never NULL */

/* Reads the pad and updates the engine's axis state. Call once per frame. */
void AccPad_ReadAxes(void);

/* Publishes button state as engine keys. Call once per frame, from the same
   place the old joystick button loop ran. */
void AccPad_ReadButtons(void);

/* Diagnostic: prints what SDL reports from the pad for `seconds`, so a pad that
   is detected but does nothing can be told apart from one the engine is
   ignoring. Reached with --padtest. */
void AccPad_SelfTest(int seconds);

/* Set to 1 to trace how pad state reaches the engine (--padtrace). */
extern int AccPadTrace;

#ifdef __cplusplus
}
#endif

#endif /* ACC_PAD_H */
