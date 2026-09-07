#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "fixer.h"
#include "win95/cd_player.h"
#include "cdplayer.h"

/* cd_player.cpp */
int CDPlayerVolume;

#if SDL_MAJOR_VERSION < 2
static int HaveCDROM = 0;
static SDL_CD *cdrom = NULL;

/* ** */

void CheckCDVolume()
{
/*
	fprintf(stderr, "CheckCDVolume()\n");
*/
}

/* ** */

void CDDA_Start()
{
/*
	fprintf(stderr, "CDDA_Start()\n");
*/

	int numdrives;
	
	if (!HaveCDROM) {
		HaveCDROM = 1;
		SDL_InitSubSystem(SDL_INIT_CDROM);
	}
	
	if (cdrom != NULL)
		CDDA_End();
	
	numdrives = SDL_CDNumDrives();
	
	if (numdrives == 0)
		return;
	
	cdrom = SDL_CDOpen(0);
}

void CDDA_End()
{
/*
	fprintf(stderr, "CDDA_End()\n");
*/

	if (cdrom != NULL) {
		CDDA_Stop();
		
		SDL_CDClose(cdrom);
	}
	
	cdrom = NULL;
}

void CDDA_ChangeVolume(int volume)
{
	fprintf(stderr, "CDDA_ChangeVolume(%d)\n", volume);
}

int CDDA_CheckNumberOfTracks()
{
/*
	fprintf(stderr, "CDDA_CheckNumberOfTracks()\n");
*/

	if (cdrom == NULL)
		return 0;
			
	return cdrom->numtracks;
}

int CDDA_IsOn()
{
/*
	fprintf(stderr, "CDDA_IsOn()\n");
*/	
	return (cdrom != NULL);
}

int CDDA_IsPlaying()
{
/*
	fprintf(stderr, "CDDA_IsPlaying()\n");
*/	
	if (cdrom == NULL)
		return 0;

	return (SDL_CDStatus(cdrom) == CD_PLAYING);
}

void CDDA_Play(int CDDATrack)
{
/*
	fprintf(stderr, "CDDA_Play(%d)\n", CDDATrack);
*/
	if (cdrom == NULL)
		return;
		
	if (CD_INDRIVE(SDL_CDStatus(cdrom))) {
		int track = CDDATrack - 1;
		int i;
		
		if (cdrom->numtracks == 0)
			return;
		
		track %= cdrom->numtracks;
		
		for (i = 0; i < cdrom->numtracks; i++) {
			if (cdrom->track[track].type == SDL_AUDIO_TRACK) {
				SDL_CDPlayTracks(cdrom, track, 0, 1, 0);
				return;
			}
			
			track++;
			track %= cdrom->numtracks;			
		}
	}
}

void CDDA_PlayLoop(int CDDATrack)
{
	fprintf(stderr, "CDDA_PlayLoop(%d)\n", CDDATrack);
	
	/* can't loop with SDL without a thread, so just play the track */
	CDDA_Play(CDDATrack);
}

void CDDA_Stop()
{
/*
	fprintf(stderr, "CDDA_Stop()\n");
*/
	if (cdrom == NULL)
		return;
	
	if (CD_INDRIVE(SDL_CDStatus(cdrom)))
		SDL_CDStop(cdrom);	
}

void CDDA_SwitchOn()
{
/*
	fprintf(stderr, "CDDA_SwitchOn()\n");
*/	
}

#else

/* AVP Access ------------------------------------------------------------------
  The original game streamed its soundtrack from CD audio. The GOG and Steam
  releases ship those same 15 tracks as Bink files in FMVs/, so "CD playback"
  is now decoding one of those and streaming it to OpenAL.

  Deliberately no looping: the game's own chooser in cdtrackselection.cpp polls
  CDDA_IsPlaying() and moves to the next track for the level when the current
  one ends, which is how track rotation is supposed to work.
  ---------------------------------------------------------------------------*/

#include "access/acc_media.h"

void CheckCDVolume()
{
	AccMedia_SetVolume(CDPlayerVolume);
}

void CDDA_Start()
{
	AccMedia_Init();
}

void CDDA_End()
{
	AccMedia_StopTrack();
}

void CDDA_ChangeVolume(int volume)
{
	CDPlayerVolume = volume;
	AccMedia_SetVolume(volume);
}

int CDDA_CheckNumberOfTracks()
{
	/* CD Tracks.txt assigns tracks 1-15; the files ship with the game. */
	return AccMedia_IsAvailable() ? 15 : 0;
}

int CDDA_IsOn()
{
	/* Gates CheckCDAndChooseTrackIfNeeded(); returning 0 here is what kept the
	   game silent even once the decoder existed. */
	/* Self-initialising: the decoder needs the OpenAL context, which is not up
	   yet the first time this is asked. Returning a flat 0 here would mean the
	   music system never got a second chance. */
	return AccMedia_Init();
}

int CDDA_IsPlaying()
{
	return AccMedia_TrackIsPlaying();
}

void CDDA_Play(int CDDATrack)
{
	AccMedia_PlayTrack(CDDATrack);
}

void CDDA_PlayLoop(int CDDATrack)
{
	AccMedia_PlayTrack(CDDATrack);
}

void CDDA_Stop()
{
	AccMedia_StopTrack();
}

void CDDA_SwitchOn()
{
	AccMedia_Init();
}

#endif
