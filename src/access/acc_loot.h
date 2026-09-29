#ifndef ACC_LOOT_H
#define ACC_LOOT_H
#include <stddef.h>
struct strategyblock;
struct vectorch;
/* Read-only usefulness/name query for single-player Marine supplies. */
int AccLoot_Describe(struct strategyblock *object,char *name,size_t size);
int AccLoot_Cycle(void);
/* 1 live selection, 0 none, -1 unavailable/unneeded, -2 confirmed collected. */
int AccLoot_GetTarget(struct vectorch *point,char *name,size_t size);
void AccLoot_PickedUp(struct strategyblock *object);
void AccLoot_Reset(void);
int AccLoot_CloseAndVisible(void);
#endif
