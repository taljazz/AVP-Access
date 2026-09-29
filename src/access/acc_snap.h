#ifndef ACC_SNAP_H
#define ACC_SNAP_H
/* Explicit, one-shot Marine facing assistance. No movement or firing. */
void AccSnap_Request(unsigned int nowMs);
/* Apply a scoped, explicit yaw request without touching position or velocity. */
void AccSnap_FaceYaw(int yaw);
#endif
