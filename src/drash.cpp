#include <stdlib.h>
#include "sys/stat.h"

#include "./bump_allocator.cpp"

struct Drash {
  // Heap pointer to filepath of removed files
  char *files;
  // Heap pointer to filepath of metadata about the removed files
  char *metadata;

  Drash(bump_allocator &buffer_alloc) {
    const char *home_env = getenv("HOME");
    assert(home_env == nullptr, "failed to get HOME environment variable");

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

    files = (char *)buffer_alloc.alloc(sizeof(files) + strlen(drash_dir) + strlen("/files") + 1);
    sprintf(files, "%s/files", drash_dir);
    err = mkdir(files, 0700);
    if (errno != EEXIST) {
      assert(err != 0, "mkdir failed to creaet drash directory");
    }

    metadata = (char *)buffer_alloc.alloc(sizeof(metadata) + strlen(drash_dir) + strlen("/metadata") + 1);
    sprintf(metadata, "%s/metadata", drash_dir);
    err = mkdir(metadata, 0700);
    if (errno != EEXIST) {
      assert(err != 0, "mkdir failed to creaet drash directory");
    }
  }
};
