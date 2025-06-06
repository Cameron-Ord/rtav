#include "entry.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int close_directory(DIR *dirp) { return closedir(dirp); }

Entries read_directory(const char *path) {
  DIR *dirp = NULL;
  if (!(dirp = opendir(path))) {
    fprintf(stderr, "Could not open directory : %s\n", strerror(errno));
    return (Entries){NULL, 0};
  }

  int size = 1;
  Entry *entbuf = calloc(size, sizeof(Entry));
  if (!entbuf) {
    fprintf(stderr, "Could not allocate memory : %s\n", strerror(errno));
    return (Entries){NULL, 0};
  }
  Entries ents = {entbuf, size};

  fprintf(stdout, "====DIRSTART====\n");

  struct dirent *d_ent;
  int i = 0;
  while ((d_ent = readdir(dirp)) != NULL) {
    if (!(d_ent->d_type == DT_REG)) {
      continue;
    }

    if (i >= size) {
      size++;
      Entry *tmp = realloc(entbuf, size * sizeof(Entry));
      if (!tmp) {
        fprintf(stderr, "Realloc failed : %s\n", strerror(errno));
        return ents;
      }
      entbuf = tmp;
    }

    Entry *e = &entbuf[i];

    e->size = 0;
    e->is_audio_file = 0;
    e->namelen = strlen(d_ent->d_name);
    e->pathlen = strlen(path);

    snprintf(e->path, sizeof(e->path), "%s", path);
    snprintf(e->name, sizeof(e->name), "%s", d_ent->d_name);
    snprintf(e->fullpath, sizeof(e->fullpath), "%s/%s", path, d_ent->d_name);

    fprintf(stdout, "REGULAR FILE -> %s\n", e->fullpath);

    i++;
    ents.size = i;
  }

  fprintf(stdout, "====DIREND====\n");
  return ents;
}

int parse_headers(Entry *ent) {

  const char *headers[] = {"fLaC", "OggS", "FORM", "AIFF",
                           "RIFF", "WAVE", "ID3"};

  for (size_t i = 0; i < ent->size; i++) {
    Entry *start = &ent[i];
  }
  return 1;
}
