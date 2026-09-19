/* AVP Access: spoken mission objectives -- see acc_objectives.h. */
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "language.h"

#include "acc_objectives.h"
#include "acc_speech.h"
#include "acc_pad.h"

#include <stdio.h>
#include <string.h>

/* Which objective the next press will read. Kept across presses so repeated
   requests walk the list rather than repeating the first entry. */
static int Cursor;

void AccObjectives_Reset(void)
{
    Cursor = 0;
}

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

int AccObjectives_Format(int index, int count, int achieved, int achievable,
                         const char *description, char *text, size_t size)
{
    const char *state;

    if (!text || size == 0) return 0;

    if (count <= 0) {
        snprintf(text, size, "No objectives yet.");
        return 1;
    }

    if (index < 0 || index >= count) return 0;

    /* "Not yet possible" is worth distinguishing from merely unfinished: it
       tells the player this one is gated on something else and not the thing
       to be working on now. */
    if (achieved)         state = "complete";
    else if (!achievable) state = "not yet possible";
    else                  state = "not complete";

    if (description && description[0])
        snprintf(text, size, "Objective %d of %d, %s. %s",
                 index + 1, count, state, description);
    else
        /* Many levels store no description text -- the mission is told through
           on-screen messages instead. Say so, rather than trailing off and
           leaving the player wondering whether speech failed. */
        snprintf(text, size, "Objective %d of %d, %s. No description recorded.",
                 index + 1, count, state);

    return 1;
}

void AccObjectives_Announce(void)
{
    char text[512];
    int count, achieved = 0, achievable = 1, stringID = -1;

    if (!AccSpeech_IsAvailable()) return;

    count = AccObjectives_Count();

    if (count <= 0) {
        if (AccObjectives_Format(0, 0, 0, 0, NULL, text, sizeof(text)))
            AccSpeech_Say(text, 1);
        Cursor = 0;
        return;
    }

    /* The list can shrink between presses, so wrap against the current count
       rather than trusting a cursor left over from a longer list. */
    if (Cursor < 0 || Cursor >= count) Cursor = 0;

    if (!AccObjectives_Get(Cursor, &achieved, &achievable, &stringID)) {
        Cursor = 0;
        return;
    }

    /* Deliberately not gated behind --padtrace: this only prints when the
       player actually asks for an objective, so it is at most one line per
       press, and it is the quickest way to tell a level with no description
       text from a lookup that is failing. */
    {
        const char *desc = SafeTextString(stringID);
        fprintf(stderr,
                "AVP Access: objective %d of %d, stringID=%d achieved=%d achievable=%d, text=%s\n",
                Cursor + 1, count, stringID, achieved, achievable,
                desc ? desc : "(blank)");
        fflush(stderr);
    }

    if (AccObjectives_Format(Cursor, count, achieved, achievable,
                             SafeTextString(stringID), text, sizeof(text)))
        AccSpeech_Say(text, 1);

    Cursor++;
    if (Cursor >= count) Cursor = 0;
}
