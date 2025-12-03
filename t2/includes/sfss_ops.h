#ifndef SFSS_FUNC
#define SFSS_FUNC

#define ROOT_DIR "SFSS-root-dir"
#define MAX_PATH_LEN 1024
#define BLOCK_SIZE 16

#include "aux.h"

void build_real_path(char *buffer, int owner, const char *virtual_path);

int sfss_read(int owner, char *path, int offset, char *buffer_out);

int sfss_write(int owner, char *path, char *payload, int offset);

int sfss_add(int owner, char *path, char *dirname);

int sfss_rem(int owner, char *path, char *name);

int sfss_listDir(int owner, char *path, char *allfilenames, IndexInfo *positions);

#endif
