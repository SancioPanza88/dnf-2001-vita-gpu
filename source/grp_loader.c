// Parser GRP da zero: niente riuso EDuke32.
#include "grp_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dnf_grp_index(const char *path, DnfGrpIndex *out) {
  memset(out, 0, sizeof(*out));
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  char magic[12];
  if (fread(magic, 1, 12, f) != 12) { fclose(f); return -2; }
  if (memcmp(magic, "KenSilverman", 12) != 0) { fclose(f); return -3; }
  uint32_t n = 0;
  if (fread(&n, 4, 1, f) != 1) { fclose(f); return -4; }
  if (n > DNF_GRP_MAX_FILES) { fclose(f); return -5; }
  uint32_t data_off = 12 + 4 + n * (12 + 4);
  for (uint32_t i = 0; i < n; i++) {
    char name[12]; uint32_t sz = 0;
    if (fread(name, 1, 12, f) != 12) { fclose(f); return -6; }
    if (fread(&sz, 4, 1, f) != 1) { fclose(f); return -7; }
    DnfGrpEntry *e = &out->entries[out->count++];
    memcpy(e->name, name, 12);
    e->name[12] = '\0';
    // trim spazi/null finali
    for (int k = 11; k >= 0; k--) {
      if (e->name[k] == ' ' || e->name[k] == '\0') e->name[k] = '\0';
      else break;
    }
    e->size = sz;
    e->offset = data_off;
    data_off += sz;
  }
  fclose(f);
  return 0;
}

int dnf_grp_read_lump(const char *grp_path, const DnfGrpEntry *e,
                      uint8_t **out_buf) {
  *out_buf = NULL;
  FILE *f = fopen(grp_path, "rb");
  if (!f) return -1;
  uint8_t *buf = (uint8_t *)malloc(e->size ? e->size : 1);
  if (!buf) { fclose(f); return -2; }
  if (fseek(f, (long)e->offset, SEEK_SET) != 0) { free(buf); fclose(f); return -3; }
  if (e->size && fread(buf, 1, e->size, f) != e->size) { free(buf); fclose(f); return -4; }
  fclose(f);
  *out_buf = buf;
  return (int)e->size;
}
