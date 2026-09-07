/* AVP Access ------------------------------------------------------------------
  Screen-reader speech output.

  Backed by Tolk (NVDA / JAWS / Window-Eyes, with a SAPI fallback) on Windows.
  Tolk.dll is loaded dynamically at run time rather than linked, so the game
  still starts normally when it is absent -- speech simply goes quiet. On other
  platforms every entry point is a no-op, so this compiles everywhere.
  ---------------------------------------------------------------------------*/
#ifndef ACC_SPEECH_H
#define ACC_SPEECH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if speech is available afterwards, 0 otherwise. Safe to call twice. */
int AccSpeech_Init(void);
void AccSpeech_Shutdown(void);

int AccSpeech_IsAvailable(void);

/* Name of the detected screen reader, or "SAPI"/"none". Never NULL. */
const char *AccSpeech_Backend(void);

/* interrupt != 0 cuts off whatever is currently being spoken. Use it for
   anything the player triggered directly (moving the menu cursor, asking for
   status); leave it 0 for background chatter that should queue politely. */
void AccSpeech_Say(const char *text, int interrupt);
void AccSpeech_Sayf(int interrupt, const char *fmt, ...);

void AccSpeech_Silence(void);

#ifdef __cplusplus
}
#endif

#endif /* ACC_SPEECH_H */
