#ifndef ACC_COMBAT_H
#define ACC_COMBAT_H
struct strategyblock;
struct vectorch;
/* Revalidate the retained target against live identity, range and visibility. */
int AccCombat_GetSnapTarget(struct vectorch *point);
/* Called while continuous Marine guidance is enabled. Returns nonzero while
   combat owns guidance, including the short lost-sight grace period. */
int AccCombat_Update(unsigned int nowMs);
void AccCombat_Reset(void);
/* Single-player Marine policy, separate from weapon smart-target filters. */
const char *AccCombat_HostileName(struct strategyblock *candidate);
#endif
