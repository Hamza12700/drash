#include <limits.h>
#include "cli.cpp"
#include "arena_allocator.cpp"

int main(int argc, char *argv[]) {
   if (argc == 1) {
      fprintf(stderr, "Missing argument file(s)\n");
      return -1;
   }

   // Skip the binary path
   argv += 1;
   argc -= 1;

   auto arena = make_arena(page_size*4);
   auto arena_alloc = arena.allocator();

   // Handle option arguments
   if (argv[0][0] == '-') {
      handle_opts(arena_alloc, argv, argc);
      return 0;
   }

   const char *current_dir = getenv("PWD");
   assert(current_dir == NULL, "PWD envirnoment not found");

   auto drash = init_drash(arena_alloc);

   auto filestat = exists(argv[0]);
   if (!filestat.found) {
      for (auto cmd : commands) {
         if (match_string(argv[0], cmd.name)) {
            handle_commands(argv, argc, &drash, arena_alloc);
            return 0;
         }
      }
   }

   // Save the last filename
   const char *file = argv[argc-1];
   auto ex_res = exists(file);
   const uint file_len = strlen(file);

   if (file_len >= PATH_MAX) {
      fprintf(stderr, "path is too long: %u\n", file_len);
      fprintf(stderr, "max path-length is %d\n", PATH_MAX);
      argc -= 1;

   } else if (!ex_res.found) {
      for (auto cmd : commands) {
         if (match_string(file, cmd.name)) {
            handle_commands(argv, argc, &drash, arena_alloc);
            return 0;
         }
      }

      fprintf(stderr, "file not found: %s\n", file);
      argc -= 1;

   } else if (ex_res.type == ft_lnk) {
      remove_file(file);
      printf("Remove symlink: %s\n", file);
      argc -= 1;

   } else {
      auto filepath = format_string(arena_alloc, "%/last", drash.metadata.buf);
      auto lastfile = open_file(filepath.buf, "w");

      auto current_file = alloc_string(arena_alloc, file);

      if (current_file[0] == '/') {
         auto filename = file_basename(arena_alloc, &current_file);
         fprintf(lastfile.fd, "%s", filename.buf);
      } else fprintf(lastfile.fd, "%s", current_file.buf);
   }


   for (int i = 0; i < argc; i++) {
      uint mem_usage = 0; // :MemoryUsage Reset the allocated memory for each iteration of the loop

      const char *arg = argv[i];
      auto filestat = exists(arg);

      if (filestat.type == ft_lnk) {
         remove_file(arg);
         printf("Removed symlink: %s\n", arg);
         continue;
      }

      if (!filestat.found) {
         fprintf(stderr, "file not found: %s\n", arg);
         continue;
      }

      const int path_len = strlen(arg);
      if (path_len >= PATH_MAX) {
         fprintf(stderr, "path is too long: %d\n", path_len);
         fprintf(stderr, "max path-length is %d\n", PATH_MAX);
         continue;
      }

      auto path = alloc_string(arena_alloc, arg);
      auto filename = file_basename(arena_alloc, &path);
      auto metadata_path = format_string(arena_alloc, "%/%.info", drash.metadata.buf, filename.buf);
      mem_usage += (path.cap + filename.cap + metadata_path.cap);

      ex_res = exists(metadata_path.buf);
      if (ex_res.found) {
         fprintf(stderr, "Error: file '%s' already exists in the drashcan\n", arg);
         fprintf(stderr, "Can't overwrite it\n");
         continue;
      }

      if (ex_res.type == ft_lnk) {
         fprintf(stderr, "Error: metadata file is a symlink-file\n");
         continue;
      }

      auto file_info = open_file(metadata_path.buf, "w");
      char type[20] = {0};
      if (is_file(path.buf)) sprintf(type, "file");
      else sprintf(type, "directory");

      fprintf(file_info.fd, "Path: %s/%s\nType: %s\n", current_dir, path.buf, type);

      auto drash_file = format_string(arena_alloc, "%/%", drash.files.buf, filename.buf);
      mem_usage += drash_file.cap;

      if (filestat.type == ft_dir) move_directory(arena_alloc, arg, drash_file.buf);
      else move_file(arena_alloc, arg, drash_file.buf);
      arena_alloc.reset(mem_usage);
   }

   arena.arena_free();
}
