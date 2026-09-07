/* AVP Access ------------------------------------------------------------------
  Spoken front-end menus -- see acc_menu.h.
  ---------------------------------------------------------------------------*/
#include <SDL3/SDL.h>
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "bh_types.h"
#include "avp_menudata.h"
#include "avp_menus.h"
#include "language.h"
#include "avp_userprofile.h"

#include "acc_speech.h"
#include "acc_menu.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The menu table, indexed by AVPMENU_ID; carries each menu's title. */
extern AVPMENU AvPMenusData[];

#define ACC_MAX_ELEMENTS 64
#define ACC_ELEMENT_TEXT 192
#define ACC_LINE_LEN     2048
#define ACC_BRIEFING_LINES 5

/* ---------------------------------------------------------------- capture -- */

static char CapturedText[ACC_MAX_ELEMENTS][ACC_ELEMENT_TEXT];
static int  CaptureSlot = -1;

static ACCMENU_RENDERTEXT          OrigRenderText;
static ACCMENU_RENDERTEXT_COLOURED OrigRenderTextColoured;

static void CaptureAppend(const char *textPtr)
{
	char *slot;
	size_t used;

	if (CaptureSlot < 0 || !textPtr || !textPtr[0]) return;

	/* The renderer draws a lone "I" as the text-entry caret. Speaking it would
	   turn every edit field into "name I". */
	if (textPtr[0] == 'I' && textPtr[1] == 0) return;

	slot = CapturedText[CaptureSlot];
	used = strlen(slot);

	if (used + 1 >= ACC_ELEMENT_TEXT) return;

	/* Separate the fragments an element is drawn from (label, value, units) so
	   they do not run together into a single word. */
	if (used) {
		slot[used++] = ' ';
		slot[used] = 0;
	}

	strncpy(slot + used, textPtr, ACC_ELEMENT_TEXT - used - 1);
	slot[ACC_ELEMENT_TEXT - 1] = 0;
}

int AccMenu_BeginCapture(int elementIndex,
                         ACCMENU_RENDERTEXT rt,
                         ACCMENU_RENDERTEXT_COLOURED rtc)
{
	if (!AccSpeech_IsAvailable()) return 0;
	if (elementIndex < 0 || elementIndex >= ACC_MAX_ELEMENTS) return 0;

	CaptureSlot            = elementIndex;
	OrigRenderText         = rt;
	OrigRenderTextColoured = rtc;

	/* Every element is redrawn each frame, so start this slot empty. */
	CapturedText[elementIndex][0] = 0;

	return 1;
}

int AccMenu_CaptureRenderText(char *textPtr, int x, int y, int alpha,
                              enum AVPMENUFORMAT_ID format)
{
	CaptureAppend(textPtr);

	if (!OrigRenderText) return x;
	return OrigRenderText(textPtr, x, y, alpha, format);
}

int AccMenu_CaptureRenderTextColoured(char *textPtr, int x, int y, int alpha,
                                      enum AVPMENUFORMAT_ID format,
                                      int r, int g, int b)
{
	CaptureAppend(textPtr);

	if (!OrigRenderTextColoured) return x;
	return OrigRenderTextColoured(textPtr, x, y, alpha, format, r, g, b);
}

/* ------------------------------------------------------------ description -- */

/* GetTextString() is bounds-checked at the top end but not against negative
   IDs, and an unloaded slot can still come back empty. */
static const char *SafeTextString(int stringID)
{
	char *s;

	if (stringID < 0) return NULL;

	s = GetTextString((enum TEXTSTRING_ID)stringID);
	if (!s || !s[0]) return NULL;

	return s;
}

static void AppendStr(char *dst, size_t cap, const char *src)
{
	size_t used;

	if (!src || !src[0]) return;

	used = strlen(dst);
	if (used + 1 >= cap) return;

	strncpy(dst + used, src, cap - used - 1);
	dst[cap - 1] = 0;
}

static void AppendFmt(char *dst, size_t cap, const char *fmt, ...)
{
	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	tmp[sizeof(tmp) - 1] = 0;

	AppendStr(dst, cap, tmp);
}

