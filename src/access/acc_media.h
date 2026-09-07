/* AVP Access ------------------------------------------------------------------
  Bink / Smacker playback.

  This port never implemented either: PlayBinkedFMV() and the whole CDDA family
  were empty stubs, so the game shipped with no music and no cutscenes. The GOG
  and Steam releases store the soundtrack as Bink files in FMVs/ ("01 Marine
  Music 1.bik" .. "15 Earthbound.bik", matching tracks 1-15 in CD Tracks.txt),
  and the cutscenes as .bik/.smk alongside them.

  FFmpeg decodes both formats, so one streaming decoder serves both: the music
  system plays a track, and a cutscene plays once with picture and sound.

  Every entry point is safe to call when FFmpeg or the file is missing -- they
  simply do nothing, exactly as the stubs did.
  ---------------------------------------------------------------------------*/
#ifndef ACC_MEDIA_H
#define ACC_MEDIA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call once the OpenAL context exists, and at shutdown. */
int  AccMedia_Init(void);
void AccMedia_Shutdown(void);

int  AccMedia_IsAvailable(void);

/* --- music ------------------------------------------------------------- */

/* track is a CD track number (1-based) as used by CD Tracks.txt. Returns 1 if
   playback started. */
int  AccMedia_PlayTrack(int track);
void AccMedia_StopTrack(void);
int  AccMedia_TrackIsPlaying(void);

/* volume is on the game's 0..127 scale. */
void AccMedia_SetVolume(int volume);

/* Refills the streaming buffers. Must be called regularly -- it is hooked into
   SoundSys_Management(), which every game and menu loop already calls. */
void AccMedia_Update(void);

/* --- cutscenes --------------------------------------------------------- */

/* Plays a movie (path relative to the game data directory) with both picture
   and sound, returning when it ends or the player presses a key. Video is
   drawn into the game's 640x480 software surface, letterboxed to preserve the
   source aspect, and paced against the audio clock so speech stays in sync.
   A file with no video stream plays as audio only. */
void AccMedia_PlayMovie(const char *filename);

/* --- in-game plot messages --------------------------------------------- */

/* The story briefings that appear on wall monitors mid-level (message<N>.smk,
   triggered from bh_mission.c). Unlike a cutscene these do not take over the
   screen: the game asks for one frame at a time and paints it onto a texture,
   so playback is pulled rather than pushed.

   All of them are 128x96 8-bit paletted at 15fps with mono audio, which is
   exactly the format the engine's FMVTEXTURE path already expects -- Smacker
   decodes natively to paletted, so no conversion is involved. */

int  AccMedia_PlotStart(int messageNumber);
void AccMedia_PlotStop(void);
int  AccMedia_PlotIsPlaying(void);

/* Writes the frame due at the current audio position into indices (w*h bytes)
   and its colour table into palette. Returns 1 if a frame was written. Safe to
   call several times a frame -- the picture only advances when the audio clock
   says it should, so two screens showing one message stay in step. */
int  AccMedia_PlotFrame(unsigned char *indices, int w, int h,
                        unsigned char palette[256][3]);

#ifdef __cplusplus
}
#endif

#endif /* ACC_MEDIA_H */
