# DNF 2001 Vita GPU — engine da zero, rendering solo GPU

Nuovo progetto, **zero codice riusato** dal port `dnf-2001-vita` (niente EDuke32-Vita,
niente `vita2d` framebuffer software, niente `memcpy` CPU per-pixel).

## Principio GPU

| Vecchio (CPU) | Nuovo (GPU) |
|---|---|
| Framebuffer software `320x200 P8` in RAM + `memcpy` ogni frame | Niente framebuffer CPU: color/depth buffer in VRAM via `vitaGL` `960x544x32` |
| Raster pareti/sprite in ARM | Pareti come VBO + `glDrawArrays` batchato, raster su PowerVR SGX |
| Palette `P8->BGR` espansa in CPU | Texture `R8` (1 byte indice) + LUT palette `256x1` in GPU, conversione in fragment shader |
| Present `vita2d_swap_buffers` con upscale CPU-assisted | `vglSwapBuffers` page-flip vsyncato 60Hz |

La CPU fa solo: logica gioco, parsing GRP/MAP al load, frustum-cull, setup matrice
vista (16 float/frame). **Zero lavoro per-pixel in CPU — per scelta non è
eliminabile del tutto** (logica/CON/fisica/audio restano CPU su qualunque engine).

## Struttura (scritta da zero)

```
source/main.c         boot Vita + loop GPU 60Hz
source/gpu_renderer.* renderer vitaGL: shader, batch quad, LUT palette, present
source/grp_loader.*   parser GRP "KenSilverman" minimale (indice + lump)
source/map_loader.*   parser .MAP + emissore quad GPU
shaders/wall_v.glsl   vertex shader pareti
shaders/wall_f.glsl   fragment shader P8->RGB+luce in GPU
```

## Build (CI = build ufficiale)

Push su `main` -> Actions `Build GPU VPK` (container `vitasdk/vitasdk`, `vdpm vitaGL`)
-> artefatto `DNF2001_GPU-fresh/*.vpk`.

Locale con VitaSDK:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Installazione

1. Installa il VPK da `Actions`.
2. Copia in `ux0:data/DNF/`: `DNF.GRP` (+ `DUKE3D.GRP` retail). Se assenti parte
   comunque la stanza demo procedurale (prova del path GPU senza dati).
3. Title ID: `DNFG20010` (convive col vecchio port).

## Roadmap

- [x] M0: scaffold GPU, stanza demo, VPK in CI
- [ ] M1: parse completo settori/muri/sprites + texture ART in VRAM
- [ ] M2: CON/game logic + audio (CPU) con render sempre GPU
- [ ] M3: 60fps lock su mappe DNF reali
