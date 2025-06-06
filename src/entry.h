#ifndef ENTRY_H
#define ENTRY_H
#include <linux/limits.h>
#include <stddef.h>
struct __dirstream;
typedef struct __dirstream DIR;

typedef struct {
  char name[NAME_MAX];
  char path[PATH_MAX];
  char fullpath[PATH_MAX];
  size_t fplen;
  size_t namelen;
  size_t pathlen;
  int is_audio_file;
  size_t size;
} Entry;

int parse_headers(Entry *ent);
Entry *read_directory(const char *path);
int close_directory(DIR *dirp);
#endif
