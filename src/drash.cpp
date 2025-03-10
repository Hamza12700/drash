#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "fixed_allocator.cpp"
#include "strings.cpp"

#define DIR_PERM 0740

struct Drash {
  // Heap pointer to filepath of removed files
  String files;

  // Heap pointer to filepath of metadata about the removed files
  String metadata;

  Drash(Fixed_Allocator &allocator) {
    const char *home_env = getenv("HOME");
    assert_err(home_env == NULL, "failed to get HOME environment variable");

    // @Personal: I'm not gona have the HOME env greater than 25 chars!
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
    err = mkdir(drash_dir, DIR_PERM);

    // Skip the error check if the directory already exists
    if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
    }

    auto drash_files = dynamic_string_with_size(allocator, sizeof(files) + strlen(drash_dir) + strlen("/files"));
    sprintf(drash_files.buf, "%s/files", drash_dir);
    err = mkdir(drash_files.buf, DIR_PERM);
    if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
    }

    auto metadata_files = dynamic_string_with_size(allocator, sizeof(metadata) + strlen(drash_dir) + strlen("/metadata"));
    sprintf(metadata_files.buf, "%s/metadata", drash_dir);
    err = mkdir(metadata_files.buf, DIR_PERM);
    if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
    }

    files = drash_files.to_string();
    metadata = metadata_files.to_string();
  }

  void empty_drash(Fixed_Allocator &allocator) {
    auto string = dynamic_string_with_size(allocator, files.len + 10);
    sprintf(string.buf, "rm -rf %s", files.buf); // @Incomplete: Implement a function that will delete files and directories recursively
    assert_err(system(string.buf) != 0, "failed to remove drashd files");
    assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");

    DIR *dir = opendir(metadata.buf);
    assert_err(dir == NULL, "failed to open drash metadata directory");

    struct dirent *dst;
    while ((dst = readdir(dir)) != NULL) {}
  }
};
