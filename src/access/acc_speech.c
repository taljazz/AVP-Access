/* AVP Access ------------------------------------------------------------------
  Screen-reader speech output -- see acc_speech.h.
  ---------------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "acc_speech.h"

#define ACC_SPEECH_MAXLEN 4096

#if defined(_WIN32)

#include <windows.h>

/* Tolk exports plain __cdecl C symbols. Its C++ "bool" is a single byte, so
   unsigned char is ABI-compatible in the typedefs below. */
typedef void          (*PFN_Tolk_Load)(void);
typedef void          (*PFN_Tolk_Unload)(void);
typedef unsigned char (*PFN_Tolk_IsLoaded)(void);
typedef void          (*PFN_Tolk_TrySAPI)(unsigned char);
typedef unsigned char (*PFN_Tolk_Output)(const wchar_t *, unsigned char);
typedef unsigned char (*PFN_Tolk_Silence)(void);
typedef unsigned char (*PFN_Tolk_HasSpeech)(void);
typedef const wchar_t *(*PFN_Tolk_DetectScreenReader)(void);

static HMODULE                     TolkModule;
static PFN_Tolk_Load               pTolk_Load;
static PFN_Tolk_Unload             pTolk_Unload;
static PFN_Tolk_IsLoaded           pTolk_IsLoaded;
static PFN_Tolk_TrySAPI            pTolk_TrySAPI;
static PFN_Tolk_Output             pTolk_Output;
static PFN_Tolk_Silence            pTolk_Silence;
static PFN_Tolk_HasSpeech          pTolk_HasSpeech;
static PFN_Tolk_DetectScreenReader pTolk_DetectScreenReader;

static int  SpeechAvailable;
static char BackendName[64] = "none";

/* The last thing we said, so callers can spam a description every frame
   without the screen reader stuttering. */
static char LastSpoken[ACC_SPEECH_MAXLEN];

static void RecordBackendName(void)
{
	const wchar_t *reader = NULL;

	if (pTolk_DetectScreenReader) reader = pTolk_DetectScreenReader();

	if (reader && reader[0]) {
		int n = WideCharToMultiByte(CP_ACP, 0, reader, -1,
		                            BackendName, sizeof(BackendName) - 1, NULL, NULL);
		if (n <= 0) strcpy(BackendName, "screen reader");
	} else if (pTolk_HasSpeech && pTolk_HasSpeech()) {
		strcpy(BackendName, "SAPI");
	} else {
		strcpy(BackendName, "none");
	}
}

int AccSpeech_Init(void)
{
	if (SpeechAvailable) return 1;

	TolkModule = LoadLibraryA("Tolk.dll");
	if (!TolkModule) return 0;

	pTolk_Load               = (PFN_Tolk_Load)              (void *)GetProcAddress(TolkModule, "Tolk_Load");
	pTolk_Unload             = (PFN_Tolk_Unload)            (void *)GetProcAddress(TolkModule, "Tolk_Unload");
	pTolk_IsLoaded           = (PFN_Tolk_IsLoaded)          (void *)GetProcAddress(TolkModule, "Tolk_IsLoaded");
	pTolk_TrySAPI            = (PFN_Tolk_TrySAPI)           (void *)GetProcAddress(TolkModule, "Tolk_TrySAPI");
	pTolk_Output             = (PFN_Tolk_Output)            (void *)GetProcAddress(TolkModule, "Tolk_Output");
	pTolk_Silence            = (PFN_Tolk_Silence)           (void *)GetProcAddress(TolkModule, "Tolk_Silence");
	pTolk_HasSpeech          = (PFN_Tolk_HasSpeech)         (void *)GetProcAddress(TolkModule, "Tolk_HasSpeech");
	pTolk_DetectScreenReader = (PFN_Tolk_DetectScreenReader)(void *)GetProcAddress(TolkModule, "Tolk_DetectScreenReader");

	if (!pTolk_Load || !pTolk_Output) {
		FreeLibrary(TolkModule);
		TolkModule = NULL;
		return 0;
	}

	/* Ask for SAPI before loading, so a player with no screen reader running
	   still gets spoken menus out of the box. */
	if (pTolk_TrySAPI) pTolk_TrySAPI(1);
	pTolk_Load();

	if (pTolk_IsLoaded && !pTolk_IsLoaded()) {
		FreeLibrary(TolkModule);
		TolkModule = NULL;
		return 0;
	}

	SpeechAvailable = 1;
	RecordBackendName();
	return 1;
}

void AccSpeech_Shutdown(void)
{
	if (TolkModule) {
		if (SpeechAvailable && pTolk_Unload) pTolk_Unload();
		FreeLibrary(TolkModule);
		TolkModule = NULL;
	}
	SpeechAvailable = 0;
	LastSpoken[0] = '\0';
	strcpy(BackendName, "none");
}

void AccSpeech_Say(const char *text, int interrupt)
{
	wchar_t wide[ACC_SPEECH_MAXLEN];

	if (!SpeechAvailable || !text || !text[0]) return;

	/* No duplicate suppression here on purpose. The menus already compare their
	   own rendered line before calling, and in-game messages genuinely repeat --
	   picking up two clips of ammo should say so twice. */
	strncpy(LastSpoken, text, sizeof(LastSpoken) - 1);
	LastSpoken[sizeof(LastSpoken) - 1] = '\0';

	/* Game text comes out of language.txt in a legacy 8-bit encoding, so the
	   system codepage is the right interpretation, not UTF-8. */
	if (MultiByteToWideChar(CP_ACP, 0, text, -1, wide, ACC_SPEECH_MAXLEN) <= 0) return;

	pTolk_Output(wide, (unsigned char)(interrupt ? 1 : 0));
}

void AccSpeech_Silence(void)
{
	if (SpeechAvailable && pTolk_Silence) pTolk_Silence();
	LastSpoken[0] = '\0';
}

int AccSpeech_IsAvailable(void)   { return SpeechAvailable; }
const char *AccSpeech_Backend(void) { return BackendName; }

#else /* not _WIN32 ---------------------------------------------------------- */

int  AccSpeech_Init(void)          { return 0; }
void AccSpeech_Shutdown(void)      { }
int  AccSpeech_IsAvailable(void)   { return 0; }
const char *AccSpeech_Backend(void){ return "none"; }
void AccSpeech_Say(const char *text, int interrupt) { (void)text; (void)interrupt; }
void AccSpeech_Silence(void)       { }

#endif /* _WIN32 */

/* Shared by both builds: formatting costs nothing when speech is off because
   we bail before touching the varargs. */
void AccSpeech_Sayf(int interrupt, const char *fmt, ...)
{
	char buf[ACC_SPEECH_MAXLEN];
	va_list ap;

	if (!AccSpeech_IsAvailable() || !fmt) return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	buf[sizeof(buf) - 1] = '\0';
	AccSpeech_Say(buf, interrupt);
}
