#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "assert.c"
#include "fixed_allocator.cpp"
#include "drash.cpp"
#include "array.cpp"
#include "strings.cpp"
#include "types.cpp"

#define VERSION "0.1.0"
#define MAX_ARGLEN 1000 // Reasonable default length for file-path argument

String file_basename(Fixed_Allocator *allocator, const String *path) {
   bool contain_slash = false;
   for (uint i = 0; i < path->len(); i++) {
      if ((*path)[i] == '/') {
         contain_slash = true;
         break;
      }
   }

   if (!contain_slash) return *path;

   auto char_array = Array::make(allocator, path->nlen());
   uint file_idx;

   for (int i = path->len(); (*path)[i] != '/'; i--) {
      file_idx++;
   }

   char_array = path;
   file_idx += 1;
   char_array.skip(file_idx);

   return char_array.to_string();
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
      for (uint i = 0; i < strlen(name) - 1; i++) {
         const char name_char = name[i];

         if (name_char != '|') {
            lbuf[i] = name_char;
            continue;
         }

         sbuf[x] = name[i+1];
         x++;
      }

      if (strcmp(str, lbuf) == 0 || strcmp(str, sbuf) ==  0) return true;
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

   // Skip the binary path
   argv++;
   argc -= 1;

   auto scratch_allocator = Fixed_Allocator::make(getpagesize());
   Drash drash;

   // Handle option arguments
   if (argv[0][0] == '-') {
      const char *arg = argv[0];

      arg++;
      if (arg[0] == '-') arg++;

      // @NOTE: Check files and directories if they exists so that way I don't have to
      // do error checking in every case for the command line option.

      for (const auto opt : options) {
         if (!opt.cmp(arg)) {
            continue;
         }

         // @Factor: Handle options in a separate function/file because of indentation-level
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
                  auto string = format_string(&scratch_allocator, "rm -rf %", path);

                  assert_err(system(string.buf) != 0, "system command failed");
                  scratch_allocator.reset();
               }

               return 0;
            }
         }
      }

      fprintf(stderr, "Unkonwn option: %s\n", arg);
      return -1;
   }

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
                  DIR *metadata_dir = opendir(drash.metadata.buf);
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

                  drash.empty_drash(&scratch_allocator);

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

      const int path_len = strlen(arg);
      if (path_len > MAX_ARGLEN) {
         fprintf(stderr, "path is too long: %d", path_len);
         fprintf(stderr, "max length is %d", MAX_ARGLEN);
         continue;
      }

      auto path = String::with_size(&scratch_allocator, path_len);
      path = arg;

      // Remove the trailing slash
      if (path[path.len()-1] == '/') path.remove(path.len()-1);

      auto filename = file_basename(&scratch_allocator, &path);
      auto file_metadata_path = format_string(&scratch_allocator, "%/%.info", drash.metadata.buf, filename.buf);

      err = lstat(file_metadata_path.buf, &statbuf);
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

      FILE *file_metadata = fopen(file_metadata_path.buf, "w");
      assert_err(file_metadata == NULL, "fopen failed");

      auto absolute_path = format_string(&scratch_allocator, "%/%", (char *)current_dir, path.buf);
      fprintf(file_metadata, "Path: %s", absolute_path.buf);

      auto drash_file = format_string(&scratch_allocator, "%/%", drash.files.buf, filename.buf);
      assert_err(rename(arg, drash_file.buf) != 0, "failed to renamae file to new location");

      fclose(file_metadata);
      scratch_allocator.reset();
   }

   scratch_allocator.free();
}
