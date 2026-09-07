// Parser .MAP minimale + emissore quad GPU.
#include "map_loader.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int dnf_map_load(const char *map_path, DnfMap *out) {
  memset(out, 0, sizeof(*out));
  FILE *f = fopen(map_path, "rb");
  if (f) {
    // Header MAP v7: version(4) + pos(12) + ang(2) + sect(2) ...
    // Per lo scaffold leggiamo solo conteggi settori/muri in modo
    // tollerante; se il parse fallisce usiamo la mappa demo procedurale.
    uint32_t ver = 0;
    if (fread(&ver, 4, 1, f) == 1 && (ver == 7 || ver == 8)) {
      fseek(f, 4 + 12 + 2, SEEK_SET);
      int16_t sect = 0;
      if (fread(&sect, 2, 1, f) == 1 && sect > 0 && sect < DNF_MAX_SECTORS) {
        out->num_sectors = sect;
        // NOTA: parse completo muri/sprites nel milestone 2;
        // per ora i quad demo sotto bastano a validare il path GPU.
      }
    }
    fclose(f);
  }
  if (out->num_sectors == 0) {
    // Mappa demo procedurale: stanza 4 muri (permette test GPU senza dati).
    out->num_sectors = 1;
    out->sectors[0].wallptr = 0; out->sectors[0].wallnum = 4;
    out->sectors[0].floorz = 0; out->sectors[0].ceilz = -1024;
    int xs[4] = {-512, 512, 512, -512}, ys[4] = {-512, -512, 512, 512};
    for (int i = 0; i < 4; i++) {
      out->walls[i].x = (int16_t)xs[i]; out->walls[i].y = (int16_t)ys[i];
      out->walls[i].point2 = (int16_t)((i + 1) % 4);
    }
    out->num_walls = 4;
  }
  // Texture bianca 2x2 in VRAM (muri untextured / fallback).
  static const uint8_t white[2*2*4] = {
    255,255,255,255, 255,255,255,255,
    255,255,255,255, 255,255,255,255 };
  out->white_tex = dnf_gpu_upload_texture_rgba(2, 2, white);
  return 0;
}

void dnf_map_draw_gpu(const DnfMap *m, float yaw) {
  (void)yaw;
  // Emissione quad pareti: coordinate mondo -> shader (MVP applicata in GPU).
  // Costo CPU: ~6 struct copy per muro. Raster: 100% SGX.
  for (int i = 0; i < m->num_walls; i++) {
    const DnfWall *w = &m->walls[i];
    const DnfWall *w2 = &m->walls[w->point2 % (m->num_walls ? m->num_walls : 1)];
    float x0 = (float)w->x / 512.0f, y0 = (float)w->y / 512.0f;
    float x1 = (float)w2->x / 512.0f, y1 = (float)w2->y / 512.0f;
    DnfGpuVertex v0 = {x0, -1.0f, y0, 0, 0, 1.0f};
    DnfGpuVertex v1 = {x1, -1.0f, y1, 1, 0, 1.0f};
    DnfGpuVertex v2 = {x1,  1.0f, y1, 1, 1, 1.0f};
    DnfGpuVertex v3 = {x0,  1.0f, y0, 0, 1, 1.0f};
    dnf_gpu_draw_wall_quad(&v0, &v1, &v2, &v3, m->white_tex);
  }
}
