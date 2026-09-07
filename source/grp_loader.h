#pragma once
// Parser GRP minimale scritto da zero (formato KenSilverman).
// Niente codice EDuke32: header 12 byte "KenSilverman" + dir entries.
#ifndef DNF_GRP_LOADER_H
#define DNF_GRP_LOADER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DNF_GRP_MAX_FILES 4096
#define DNF_GRP_NAME_LEN 12

typedef struct {
  char name[DNF_GRP_NAME_LEN + 1];
  uint32_t size;
  uint32_t offset; // offset dati nel file GRP
} DnfGrpEntry;

typedef struct {
  DnfGrpEntry entries[DNF_GRP_MAX_FILES];
  int count;
} DnfGrpIndex;

// Indicizza ux0:data/DNF/DNF.GRP senza caricarlo tutto in RAM.
int dnf_grp_index(const char *path, DnfGrpIndex *out);
// Carica un lump in un buffer mallocato (il chiamante fa free).
// Ritorna size o -1.
int dnf_grp_read_lump(const char *grp_path, const DnfGrpEntry *e,
                      uint8_t **out_buf);

#ifdef __cplusplus
}
#endif

#endif
