/* AVP Access ------------------------------------------------------------------
  Bink / Smacker playback -- see acc_media.h.
  ---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

/* Without FFmpeg the whole module degrades to no-ops, so platforms that lack it
   still build and simply have no music or cutscenes -- exactly the behaviour of
   the stubs this replaces. */
#if defined(ACC_HAVE_FFMPEG)

#include <al.h>
#include <alc.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "files.h"
#include "acc_media.h"

#define ACC_NUM_BUFFERS   6
#define ACC_BUFFER_BYTES  32768
#define ACC_PCM_STAGE     (256 * 1024)
#define ACC_VIDEO_QUEUE   8

typedef struct
{
	/* --- source ------------------------------------------------------- */
	FILE          *fp;
	AVIOContext   *avio;
	AVFormatContext *fmt;
	AVPacket      *pkt;

	/* --- audio -------------------------------------------------------- */
	int              aIndex;
	AVCodecContext  *aDec;
	AVFrame         *aFrame;
	SwrContext      *swr;
	int              outRate;

	unsigned char pcm[ACC_PCM_STAGE];
	int           pcmLen;      /* bytes valid */
	int           pcmPos;      /* bytes consumed */

	ALuint source;
	ALuint buffers[ACC_NUM_BUFFERS];
	ALuint freeBuffers[ACC_NUM_BUFFERS];
	int    numFree;
	int    bufSamples[ACC_NUM_BUFFERS];   /* parallel to buffers[] */
	int64_t samplesRetired;               /* samples in buffers already drained */

	/* --- video -------------------------------------------------------- */
	int               vIndex;
	AVCodecContext   *vDec;
	AVFrame          *vFrame;
	struct SwsContext *sws;
	double            vTimeBase;

	AVFrame *vQueue[ACC_VIDEO_QUEUE];
	double   vPts[ACC_VIDEO_QUEUE];
	int      vCount;

	/* The frame currently on display, held so a pulled consumer (the in-game
	   monitors) can be asked for the picture repeatedly between decodes. */
	AVFrame *vCurrent;

	int dstX, dstY, dstW, dstH;   /* letterbox rect inside the 640x480 surface */

	/* --- state -------------------------------------------------------- */
	int open;
	int eof;
	int started;

	/* A packet the decoder refused (AVERROR(EAGAIN)) because its output queue
	   is full. It is held and retried rather than dropped -- discarding it
	   silently loses picture. */
	int pktPending;
} ACC_STREAM;

static ACC_STREAM Music;
static ACC_STREAM Plot;      /* in-game wall-monitor briefings */
static int MediaReady;
static int MediaFailed;
static int MenuMusicFailed;  /* avoid retrying a missing/bad theme every frame */
static int MusicVolume = 127;   /* game scale, 0..127 */

/* The game's 640x480 RGB565 software surface, and the routine that puts it on
   screen with the right aspect fitting. Reusing them means cutscenes letterbox
   and scale exactly like the menus do. */
extern SDL_Surface *surface;
extern void FlipBuffers(void);
extern void CheckForWindowsMessages(void);
extern void DirectReadKeyboard(void);
extern unsigned char GotAnyKey;   /* defined in win95/io.c; one byte, not an int */

/* ------------------------------------------------------------------ io -- */

/* FFmpeg reads through the game's own file layer rather than a raw path, so
   AVP_DATA, the local/global directory split and case fixing all still apply. */
static int AvioRead(void *opaque, uint8_t *buf, int size)
{
	size_t n = fread(buf, 1, (size_t)size, (FILE *)opaque);

	if (n == 0) return AVERROR_EOF;
	return (int)n;
}

static int64_t AvioSeek(void *opaque, int64_t offset, int whence)
{
	FILE *fp = (FILE *)opaque;

	/* FFmpeg may OR in AVSEEK_FORCE, which is not a valid fseek origin. */
	whence &= ~AVSEEK_FORCE;

	if (whence == AVSEEK_SIZE) {
		long cur = ftell(fp);
		long end;

		if (cur < 0 || fseek(fp, 0, SEEK_END) != 0) return -1;
		end = ftell(fp);
		fseek(fp, cur, SEEK_SET);
		return (int64_t)end;
	}

	if (fseek(fp, (long)offset, whence) != 0) return -1;
	return (int64_t)ftell(fp);
}

