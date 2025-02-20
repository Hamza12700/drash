#include <stdlib.h>
#include "sys/stat.h"

#include "./bump_allocator.c"

typedef struct {
  // Heap pointer to filepath of removed files
  char *files;
  // Heap pointer to filepath of metadata about the removed files
  char *metadata;

} Drash;

Drash make_drash(bump_allocator *buffer_alloc) {
  const char *home_env = getenv("HOME");
  assert(home_env == NULL, "failed to get HOME environment variable");

  Drash drash = {0};

  // HOME environment variable grater than 25 chars seems odd or rather malformed/malicious
  if (strlen(home_env) > 25) {
    fprintf(stderr, "HOME environment variable too long or malformed, length: %zu\n", strlen(home_env));
    printf("max length 25 characters\n");
    exit(-1);
  }

  // Buffer to hold the drash directory path
  // Don't need to compute the home_env size because we know the max size of the string
  char drash_dir[25 + strlen("/.local/share/Drash") + 1];
  sprintf(drash_dir, "%s/.local/share/Drash", home_env);
  int err = 0;

  // Premission: rwx|r--|---
  err = mkdir(drash_dir, 0740);

  // Skip the error check if the directory already exists
  if (errno != EEXIST) {
    assert(err != 0, "mkdir failed to creaet drash directory");
  }

  drash.files = (char *)bump_alloc(buffer_alloc, (sizeof(drash.files) + strlen(drash_dir) + strlen("/files") + 1));
  sprintf(drash.files, "%s/files", drash_dir);
  err = mkdir(drash.files, 0700);
  if (errno != EEXIST) {
    assert(err != 0, "mkdir failed to creaet drash directory");
  }

  drash.metadata = (char *)bump_alloc(buffer_alloc, sizeof(drash.metadata) + strlen(drash_dir) + strlen("/metadata") + 1);
  sprintf(drash.metadata, "%s/metadata", drash_dir);
  err = mkdir(drash.metadata, 0700);
  if (errno != EEXIST) {
    assert(err != 0, "mkdir failed to creaet drash directory");
  }

  return drash;
}
