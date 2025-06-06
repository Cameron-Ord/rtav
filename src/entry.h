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
  size_t size;
} Entry;

typedef struct {
  Entry *list;
  size_t size;
} Entries;

int parse_headers(Entry *ent);
Entries read_directory(const char *path);
int close_directory(DIR *dirp);
#endif