/* ------------------------------------------------------------ lifecycle -- */

static void VideoQueueClear(ACC_STREAM *s)
{
	int i;
	for (i = 0; i < s->vCount; i++) {
		if (s->vQueue[i]) av_frame_free(&s->vQueue[i]);
	}
	s->vCount = 0;
}

static void StreamClose(ACC_STREAM *s)
{
	if (!s->open) {
		/* A half-built stream can still own the file and AVIO. */
		if (s->avio) {
			if (s->avio->buffer) av_freep(&s->avio->buffer);
			avio_context_free(&s->avio);
		}
		if (s->fp) { CloseGameFile(s->fp); s->fp = NULL; }
		return;
	}

	if (s->source) {
		alSourceStop(s->source);
		/* Detaching the whole queue is the only reliable way to get every
		   buffer back before they are reused. */
		alSourcei(s->source, AL_BUFFER, 0);
	}

	VideoQueueClear(s);
	if (s->vCurrent) av_frame_free(&s->vCurrent);

	if (s->sws)    { sws_freeContext(s->sws); s->sws = NULL; }
	if (s->swr)    swr_free(&s->swr);
	if (s->aFrame) av_frame_free(&s->aFrame);
	if (s->vFrame) av_frame_free(&s->vFrame);
	if (s->pkt)    av_packet_free(&s->pkt);
	if (s->aDec)   avcodec_free_context(&s->aDec);
	if (s->vDec)   avcodec_free_context(&s->vDec);

	if (s->fmt) avformat_close_input(&s->fmt);   /* leaves a custom pb alone */

	if (s->avio) {
		if (s->avio->buffer) av_freep(&s->avio->buffer);
		avio_context_free(&s->avio);
	}

	if (s->fp) { CloseGameFile(s->fp); s->fp = NULL; }

	s->open = 0;
	s->eof = 0;
	s->started = 0;
	s->pktPending = 0;
	s->pcmLen = s->pcmPos = 0;
	s->samplesRetired = 0;
	s->aIndex = s->vIndex = -1;

	s->numFree = ACC_NUM_BUFFERS;
	{
		int i;
		for (i = 0; i < ACC_NUM_BUFFERS; i++) {
			s->freeBuffers[i] = s->buffers[i];
			s->bufSamples[i] = 0;
		}
	}
}

static int OpenDecoder(ACC_STREAM *s, enum AVMediaType type,
                       int *indexOut, AVCodecContext **decOut)
{
	const AVCodec *codec = NULL;
	AVCodecContext *dec;
	int idx = av_find_best_stream(s->fmt, type, -1, -1, &codec, 0);

	*indexOut = -1;
	*decOut = NULL;

	if (idx < 0 || !codec) return 0;

	dec = avcodec_alloc_context3(codec);
	if (!dec) return 0;

	if (avcodec_parameters_to_context(dec, s->fmt->streams[idx]->codecpar) < 0 ||
	    avcodec_open2(dec, codec, NULL) < 0) {
		avcodec_free_context(&dec);
		return 0;
	}

	*indexOut = idx;
	*decOut = dec;
	return 1;
}

