// M1b: parser ART v1 + PALETTE.DAT da zero (formato KenSilverman/Build).
// Layout ART: int32 artversion(1) + numtiles + tilestart + tileend,
// poi int16 tilesizx[N] + int16 tilesizy[N] + int32 picanm[N],
// poi pixel grezzi 8-bit concatenati (tilesizx*tilesizy byte per tile).
#include "art_loader.h"
#include "grp_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_art_ext(const char *name) {
  size_t n = strlen(name);
  if (n < 4) return 0;
  const char *e = name + n - 4;
  return (e[0] == '.' &&
          (e[1] == 'A' || e[1] == 'a') &&
          (e[2] == 'R' || e[2] == 'r') &&
          (e[3] == 'T' || e[3] == 't'));
}

static int name_is_palette(const char *name) {
  return strcasecmp(name, "PALETTE.DAT") == 0;
}

int dnf_art_load_wall_rgba(const char *grp_path,
                           int *out_w, int *out_h, uint8_t **out_rgba) {
  *out_w = 0; *out_h = 0; *out_rgba = NULL;

  // M1c: indice in statica (96KB) — sullo stack overflowava il main thread.
  static DnfGrpIndex idx;
  memset(&idx, 0, sizeof(idx));
  if (dnf_grp_index(grp_path, &idx) != 0) return -1;

  // 1. Palette 0 (primi 768 byte di PALETTE.DAT).
  uint8_t pal[768];
  {
    const DnfGrpEntry *pe = NULL;
    for (int i = 0; i < idx.count; i++)
      if (name_is_palette(idx.entries[i].name)) { pe = &idx.entries[i]; break; }
    if (!pe || pe->size < 768) return -2;
    uint8_t *pbuf = NULL;
    if (dnf_grp_read_lump(grp_path, pe, &pbuf) < 0) return -3;
    memcpy(pal, pbuf, 768);
    free(pbuf);
  }

  // 2. Primo lump *.ART in ordine alfabetico (== TILES000.ART di solito).
  const DnfGrpEntry *ae = NULL;
  for (int i = 0; i < idx.count; i++) {
    if (!has_art_ext(idx.entries[i].name)) continue;
    if (!ae || strcasecmp(idx.entries[i].name, ae->name) < 0)
      ae = &idx.entries[i];
  }
  if (!ae) return -4;

  uint8_t *abuf = NULL;
  int asize = dnf_grp_read_lump(grp_path, ae, &abuf);
  if (asize < 16) { free(abuf); return -5; }

  int32_t ver, num, tstart, tend;
  memcpy(&ver, abuf, 4); memcpy(&num, abuf + 4, 4);
  memcpy(&tstart, abuf + 8, 4); memcpy(&tend, abuf + 12, 4);
  if (ver != 1 || num <= 0 || num > 4096) { free(abuf); return -6; }
  if (tstart < 0 || tend >= num || tend < tstart) { free(abuf); return -7; }

  int32_t off = 16;
  int16_t *sx = (int16_t *)(abuf + off); off += num * 2;
  int16_t *sy = (int16_t *)(abuf + off); off += num * 2;
  off += num * 4; // picanm
  if (off > asize) { free(abuf); return -8; }

  // 3. Scegli tile murario: primo 64x64, altrimenti primo >=32x32, altrimenti tstart.
  int pick = -1, fallback = -1;
  for (int t = tstart; t <= tend; t++) {
    int w = sx[t], h = sy[t];
    if (w <= 0 || h <= 0) continue;
    if (fallback < 0 && w >= 16 && h >= 16) fallback = t;
    if (w == 64 && h == 64) { pick = t; break; }
    if (pick < 0 && w >= 32 && h >= 32 && w <= 128 && h <= 128) pick = t;
  }
  if (pick < 0) pick = fallback;
  if (pick < 0) pick = tstart;
  int w = sx[pick], h = sy[pick];
  if (w <= 0 || h <= 0 || w > 256 || h > 256) { free(abuf); return -9; }

  // Offset dati del tile pick (somma dei tile precedenti).
  int32_t data = off;
  for (int t = tstart; t < pick; t++) {
    int tw = sx[t], th = sy[t];
    if (tw > 0 && th > 0) data += tw * th;
  }
  if (data + w * h > asize) { free(abuf); return -10; }

  // 4. Espandi indici -> RGBA (una volta al load, mai per-frame).
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  if (!rgba) { free(abuf); return -11; }
  uint8_t *src = abuf + data;
  for (int i = 0; i < w * h; i++) {
    // Indice 255 = trasparente in Build: lo rendiamo magenta per vederlo.
    uint8_t p = src[i];
    if (p == 255) { rgba[i*4+0]=255; rgba[i*4+1]=0; rgba[i*4+2]=255; }
    else { rgba[i*4+0]=pal[p*3+0]*4; rgba[i*4+1]=pal[p*3+1]*4; rgba[i*4+2]=pal[p*3+2]*4; }
    rgba[i*4+3]=255;
  }
  // Build memorizza i canali palette a 6 bit (0..63) -> scala x4.
  free(abuf);

  *out_w = w; *out_h = h; *out_rgba = rgba;
  return 0;
}