/* Only used when an element rendered no text of its own. */
static void DescribeElementFallback(const AVPMENU_ELEMENT *e, char *out, size_t cap)
{
	const char *label;

	out[0] = 0;
	if (!e) return;

	/* The species entries on the Single Player menu are pictures with no text,
	   so capture comes back empty. Their help string is a proper localised
	   label -- "Play as an Alien" and so on -- which beats announcing the
	   utterly useless "picture, 1 of 3". */
	if (e->ElementID != AVPMENU_ELEMENT_GOTOMENU_GFX) {
		label = SafeTextString((int)e->a.TextDescription);
		if (label) { AppendStr(out, cap, label); return; }
	}

	label = SafeTextString((int)e->HelpString);
	if (label) { AppendStr(out, cap, label); return; }

	AppendStr(out, cap, "item");
}

/* The profile list is a single menu element that paints its own scrolling list,
   so there is nothing for the capture to catch and no per-profile element to
   count. Read the selected profile straight out of the profile list instead. */
static int DescribeUserProfile(const AVPMENU_ELEMENT *e, char *out, size_t cap)
{
	AVP_USER_PROFILE *p;
	int index, count, i;

	if (!e || e->ElementID != AVPMENU_ELEMENT_USERPROFILE) return 0;
	if (!e->c.SliderValuePtr) return 0;

	index = *e->c.SliderValuePtr;
	count = e->b.MaxSliderValue + 1;

	p = GetFirstUserProfile();
	for (i = 0; i < index && p; i++) p = GetNextUserProfile();

	if (p && p->Name[0]) AppendStr(out, cap, p->Name);
	else                 AppendStr(out, cap, "new profile");

	if (count > 1) AppendFmt(out, cap, ", %d of %d", index + 1, count);

	return 1;
}

/* ----------------------------------------------------------------- state -- */

static int  LastMenuID       = -1;
static int  LastSelected     = -1;
static int  LastEnteringText = -1;
static int  LastChangingKeys = -1;
static char LastLine[ACC_LINE_LEN];

static const AVPMENU_ELEMENT *CachedElements;
static int CachedNumElements;
static int CachedSelected;
static int CachedMenuID = -1;

/* Counts down once a frame; refreshed on every menu poll. A couple of frames of
   slack keeps it true across the gap between the pad read and the menu update,
   whichever order they happen to run in. */
static int MenusActiveFrames;

int AccMenu_MenusActive(void)
{
	return MenusActiveFrames > 0;
}

void AccMenu_DecayMenusActive(void)
{
	if (MenusActiveFrames > 0) MenusActiveFrames--;
}

/* The mission prelude on the level-select screens. A sighted player reads it
   while choosing a level, so it belongs in the announcement rather than behind
   an extra keypress. AccMenu_BriefingLine() lives in avp_menus.c, where the
   strings are file-static, and returns NULL off the level-select screens. */
static void AppendBriefingText(char *line, size_t cap)
{
	int i;

	for (i = 0; i < ACC_BRIEFING_LINES; i++) {
		const char *s = AccMenu_BriefingLine(i);
		if (s && s[0]) AppendFmt(line, cap, ". %s", s);
	}
}

static void BuildLine(char *line, size_t cap, int includeTitle)
{
	const AVPMENU_ELEMENT *e = NULL;

	line[0] = 0;

	if (includeTitle && CachedMenuID >= 0) {
		const char *title = SafeTextString((int)AvPMenusData[CachedMenuID].MenuTitle);
		if (title) AppendFmt(line, cap, "%s. ", title);
	}

	if (CachedSelected < 0 || CachedSelected >= CachedNumElements) return;

	if (CachedElements) e = &CachedElements[CachedSelected];

	/* Profiles carry their own count, so they skip the generic position below. */
	if (DescribeUserProfile(e, line, cap)) return;

	{
		const char *captured = (CachedSelected < ACC_MAX_ELEMENTS)
		                     ? CapturedText[CachedSelected] : "";

		if (captured && captured[0]) {
			AppendStr(line, cap, captured);
		} else if (e) {
			char desc[ACC_ELEMENT_TEXT];
			DescribeElementFallback(e, desc, sizeof(desc));
			AppendStr(line, cap, desc);
		}
	}

	/* Position disambiguates entries that read identically -- several blank
	   save slots, say -- which would otherwise be silent to move between. */
	if (CachedNumElements > 1)
		AppendFmt(line, cap, ", %d of %d", CachedSelected + 1, CachedNumElements);

	AppendBriefingText(line, cap);
}

