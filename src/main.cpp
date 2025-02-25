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
  const int path_len = strlen(path) - 1;
  const int max_filename_len = 50;

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

  strcpy(path, buf);
  return OK;
}

enum CmdAction {
  List,
  Remove, 
  Empty,
  Restore,
};

struct Command {
  const char *name;
  const char *desc;

  const CmdAction action;
};

const Command commands[] = {
  {.name = "list", .desc = "List Drash'd files", .action = List },
  {.name = "remove", .desc = "Remove files from the Drash/can", .action = Remove },
  {.name = "empty", .desc = "Empty the Drash/can", .action = Empty },
  {.name = "restore", .desc = "Restore removed files", .action = Restore },
};

enum OptAction {
  Force,    // Remove the file without storing it in the drashcan
  Help,     // Help message. @NOTE: Add ability to show help for separate commands
  Version,  // Print version of the project
};

struct Option {
  const char *name;
  const char *desc;
  
  const OptAction action;

  // Compare a string with an option->name
  bool cmp(const char *str) const {
    char lbuf[10] = {0};
    char sbuf[5] = {0};

    int x = 0;
    for (size_t i = 0; i < strlen(name) - 1; i++) {
      const char name_char = name[i];

      if (name_char != '|') {
        lbuf[i] = name_char;
        continue;
      }

      sbuf[x] = name[i+1];
      x++;
    }

    if (strcmp(str, lbuf) == 0 || strcmp(str, sbuf) ==  0) { return true; }
    return false;
  }
};

const Option options[] = {
  { .name = "force|f", .desc = "Force remove file" , .action = Force },
  { .name = "help|h", .desc = "Display the help message" , .action = Help },
  { .name = "version|v", .desc = "Print the version" , .action = Version },
};

void print_help() {
  int longest_desc = 0;
  int longest_name = 0;

  // Get the largest desc and name from the commands
  for (auto cmd : commands) {
    int current_desc_len = strlen(cmd.desc);
    int current_name_len = strlen(cmd.name);

    if (current_desc_len > longest_desc) { longest_desc = current_desc_len; }
    if (current_name_len > longest_name) { longest_name = current_name_len; }
  }

  // Print commands
  printf("Usage: drash [OPTIONS] [FILES].. [SUB-COMMANDS]\n");
  printf("\nCommands:\n");

  uint16_t total_size = longest_name + longest_desc + 30;

  {
    char buffer[total_size];
    uint16_t cursor = 0;

    for (const auto cmd : commands) {
      cursor = 0;

      // Fill the buffer with whitespace
      for (int j = 0; j < total_size; j++) { buffer[j] = ' '; }

      // Write the command name in the buffer
      for (size_t x = 0; x < strlen(cmd.name); x++) { buffer[x+3] = cmd.name[x]; }
      cursor = strlen(cmd.name) + 3;

      // Insert a semicolon after the command name
      buffer[cursor] = ':';

      // Write the command description in the buffer
      for (size_t x = 0; x < strlen(cmd.desc); x++) { buffer[x+longest_name+6] = cmd.desc[x]; }

      // @NOTE: Because the buffer isn't null-terminated; only print the
      // buffer by its total_size.
      printf("%.*s\n", total_size, buffer);
    }
  }

  printf("\nOptions:\n");

  // Get the longest desc and name for the options commands
  for (auto opt : options) {
    // NOTE: Don't need to compute the short name because it should be longer than 2 characters
    int current_name_len = strlen(opt.name);
    int current_desc_len = strlen(opt.desc);
 
    if (current_desc_len > longest_desc) { longest_desc = current_desc_len; }
    if (current_name_len > longest_name) { longest_name = current_name_len; }
  }

  total_size = longest_name + longest_desc + 20;
  char buffer[total_size];

  for (const auto opt : options) {
    // Fill the buffer with whitespace
    for (int i = 0; i < total_size; i++) { buffer[i] = ' '; }

    // Write the option in the buffer
    for (size_t i = 0; i < strlen(opt.name); i++) { buffer[i+3] = opt.name[i]; }

    // Write the option description in the buffer
    for (size_t i = 0; i < strlen(opt.desc); i++) { buffer[i+longest_name+6] = opt.desc[i]; }

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
    char *arg = argv[0];

    // Remove the leading '-'
    // @Incomplete: Check for double '--'
    arg++;

    for (const auto opt : options) {
      if (!opt.cmp(arg)) {
        continue;
      }

      switch (opt.action) {
        case Help: {
          print_help();
          return 0;
        }

        case Version: {
          printf("drash version: %s\n", VERSION);
          return 0;
        }

        case Force: {
          // Skip the current argument
          for (int i = 1; i < argc; i++) {
            const char *file = argv[i];
            assert_err(remove(file) != 0, "failed to remove file");
          }
          return 0;
        }
      }
    }

    fprintf(stderr, "Unkonwn option: %s\n", arg);
    return -1;
  }

  // @Optimize: Add the ability to get an allocator from the bump_allocator struct.
  // This is way we can have multiple allocators without any allocations.
  auto scratch_alloc = bump_allocator(500);

  for (int i = 0; i < argc; i++) {
    const char *arg = argv[i];

    for (auto cmd : commands) {
      if (strcmp(arg, cmd.name) == 0) {
        switch (cmd.action) {
          case List: {
            printf("TODO: LIST");
            return 0;
          }
          case Restore: {
            printf("TODO: Restore");
            return 0;
          }
          case Empty: {
            printf("TODO: Empty");
            return 0;
          }
          case Remove: {
            printf("TODO: Remove");
            return 0;
          }
        }
      }
    }

    const int path_len = strlen(arg) - 1;

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

      assert_err(err != 0, "lstat failed");

      // Remove symlink files
      if ((statbuf.st_mode & S_IFMT) == S_IFLNK) {
        assert_err(remove(arg) != 0, "failed to remove symlink-file");
        printf("Removed symlink: %s\n", arg);
      }
    }

    if (path_len > MAX_ARGLEN) {
      fprintf(stderr, "path is too long: %d", path_len);
      fprintf(stderr, "max length is %d", MAX_ARGLEN);
      continue;
    }

    char *filename = (char *)scratch_alloc.alloc(path_len);
    strcpy(filename, arg);

    // Get the filename
    BN_RET bn_err = OK;
    bn_err = file_basename(filename);
    if (bn_err == TOO_LONG) {
      fprintf(stderr, "basename failed because path is too long\n");
      continue;
    } else if (bn_err == EMPTY) {
      fprintf(stderr, "basename: empty path: %s\n", arg);
      continue;
    }

    char *file_metadata_path = (char *)scratch_alloc.alloc(sizeof(file_metadata_path) + strlen(filename) + strlen(drash.metadata));
    sprintf(file_metadata_path, "%s/%s.info", drash.metadata, filename);

    FILE *file_metadata = fopen(file_metadata_path, "a");
    assert_err(file_metadata == NULL, "fopen failed");

    fclose(file_metadata);
    scratch_alloc.reset();
  }

  scratch_alloc.free();
  buffer_alloc.free();
}
