// DNF 2001 Vita GPU — implementazione renderer 100% GPU.
// Niente vita2d, niente fb_texture, niente memcpy: solo vitaGL + SGX.
//
// Pipeline:
//   1. init: vglInitExtended(960x544) + vsync, compile shader pareti,
//      crea LUT palette 256x1 RGBA in VRAM.
//   2. per frame: clear GPU, upload matrici vista (CPU: solo 16 float),
//      batch di quad pareti -> UN glDrawArrays (1 draw call per N quad).
//   3. fragment shader: texture P8 (R8) + LUT palette + luce settore.
//      Conversione palette AVVIENE in GPU, mai in CPU.
//   4. present: vglSwapBuffers -> page-flip vsyncato a 60Hz.

#include "gpu_renderer.h"

#include <psp2/power.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef VITA
#include <vitaGL.h>
#else
// Host build (CI syntax check senza VitaSDK): stub minimi.
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint;
typedef int GLsizei; typedef unsigned char GLboolean; typedef float GLfloat;
typedef unsigned int GLbitfield;
#define GL_TEXTURE_2D 0x0DE1
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_TRIANGLES 0x0004
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
static void vglInitExtended(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}
static void vglWaitVblankStart(int a){(void)a;}
static void vglSwapBuffers(int a){(void)a;}
static void glViewport(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void glClearColor(float a,float b,float c,float d){(void)a;(void)b;(void)c;(void)d;}
static void glClear(unsigned a){(void)a;}
static void glEnable(unsigned a){(void)a;}
static void glDisable(unsigned a){(void)a;}
static void glGenTextures(int a,unsigned*b){(void)a;(void)b;}
static void glBindTexture(unsigned a,unsigned b){(void)a;(void)b;}
static void glTexImage2D(unsigned a,int b,int c,int d,int e,int f,unsigned g,unsigned h,const void*i){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;}
static void glTexParameteri(unsigned a,unsigned b,int c){(void)a;(void)b;(void)c;}
#endif

// ---- Batch quad pareti: accumulo CPU-side, UN solo upload+draw in GPU ----
#define DNF_BATCH_QUADS 512
static DnfGpuVertex g_batch[DNF_BATCH_QUADS * 6]; // ogni quad = 2 triangoli
static int g_batch_quads = 0;
static uint32_t g_batch_tex = 0;

static GLuint g_prog = 0;
static GLuint g_lut_tex = 0;
static int g_tex_count = 0;

// Vertex shader: pass-through + luce.
static const char *K_VS =
  "attribute vec3 a_pos; attribute vec2 a_uv; attribute float a_light;\n"
  "varying vec2 v_uv; varying float v_light;\n"
  "uniform mat4 u_mvp;\n"
  "void main(){ v_uv=a_uv; v_light=a_light;\n"
  " gl_Position = u_mvp * vec4(a_pos,1.0); }\n";

// Fragment shader: fetch indice P8 (canale R), LUT palette in GPU, * luce.
static const char *K_FS =
  "precision mediump float;\n"
  "varying vec2 v_uv; varying float v_light;\n"
  "uniform sampler2D u_tex; uniform sampler2D u_pal;\n"
  "void main(){ float idx = texture2D(u_tex, v_uv).r;\n"
  " vec3 rgb = texture2D(u_pal, vec2(idx, 0.5)).rgb;\n"
  " gl_FragColor = vec4(rgb * v_light, 1.0); }\n";

#ifdef VITA
static GLuint compile_shader(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, 0);
  glCompileShader(sh);
  return sh;
}
#endif

int dnf_gpu_init(void) {
#ifdef VITA
  // M0.3: identico ai sample ufficiali (immediate_mode): vglInit semplice,
  // niente power/clock, niente threshold custom. Il crash C2-12828-1 era
  // dopo il logo -> uno di quegli extra.
  vglInit(0x800000);
  glClearColor(0.1f, 0.2f, 0.8f, 1.0f); // BLU: se vedi blu la GPU presenta
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, DNF_FB_W, DNF_FB_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
#endif
  g_batch_quads = 0;
  return 0;
}

void dnf_gpu_shutdown(void) {
#ifdef VITA
  vglSwapBuffers(0);
#endif
}

void dnf_gpu_begin_frame(float yaw, float pitch, float px, float py, float pz) {
  (void)yaw; (void)pitch; (void)px; (void)py; (void)pz;
  g_batch_quads = 0;
#ifdef VITA
  glClearColor(0.1f, 0.2f, 0.8f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // Stessa ortho dei sample, ogni frame (vitaGL la resetta sullo swap).
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, DNF_FB_W, DNF_FB_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_TEXTURE_2D);
#endif
}

