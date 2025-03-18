#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "assert.c"
#include "fixed_allocator.cpp"
#include "drash.cpp"
#include "array.cpp"
#include "strings.cpp"
#include "types.cpp"
#include "cli.cpp"

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

int main(int argc, char *argv[]) {
   if (argc == 1) {
      fprintf(stderr, "Missing argument file(s)\n");
      return -1;
   }

   // Skip the binary path
   argv += 1;
   argc -= 1;

   // Handle option arguments
   if (argv[0][0] == '-') {
      handle_opts((const char **)argv, argc);
      return 0;
   }

   const char *current_dir = getenv("PWD");
   assert(current_dir == NULL, "PWD envirnoment not found");

   auto scratch_allocator = Fixed_Allocator::make(getpagesize());
   Drash drash;

   for (int i = 0; i < argc; i++) {
      const char *arg = argv[i];

      // Handle commands
      if (handle_commands((const char **)argv, argc, &drash)) return 0;

      // Remove symlinks
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
