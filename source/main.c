// DNF 2001 Vita GPU — main da zero.
// Niente launcher EDuke32, niente SDL software, niente vita2d.
// Boot: power -> vitaGL 960x544 vsync -> indice GRP -> palette in GPU ->
// mappa in VBO -> loop GPU (60Hz page-flip).
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gpu_renderer.h"
#include "grp_loader.h"
#include "map_loader.h"

#define DNF_DATA_DIR "ux0:data/DNF"
#define DNF_GRP_PATH DNF_DATA_DIR "/DNF.GRP"
#define DNF_MAP_PATH DNF_DATA_DIR "/_CLIPSHAPE0.MAP"

static uint8_t s_default_pal[256 * 3];

static void build_default_palette(void) {
  // Gradiente diagnostico: se DNF.GRP manca si vede subito che la LUT GPU va.
  for (int i = 0; i < 256; i++) {
    s_default_pal[i*3+0] = (uint8_t)i;
    s_default_pal[i*3+1] = (uint8_t)(255 - i);
    s_default_pal[i*3+2] = (uint8_t)128;
  }
}

int main(void) {
  dnf_gpu_init(); // niente log file, niente palette: solo GPU (come i sample)
  sceCtrlSetSamplingMode(1); // analogici

  // Dati DNF (best-effort): se assenti parte la stanza demo GPU.
  DnfGrpIndex idx; memset(&idx, 0, sizeof(idx));
  int has_grp = (dnf_grp_index(DNF_GRP_PATH, &idx) == 0) ? 1 : 0;

  DnfMap map;
  dnf_map_load(DNF_MAP_PATH, &map);

  SceCtrlData pad, old = {0};
  sceCtrlPeekBufferPositive(0, &old, 1);
  float yaw = 0.0f, px = 0, py = 0;
  int frame = 0;

  while (1) {
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if (pad.buttons & 0x8000 /*START*/ && !(old.buttons & 0x8000)) break;
    float rx = ((int)pad.lx - 128) / 128.0f;
    float mx = ((int)pad.ly - 128) / 128.0f;
    if (rx > 0.15f || rx < -0.15f) yaw += rx * 0.05f;
    if (mx > 0.15f || mx < -0.15f) { px += (float)cos(yaw) * mx * 0.05f; py += (float)sin(yaw) * mx * 0.05f; }
    old = pad;

    // Frame 100% GPU: clear+draw in VRAM, present vsyncato.
    dnf_gpu_begin_frame(yaw, 0, px, py, 0);
    // M0.2: triangolo ROSSO in immediate mode (texture off) + stanza VERDE.
    // Atteso: fondo blu, triangolo rosso davanti, stanza verde al centro.
    dnf_gpu_debug_triangle(px * 0.1f);
    dnf_map_draw_gpu(&map, yaw);
    (void)has_grp;
    dnf_gpu_end_frame();
    frame++;
  }

  dnf_gpu_shutdown();
  sceKernelExitProcess(0);
  return 0;
}