void AccMenu_Poll(int menuID, const AVPMENU_ELEMENT *elements, int numElements,
                  int selectedElement, int userEnteringText,
                  int userChangingKeyConfig)
{
	char line[ACC_LINE_LEN];
	int menuChanged, modeChanged;

	/* Set before the speech check: the gamepad needs to know a menu is up even
	   when there is no screen reader running. */
	MenusActiveFrames = 3;

	if (!AccSpeech_IsAvailable()) return;

	CachedElements    = elements;
	CachedNumElements = numElements;
	CachedSelected    = selectedElement;
	CachedMenuID      = menuID;

	menuChanged = (menuID != LastMenuID);
	modeChanged = (userEnteringText != LastEnteringText) ||
	              (userChangingKeyConfig != LastChangingKeys);

	LastEnteringText = userEnteringText;
	LastChangingKeys = userChangingKeyConfig;

	/* While the player is typing, their own keystrokes are the feedback;
	   re-reading the field on every character fights the screen reader echo. */
	if (userEnteringText) {
		LastMenuID   = menuID;
		LastSelected = selectedElement;
		if (modeChanged) {
			AccSpeech_Say("editing. type, then press enter", 1);
			LastLine[0] = 0;
		}
		return;
	}

	if (userChangingKeyConfig) {
		LastMenuID   = menuID;
		LastSelected = selectedElement;
		if (modeChanged) {
			AccSpeech_Say("press a key to bind, or escape to cancel", 1);
			LastLine[0] = 0;
		}
		return;
	}

	BuildLine(line, sizeof(line), menuChanged);

	/* Comparing the whole rendered line, rather than just the selection index,
	   is what makes a toggle or slider announce its new value: the selection has
	   not moved, but the text has. */
	if (!menuChanged && strncmp(line, LastLine, sizeof(LastLine) - 1) == 0) {
		LastMenuID   = menuID;
		LastSelected = selectedElement;
		return;
	}

	LastMenuID   = menuID;
	LastSelected = selectedElement;
	strncpy(LastLine, line, sizeof(LastLine) - 1);
	LastLine[sizeof(LastLine) - 1] = 0;

	AccSpeech_Say(line, 1);
}

void AccMenu_RepeatCurrent(void)
{
	char line[ACC_LINE_LEN];

	if (!AccSpeech_IsAvailable()) return;

	BuildLine(line, sizeof(line), 1);
	AccSpeech_Silence();      /* also clears the duplicate-suppression cache */
	AccSpeech_Say(line, 1);
}

void AccMenu_SpeakHelp(void)
{
	const char *help;

	if (!AccSpeech_IsAvailable()) return;
	if (!CachedElements || CachedSelected < 0 || CachedSelected >= CachedNumElements) return;

	help = SafeTextString((int)CachedElements[CachedSelected].HelpString);

	AccSpeech_Silence();
	AccSpeech_Say(help ? help : "no help for this item", 1);
}

void AccMenu_Reset(void)
{
	int i;

	LastMenuID       = -1;
	LastSelected     = -1;
	LastEnteringText = -1;
	LastChangingKeys = -1;
	LastLine[0]      = 0;

	CachedElements    = NULL;
	CachedNumElements = 0;
	CachedSelected    = -1;
	CachedMenuID      = -1;

	CaptureSlot = -1;
	for (i = 0; i < ACC_MAX_ELEMENTS; i++) CapturedText[i][0] = 0;
}
