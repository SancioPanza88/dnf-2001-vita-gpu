#pragma once
// DNF 2001 Vita GPU — renderer 100% GPU (vitaGL / PowerVR SGX).
// Principio: ZERO framebuffer software, ZERO memcpy CPU per-pixel.
// La CPU fa solo logica/culling/setup matrici. Il raster è tutto GPU:
// pareti/setori come VBO + texture, palette P8->RGB in fragment shader
// tramite LUT 256x1 caricata una volta in VRAM.

#ifndef DNF_GPU_RENDERER_H
#define DNF_GPU_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Risoluzione nativa pannello Vita. Niente 320x200 software.
#define DNF_FB_W 960
#define DNF_FB_H 544

// Un vertice GPU: posizione clip-space + UV + indice palette/luce.
typedef struct {
  float x, y, z;     // posizione (world o clip, a seconda del pass)
  float u, v;        // coordinate texture
  float light;       // luce settore (0..1)
} DnfGpuVertex;

int  dnf_gpu_init(void);            // vglInit + shader + LUT + stati GL
void dnf_gpu_shutdown(void);
void dnf_gpu_begin_frame(float yaw, float pitch, float px, float py, float pz);
void dnf_gpu_draw_wall_quad(const DnfGpuVertex *v0, const DnfGpuVertex *v1,
                            const DnfGpuVertex *v2, const DnfGpuVertex *v3,
                            uint32_t tex_id);
void dnf_gpu_draw_queued(void);     // flush batch -> singolo glDrawArrays
void dnf_gpu_debug_triangle(float dx); // triangolo rosso immediate mode (M0.2)
void dnf_gpu_end_frame(void);       // vglSwapBuffers (vsync GPU)
uint32_t dnf_gpu_upload_texture_rgba(int w, int h, const void *rgba);
uint32_t dnf_gpu_upload_texture_p8(int w, int h, const uint8_t *indices);
void dnf_gpu_upload_palette(const uint8_t pal[256 * 3]);
int  dnf_gpu_tex_count(void);

#ifdef __cplusplus
}
#endif

#endif