/* wantVideo lets the music path skip the video decoder entirely. */
static int StreamOpen(ACC_STREAM *s, const char *filename, int wantVideo)
{
	StreamClose(s);

	s->aIndex = s->vIndex = -1;

	s->fp = OpenGameFile(filename, FILEMODE_READONLY, FILETYPE_OPTIONAL);
	if (!s->fp) return 0;

	s->avio = avio_alloc_context((unsigned char *)av_malloc(4096), 4096, 0,
	                             s->fp, AvioRead, NULL, AvioSeek);
	if (!s->avio) { CloseGameFile(s->fp); s->fp = NULL; return 0; }

	s->fmt = avformat_alloc_context();
	if (!s->fmt) { StreamClose(s); return 0; }
	s->fmt->pb = s->avio;
	s->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

	if (avformat_open_input(&s->fmt, NULL, NULL, NULL) < 0) {
		s->fmt = NULL;               /* open_input freed it */
		StreamClose(s);
		return 0;
	}

	s->open = 1;                     /* StreamClose owns everything from here */

	if (avformat_find_stream_info(s->fmt, NULL) < 0) { StreamClose(s); return 0; }

	s->pkt = av_packet_alloc();
	if (!s->pkt) { StreamClose(s); return 0; }

	/* --- audio --- */
	if (OpenDecoder(s, AVMEDIA_TYPE_AUDIO, &s->aIndex, &s->aDec)) {
		AVChannelLayout outLayout;
		int err;

		s->aFrame = av_frame_alloc();
		if (!s->aFrame) { StreamClose(s); return 0; }

		/* OpenAL takes interleaved 16-bit; forcing stereo gives one code path
		   for the mono Smacker files and the stereo Bink ones alike. The
		   sample rate is left alone so nothing is resampled needlessly. */
		s->outRate = s->aDec->sample_rate > 0 ? s->aDec->sample_rate : 44100;
		av_channel_layout_default(&outLayout, 2);

		err = swr_alloc_set_opts2(&s->swr,
		                          &outLayout, AV_SAMPLE_FMT_S16, s->outRate,
		                          &s->aDec->ch_layout, s->aDec->sample_fmt,
		                          s->aDec->sample_rate, 0, NULL);
		av_channel_layout_uninit(&outLayout);

		if (err < 0 || !s->swr || swr_init(s->swr) < 0) { StreamClose(s); return 0; }
	}

	/* --- video --- */
	if (wantVideo && OpenDecoder(s, AVMEDIA_TYPE_VIDEO, &s->vIndex, &s->vDec)) {
		AVRational tb = s->fmt->streams[s->vIndex]->time_base;

		s->vFrame = av_frame_alloc();
		if (!s->vFrame) { StreamClose(s); return 0; }

		s->vTimeBase = (tb.den > 0) ? ((double)tb.num / (double)tb.den) : 0.0;
	}

	if (s->aIndex < 0 && s->vIndex < 0) { StreamClose(s); return 0; }

	return 1;
}

/* --------------------------------------------------------------- decode -- */

static void DrainAudio(ACC_STREAM *s)
{
	while (avcodec_receive_frame(s->aDec, s->aFrame) == 0) {
		int room = (ACC_PCM_STAGE - s->pcmLen) / 4;    /* stereo S16 */
		uint8_t *outPtr = s->pcm + s->pcmLen;
		int converted;

		if (room > 0) {
			converted = swr_convert(s->swr, &outPtr, room,
			                        (const uint8_t **)s->aFrame->extended_data,
			                        s->aFrame->nb_samples);
			if (converted > 0) s->pcmLen += converted * 4;
		}
		av_frame_unref(s->aFrame);
	}
}

static void DrainVideo(ACC_STREAM *s)
{
	while (s->vCount < ACC_VIDEO_QUEUE &&
	       avcodec_receive_frame(s->vDec, s->vFrame) == 0) {
		AVFrame *held = av_frame_alloc();

		if (held) {
			int64_t pts = s->vFrame->best_effort_timestamp;

			av_frame_move_ref(held, s->vFrame);

			s->vPts[s->vCount] = (pts == AV_NOPTS_VALUE)
			                   ? -1.0
			                   : (double)pts * s->vTimeBase;
			s->vQueue[s->vCount] = held;
			s->vCount++;
		}
		av_frame_unref(s->vFrame);
	}
}

/* Hands the packet currently held in s->pkt to its decoder. Returns 1 if it was
   consumed, 0 if the decoder is backed up and the packet must be retried. */
static int SendPendingPacket(ACC_STREAM *s)
{
	int ret = 0;

	if (s->aDec && s->pkt->stream_index == s->aIndex) {
		ret = avcodec_send_packet(s->aDec, s->pkt);
		if (ret == AVERROR(EAGAIN)) {
			DrainAudio(s);                                   /* make room */
			ret = avcodec_send_packet(s->aDec, s->pkt);
		}
		if (ret == AVERROR(EAGAIN)) return 0;
		DrainAudio(s);
	} else if (s->vDec && s->pkt->stream_index == s->vIndex) {
		ret = avcodec_send_packet(s->vDec, s->pkt);
		if (ret == AVERROR(EAGAIN)) {
			DrainVideo(s);
			ret = avcodec_send_packet(s->vDec, s->pkt);
		}
		/* Still refused means the frame queue is full and nothing can be
		   drained until the consumer catches up. Keep the packet. */
		if (ret == AVERROR(EAGAIN)) return 0;
		DrainVideo(s);
	}

	av_packet_unref(s->pkt);
	s->pktPending = 0;
	return 1;
}

