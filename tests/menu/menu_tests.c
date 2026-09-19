/* Exercises the real acc_menu.c with a small string table and speech sink. */
#include <stdio.h>
#include <string.h>

#include "fixer.h"
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "bh_types.h"
#include "vision.h"
#include "avp_menus.h"
#include "avp_menudata.h"
#include "avp_userprofile.h"
#include "acc_speech.h"
#include "acc_menu.h"

#define TEXT_CAP 32
#define MENU_COUNT (AVPMENU_LOADGAME + 1)

AVPMENU AvPMenusData[MENU_COUNT];
static char strings[TEXT_CAP][96];
static int speech_calls, silence_calls;
static char spoken[2048];
static int assertions, failures;
static int slider_value;
static AVP_USER_PROFILE profile;
static int localized_species = 1;
static int briefing_menu = -1;

char *GetTextString(enum TEXTSTRING_ID id)
{
    int index = (int)id;
    if (id == TEXTSTRING_MULTIPLAYER_ALIEN) return localized_species ? "Xenomorph" : "";
    if (id == TEXTSTRING_MULTIPLAYER_MARINE) return localized_species ? "Colonial Marine" : "";
    if (id == TEXTSTRING_MULTIPLAYER_PREDATOR) return localized_species ? "Yautja" : "";
    return index >= 0 && index < TEXT_CAP ? strings[index] : "";
}

AVP_USER_PROFILE *GetFirstUserProfile(void) { return &profile; }
AVP_USER_PROFILE *GetNextUserProfile(void) { return NULL; }

int AccSpeech_IsAvailable(void) { return 1; }
void AccSpeech_Say(const char *message, int interrupt)
{
    (void)interrupt;
    ++speech_calls;
    strncpy(spoken, message ? message : "", sizeof(spoken) - 1);
    spoken[sizeof(spoken) - 1] = 0;
}
void AccSpeech_Silence(void) { ++silence_calls; }

const char *AccMenu_BriefingLine(int index)
{
    return index == 0 && (briefing_menu == AVPMENU_LEVELBRIEFING_BASIC ||
                          briefing_menu == AVPMENU_LEVELBRIEFING_BONUS)
         ? "Briefing detail" : NULL;
}

static void check(int ok, const char *description)
{
    ++assertions;
    printf("%s: %s\n", ok ? "PASS" : "FAIL", description);
    if (!ok) ++failures;
}

static void setup(void)
{
    int i;
    memset(strings, 0, sizeof(strings));
    for (i = 0; i < MENU_COUNT; ++i) AvPMenusData[i].MenuTitle = (enum TEXTSTRING_ID)0;
    strcpy(strings[0], "Menu title");
    strcpy(strings[1], "Play as alien");
    strcpy(strings[2], "Graphic help");
    strcpy(strings[3], "Volume");
    strcpy(strings[4], "Profile help");
    strcpy(strings[5], "Profile name");
    strcpy(strings[6], "Old label");
    strcpy(strings[7], "New label");
    strcpy(profile.Name, "Ada");
    slider_value = 0;
    localized_species = 1; briefing_menu = -1;
    speech_calls = silence_calls = assertions = failures = 0;
    spoken[0] = 0;
    AccMenu_Reset();
}

static int renderer(char *text, int x, int y, int alpha, enum AVPMENUFORMAT_ID format)
{
    (void)text; (void)y; (void)alpha; (void)format;
    return x + 1;
}

static int renderer_colour(char *text, int x, int y, int alpha, enum AVPMENUFORMAT_ID format,
                          int r, int g, int b)
{
    (void)text; (void)y; (void)alpha; (void)format; (void)r; (void)g; (void)b;
    return x + 2;
}

static AVPMENU_ELEMENT graphic(enum AVPMENU_ID destination)
{
    AVPMENU_ELEMENT e;
    memset(&e, 0, sizeof(e));
    e.ElementID = AVPMENU_ELEMENT_GOTOMENU_GFX;
    e.b.MenuToGoTo = destination;
    e.HelpString = (enum TEXTSTRING_ID)2;
    return e;
}

