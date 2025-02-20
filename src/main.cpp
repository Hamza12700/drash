#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "./assert.c"
#include "./bump_allocator.cpp"

#define VERSION "0.1.0"

struct drash_t {
  // Heap pointer to filepath of removed files
  char *files;
  // Heap pointer to filepath of metadata about the removed files
  char *metadata;

  drash_t(bump_allocator &buffer_alloc) {
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

struct command_t {
  const char *name;
  const char *desc;
};

const command_t commands[] = {
  {.name = "list", .desc = "List Drash'd files"},
  {.name = "remove", .desc = "Remove files from the Drash/can"},
  {.name = "empty", .desc = "Empty the Drash/can"},
  {.name = "restore", .desc = "Restore removed files"},
};

void print_command_options() {
  uint8_t longest_desc = 0;
  uint8_t longest_name = 0;

  // Get the largest desc and name from the commands
  for (command_t cmd : commands) {
    uint8_t current_desc_len = strlen(cmd.desc);
    uint8_t current_name_len = strlen(cmd.name);

    if (current_desc_len > longest_desc) { longest_desc = current_desc_len; }
    if (current_name_len > longest_name) { longest_name = current_name_len; }
  }

  uint16_t total_size = longest_name + longest_desc + 30;

  // Create a tmp buffer
  char buffer[total_size];
  uint16_t buf_cursor = 0;

  // Fill the buffer
  for (command_t cmd : commands) {
    buf_cursor = 0;

    // Fill the buffer with whitespace
    for (int j = 0; j < total_size; j++) { buffer[j] = ' '; }

    // Write the command name in the buffer
    for (size_t x = 0; x < strlen(cmd.name); x++) { buffer[x+3] = cmd.name[x]; }
    buf_cursor = strlen(cmd.name) + 3;

    // Insert a semicolon after the command name
    buffer[buf_cursor] = ':';

    // Write the command description in the buffer
    for (size_t x = 0; x < strlen(cmd.desc); x++) { buffer[x+longest_name+6] = cmd.desc[x]; }

    // @NOTE: Because the buffer isn't null-terminated; only print the
    // buffer by its total_size.
    printf("%.*s\n", total_size, buffer);
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "Missing argument file(s)\n");
    return -1;
  }

  // Skip the binary path, because I'm always off by one error when looping throught the array
  argv++;
  argc -= 1;

  // @NOTE: shouldn't exceed more than 1000 bytes
  auto buffer_allocator = bump_allocator(1000);
  auto drash = drash_t(buffer_allocator);

  // Handle option arguments
  if (argv[0][0] == '-') {
    const char *arg = argv[0];

    if (strcmp(arg, "-help") == 0 || strcmp(arg, "-h") == 0) {
      printf("Usage: drash [OPTIONS] [FILES].. [SUB-COMMANDS]\n");
      printf("\nCommands:\n");
      print_command_options();
      return 0;
    } else if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0) {
      printf("drash version: %s\n", VERSION);
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", arg);
      return -1;
    }
  }

  for (int i = 0; i < argc; i++) {
    const char *arg = argv[i];

    // Check for symlink files and delete if found
    // If file doesn't exist then report the error and continue to next file
    {
      struct stat statbuf;
      int err = 0;
      err = lstat(arg, &statbuf);

      // If file not found
      if (err != 0 && errno == 2) {
        fprintf(stderr, "file not found: %s\n", arg);
        continue;
      }

      assert(err != 0, "lstat failed");

      // Remove symlink files
      if ((statbuf.st_mode & S_IFMT) == S_IFLNK) {
        assert(remove(arg) != 0, "failed to remove symlink-file");
        printf("Removed symlink: %s\n", arg);
      }
    }

    printf("file: %s\n", arg);
  }

  buffer_allocator.free();
}