/* Advances demuxing by one packet. Returns 0 when nothing more can be done
   right now -- either the file is exhausted, or a decoder is backed up and the
   caller must consume some output first. Callers must treat 0 as "stop", or
   they will spin. */
static int DecodeStep(ACC_STREAM *s)
{
	if (s->pktPending) {
		if (!SendPendingPacket(s)) return 0;
		return 1;
	}

	if (s->eof) return 0;

	if (av_read_frame(s->fmt, s->pkt) < 0) {
		if (s->aDec) { avcodec_send_packet(s->aDec, NULL); DrainAudio(s); }
		if (s->vDec) { avcodec_send_packet(s->vDec, NULL); DrainVideo(s); }
		s->eof = 1;
		return 0;
	}

	s->pktPending = 1;
	SendPendingPacket(s);
	return 1;
}

/* Pulls up to maxBytes of PCM, demuxing as required. 0 means exhausted. */
static int TakePCM(ACC_STREAM *s, unsigned char *out, int maxBytes)
{
	int avail;

	while ((s->pcmLen - s->pcmPos) < maxBytes && !s->eof) {
		if (!DecodeStep(s)) break;
	}

	avail = s->pcmLen - s->pcmPos;
	if (avail <= 0) return 0;
	if (avail > maxBytes) avail = maxBytes;

	memcpy(out, s->pcm + s->pcmPos, (size_t)avail);
	s->pcmPos += avail;

	if (s->pcmPos >= s->pcmLen) {
		s->pcmPos = s->pcmLen = 0;
	} else if (s->pcmPos > ACC_PCM_STAGE / 2) {
		memmove(s->pcm, s->pcm + s->pcmPos, (size_t)(s->pcmLen - s->pcmPos));
		s->pcmLen -= s->pcmPos;
		s->pcmPos = 0;
	}

	return avail;
}

/* ---------------------------------------------------------------- audio -- */

static int BufferSlot(ACC_STREAM *s, ALuint b)
{
	int i;
	for (i = 0; i < ACC_NUM_BUFFERS; i++)
		if (s->buffers[i] == b) return i;
	return -1;
}

static void StreamPump(ACC_STREAM *s)
{
	unsigned char chunk[ACC_BUFFER_BYTES];
	ALint processed = 0, state = 0;

	if (!s->open || !s->source || !s->aDec) return;

	alGetSourcei(s->source, AL_BUFFERS_PROCESSED, &processed);
	while (processed-- > 0) {
		ALuint b = 0;
		int slot;

		alSourceUnqueueBuffers(s->source, 1, &b);
		if (!b) break;

		slot = BufferSlot(s, b);
		if (slot >= 0) {
			s->samplesRetired += s->bufSamples[slot];
			s->bufSamples[slot] = 0;
		}
		if (s->numFree < ACC_NUM_BUFFERS) s->freeBuffers[s->numFree++] = b;
	}

	while (s->numFree > 0) {
		int bytes = TakePCM(s, chunk, ACC_BUFFER_BYTES);
		ALuint b;
		int slot;

		if (bytes <= 0) break;

		b = s->freeBuffers[--s->numFree];
		alBufferData(b, AL_FORMAT_STEREO16, chunk, bytes, s->outRate);
		alSourceQueueBuffers(s->source, 1, &b);

		slot = BufferSlot(s, b);
		if (slot >= 0) s->bufSamples[slot] = bytes / 4;
	}

	alGetSourcei(s->source, AL_SOURCE_STATE, &state);

	if (state != AL_PLAYING) {
		ALint queued = 0;
		alGetSourcei(s->source, AL_BUFFERS_QUEUED, &queued);

		if (queued > 0) {
			alSourcePlay(s->source);
			s->started = 1;
		}
	}
}

