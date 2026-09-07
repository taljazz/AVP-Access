/* Optional guided listening diagnostic; never entered by ordinary gameplay. */
#ifndef ACC_TRACKER_TEST_H
#define ACC_TRACKER_TEST_H
#ifdef __cplusplus
extern "C" {
#endif
/* Returns a process exit code: zero for completion/cancel, one for audio failure. */
int AccTracker_RunListeningTest(void);
#ifdef __cplusplus
}
#endif
#endif
