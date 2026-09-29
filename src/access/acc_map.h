#ifndef ACC_MAP_H
#define ACC_MAP_H
#include <stddef.h>
/* Read-only snapshot. Contains proprietary level geometry: keep exports local. */
int AccMap_Export(const char *path, char *error, size_t errorSize);
#endif