static void test_graphic_fallback(void)
{
    static const struct { enum AVPMENU_ID destination; const char *label; } species[] = {
        {AVPMENU_ALIENLEVELS, "Xenomorph"},
        {AVPMENU_MARINELEVELS, "Colonial Marine"},
        {AVPMENU_PREDATORLEVELS, "Yautja"}
    };
    AVPMENU_ELEMENT e;
    size_t i;
    setup();
    for (i = 0; i < sizeof(species) / sizeof(species[0]); ++i) {
        e = graphic(species[i].destination);
        AccMenu_BeginCapture(0, renderer, renderer_colour);
        AccMenu_CaptureRenderText("polluted renderer text", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
        AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
        check(strstr(spoken, species[i].label) != NULL && strstr(spoken, "polluted") == NULL &&
              strstr(spoken, "Graphic help") == NULL,
              "graphic species fallback uses its destination localization despite generic help and dirty capture");
        AccMenu_Reset(); speech_calls = 0;
    }
    localized_species = 0;
    e = graphic(AVPMENU_ALIENLEVELS);
    AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
    check(strstr(spoken, "Alien") != NULL && strstr(spoken, "Graphic help") == NULL,
          "missing species localization uses the English species fallback rather than generic help");
}

static void test_capture_end(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_GOTOMENU;
    e.a.TextDescription = (enum TEXTSTRING_ID)1;
    AccMenu_BeginCapture(0, renderer, renderer_colour);
    AccMenu_CaptureRenderText("renderer leak", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
    AccMenu_EndCapture();
    AccMenu_CaptureRenderText("help contamination", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
    AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
    check(strstr(spoken, "renderer leak") != NULL && strstr(spoken, "contamination") == NULL,
          "ending capture prevents subsequent help rendering from contaminating captured menu text");
}

static void test_clear_transition(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_GOTOMENU;
    e.a.TextDescription = (enum TEXTSTRING_ID)6;
    AccMenu_BeginCapture(0, renderer, renderer_colour);
    AccMenu_CaptureRenderText("Old captured label", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
    AccMenu_EndCapture();
    AccMenu_ClearCapturedText();
    e.a.TextDescription = (enum TEXTSTRING_ID)7;
    AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
    check(strstr(spoken, "New label") != NULL && strstr(spoken, "Old captured") == NULL,
          "clearing capture text on transition prevents a stale previous-menu label");
}

static void test_title_once(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_GOTOMENU;
    e.a.TextDescription = (enum TEXTSTRING_ID)1;
    AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
    check(speech_calls == 1 && strstr(spoken, "Menu title") != NULL,
          "entering a menu announces its title and selected item once");
    AccMenu_Poll(AVPMENU_SINGLEPLAYER, &e, 1, 0, 0, 0);
    check(speech_calls == 1, "an unchanged next-frame poll does not duplicate the title announcement");
}

static void test_slider(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_SLIDER;
    e.a.TextDescription = (enum TEXTSTRING_ID)3;
    AccMenu_Poll(AVPMENU_OPTIONS, &e, 1, 0, 0, 0);
    speech_calls = 0;
    AccMenu_BeginCapture(0, renderer, renderer_colour);
    AccMenu_CaptureRenderText("Volume", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
    AccMenu_CaptureRenderText("80", 0, 0, 255, AVPMENUFORMAT_LEFTJUSTIFIED);
    AccMenu_EndCapture();
    AccMenu_Poll(AVPMENU_OPTIONS, &e, 1, 0, 0, 0);
    check(speech_calls == 1 && strstr(spoken, "Volume 80") != NULL,
          "a changed rendered slider label and value are announced");
}

static void test_profile(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_USERPROFILE;
    e.c.SliderValuePtr = &slider_value; e.b.MaxSliderValue = 0;
    AccMenu_Poll(AVPMENU_USERPROFILESELECT, &e, 1, 0, 0, 0);
    check(strstr(spoken, "Ada") != NULL, "profile menu announces the selected profile name");
}

static void test_briefing(void)
{
    AVPMENU_ELEMENT e;
    setup();
    memset(&e, 0, sizeof(e)); e.ElementID = AVPMENU_ELEMENT_GOTOMENU;
    e.a.TextDescription = (enum TEXTSTRING_ID)1;
    briefing_menu = AVPMENU_MARINELEVELS;
    AccMenu_Poll(AVPMENU_MARINELEVELS, &e, 1, 0, 0, 0);
    check(strstr(spoken, "Briefing detail") == NULL,
          "episode selector menus do not include briefing text");
    AccMenu_Reset(); speech_calls = 0; briefing_menu = AVPMENU_LEVELBRIEFING_BASIC;
    AccMenu_Poll(AVPMENU_LEVELBRIEFING_BASIC, &e, 1, 0, 0, 0);
    check(strstr(spoken, "Briefing detail") != NULL,
          "basic briefing screen announcement includes its briefing text");
    AccMenu_Reset(); speech_calls = 0; briefing_menu = AVPMENU_LEVELBRIEFING_BONUS;
    AccMenu_Poll(AVPMENU_LEVELBRIEFING_BONUS, &e, 1, 0, 0, 0);
    check(strstr(spoken, "Briefing detail") != NULL,
          "bonus briefing screen announcement includes its briefing text");
}

int main(int argc, char **argv)
{
    const char *which = argc > 1 ? argv[1] : "";
    if (!strcmp(which, "graphic_fallback")) test_graphic_fallback();
    else if (!strcmp(which, "capture_end")) test_capture_end();
    else if (!strcmp(which, "clear_transition")) test_clear_transition();
    else if (!strcmp(which, "title_once")) test_title_once();
    else if (!strcmp(which, "slider")) test_slider();
    else if (!strcmp(which, "profile")) test_profile();
    else if (!strcmp(which, "briefing")) test_briefing();
    else { fprintf(stderr, "unknown case: %s\n", which); return 2; }
    printf("%s: %d assertions, %d failed\n", which, assertions, failures);
    return failures ? 1 : 0;
}
