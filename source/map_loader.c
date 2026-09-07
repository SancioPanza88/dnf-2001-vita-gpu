// Parser .MAP minimale + emissore quad GPU.
#include "map_loader.h"
#include "art_loader.h"
#include <stdio.h>
#include <stdlib.h>
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
  // M1b: texture muraria reale da DNF.GRP (tile ART + palette),
  // fallback checker se i dati mancano. Upload UNA volta in VRAM.
  {
    int tw = 0, th = 0;
    uint8_t *rgba = NULL;
    if (dnf_art_load_wall_rgba("ux0:data/DNF/DNF.GRP", &tw, &th, &rgba) == 0) {
      out->white_tex = dnf_gpu_upload_texture_rgba(tw, th, rgba);
      free(rgba);
    } else {
      static uint8_t checker[64 * 64 * 4];
      for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
          int c = (((x / 8) + (y / 8)) & 1) ? 200 : 90;
          int edge = (x < 2 || y < 2) ? 1 : 0;
          uint8_t r = edge ? 255 : (uint8_t)c;
          uint8_t g = edge ? 128 : (uint8_t)(c * 0.6f);
          uint8_t b = edge ? 0 : (uint8_t)(c * 0.3f);
          checker[(y * 64 + x) * 4 + 0] = r;
          checker[(y * 64 + x) * 4 + 1] = g;
          checker[(y * 64 + x) * 4 + 2] = b;
          checker[(y * 64 + x) * 4 + 3] = 255;
        }
      }
      out->white_tex = dnf_gpu_upload_texture_rgba(64, 64, checker);
    }
  }
  return 0;
}

void dnf_map_set_texture(DnfMap *m, uint32_t tex) {
  m->white_tex = tex;
}

void dnf_map_draw_gpu(const DnfMap *m, float yaw) {
  (void)yaw;
  // M1: stanza world-space 8x8, altezza 0..3, camera a y=1.5.
  // x = destra, z = profondità. UV: u = lunghezza muro (repeat), v = 0..1.
  for (int i = 0; i < m->num_walls; i++) {
    const DnfWall *w = &m->walls[i];
    const DnfWall *w2 = &m->walls[w->point2 % (m->num_walls ? m->num_walls : 1)];
    float x0 = (float)w->x / 512.0f * 4.0f, z0 = (float)w->y / 512.0f * 4.0f;
    float x1 = (float)w2->x / 512.0f * 4.0f, z1 = (float)w2->y / 512.0f * 4.0f;
    float dx = x1 - x0, dz = z1 - z0;
    float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.01f) len = 1.0f;
    DnfGpuVertex v0 = {x0, 0.0f, z0, 0, 0, 1.0f};
    DnfGpuVertex v1 = {x1, 0.0f, z1, len * 0.5f, 0, 1.0f};
    DnfGpuVertex v2 = {x1, 3.0f, z1, len * 0.5f, 1, 1.0f};
    DnfGpuVertex v3 = {x0, 3.0f, z0, 0, 1, 1.0f};
    dnf_gpu_draw_wall_quad(&v0, &v1, &v2, &v3, m->white_tex);
  }
  // Pavimento: quad texturizzato a y=0 (stessa checker, repeat 4x4).
  {
    DnfGpuVertex f0 = {-4.0f, 0.0f, -4.0f, 0, 0, 0.7f};
    DnfGpuVertex f1 = { 4.0f, 0.0f, -4.0f, 4, 0, 0.7f};
    DnfGpuVertex f2 = { 4.0f, 0.0f,  4.0f, 4, 4, 0.7f};
    DnfGpuVertex f3 = {-4.0f, 0.0f,  4.0f, 0, 4, 0.7f};
    dnf_gpu_draw_wall_quad(&f0, &f1, &f2, &f3, m->white_tex);
  }
}
