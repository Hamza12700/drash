#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "./assert.c"
#include "./bump_allocator.c"

#define VERSION "0.1.0"

typedef struct {
  // Heap pointer to filepath of removed files
  char *files;
  // Heap pointer to filepath of metadata about the removed files
  char *metadata;
} drash_t;

drash_t drash_make(bump_allocator *alloc) {
  drash_t drash = {0};

  char *home_env = getenv("HOME");
  assert(home_env == NULL, "failed to HOME environment variable");

  // Allocate the path to drash directory separately because it only needs to
  // live inside the function body
  char *drash_dir = malloc(sizeof(drash_dir) + strlen(home_env) + strlen("./local/share/Drash"));
  sprintf(drash_dir, "%s./local/share/Drash", home_env);

  drash.files = bump_alloc(alloc, sizeof(drash.files) + strlen(drash_dir) + strlen("/files") + 1);
  sprintf(drash.files, "%s/files", drash_dir);

  drash.metadata = bump_alloc(alloc, sizeof(drash.metadata) + strlen(drash_dir) + strlen("/metadata") + 1);
  sprintf(drash.metadata, "%s/metadata", drash_dir);

  free(drash_dir);
  return drash;
}

typedef struct {
  const char *name;
  const char *desc;
} command_t;

const command_t commands[] = {
    {.name = "list", .desc = "List Drash'd files"},
    {.name = "remove", .desc = "Remove files from the Drash/can"},
    {.name = "empty", .desc = "Empty the Drash/can"},
    {.name = "restore", .desc = "Restore removed files"},
    NULL};

void print_command_options() {
  uint8_t longest_desc = 0;
  uint8_t longest_name = 0;

  // Get the largest desc and name of the command
  for (int i = 0; commands[i].name != NULL; i++) {
    const command_t cmd = commands[i];
  
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
  for (int i = 0; commands[i].name != NULL; i++) {
    command_t cmd = commands[i];
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

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "Missing argument file(s)\n");
    return -1;
  }

  bump_allocator buffer_alloc = bump_alloc_new(1000);
  drash_t drash = drash_make(&buffer_alloc);

  // Skip the first argument for the binary path
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];

    // Handle option arguments
    if (arg[0] == '-') {
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

    {
      struct stat statbuf;
      int err = 0;
      err = lstat(arg, &statbuf);

      if (err != 0 && errno == ENOENT) {
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

  bump_free(&buffer_alloc);
}
