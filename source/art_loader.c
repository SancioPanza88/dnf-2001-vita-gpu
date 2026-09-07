// M1b: parser ART v1 + PALETTE.DAT da zero (formato KenSilverman/Build).
// Layout ART: int32 artversion(1) + numtiles + tilestart + tileend,
// poi int16 tilesizx[N] + int16 tilesizy[N] + int32 picanm[N],
// poi pixel grezzi 8-bit concatenati (tilesizx*tilesizy byte per tile).
#include "art_loader.h"
#include "grp_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
  int tile = -1;
  int r = dnf_art_load_wall_choice(grp_path, 0, 0, &tile, out_w, out_h, out_rgba);
  return r;
}

// ---- M1d: elenco candidati + scelta pos ----

#define DNF_MAX_CHOICES 256
static int s_choices[DNF_MAX_CHOICES];
static int s_nchoices = -1; // -1 = non ancora scansionato

static int scan_choices(const char *grp_path) {
  // M1c: indice in statica (96KB) — sullo stack overflowava il main thread.
  static DnfGrpIndex idx;
  memset(&idx, 0, sizeof(idx));
  s_nchoices = 0;
  if (dnf_grp_index(grp_path, &idx) != 0) return 0;
  const DnfGrpEntry *ae = NULL;
  for (int i = 0; i < idx.count; i++) {
    if (!has_art_ext(idx.entries[i].name)) continue;
    if (!ae || strcasecmp(idx.entries[i].name, ae->name) < 0)
      ae = &idx.entries[i];
  }
  if (!ae) return 0;
  uint8_t *abuf = NULL;
  int asize = dnf_grp_read_lump(grp_path, ae, &abuf);
  if (asize < 16) { free(abuf); return 0; }
  int32_t ver, num, tstart, tend;
  memcpy(&ver, abuf, 4); memcpy(&num, abuf + 4, 4);
  memcpy(&tstart, abuf + 8, 4); memcpy(&tend, abuf + 12, 4);
  if (ver != 1 || num <= 0 || num > 4096 || tstart < 0 || tend >= num || tend < tstart) {
    free(abuf); return 0;
  }
  int16_t *sx = (int16_t *)(abuf + 16);
  int16_t *sy = (int16_t *)(abuf + 16 + num * 2);
  for (int t = tstart; t <= tend && s_nchoices < DNF_MAX_CHOICES; t++) {
    int w = sx[t], h = sy[t];
    if (w >= 32 && h >= 32 && w <= 128 && h <= 128)
      s_choices[s_nchoices++] = t;
  }
  free(abuf);
  return s_nchoices;
}

int dnf_art_wall_choices(const char *grp_path) {
  if (s_nchoices < 0) scan_choices(grp_path);
  return s_nchoices;
}

int dnf_art_load_wall_choice(const char *grp_path, int pos,
                             int flip_v,
                             int *out_tile, int *out_w, int *out_h,
                             uint8_t **out_rgba) {
  *out_tile = -1; *out_w = 0; *out_h = 0; *out_rgba = NULL;
  if (s_nchoices < 0) scan_choices(grp_path);
  if (s_nchoices <= 0) return -1;
  if (pos < 0) pos = 0;
  pos %= s_nchoices;
  int pick = s_choices[pos];

  // Ricarica palette + ART (semplice e robusto; solo su pressione tasto).
  static DnfGrpIndex idx;
  memset(&idx, 0, sizeof(idx));
  if (dnf_grp_index(grp_path, &idx) != 0) return -2;
  uint8_t pal[768];
  {
    const DnfGrpEntry *pe = NULL;
    for (int i = 0; i < idx.count; i++)
      if (name_is_palette(idx.entries[i].name)) { pe = &idx.entries[i]; break; }
    if (!pe || pe->size < 768) return -3;
    uint8_t *pbuf = NULL;
    if (dnf_grp_read_lump(grp_path, pe, &pbuf) < 0) return -4;
    memcpy(pal, pbuf, 768);
    free(pbuf);
  }
  const DnfGrpEntry *ae = NULL;
  for (int i = 0; i < idx.count; i++) {
    if (!has_art_ext(idx.entries[i].name)) continue;
    if (!ae || strcasecmp(idx.entries[i].name, ae->name) < 0)
      ae = &idx.entries[i];
  }
  if (!ae) return -5;
  uint8_t *abuf = NULL;
  int asize = dnf_grp_read_lump(grp_path, ae, &abuf);
  if (asize < 16) { free(abuf); return -6; }
  int32_t ver, num, tstart, tend;
  memcpy(&ver, abuf, 4); memcpy(&num, abuf + 4, 4);
  memcpy(&tstart, abuf + 8, 4); memcpy(&tend, abuf + 12, 4);
  if (ver != 1 || num <= 0 || num > 4096) { free(abuf); return -7; }
  int32_t off = 16;
  int16_t *sx = (int16_t *)(abuf + off); off += num * 2;
  int16_t *sy = (int16_t *)(abuf + off); off += num * 2;
  off += num * 4;
  if (off > asize || pick < tstart || pick > tend) { free(abuf); return -8; }
  int w = sx[pick], h = sy[pick];
  if (w <= 0 || h <= 0 || w > 256 || h > 256) { free(abuf); return -9; }
  int32_t data = off;
  for (int t = tstart; t < pick; t++) {
    int tw = sx[t], th = sy[t];
    if (tw > 0 && th > 0) data += tw * th;
  }
  if (data + w * h > asize) { free(abuf); return -10; }
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  if (!rgba) { free(abuf); return -11; }
  uint8_t *src = abuf + data;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // flip_v ribalta le righe (se la texture risulta a testa in giù).
      int sy2 = flip_v ? (h - 1 - y) : y;
      uint8_t p = src[sy2 * w + x];
      uint8_t *d = &rgba[(y * w + x) * 4];
      if (p == 255) { d[0]=255; d[1]=0; d[2]=255; }
      else { d[0]=pal[p*3+0]*4; d[1]=pal[p*3+1]*4; d[2]=pal[p*3+2]*4; }
      d[3]=255;
    }
  }
  free(abuf);
  *out_tile = pick; *out_w = w; *out_h = h; *out_rgba = rgba;
  return 0;
}