static int StreamAudioActive(ACC_STREAM *s)
{
	ALint state = 0, queued = 0;

	if (!s->open || !s->source || !s->aDec) return 0;
	if (!s->started) return 1;

	alGetSourcei(s->source, AL_SOURCE_STATE, &state);
	alGetSourcei(s->source, AL_BUFFERS_QUEUED, &queued);

	return (state == AL_PLAYING) || queued > 0;
}

static double AudioClock(ACC_STREAM *s)
{
	ALint offset = 0;

	if (!s->source || s->outRate <= 0) return 0.0;

	alGetSourcei(s->source, AL_SAMPLE_OFFSET, &offset);
	return (double)(s->samplesRetired + offset) / (double)s->outRate;
}

static void StreamApplyVolume(ACC_STREAM *s)
{
	if (s->source) alSourcef(s->source, AL_GAIN, (float)MusicVolume / 127.0f);
}

static int StreamCreateSource(ACC_STREAM *s)
{
	int i;

	if (s->source) return 1;

	alGetError();
	alGenSources(1, &s->source);
	if (alGetError() != AL_NO_ERROR) { s->source = 0; return 0; }

	alGenBuffers(ACC_NUM_BUFFERS, s->buffers);
	if (alGetError() != AL_NO_ERROR) {
		alDeleteSources(1, &s->source);
		s->source = 0;
		return 0;
	}

	s->numFree = ACC_NUM_BUFFERS;
	for (i = 0; i < ACC_NUM_BUFFERS; i++) {
		s->freeBuffers[i] = s->buffers[i];
		s->bufSamples[i] = 0;
	}

	/* Music and speech must not be positioned in the world, or they would fade
	   as the player turns. Anchor them to the listener. */
	alSourcei(s->source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(s->source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcef(s->source, AL_ROLLOFF_FACTOR, 0.0f);
	StreamApplyVolume(s);

	return 1;
}

/* ---------------------------------------------------------------- video -- */

/* Fits the source frame inside the 640x480 virtual screen without distorting
   it -- the cutscenes are 640x360, so they letterbox with bars top and bottom. */
static int SetupVideoScaler(ACC_STREAM *s)
{
	int sw, sh, dw, dh;

	if (!s->vDec || !surface) return 0;

	sw = s->vDec->width;
	sh = s->vDec->height;
	if (sw <= 0 || sh <= 0) return 0;

	dw = surface->w;
	dh = sh * surface->w / sw;

	if (dh > surface->h) {
		dh = surface->h;
		dw = sw * surface->h / sh;
	}

	s->dstW = dw;
	s->dstH = dh;
	s->dstX = (surface->w - dw) / 2;
	s->dstY = (surface->h - dh) / 2;

	s->sws = sws_getContext(sw, sh, s->vDec->pix_fmt,
	                        dw, dh, AV_PIX_FMT_RGB565LE,
	                        SWS_BILINEAR, NULL, NULL, NULL);

	return s->sws != NULL;
}

static void PresentFrame(ACC_STREAM *s, AVFrame *f)
{
	uint8_t *dst[4] = { NULL, NULL, NULL, NULL };
	int dstStride[4] = { 0, 0, 0, 0 };

	if (!s->sws || !surface || !surface->pixels) return;

	dst[0] = (uint8_t *)surface->pixels
	       + (size_t)s->dstY * surface->pitch
	       + (size_t)s->dstX * 2;
	dstStride[0] = surface->pitch;

	sws_scale(s->sws, (const uint8_t * const *)f->data, f->linesize,
	          0, s->vDec->height, dst, dstStride);

	FlipBuffers();
}

static void ClearSurface(void)
{
	if (surface && surface->pixels)
		memset(surface->pixels, 0, (size_t)surface->pitch * surface->h);
}

/* ----------------------------------------------------------------- init -- */

int AccMedia_Init(void)
{
	if (MediaReady)  return 1;
	if (MediaFailed) return 0;

	/* Needs the game's OpenAL context to exist already. */
	if (!alcGetCurrentContext()) return 0;

	if (!StreamCreateSource(&Music)) { MediaFailed = 1; return 0; }

	if (StreamCreateSource(&Plot)) {
		/* Briefing speech is content, not background: it should not be scaled
		   down by the music volume slider. */
		alSourcef(Plot.source, AL_GAIN, 1.0f);
	}

	MediaReady = 1;
	return 1;
}

void AccMedia_Shutdown(void)
{
	if (!MediaReady) return;

	StreamClose(&Music);
	StreamClose(&Plot);

	if (Music.source) {
		alDeleteSources(1, &Music.source);
		alDeleteBuffers(ACC_NUM_BUFFERS, Music.buffers);
		Music.source = 0;
	}

	if (Plot.source) {
		alDeleteSources(1, &Plot.source);
		alDeleteBuffers(ACC_NUM_BUFFERS, Plot.buffers);
		Plot.source = 0;
	}

	MediaReady = 0;
	MenuMusicFailed = 0;
}

int AccMedia_IsAvailable(void) { return MediaReady; }

/* ---------------------------------------------------------------- music -- */

/* Track files are named "NN Something.bik". Match on the number rather than the
   full name so a differently-titled release still works. */
static int FindTrackFile(int track, char *out, size_t cap)
{
	void *dir;
	GameDirectoryFile *entry;
	char prefix[8];
	int found = 0;

	if (track < 1) return 0;

	snprintf(prefix, sizeof(prefix), "%02d ", track);

	dir = OpenGameDirectory("fmvs", "*.bik", FILETYPE_OPTIONAL);
	if (!dir) return 0;

	while ((entry = ScanGameDirectory(dir)) != NULL) {
		if (entry->filename && strncmp(entry->filename, prefix, 3) == 0) {
			snprintf(out, cap, "fmvs/%s", entry->filename);
			found = 1;
			break;
		}
	}

	CloseGameDirectory(dir);
	return found;
}

/* The title/menu theme is separate from the numbered level soundtrack. Share
   the music source so leaving the menus stops it before level music begins. */
int AccMedia_PlayMenuMusic(void)
{
	static const char path[] = "fmvs/introsound.smk";

	if (MenuMusicFailed || !AccMedia_Init()) return 0;

	/* This Smacker file contains a dummy picture; only decode its audio. */
	if (!StreamOpen(&Music, path, 0)) {
		MenuMusicFailed = 1;
		fprintf(stderr, "AVP Access: could not decode menu music %s\n", path);
		return 0;
	}

	StreamApplyVolume(&Music);
	StreamPump(&Music);
	fprintf(stderr, "AVP Access: menu music %s\n", path);
	return 1;
}

int AccMedia_PlayTrack(int track)
{
	char path[256];

	if (!AccMedia_Init()) return 0;

	if (!FindTrackFile(track, path, sizeof(path))) {
		fprintf(stderr, "AVP Access: no music file for track %d in fmvs/\n", track);
		return 0;
	}

	/* Music wants the audio stream only; decoding the video would be pure waste. */
	if (!StreamOpen(&Music, path, 0)) {
		fprintf(stderr, "AVP Access: could not decode %s\n", path);
		return 0;
	}

	StreamApplyVolume(&Music);
	StreamPump(&Music);

	return 1;
}

void AccMedia_StopTrack(void)
{
	if (!MediaReady) return;
	StreamClose(&Music);
}

int AccMedia_TrackIsPlaying(void)
{
	if (!MediaReady) return 0;
	if (!Music.open) return 0;

	/* Once the file has run out and OpenAL has drained, let go of it so the
	   game's own chooser sees the track stop and moves to the next. */
	if (Music.eof && Music.started && !StreamAudioActive(&Music)) {
		StreamClose(&Music);
		return 0;
	}

	return StreamAudioActive(&Music);
}

void AccMedia_SetVolume(int volume)
{
	if (volume < 0)   volume = 0;
	if (volume > 127) volume = 127;

	MusicVolume = volume;
	if (MediaReady) StreamApplyVolume(&Music);
}

void AccMedia_Update(void)
{
	if (!MediaReady) return;
	StreamPump(&Music);
	StreamPump(&Plot);
}

/* ------------------------------------------------------- plot messages -- */

int AccMedia_PlotStart(int messageNumber)
{
	char path[256];

	if (!AccMedia_Init()) return 0;

	StreamClose(&Plot);

	snprintf(path, sizeof(path), "fmvs/message%d.smk", messageNumber);

	if (!StreamOpen(&Plot, path, 1)) {
		/* Not every trigger has a matching file; that is normal, not an error. */
		return 0;
	}

	alSourcef(Plot.source, AL_GAIN, 1.0f);
	StreamPump(&Plot);

	fprintf(stderr, "AVP Access: plot message %d\n", messageNumber);
	return 1;
}

void AccMedia_PlotStop(void)
{
	if (!MediaReady) return;
	StreamClose(&Plot);
}

int AccMedia_PlotIsPlaying(void)
{
	if (!MediaReady || !Plot.open) return 0;

	/* Keep showing the last frame until the soundtrack has finished, then let
	   go so the screen can fall back to static. */
	if (Plot.aDec) {
		if (Plot.eof && Plot.started && !StreamAudioActive(&Plot)) {
			StreamClose(&Plot);
			return 0;
		}
		return 1;
	}

	if (Plot.eof && Plot.vCount == 0 && !Plot.vCurrent) {
		StreamClose(&Plot);
		return 0;
	}

	return 1;
}

int AccMedia_PlotFrame(unsigned char *indices, int w, int h,
                       unsigned char palette[256][3])
{
	double clock;
	AVFrame *f;
	int y;

	if (!MediaReady || !Plot.open || !Plot.vDec || !indices || !palette) return 0;
	if (w <= 0 || h <= 0) return 0;

	clock = Plot.aDec ? AudioClock(&Plot) : 0.0;

	/* Keep the queue topped up, then advance to the frame due now. Without audio
	   there is no clock to follow, so take whatever has been decoded.
	   DecodeStep returning 0 means blocked or finished -- looping on it would
	   spin forever. */
	while (Plot.vCount < ACC_VIDEO_QUEUE / 2 && !Plot.eof) {
		if (!DecodeStep(&Plot)) break;
	}

	while (Plot.vCount > 0 &&
	       (!Plot.aDec || Plot.vPts[0] < 0.0 || Plot.vPts[0] <= clock)) {
		int i;

		if (Plot.vCurrent) av_frame_free(&Plot.vCurrent);
		Plot.vCurrent = Plot.vQueue[0];

		for (i = 1; i < Plot.vCount; i++) {
			Plot.vQueue[i - 1] = Plot.vQueue[i];
			Plot.vPts[i - 1]   = Plot.vPts[i];
		}
		Plot.vCount--;

		if (!Plot.eof && Plot.vCount == 0) DecodeStep(&Plot);

		/* With no audio clock, one frame per call is the right pace. */
		if (!Plot.aDec) break;
	}

	f = Plot.vCurrent;
	if (!f || !f->data[0]) return 0;

	/* Smacker is natively 8-bit paletted, which is exactly what the engine's
	   monitor texture wants -- copy the indices row by row to honour the
	   decoder's stride. */
	{
		int copyW = (f->width  < w) ? f->width  : w;
		int copyH = (f->height < h) ? f->height : h;

		memset(indices, 0, (size_t)w * h);

		for (y = 0; y < copyH; y++)
			memcpy(indices + (size_t)y * w,
			       f->data[0] + (size_t)y * f->linesize[0],
			       (size_t)copyW);
	}

	/* PAL8 carries its colour table in data[1] as 256 native-endian words
	   laid out 0xAARRGGBB. */
	if (f->data[1]) {
		const uint32_t *pal = (const uint32_t *)f->data[1];
		int i;

		for (i = 0; i < 256; i++) {
			palette[i][0] = (unsigned char)((pal[i] >> 16) & 0xFF);
			palette[i][1] = (unsigned char)((pal[i] >>  8) & 0xFF);
			palette[i][2] = (unsigned char)( pal[i]        & 0xFF);
		}
	}

	return 1;
}

/* ------------------------------------------------------------ cutscenes -- */

void AccMedia_PlayMovie(const char *filename)
{
	ACC_STREAM movie;
	int guard = 0;
	int hasVideo;

	if (!filename || !filename[0]) return;
	if (!AccMedia_Init()) return;

	memset(&movie, 0, sizeof(movie));
	movie.aIndex = movie.vIndex = -1;

	if (!StreamCreateSource(&movie)) return;

	if (!StreamOpen(&movie, filename, 1)) {
		fprintf(stderr, "AVP Access: could not play %s\n", filename);
		alDeleteSources(1, &movie.source);
		alDeleteBuffers(ACC_NUM_BUFFERS, movie.buffers);
		return;
	}

	/* Cutscene dialogue should not be scaled by the music volume slider. */
	alSourcef(movie.source, AL_GAIN, 1.0f);

	hasVideo = (movie.vDec != NULL) && SetupVideoScaler(&movie);
	if (hasVideo) ClearSurface();

	fprintf(stderr, "AVP Access: playing %s (%s)\n", filename,
	        hasVideo ? "video and audio" : "audio only");

	GotAnyKey = 0;

	/* The guard stops a malformed file wedging the game; at ~5ms a pass this is
	   far longer than any cutscene here. */
	while (guard++ < 400000) {
		double clock;
		int stillPlaying;

		StreamPump(&movie);

		/* Audio is the master clock when present, so speech stays in sync with
		   the picture; with no audio, fall back to the frame's own timestamp
		   paced by real time. */
		clock = (movie.aDec ? AudioClock(&movie)
		                    : (double)guard * 0.005);

		if (hasVideo) {
			AVFrame *show = NULL;

			/* Keep the queue topped up; DecodeStep returning 0 means blocked
			   or finished, so stop rather than spin. */
			while (movie.vCount < ACC_VIDEO_QUEUE / 2 && !movie.eof) {
				if (!DecodeStep(&movie)) break;
			}

			while (movie.vCount > 0 &&
			       (movie.vPts[0] < 0.0 || movie.vPts[0] <= clock)) {
				int i;

				if (show) av_frame_free(&show);
				show = movie.vQueue[0];

				for (i = 1; i < movie.vCount; i++) {
					movie.vQueue[i - 1] = movie.vQueue[i];
					movie.vPts[i - 1]   = movie.vPts[i];
				}
				movie.vCount--;

				if (!movie.eof && movie.vCount == 0) DecodeStep(&movie);
			}

			if (show) {
				PresentFrame(&movie, show);
				av_frame_free(&show);
			}
		}

		CheckForWindowsMessages();
		DirectReadKeyboard();
		if (GotAnyKey) break;

		stillPlaying = movie.aDec ? StreamAudioActive(&movie)
		                          : (!movie.eof || movie.vCount > 0);
		if (!stillPlaying) break;

		SDL_Delay(5);
	}

	StreamClose(&movie);
	alDeleteSources(1, &movie.source);
	alDeleteBuffers(ACC_NUM_BUFFERS, movie.buffers);

	if (hasVideo) {
		ClearSurface();
		FlipBuffers();
	}
}

#else /* no ACC_HAVE_FFMPEG ------------------------------------------------- */

#include "acc_media.h"

int  AccMedia_Init(void)          { return 0; }
void AccMedia_Shutdown(void)      { }
int  AccMedia_IsAvailable(void)   { return 0; }
int  AccMedia_PlayMenuMusic(void) { return 0; }
int  AccMedia_PlayTrack(int track){ (void)track; return 0; }
void AccMedia_StopTrack(void)     { }
int  AccMedia_TrackIsPlaying(void){ return 0; }
void AccMedia_SetVolume(int v)    { (void)v; }
void AccMedia_Update(void)        { }
void AccMedia_PlayMovie(const char *filename) { (void)filename; }

int  AccMedia_PlotStart(int n)   { (void)n; return 0; }
void AccMedia_PlotStop(void)     { }
int  AccMedia_PlotIsPlaying(void){ return 0; }
int  AccMedia_PlotFrame(unsigned char *indices, int w, int h,
                        unsigned char palette[256][3])
{
	(void)indices; (void)w; (void)h; (void)palette;
	return 0;
}

#endif /* ACC_HAVE_FFMPEG */
