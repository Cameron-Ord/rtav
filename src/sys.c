#include "entry.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Entries *const dirp_ret(const int broken, Entries *const ents,
                               DIR *const dirp) {
  if (dirp != NULL) {
    if (closedir(dirp) < 0) {
      fprintf(stderr, "Could not close dir parent : %s\n", strerror(errno));
    }
  }
  ents->malformed = broken;
  return ents;
}

Entries read_directory(const char *path) {
  Entries ents = {NULL, 0, 0};
  DIR *dirp = NULL;
  if (!(dirp = opendir(path))) {
    fprintf(stderr, "Could not open directory : %s\n", strerror(errno));
    return *dirp_ret(1, &ents, NULL);
  }

  int size = 1;
  Entry *entbuf = calloc(size, sizeof(Entry));
  if (!entbuf) {
    fprintf(stderr, "Could not allocate memory : %s\n", strerror(errno));
    return *dirp_ret(1, &ents, dirp);
  }
  ents.list = entbuf;
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
        return *dirp_ret(1, &ents, dirp);
      }
      entbuf = tmp;
      ents.list = entbuf;
    }

    Entry *e = &entbuf[i];

    e->is_audio_file = 0;
    e->namelen = strlen(d_ent->d_name);
    e->pathlen = strlen(path);

    snprintf(e->path, sizeof(e->path), "%s", path);
    snprintf(e->name, sizeof(e->name), "%s", d_ent->d_name);
    snprintf(e->fullpath, sizeof(e->fullpath), "%s/%s", path, d_ent->d_name);

    fprintf(stdout, "REGULAR FILE -> %s\n", e->fullpath);

    ents.size = size;
    i++;
  }

  fprintf(stdout, "====DIREND====\n");
  return *dirp_ret(0, &ents, dirp);
}

// Im only allowing a subset of audio file types that contain headers with
// metadata, Not gonna bother with RAW PCM or obscure types; Even though sndfile
// does support them
int parse_headers(Entries *ent) {
  int accumulator = 0;
  if (ent && ent->list) {
    const char *headers[] = {"fLaC", "OggS", "FORM", "AIFF",
                             "RIFF", "WAVE", "ID3"};
    const size_t hc = sizeof(headers) / sizeof(headers[0]);
    for (size_t i = 0; i < ent->size; i++) {
      Entry *e = &ent->list[i];
      FILE *file = NULL;

      unsigned char buffer[12];
      memset(buffer, 0, sizeof(buffer));

      int read = 0;
      if ((file = fopen(e->fullpath, "r"))) {
        fseek(file, 0, SEEK_SET);
        read = fread(buffer, 1, 12, file);
      } else {
        continue;
      }
      fclose(file);

      int matched = 0;
      for (size_t j = 0; j < hc && !matched; j++) {
        const char *literal = headers[j];
        const size_t len = strlen(literal);

        const unsigned char *start = &buffer[0];
        const unsigned char *end = buffer + 12;

        while (start != end && !matched) {
          if (memcmp(start, literal, len) == 0) {
            e->is_audio_file = 1;
            matched = 1, accumulator += 1;
          }
          start++;
        }
      }
    }
  }
  return accumulator;
}
