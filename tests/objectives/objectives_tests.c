/* Tests the actual acc_objectives.c. The objective list, the string table and
 * speech are captured at their public boundaries, so the wording and the
 * cycling can be checked without a game or a speech backend.
 */
#include <stdio.h>
#include <string.h>

#include "fixer.h"
#include "3dc.h"
#include "acc_speech.h"
#include "acc_objectives.h"

/* --- the mocked objective list ----------------------------------------- */

#define MAX_FAKE 8

static struct {
    int achieved, achievable, stringID;
} objectives[MAX_FAKE];
static int objectiveCount;

int AccObjectives_Count(void) { return objectiveCount; }

int AccObjectives_Get(int index, int *achieved, int *achievable, int *stringID)
{
    if (index < 0 || index >= objectiveCount) return 0;
    if (achieved)   *achieved = objectives[index].achieved;
    if (achievable) *achievable = objectives[index].achievable;
    if (stringID)   *stringID = objectives[index].stringID;
    return 1;
}

/* --- the mocked string table ------------------------------------------- */

static char descriptions[MAX_FAKE][64];

char *GetTextString(enum TEXTSTRING_ID id)
{
    int i = (int)id;
    if (i < 0 || i >= MAX_FAKE) return "";
    return descriptions[i];
}

/* --- captured speech ---------------------------------------------------- */

static char spoken[512];
static int speech_calls, speech_interrupt;

int AccSpeech_IsAvailable(void) { return 1; }

void AccSpeech_Say(const char *message, int interrupt)
{
    ++speech_calls;
    speech_interrupt = interrupt;
    strncpy(spoken, message ? message : "", sizeof(spoken) - 1);
    spoken[sizeof(spoken) - 1] = 0;
}

/* --- harness ------------------------------------------------------------ */

static int assertions, failures;

static void check(int condition, const char *what)
{
    ++assertions;
    if (condition) printf("PASS: %s\n", what);
    else { ++failures; printf("FAIL: %s\n", what); }
}

static void setup(int count)
{
    int i;
    objectiveCount = count;
    for (i = 0; i < MAX_FAKE; i++) {
        objectives[i].achieved = 0;
        objectives[i].achievable = 1;
        objectives[i].stringID = i;
        snprintf(descriptions[i], sizeof(descriptions[i]), "Objective text %d", i);
    }
    speech_calls = 0;
    spoken[0] = 0;
    AccObjectives_Reset();
}

/* --- cases -------------------------------------------------------------- */

static void test_none(void)
{
    setup(0);
    AccObjectives_Announce();

    check(speech_calls == 1, "an empty list still says something");
    check(strstr(spoken, "No objectives yet") != NULL,
          "an empty list is reported as no objectives");
}

static void test_states(void)
{
    char text[256];

    AccObjectives_Format(0, 3, 0, 1, "Restore power", text, sizeof(text));
    check(strstr(text, "Objective 1 of 3, not complete. Restore power") != NULL,
          "an unfinished objective reads as not complete");

    AccObjectives_Format(1, 3, 1, 1, "Reach the lift", text, sizeof(text));
    check(strstr(text, "Objective 2 of 3, complete. Reach the lift") != NULL,
          "a finished objective reads as complete");

    /* Distinguishing "gated on something else" from "just unfinished" is the
       difference between knowing what to work on and guessing. */
    AccObjectives_Format(2, 3, 0, 0, "Escape", text, sizeof(text));
    check(strstr(text, "not yet possible") != NULL,
          "an objective that is not yet achievable says so");
}

static void test_missing_description(void)
{
    char text[256];

    check(AccObjectives_Format(0, 2, 0, 1, NULL, text, sizeof(text)) == 1,
          "a missing description still produces a line");
    check(strstr(text, "No description recorded") != NULL,
          "a missing description says so rather than trailing off");
}

static void test_out_of_range(void)
{
    char text[256];

    check(AccObjectives_Format(3, 3, 0, 1, "x", text, sizeof(text)) == 0,
          "an index past the end produces nothing");
    check(AccObjectives_Format(-1, 3, 0, 1, "x", text, sizeof(text)) == 0,
          "a negative index produces nothing");
}

static void test_cycles(void)
{
    setup(3);

    AccObjectives_Announce();
    check(strstr(spoken, "Objective 1 of 3") != NULL, "the first press reads objective one");

    AccObjectives_Announce();
    check(strstr(spoken, "Objective 2 of 3") != NULL, "the next press advances");

    AccObjectives_Announce();
    check(strstr(spoken, "Objective 3 of 3") != NULL, "the third press reads the last");

    AccObjectives_Announce();
    check(strstr(spoken, "Objective 1 of 3") != NULL, "the cycle wraps to the first");
}

static void test_shrinking_list(void)
{
    setup(4);

    AccObjectives_Announce();
    AccObjectives_Announce();
    AccObjectives_Announce();          /* cursor now sits at index 3 */

    /* Objectives can vanish between presses; a cursor left over from a longer
       list must not read off the end or go silent. */
    objectiveCount = 2;
    AccObjectives_Announce();

    check(speech_calls == 4, "a shrunken list still announces");
    check(strstr(spoken, "of 2") != NULL, "the count reflects the shorter list");
    check(strstr(spoken, "Objective 1 of 2") != NULL,
          "an out-of-range cursor wraps to the first objective");
}

static void test_reset(void)
{
    setup(3);
    AccObjectives_Announce();
    AccObjectives_Announce();

    AccObjectives_Reset();
    AccObjectives_Announce();

    check(strstr(spoken, "Objective 1 of 3") != NULL,
          "reset returns the cycle to the first objective");
}

static void test_interrupts(void)
{
    setup(2);
    AccObjectives_Announce();
    check(speech_interrupt == 1, "a requested readout interrupts older speech");
}

int main(int argc, char **argv)
{
    const char *which = (argc > 1) ? argv[1] : "";

    if (!strcmp(which, "none"))            test_none();
    else if (!strcmp(which, "states"))     test_states();
    else if (!strcmp(which, "no_desc"))    test_missing_description();
    else if (!strcmp(which, "range"))      test_out_of_range();
    else if (!strcmp(which, "cycles"))     test_cycles();
    else if (!strcmp(which, "shrinking"))  test_shrinking_list();
    else if (!strcmp(which, "reset"))      test_reset();
    else if (!strcmp(which, "interrupts")) test_interrupts();
    else { printf("unknown case: %s\n", which); return 2; }

    printf("%s: %d assertions, %d failed\n", which, assertions, failures);
    return failures ? 1 : 0;
}
