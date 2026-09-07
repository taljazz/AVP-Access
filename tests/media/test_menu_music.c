/* Actual production playback-control functions with a fake stream backend.
 * Decoder/device audibility is checked separately in the running game. */
#include <stdio.h>
#include <string.h>

typedef struct { int playing; } ACC_STREAM;
static ACC_STREAM Music;
static int MenuMusicFailed;
static int initialized, cd_on, file_ok, open_count, video_requested;
static int volume_count, pump_count, stop_count, update_count, track_number;
static int refill_pending, checks, failures;
static char opened_path[256];

static void check(int condition, const char *label)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL: %s\n", label); }
}

int AccMedia_Init(void) { return initialized; }
int CDDA_IsOn(void) { return cd_on; }
static int StreamOpen(ACC_STREAM *stream, const char *path, int want_video)
{
    ++open_count;
    video_requested = want_video;
    snprintf(opened_path, sizeof(opened_path), "%s", path);
    stream->playing = file_ok;
    return file_ok;
}
static void StreamApplyVolume(ACC_STREAM *stream) { (void)stream; ++volume_count; }
static void StreamPump(ACC_STREAM *stream) { (void)stream; ++pump_count; }
static int FindTrackFile(int track, char *path, size_t cap)
{
    track_number = track;
    snprintf(path, cap, "fmvs/%02d gameplay.bik", track);
    return track > 0;
}
void AccMedia_Update(void)
{
    ++update_count;
    if (refill_pending) { Music.playing = 1; refill_pending = 0; }
}
int AccMedia_TrackIsPlaying(void) { return Music.playing; }
void AccMedia_StopTrack(void) { ++stop_count; Music.playing = 0; }

#include "menu_source.generated.h"

static void reset(void)
{
    memset(&Music, 0, sizeof(Music));
    MenuMusicFailed = 0;
    initialized = cd_on = file_ok = 1;
    open_count = video_requested = volume_count = pump_count = 0;
    stop_count = update_count = track_number = refill_pending = 0;
    opened_path[0] = 0;
}

int main(void)
{
    int i;
    reset();
    StartMenuMusic();
    check(open_count == 1, "menu starts one stream");
    check(!strcmp(opened_path, "fmvs/introsound.smk"), "menu uses original Smacker theme");
    check(video_requested == 0, "dummy menu picture is not decoded");
    check(volume_count == 1 && pump_count == 1, "menu volume and initial queue are prepared");
    for (i = 0; i < 120; ++i) PlayMenuMusic();
    check(open_count == 1, "active theme is not restarted each frame");
    check(update_count == 120, "procedural intro refills the audio every frame");

    Music.playing = 0;
    refill_pending = 1;
    PlayMenuMusic();
    check(open_count == 1 && Music.playing, "refill happens before deciding the theme has ended");
    Music.playing = 0;
    PlayMenuMusic();
    check(open_count == 2 && Music.playing, "completed theme repeats once");
    check(!strcmp(opened_path, "fmvs/introsound.smk"), "repeat keeps original menu theme");
    EndMenuMusic();
    check(stop_count == 1 && !Music.playing, "leaving menus stops their music");
    check(AccMedia_PlayTrack(7) == 1 && track_number == 7, "gameplay preserves numbered track selection");
    check(!strcmp(opened_path, "fmvs/07 gameplay.bik"), "gameplay uses its own selected file");
    check(video_requested == 0, "gameplay music remains audio only");
    EndMenuMusic();
    StartMenuMusic();
    check(!strcmp(opened_path, "fmvs/introsound.smk"), "returning to menus restores their own theme");

    reset();
    file_ok = 0;
    StartMenuMusic();
    for (i = 0; i < 1000; ++i) PlayMenuMusic();
    check(open_count == 1 && MenuMusicFailed, "missing or invalid theme is attempted only once");
    check(volume_count == 0 && pump_count == 0, "failed theme does not prepare a nonexistent stream");
    file_ok = 1;
    check(AccMedia_PlayTrack(3) == 1, "missing theme does not disable level soundtrack");
    check(!strcmp(opened_path, "fmvs/03 gameplay.bik"), "level selection survives a menu failure");

    reset();
    initialized = 0;
    check(AccMedia_PlayMenuMusic() == 0, "missing audio context safely defers playback");
    check(open_count == 0 && !MenuMusicFailed, "missing context does not permanently suppress theme");
    initialized = 1;
    check(AccMedia_PlayMenuMusic() == 1 && open_count == 1, "theme starts once audio context becomes ready");

    reset();
    cd_on = 0;
    StartMenuMusic();
    PlayMenuMusic();
    check(open_count == 0, "menu respects unavailable music backend");
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
