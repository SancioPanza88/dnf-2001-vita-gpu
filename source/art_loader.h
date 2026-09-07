#pragma once
// M1b: carica una texture muraria reale da DNF.GRP.
// Cerca PALETTE.DAT + il primo lump *.ART, estrae un tile murario
// (preferito 64x64) e lo espande in RGBA con la palette 0.
// L'espansione è UNA volta al load in CPU; il raster resta 100% GPU.
#ifndef DNF_ART_LOADER_H
#define DNF_ART_LOADER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ritorna 0 e riempie w/h/rgba (malloc, il chiamante fa free) se ok.
int dnf_art_load_wall_rgba(const char *grp_path,
                           int *out_w, int *out_h, uint8_t **out_rgba);

#ifdef __cplusplus
}
#endif

#endif
