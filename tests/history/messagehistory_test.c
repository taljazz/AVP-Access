#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Compile the real source in this translation unit; mock only its boundaries. */
#include "../../src/avp/messagehistory.c"

AVP_GAME_DESC AvP;
int NormalFrameTime;
static unsigned char captured[2048];
static unsigned char saved[8192];
static unsigned char valid_saved[8192];
static size_t valid_saved_size;
static size_t saved_size;
static size_t save_offset;
static int calls, failures, checks;

static void check(int ok, const char *name)
{
    checks++;
    if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; }
}

void NewOnScreenMessage(unsigned char *messagePtr)
{
    strncpy((char *)captured, (char *)messagePtr, sizeof(captured)-1);
    captured[sizeof(captured)-1] = 0;
    calls++;
}

char *GetTextString(enum TEXTSTRING_ID id)
{
    static char long_text[5000];
    if (id == TEXTSTRING_INGAME_MESSAGENUMBER) return "Message";
    if (id == TEXTSTRING_LEVELMSG_001) return "first briefing";
    if (id == TEXTSTRING_LEVELMSG_002) return "second briefing";
    if (id == TEXTSTRING_LEVELMSG_003) {
        memset(long_text, 'X', sizeof(long_text)-1);
        long_text[sizeof(long_text)-1] = 0;
        return long_text;
    }
    return "unexpected string id";
}

void *GetPointerForSaveBlock(unsigned int size)
{
    void *result;
    if (save_offset + size > sizeof(saved)) abort();
    result = saved + save_offset;
    if (save_offset == 0) memset(saved, 0, sizeof(saved));
    save_offset += size;
    saved_size = save_offset;
    return result;
}
static void reset(void)
{
    MessageHistory_Initialise();
    memset(&AvP, 0, sizeof(AvP));
    NormalFrameTime = 0;
    memset(captured, 0, sizeof(captured));
    calls = 0;
    save_offset = 0;
}

static void test_empty(void)
{
    reset();
    MessageHistory_DisplayPrevious();
    check(calls == 1 && strcmp((char *)captured, "No mission messages yet.") == 0, "empty response");
    MessageHistory_DisplayPrevious();
    check(calls == 2 && strcmp((char *)captured, "No mission messages yet.") == 0, "empty response is not stored");
}

static void test_browse_and_expiry(void)
{
    reset();
    MessageHistory_Add(TEXTSTRING_LEVELMSG_001);
    MessageHistory_Add(TEXTSTRING_LEVELMSG_002);
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 2") && strstr((char *)captured, "second briefing"), "newest available immediately after Add");
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "browse backward");
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 2") != NULL, "wrap to newest");
    NormalFrameTime = 65536 * 4;
    MessageHistory_Maintain();
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 2") != NULL, "expiry tick resets cursor to newest");
}

static void test_long_text(void)
{
    reset();
    MessageHistory_Add(TEXTSTRING_LEVELMSG_003);
    MessageHistory_DisplayPrevious();
    check(strlen((char *)captured) == 1023, "long message truncates within 1024 byte buffer");
}

static int *metadata(void) { return (int *)(((SAVE_BLOCK_HEADER *)saved) + 1); }
static void restore_valid_save(void)
{
    MessageHistory_Initialise();
    memcpy(saved, valid_saved, valid_saved_size);
    Load_MessageHistory((SAVE_BLOCK_HEADER *)saved);
    calls = 0;
}
static void assert_valid_state(const char *description)
{
    const MESSAGE_HISTORY_SAVE_BLOCK *block = (const MESSAGE_HISTORY_SAVE_BLOCK *)valid_saved;
    check(NumberOfEntriesInMessageHistory == block->NumberOfEntriesInMessageHistory &&
          EntryToNextShow == block->EntryToNextShow &&
          MessageHistoryAccessedTimer == block->MessageHistoryAccessedTimer &&
          !memcmp(MessageHistoryStore, block + 1,
                  sizeof(struct MessageHistory) * block->NumberOfEntriesInMessageHistory), description);
}
static void test_save_load_validation(void)
{
    SAVE_BLOCK_HEADER *header;
    reset();
    MessageHistory_Add(TEXTSTRING_LEVELMSG_001);
    MessageHistory_Add(TEXTSTRING_LEVELMSG_002);
    MessageHistory_DisplayPrevious();
    Save_MessageHistory();
    header = (SAVE_BLOCK_HEADER *)saved;
    check(saved_size == (size_t)header->size, "save format size preserved");
    memcpy(valid_saved, saved, saved_size); valid_saved_size = saved_size;
    restore_valid_save();
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "valid load preserves cursor");

    restore_valid_save(); metadata()[0]=65; Load_MessageHistory(header); MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject invalid count");
    restore_valid_save(); metadata()[1]=3; Load_MessageHistory(header); MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject invalid cursor");
    restore_valid_save(); metadata()[2]=-1; Load_MessageHistory(header);
    assert_valid_state("negative timer leaves all existing history state unchanged");
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject negative timer");
    restore_valid_save(); metadata()[2]=65536*4+1; Load_MessageHistory(header);
    assert_valid_state("oversized timer leaves all existing history state unchanged");
    MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject timer beyond four seconds");
    restore_valid_save(); ((int *)(header + 1))[3]=-1; Load_MessageHistory(header); MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject negative string ID");
    restore_valid_save(); header->size=(int)sizeof(SAVE_BLOCK_HEADER); Load_MessageHistory(header); MessageHistory_DisplayPrevious();
    check(strstr((char *)captured, "Message 1") != NULL, "reject truncated header");
    restore_valid_save(); metadata()[0]=-1; Load_MessageHistory(header);
    assert_valid_state("negative count is rejected before size arithmetic");
    restore_valid_save(); metadata()[1]=-1; Load_MessageHistory(header);
    assert_valid_state("negative cursor cannot underflow history access");
    restore_valid_save(); header->size--; Load_MessageHistory(header);
    assert_valid_state("short payload leaves existing history unchanged");
    MessageHistory_Initialise(); MessageHistory_DisplayPrevious();
    check(!NumberOfEntriesInMessageHistory && !MessageHistoryAccessedTimer &&
          !strcmp((char *)captured,"No mission messages yet."), "level reset clears populated history and timer");
}
int main(void)
{
    test_empty();
    test_browse_and_expiry();
    test_long_text();
    test_save_load_validation();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
