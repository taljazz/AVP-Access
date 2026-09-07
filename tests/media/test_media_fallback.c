/* Link the complete production module without FFmpeg, checking its safe API. */
#include "acc_media.h"
int main(void)
{
    unsigned char pixel[1] = { 0 };
    unsigned char palette[256][3] = { { 0 } };
    if (AccMedia_Init() || AccMedia_IsAvailable() || AccMedia_PlayMenuMusic() ||
        AccMedia_PlayTrack(1) || AccMedia_TrackIsPlaying() ||
        AccMedia_PlotStart(1) || AccMedia_PlotIsPlaying() ||
        AccMedia_PlotFrame(pixel, 1, 1, palette)) return 1;
    AccMedia_StopTrack();
    AccMedia_SetVolume(127);
    AccMedia_Update();
    AccMedia_PlayMovie("fmvs/logos.bik");
    AccMedia_PlotStop();
    AccMedia_Shutdown();
    return 0;
}
