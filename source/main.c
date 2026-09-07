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

  // M1c: indice GRP (~96KB) + mappa (~90KB) in statica —
  // come locali sforavano lo stack del main thread (C2-12828-1).
  static DnfGrpIndex idx; memset(&idx, 0, sizeof(idx));
  int has_grp = (dnf_grp_index(DNF_GRP_PATH, &idx) == 0) ? 1 : 0;

  static DnfMap map;
  dnf_map_load(DNF_MAP_PATH, &map);

  SceCtrlData pad, old = {0};
  sceCtrlPeekBufferPositive(0, &old, 1);
  float yaw = 0.0f, px = 0, py = 0;
  int frame = 0;

  while (1) {
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if (pad.buttons & 0x8000 /*START*/ && !(old.buttons & 0x8000)) break;
    // M1: sinistro = muovi (avanti/strafe), destro X = gira.
    float fwd = ((int)pad.ly - 128) / 128.0f;
    float str = ((int)pad.lx - 128) / 128.0f;
    float turn = ((int)pad.rx - 128) / 128.0f;
    if (turn > 0.15f || turn < -0.15f) yaw += turn * 0.06f;
    if (fwd > 0.15f || fwd < -0.15f) {
      px += (float)cos(yaw) * -fwd * 0.08f;
      py += (float)sin(yaw) * -fwd * 0.08f;
    }
    if (str > 0.15f || str < -0.15f) {
      px += (float)cos(yaw + 1.5708f) * str * 0.08f;
      py += (float)sin(yaw + 1.5708f) * str * 0.08f;
    }
    // Resta nella stanza 8x8.
    if (px < -3.2f) px = -3.2f; if (px > 3.2f) px = 3.2f;
    if (py < -3.2f) py = -3.2f; if (py > 3.2f) py = 3.2f;
    old = pad;

    // Frame 100% GPU: prospettiva + muri/pavimento texturizzati in VRAM.
    dnf_gpu_set_camera(yaw, px, py);
    dnf_gpu_begin_frame(yaw, 0, px, py, 0);
    dnf_map_draw_gpu(&map, yaw);
    (void)has_grp;
    dnf_gpu_end_frame();
    frame++;
  }

  dnf_gpu_shutdown();
  sceKernelExitProcess(0);
  return 0;
}
