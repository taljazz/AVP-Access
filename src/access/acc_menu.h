/* AVP Access ------------------------------------------------------------------
  Spoken front-end menus.

  Narration is driven by capturing the text the menu renderer actually draws for
  each element, rather than by re-deriving it from the element data. AvP has ~40
  menu element types whose label and value come from different places (episode
  names are TextDescription plus a slider offset, profile slots carry their own
  string, number fields have a special zero string, and so on). Mirroring that
  logic means duplicating it, and duplicated logic drifts -- which is exactly how
  whole menus end up silent. Capturing the rendered strings is correct for every
  element type by construction, including ones added later.

  A semantic fallback still describes elements that draw no text at all (a
  graphic-only entry), so nothing is ever completely mute.

  Include after avp_menus.h; AVPMENU_ELEMENT and AVPMENUFORMAT_ID must be visible.
  ---------------------------------------------------------------------------*/
#ifndef ACC_MENU_H
#define ACC_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ACCMENU_RENDERTEXT)(char *textPtr, int x, int y, int alpha,
                                  enum AVPMENUFORMAT_ID format);
typedef int (*ACCMENU_RENDERTEXT_COLOURED)(char *textPtr, int x, int y, int alpha,
                                           enum AVPMENUFORMAT_ID format,
                                           int r, int g, int b);

/* Called at the top of RenderMenuElement with the element's index and the two
   text routines it is about to use. Returns 1 if the caller should substitute
   the capture wrappers below; they record the text and then forward to the
   originals, so rendering is unaffected. */
int AccMenu_BeginCapture(int elementIndex,
                         ACCMENU_RENDERTEXT rt,
                         ACCMENU_RENDERTEXT_COLOURED rtc);

int AccMenu_CaptureRenderText(char *textPtr, int x, int y, int alpha,
                              enum AVPMENUFORMAT_ID format);
int AccMenu_CaptureRenderTextColoured(char *textPtr, int x, int y, int alpha,
                                      enum AVPMENUFORMAT_ID format,
                                      int r, int g, int b);

/* Call once per menu frame, after input has been applied. Speaks when the menu,
   the selection, or the selected item's text changes -- the last of these is
   what makes toggles and sliders announce their new value. */
void AccMenu_Poll(int menuID, const AVPMENU_ELEMENT *elements, int numElements,
                  int selectedElement, int userEnteringText,
                  int userChangingKeyConfig);

void AccMenu_RepeatCurrent(void);
void AccMenu_SpeakHelp(void);

/* Implemented in avp_menus.c, where the briefing strings are file-static.
   Returns line `index` (0..4) of the mission prelude on the level-select
   screens, or NULL anywhere else. */
const char *AccMenu_BriefingLine(int index);

/* True while a menu is on screen -- AccMenu_Poll() only runs from the menu
   update. The gamepad uses this to decide whether its face buttons should act
   as cursor keys or stay free for in-game bindings. Call the decay once per
   frame from whoever polls the pad. */
int  AccMenu_MenusActive(void);
/* Read the current engine binding mode, including when speech is unavailable. */
int  AccMenu_BindingActive(void);
void AccMenu_DecayMenusActive(void);

/* Forget cached state so the next poll always announces. */
void AccMenu_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ACC_MENU_H */
