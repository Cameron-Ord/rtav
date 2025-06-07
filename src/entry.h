#ifndef ENTRY_H
#define ENTRY_H
#include <linux/limits.h>
#include <stddef.h>
struct __dirstream;
typedef struct __dirstream DIR;

typedef struct {
  char name[NAME_MAX + 1];
  char path[PATH_MAX + 1];
  char fullpath[PATH_MAX + 1];
  size_t fplen;
  size_t namelen;
  size_t pathlen;
  int is_audio_file;
} Entry;

typedef struct {
  Entry *list;
  size_t size;
  int malformed;
} Entries;

int parse_headers(Entries *ent);
Entries read_directory(const char *path);
int close_directory(DIR *dirp);
#endif
