#pragma once
// Parser mappe Build (.MAP v7) da zero + upload GPU.
// La CPU legge settori/muri una volta al load e crea VBO.
// Per-frame la CPU fa solo frustum-cull + invio quad già pronti.
#ifndef DNF_MAP_LOADER_H
#define DNF_MAP_LOADER_H

#include <stdint.h>
#include "gpu_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DNF_MAX_SECTORS 1024
#define DNF_MAX_WALLS 8192

typedef struct { int16_t x, y; int16_t point2, nextwall, nextsector; } DnfWall;
typedef struct { int16_t wallptr, wallnum; int16_t floorz, ceilz; uint8_t floorshade; } DnfSector;

typedef struct {
  DnfSector sectors[DNF_MAX_SECTORS];
  DnfWall walls[DNF_MAX_WALLS];
  int num_sectors, num_walls;
  uint32_t white_tex; // texture 2x2 bianca per muri senza art
} DnfMap;

// Carica ux0:data/DNF/_CLIPSHAPE0.MAP (o E1L1 di DNF.GRP) e crea white_tex GPU.
int dnf_map_load(const char *map_path, DnfMap *out);
// Disegna muri visibili come quad GPU (batching interno al renderer).
void dnf_map_draw_gpu(const DnfMap *m, float yaw);

#ifdef __cplusplus
}
#endif

#endif