void dnf_gpu_draw_wall_quad(const DnfGpuVertex *v0, const DnfGpuVertex *v1,
                            const DnfGpuVertex *v2, const DnfGpuVertex *v3,
                            uint32_t tex_id) {
  if (tex_id != g_batch_tex && g_batch_quads > 0)
    dnf_gpu_draw_queued();
  g_batch_tex = tex_id;
  if (g_batch_quads >= DNF_BATCH_QUADS)
    dnf_gpu_draw_queued();
  DnfGpuVertex *d = &g_batch[g_batch_quads * 6];
  // Tri 1: v0 v1 v2 | Tri 2: v0 v2 v3 — raster interamente GPU.
  d[0]=*v0; d[1]=*v1; d[2]=*v2; d[3]=*v0; d[4]=*v2; d[5]=*v3;
  g_batch_quads++;
}

void dnf_gpu_debug_triangle(float dx) {
#ifdef VITA
  // Coordinate pixel come il sample immediate_mode (niente clip-space).
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLES);
  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(400 + dx, 100, 0);
  glVertex3f(800 + dx, 100, 0);
  glVertex3f(600 + dx, 400, 0);
  glEnd();
#else
  (void)dx;
#endif
}

void dnf_gpu_draw_queued(void) {
  if (g_batch_quads == 0) return;
#ifdef VITA
  // M0.3: muri come quad VERDE in pixel-coords (come il sample), niente
  // texture, niente vertex-array client: solo immediate mode sicuro.
  glDisable(GL_TEXTURE_2D);
  glColor3f(0.0f, 1.0f, 0.0f);
  glBegin(GL_QUADS);
  for (int q = 0; q < g_batch_quads; q++) {
    // Ogni quad occupa 6 vertici (2 triangoli): riuso i 4 angoli.
    DnfGpuVertex *b = &g_batch[q * 6];
    // Mappa clip [-0.45,0.45] -> pixel schermo.
    float px0 = (b[0].x * 0.5f + 0.5f) * DNF_FB_W;
    float py0 = (b[0].y * 0.5f + 0.5f) * DNF_FB_H;
    float px1 = (b[1].x * 0.5f + 0.5f) * DNF_FB_W;
    float py1 = (b[2].y * 0.5f + 0.5f) * DNF_FB_H;
    glVertex3f(px0, (DNF_FB_H - py0), 0);
    glVertex3f(px1, (DNF_FB_H - py0), 0);
    glVertex3f(px1, (DNF_FB_H - py1), 0);
    glVertex3f(px0, (DNF_FB_H - py1), 0);
  }
  glEnd();
#endif
  g_batch_quads = 0;
}

void dnf_gpu_end_frame(void) {
  dnf_gpu_draw_queued();
#ifdef VITA
  vglSwapBuffers(0); // page-flip GPU vsyncato, niente tearing
#endif
}

uint32_t dnf_gpu_upload_texture_rgba(int w, int h, const void *rgba) {
  GLuint t = 0;
#ifdef VITA
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, 0x2800, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, 0x2801, GL_LINEAR);
#else
  (void)w; (void)h; (void)rgba; t = (GLuint)(0x1000 + g_tex_count);
#endif
  g_tex_count++;
  return (uint32_t)t;
}

uint32_t dnf_gpu_upload_texture_p8(int w, int h, const uint8_t *indices) {
#ifdef VITA
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  // Formato R8: un byte indice per texel, mai espanso in CPU.
  glTexImage2D(GL_TEXTURE_2D, 0, 0x1903 /*GL_RED*/, w, h, 0,
               0x1903, GL_UNSIGNED_BYTE, indices);
  glTexParameteri(GL_TEXTURE_2D, 0x2800, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, 0x2801, GL_NEAREST);
  g_tex_count++;
  return (uint32_t)t;
#else
  (void)w; (void)h; (void)indices;
  g_tex_count++;
  return (uint32_t)(0x2000 + g_tex_count);
#endif
}

void dnf_gpu_upload_palette(const uint8_t pal[256 * 3]) {
#ifdef VITA
  uint8_t rgba[256 * 4];
  for (int i = 0; i < 256; i++) {
    rgba[i*4+0]=pal[i*3+0]; rgba[i*4+1]=pal[i*3+1]; rgba[i*4+2]=pal[i*3+2];
    rgba[i*4+3]=255;
  }
  glBindTexture(GL_TEXTURE_2D, g_lut_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
#else
  (void)pal;
#endif
}

int dnf_gpu_tex_count(void) { return g_tex_count; }
