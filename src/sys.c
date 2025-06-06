#include "entry.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int close_directory(DIR *dirp) { return closedir(dirp); }

Entry *read_directory(const char *path) {
  DIR *dirp = NULL;
  if (!(dirp = opendir(path))) {
    fprintf(stderr, "Could not open directory : %s\n", strerror(errno));
    return NULL;
  }

  Entry *entbuf = malloc(sizeof(Entry));
  if (!entbuf) {
    fprintf(stderr, "Could not allocate memory : %s\n", strerror(errno));
    return NULL;
  }

  fprintf(stdout, "====DIRSTART====\n");

  struct dirent *d_ent;
  int i = 0, size = 1;
  while ((d_ent = readdir(dirp)) != NULL) {
    if (!(d_ent->d_type == DT_REG)) {
      continue;
    }

    if (i >= size) {
      size++;
      Entry *tmp = realloc(entbuf, size * sizeof(Entry));
      if (!tmp) {
        fprintf(stderr, "Realloc failed : %s\n", strerror(errno));
        return entbuf;
      }
      entbuf = tmp;
    }

    fprintf(stdout, "REGULAR FILE -> %s\n", d_ent->d_name);
    Entry *e = &entbuf[i];

    e->size = 0;
    e->is_audio_file = 0;
    e->namelen = strlen(d_ent->d_name);
    e->pathlen = strlen(path);

    memset(e->path, 0, e->pathlen);
    memset(e->name, 0, e->namelen);

    if (!strncpy(e->path, path, e->pathlen * sizeof(char))) {
      fprintf(stderr, "Failed to copy str : %s\n", strerror(errno));
      return entbuf;
    }

    if (!strncpy(e->name, d_ent->d_name, e->namelen * sizeof(char))) {
      fprintf(stderr, "Failed to copy str : %s\n", strerror(errno));
      return entbuf;
    }

    const char *delimiter = "/";
    const size_t dlen = strlen(delimiter);

    memset(e->fullpath, 0, e->pathlen + e->namelen + dlen);

    if (!strncpy(e->fullpath, e->path, e->pathlen)) {
      fprintf(stderr, "Failed to copy str : %s\n", strerror(errno));
      return entbuf;
    }

    if (!strncat(e->fullpath, delimiter, dlen)) {
      fprintf(stderr, "Failed to copy str : %s\n", strerror(errno));
      return entbuf;
    }

    if (!strncat(e->fullpath, e->name, e->namelen)) {
      fprintf(stderr, "Failed to copy str : %s\n", strerror(errno));
      return entbuf;
    }

    i++;
  }

  entbuf->size = i;
  fprintf(stdout, "====DIREND====\n");
  return entbuf;
}

int parse_headers(Entry *ent) {

  const char *headers[] = {"fLaC", "OggS", "FORM", "AIFF",
                           "RIFF", "WAVE", "ID3"};

  for (size_t i = 0; i < ent->size; i++) {
    Entry *start = &ent[i];
  }
  return 1;
}
