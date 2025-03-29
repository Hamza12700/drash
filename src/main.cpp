#include "assert.cpp"
#include "fixed_allocator.cpp"
#include "drash.cpp"
#include "strings.cpp"
#include "cli.cpp"
#include "file_system.cpp"

#define MAX_ARGLEN 1000 // Reasonable default length for file-path argument

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

   auto scratch_allocator = fixed_allocator(getpagesize());
   Drash drash;

   for (int i = 0; i < argc; i++) {
      // Handle commands
      if (handle_commands((const char **)argv, argc, &drash, &scratch_allocator)) {
         return 0;
      }

      const char *arg = argv[i];

      if (is_symlink(arg)) {
         assert_err(unlink(arg) != 0, "failed to remove symlink-file");
         printf("Removed symlink: %s\n", arg);
         continue;
      }

      if (!file_exists(arg)) {
         fprintf(stderr, "file not found: %s\n", arg);
         continue;
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

      if (file_exists(file_metadata_path.buf)) {
         fprintf(stderr, "Error: file '%s' already exists in the drashcan\n", arg);
         fprintf(stderr, "Can't overwrite it\n");
         continue;
      }

      if (is_symlink(file_metadata_path.buf)) {
         fprintf(stderr, "Error: metadata file is a symlink-file\n");
         continue;
      }

      auto file_info = open_file(file_metadata_path.buf, "a");

      // @Hack: I know this is stupid to allocate memory for a static variable but
      // this way I don't have to do pedantic work of assigning a null-terminated string to another string.
      auto type = String::with_size(&scratch_allocator, 20);

      if (is_file(path.buf)) type = "file";
      else type = "directory";

      auto data = format_string(&scratch_allocator, "Path: %/%\nType: %\n", (char *)current_dir, path.buf, type.buf);
      file_info.write(data.buf);

      auto drash_file = format_string(&scratch_allocator, "%/%", drash.files.buf, filename.buf);
      move_file(arg, drash_file.buf);

      scratch_allocator.reset();
   }

   scratch_allocator.free();
}
