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
  sceCtrlSetSamplingMode(1); // analogici
  dnf_gpu_init();
  build_default_palette();
  dnf_gpu_upload_palette(s_default_pal);

  // Dati DNF (best-effort): se assenti parte la stanza demo GPU.
  DnfGrpIndex idx; memset(&idx, 0, sizeof(idx));
  int has_grp = (dnf_grp_index(DNF_GRP_PATH, &idx) == 0) ? 1 : 0;

  DnfMap map;
  dnf_map_load(DNF_MAP_PATH, &map);

  // Log diagnostico: ci dice se il loop emette davvero quad GPU.
  SceUID logfd = -1;
  {
    // sceIoOpen senza include extra: path ux0 scrivibile.
    extern int sceIoOpen(const char *, int, int);
    extern int sceIoWrite(SceUID, const void *, int);
    logfd = sceIoOpen(DNF_DATA_DIR "/gpu_log.txt", 0x601 /*WR|CRE|TRUNC*/, 0777);
    (void)sceIoWrite;
  }

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
#ifdef VITA
    glDisable(0x0DE1 /*TEXTURE_2D*/);
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(0x0004 /*TRIANGLES*/);
    glVertex3f(-0.9f + px * 0.1f, -0.9f, -0.5f);
    glVertex3f( 0.9f + px * 0.1f, -0.9f, -0.5f);
    glVertex3f( 0.0f + px * 0.1f,  0.9f, -0.5f);
    glEnd();
#endif
    dnf_map_draw_gpu(&map, yaw);
    (void)has_grp;
    dnf_gpu_end_frame();

    if (logfd >= 0 && (frame % 60) == 0) {
      extern int sceIoWrite(SceUID, const void *, int);
      char line[128];
      int n = snprintf(line, sizeof(line),
        "frame=%d grp=%d walls=%d whitetex=%u yaw=%.2f\n",
        frame, has_grp, map.num_walls, (unsigned)map.white_tex, yaw);
      if (n > 0) sceIoWrite(logfd, line, n);
    }
    frame++;
  }

  dnf_gpu_shutdown();
  sceKernelExitProcess(0);
  return 0;
}
