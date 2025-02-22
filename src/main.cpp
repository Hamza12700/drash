#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "./assert.c"
#include "./bump_allocator.cpp"
#include "./drash.cpp"

#define VERSION "0.1.0"
#define MAX_ARGLEN 300

// Error enum for function: basename
typedef enum {
  OK = 0,

  EMPTY = 1,    // Path is NULL
  TOO_LONG = 2, // Path is too long
} BN_RET;

// Get the basename of the file or directory
// Mutates the string
BN_RET file_basename(char *path) {
  int path_len = strlen(path) - 1;
  const uint8_t max_filename_len = 50;

  if (path == NULL) return EMPTY;
  else if (path_len <= 0) return EMPTY;
  else if (path[path_len] == '/') { // remove any trailing forward-slashs
    path[path_len] = '\0';
    basename(path);
  }

  // Return if the string doesn't contain any forward-slashs
  {
    bool contain_slash = false;
    for (int i = 0; i < path_len; i++) {
      if (path[i] == '/') {
        contain_slash = true;
        break;
      }
    }

    if (!contain_slash) return OK;
  }

  int filename_len = 0;
  for (int i = path_len; path[i] != '/'; i--) {
    filename_len++;
  }

  if (filename_len > max_filename_len) return TOO_LONG;

  char buf[max_filename_len];
  memset(buf, 0, max_filename_len);

  int j = 0;
  for (int i = path_len; path[i] != '/'; i--) {
    buf[j] = path[i];
    j++;
  }

  int end = j - 1;
  for (int i = 0; i < end; i++) {
    char tmp = buf[i];
    buf[i] = buf[end]; 
    buf[end] = tmp;
    end -= 1;
  }

  // Copy the buffer into the string
  strcpy(path, buf);
  return OK;
}

typedef struct {
  const char *name;
  const char *desc;
} Command;

const Command commands[] = {
  {.name = "list", .desc = "List Drash'd files"},
  {.name = "remove", .desc = "Remove files from the Drash/can"},
  {.name = "empty", .desc = "Empty the Drash/can"},
  {.name = "restore", .desc = "Restore removed files"},
  NULL
};

void print_command_options() {
  uint8_t longest_desc = 0;
  uint8_t longest_name = 0;

  // Get the largest desc and name from the commands
  for (int i = 0; commands[i].name != NULL; i++) {
    const Command cmd = commands[i];

    uint8_t current_desc_len = strlen(cmd.desc);
    uint8_t current_name_len = strlen(cmd.name);

    if (current_desc_len > longest_desc) { longest_desc = current_desc_len; }
    if (current_name_len > longest_name) { longest_name = current_name_len; }
  }

  const uint16_t total_size = longest_name + longest_desc + 30;

  // Create a tmp buffer
  char buffer[total_size];
  uint16_t buf_cursor = 0;

  // Fill the buffer
  for (int i = 0; commands[i].name != NULL; i++) {
    const Command cmd = commands[i];
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
  auto buffer_alloc = bump_allocator(1000);
  auto drash = Drash(buffer_alloc);

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
    const char *path = argv[i];
    const uint16_t path_len = strlen(path);

    // Check for symlink files and delete if found
    // If file doesn't exist then report the error and continue to next file
    {
      struct stat statbuf;
      int err = 0;
      err = lstat(path, &statbuf);

      // If file not found
      if (err != 0 && errno == 2) {
        fprintf(stderr, "file not found: %s\n", path);
        continue;
      }

      assert_err(err != 0, "lstat failed");

      // Remove symlink files
      if ((statbuf.st_mode & S_IFMT) == S_IFLNK) {
        assert_err(remove(path) != 0, "failed to remove symlink-file");
        printf("Removed symlink: %s\n", path);
      }
    }

    if (path_len > MAX_ARGLEN) {
      fprintf(stderr, "path is too long: %d", path_len);
      fprintf(stderr, "max length is %d", MAX_ARGLEN);
      continue;
    }

    // Copy the path argument in this buffer and pass it to basename to get the filename.
    // Because basename muteates the string that's why it needs to copy the path argument in this buffer.
    char filename[MAX_ARGLEN] = {0};

    strcpy(filename, path);

    // Get the filename
    BN_RET bn_err = OK;
    bn_err = file_basename(filename);
    if (bn_err == TOO_LONG) fprintf(stderr, "basename failed because path is too long\n");
    else if (bn_err == EMPTY) fprintf(stderr, "basename: empty path: %s\n", path);
  }

  buffer_alloc.free();
}
