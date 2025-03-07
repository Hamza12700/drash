#include <alloca.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ftw.h>
#include <dirent.h>
#include <unistd.h>

#include "assert.c"
#include "bump_allocator.cpp"
#include "drash.cpp"

#define VERSION "0.1.0"
#define MAX_ARGLEN 300

auto root_allocator = new_bump_allocator(getpagesize());
auto drash = Drash(root_allocator);

template<const size_t N>
struct Static_Array {
  char elm[N] = {0};
  const size_t capacity = N;

  size_t len() const {
    size_t count = 0;
    while (elm[count] != '\0') count++;

    return count;
  }

  char& operator[] (size_t idx) {
    if (idx > capacity) {
      fprintf(stderr, "array - attempted to index into position '%zu' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%zu'.\n", capacity);
      raise(SIGTRAP);
    }

    return elm[idx];
  }
};

struct Static_String {
  char *buf;
  size_t len;

  Static_String(const char *string) {
    size_t str_len = strlen(string);
    buf = (char *)alloca(str_len + 1);
    len = str_len;

    memcpy(buf, string, len + 1);
  }

  char& operator[] (size_t idx) {
    if (idx > len) {
      fprintf(stderr, "string - attempted to index into position '%zu' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%zu'.\n", len);
      raise(SIGTRAP);
    }

    return buf[idx];
  }
};

struct Dynamic_String {
  char *buf;
  size_t capacity;

  Dynamic_String(Bump_Allocator &allocator, const size_t size) {
    buf = (char *)allocator.alloc(size);
    capacity = size;
  }

  size_t len() const {
    int count = 0;

    while (buf[count] != '\0') count++;
    return count;
  }

  char& operator[] (size_t idx) {
    if (idx > capacity) {
      fprintf(stderr, "dynamic-string - attempted to index into position '%zu' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%zu'.\n", capacity);
      raise(SIGTRAP);
    }

    return buf[idx];
  }
};

// Error enum for 'file_basename' function
typedef enum {
  OK = 0,

  EMPTY = 1,    // Path is NULL
  TOO_LONG = 2, // Path is too long
} BN_RET;

// Get the basename of the file or directory
// Mutates the string
BN_RET file_basename(Dynamic_String path) {
  const int max_filename_len = 50;

  if (path.len() <= 0) return EMPTY;

  // Return if the string doesn't contain any forward-slashs
  bool contain_slash = false;
  for (size_t i = 0; i < path.len(); i++) {
    if (path[i] == '/') {
      contain_slash = true;
      break;
    }
  }

  if (!contain_slash) return OK;

  int filename_len = 0;
  for (int i = path.len(); path[i] != '/'; i--) {
    filename_len++;
  }

  if (filename_len > max_filename_len) return TOO_LONG;
  Static_Array<max_filename_len> buf;

  int j = 0;
  for (int i = path.len(); path[i] != '/'; i--) {
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

  strcpy(path.buf, buf.elm);
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
  printf("Usage: drash [OPTIONS] [FILES].. [SUB-COMMANDS]\n");
  printf("\nCommands:\n");
  for (const auto cmd : commands) {
      printf("   %-10s", cmd.name);
      printf("   %-10s\n", cmd.desc);
  }


  printf("\nOptions:\n");
  for (const auto opt : options) {
    printf("   %-10s", opt.name);
    printf("   %-10s\n", opt.desc);
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


  // Handle option arguments
  if (argv[0][0] == '-') {
    char *arg = argv[0];

    arg++;
    if (arg[0] == '-') arg++;

    // @NOTE: Check files/directories if they exists so that way I don't have to
    // do error checking in every case for the command line option.

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
          if (argc == 1) {
            fprintf(stderr, "Missing argument file(s)\n");
            return -1;
          }

          // Skip the current argument
          for (int i = 1; i < argc; i++) {
            const char *path = argv[i];

            struct stat sb;
            assert_err(lstat(path, &sb) != 0, "lstat failed");

            if ((sb.st_mode & S_IFMT) == S_IFREG) {
              assert_err(unlink(path) != 0, "failed to remove file");
              continue;
            }

            // @Incomplete: Implement a function that will delete files and directories recursively
            auto string = Dynamic_String(root_allocator, strlen(path) + 10);
            sprintf(string.buf, "rm -rf %s", path);

            assert_err(system(string.buf) != 0, "system command failed");
            root_allocator.reset();
          }

          return 0;
        }
      }
    }

    fprintf(stderr, "Unkonwn option: %s\n", arg);
    return -1;
  }

  auto scratch_alloc = root_allocator.sub_allocator(500);

  const char *current_dir = getenv("PWD");
  assert(current_dir == NULL, "PWD envirnoment not found");

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
            DIR *metadata_dir = opendir(drash.metadata);
            assert_err(metadata_dir == NULL, "failed to open drash metadata directory");

            struct dirent *dir_stat;
            int count = 0;

            while ((dir_stat = readdir(metadata_dir)) != NULL) {
              count++; 
            }

            if (count <= 2) {
              printf("Drashcan is already empty\n");
              closedir(metadata_dir);
              return 0;
            }

            nftw(drash.files, recursive_remove, 8, FTW_DEPTH|FTW_MOUNT|FTW_PHYS);
            nftw(drash.metadata, recursive_remove, 0, FTW_DEPTH|FTW_MOUNT|FTW_PHYS);

            closedir(metadata_dir);
            return 0;
          }

          case Remove: {
            printf("TODO: Remove");
            return 0;
          }
        }
      }
    }

    // Check for symlink files and delete if found
    // If file doesn't exist then report the error and continue to next file
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

    const int path_len = strlen(arg) - 1;
    if (path_len > MAX_ARGLEN) {
      fprintf(stderr, "path is too long: %d", path_len);
      fprintf(stderr, "max length is %d", MAX_ARGLEN);
      continue;
    }

    auto filename = Dynamic_String(scratch_alloc, path_len + 1);
    strcpy(filename.buf, arg);

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

    char *file_metadata_path = (char *)scratch_alloc.alloc(filename.capacity + strlen(drash.metadata) + 1);
    sprintf(file_metadata_path, "%s/%s.info", drash.metadata, filename.buf);

    err = lstat(file_metadata_path, &statbuf);
    bool file_exists = true;

    if ((statbuf.st_mode & S_IFMT) == S_IFLNK) {
      fprintf(stderr, "Error: metadata file is a symlink-file\n");
      continue;
    }

    if (err != 0 && errno == 2) file_exists = false;

    if (file_exists) {
      fprintf(stderr, "Error: file '%s' already exists in the drashcan\n", arg);
      fprintf(stderr, "Can't overwrite it\n");
      continue;
    }

    FILE *file_metadata = fopen(file_metadata_path, "w");
    assert_err(file_metadata == NULL, "fopen failed");

    char *absolute_path = (char *)scratch_alloc.alloc(strlen(current_dir) + filename.capacity + 1);
    sprintf(absolute_path, "%s/%s", current_dir, filename.buf);
    fprintf(file_metadata, "Path: %s", absolute_path);

    char *drashd_file = (char *)scratch_alloc.alloc(sizeof(char *) + path_len);
    sprintf(drashd_file, "%s/%s", drash.files, filename.buf);

    assert_err(rename(arg, drashd_file) != 0, "failed to renamae file to new location");

    fclose(file_metadata);
    scratch_alloc.reset();
  }

  root_allocator.free();
}
