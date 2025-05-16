#ifndef DRASH_H
#define DRASH_H

#include "strings.cpp"
#include "file_system.cpp"

#define DIR_PERM 0740

struct Drash_Info {
   char *path = NULL; // @Temporary: We should be using the 'New_String' type here
   File_Type type = ft_unknown;
};

struct Drash {
   New_String files;    // Path to drash files directory
   New_String metadata; // Path to metadata about the drash files

   Drash_Info parse_info(Arena_Allocator *arena, New_String *file_content) const;

   void empty_drash(Arena_Allocator *arena) const;
   void list_files(Arena_Allocator *arena) const;
   void restore(Arena_Allocator *arena, const int argc, char **argv) const;
   void remove(Arena_Allocator *arena, int argc, char **argv) const;

private:
   bool is_empty() const;
};

Drash init_drash(Arena_Allocator *arena) {
   const char *home_env = getenv("HOME");
   assert_err(home_env == NULL, "failed to get HOME environment variable");

   const u8 home_len = 30; // @Personal: I'm not gona have the HOME env greater than this.
   if (strlen(home_env) > home_len) {
      fprintf(stderr, "HOME environment variable too long: %zu\n", strlen(home_env));
      printf("max length %u characters\n", home_len);
      STOP;
   }

   char drash_dir[150] = {0};
   format_string(drash_dir, "%/%", home_env, (const char *)".local/share/Drash");

   int err = 0;
   err = mkdir(drash_dir, DIR_PERM);

   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   Drash drash;
   drash.files = alloc_string(arena, sizeof(drash_dir)+50);
   drash.metadata = alloc_string(arena, sizeof(drash_dir)+50);

   format_string(&drash.files, "%/files", drash_dir);
   err = mkdir(drash.files.buf, DIR_PERM);
   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   format_string(&drash.metadata, "%/metadata", drash_dir);
   err = mkdir(drash.metadata.buf, DIR_PERM);
   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   return drash;
}

bool Drash::is_empty() const {
   auto dir = open_dir(metadata.buf);
   struct dirent *rdir;
   int count = 0;

   while ((rdir = readdir(dir.fd))) {
      count += 1;

      if (strcmp(rdir->d_name, ".") == 0 || strcmp(rdir->d_name, "..") == 0) continue;
      if (strcmp(rdir->d_name, "last") == 0) {
         count -= 1;
         continue;
      }
   }

   if (count > 2) return false;
   return true;
}

Drash_Info Drash::parse_info(Arena_Allocator *arena, New_String *file_content) const {
   Drash_Info drash_info;
   file_content->skip(strlen("Path: ")); // Skip the prefix

   int count = 0;
   while ((*file_content)[count] != '\n') count += 1;

   drash_info.path = (char *)arena->alloc(count+1);
   const u8 type_len = strlen("directory")+1; // Max that 'Type' can hold.

   count = 0;
   while ((*file_content)[count] != '\n') {
      char line_char = (*file_content)[count];

      drash_info.path[count] = line_char;
      count += 1;
   }

   file_content->skip(count+1); // Skip the Path including the newline character.
   file_content->skip(strlen("Type: ")); // Skip the prefix

   if (strlen(file_content->buf) > type_len) {
      fprintf(stderr, "Error: 'Type' feild is invalid: %s\n", file_content->buf);
      STOP;
   }

   char type[type_len] = {0};
   for (int i = 0; (*file_content)[i] != '\n'; i++) {
      type[i] = (*file_content)[i];
   }

   if (match_string(type, "file") && match_string(type, "directory")) {
      fprintf(stderr, "Error: Unknown 'Type' value: %s\n", type);
      STOP;
   }

   if (match_string("file", type)) drash_info.type = ft_file;
   else drash_info.type = ft_dir;

   return drash_info;
}

void Drash::empty_drash(Arena_Allocator *arena) const {
   auto dir = open_dir(metadata.buf);
   if (dir.is_empty()) {
      printf("Drashcan is already empty\n");
      return;
   }

   struct dirent *rdir;
   int count = 0;
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      if (strcmp(rdir->d_name, "last") == 0) continue;
      count += 1;
   }
   rewinddir(dir.fd); // Reset the position of the directory back to start.

   printf("\nTotal entries: %d\n", count);
   printf("Empty drash directory? [Y/n]: ");

   char input[5];
   fgets(input, sizeof(input), stdin);

   if (strcmp(input, "n\n") == 0) {
      printf("Canceled\n");
      return;
   }

   if (!remove_files(arena, files.buf)) return;

   while ((rdir = readdir(dir.fd)) != NULL) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      auto path = format_string(arena, "%/%", metadata.buf, rdir->d_name);
      unlink(path.buf);
   }
}

void Drash::list_files(Arena_Allocator *arena) const {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   auto dir = open_dir(metadata.buf);
   printf("\nDrash'd Files:\n\n");

   struct dirent *rdir;
   while ((rdir = readdir(dir.fd)) != NULL) {

      // We can't simply compare the first character of 'd_name' because if a file starts with a '.' it gets skipped.
      // That's because 'd_name[256]' holds every file names with some sort-of padding between them.
      //
      // Call to 'readdir' mutates the 'd_name' pointer to point to next filename null-terminated string.

      if (strcmp(rdir->d_name, ".") == 0 || strcmp(rdir->d_name, "..") == 0) continue;
      if (strcmp(rdir->d_name, "last") == 0) continue;

      auto path = format_string(arena, "%/%", metadata.buf, rdir->d_name);
      auto file = open_file(path.buf, "r");
      auto content = file.read_into_string(arena);

      auto info = parse_info(arena, &content);

      if (info.type == ft_file) {
         auto filename = file_basename(arena, info.path);
         auto drashpath = format_string(arena, "%/%", files.buf, filename.buf);

         auto file = open_file(drashpath.buf, "r");
         auto filelen = file.file_length();

         const int kilobyte = 1000;
         const int megabyte = kilobyte*kilobyte;
         const int gigabyte = megabyte*1000;

         if (filelen > gigabyte) {
            float size = (float)filelen/gigabyte;
            printf("- %s | %.1fG\n", info.path, size);

         } else if (filelen > megabyte) {
            float size = (float)filelen/megabyte;
            printf("- %s | %.1fM\n", info.path, size);

         } else if (filelen > kilobyte) {
            float size = (float)filelen/kilobyte;
            printf("- %s | %.1fK\n", info.path, size);

         } else {
            printf("- %s | %d\n", info.path, filelen);
         }

      } else printf("- %s/\n", info.path);
   }
}

void Drash::restore(Arena_Allocator *arena, const int argc, char **argv) const {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing argment 'file|directory' name\n");
      return;
   }

   // Restore the last drash'd file/directory
   if (argv[0][0] == '-') {
      auto last = format_string(arena, "%/last", metadata.buf);
      auto last_filestat = exists(last.buf);
      if (!last_filestat.found) {
         printf("Nothing to restore!\n");
         return;
      }

      auto file = open_file(last.buf, "r");
      auto filename = file.read_into_string(arena);

      auto oldpath = format_string(arena, "%/%", files.buf, filename.buf);
      auto info_path = format_string(arena, "%/%.info", metadata.buf, filename.buf);

      auto file_info = open_file(info_path.buf, "r");
      auto file_info_content = file_info.read_into_string(arena);

      auto info = parse_info(arena, &file_info_content);
      auto filestat = exists(info.path);

      if (filestat.found) {
         if (filestat.type == ft_dir) {
            printf("Directory already exists: %s\n", info.path);
         } else if (filestat.type == ft_file) {
            printf("File already exists: %s\n", info.path);
         } else {
            fprintf(stderr, "Unknown filetype encounter: %s\n", info.path);
            return;
         }

         printf("Would like to overwrite it? [Y/n]: ");

         char input[5];
         fgets(input, sizeof(input), stdin);
         if (strcmp(input, "n\n") == 0) {
            printf("Canceled\n");
            return;
         }

         if (filestat.type == ft_dir) remove_dir(arena, info.path);
         else remove_file(info.path);

         auto drashfile = format_string(arena, "%/%", files.buf, (char *)filename.buf);
         if (filestat.type == ft_file) move_file(arena, drashfile.buf, info.path);
         else move_directory(arena, drashfile.buf, info.path);

         remove_file(info_path.buf);
         remove_file(last.buf);
         return;
      }

      auto drashfile = format_string(arena, "%/%", files.buf, (char *)filename.buf);
      if (filestat.type == ft_file) move_file(arena, drashfile.buf, info.path);
      else move_directory(arena, drashfile.buf, info.path);
      remove_file(info_path.buf);
      remove_file(last.buf);
      return;
   }

   for (int i = 0; i < argc; i++) {
      const char *file = argv[i];
      auto path = format_string(arena, "%/%.info", metadata.buf, (char *)file);

      if (!file_exists(path.buf)) {
         printf("No file or directory named: %s\n", file);
         continue;
      }

      auto info_file = open_file(path.buf, "r");
      auto content = info_file.read_into_string(arena);
      auto info = parse_info(arena, &content);
      auto filestat = exists(info.path);

      if (filestat.found) {
         if (filestat.type == ft_dir) {
            printf("Directory already exists: %s\n", info.path);
         } else if (filestat.type == ft_file) {
            printf("File already exists: %s\n", info.path);
         } else {
            fprintf(stderr, "Unknown filetype encounter: %s\n", file);
            continue;
         }

         printf("Would you like to overwrite it? [Y/n]: ");

         char input[5];
         fgets(input, sizeof(input), stdin);
         if (strcmp(input, "n\n") == 0) {
            printf("Canceled\n");
            continue;
         }

         if (filestat.type == ft_dir) remove_dir(arena, info.path);
         else remove_file(info.path);

         auto drashpath = format_string(arena, "%/%", files.buf, (char *)file);
         auto drash_filestat = exists(drashpath.buf);
         if (drash_filestat.type == ft_dir) {
            if (!move_directory(arena, drashpath.buf, info.path)) continue;
         } else if (!move_file(arena, drashpath.buf, info.path)) continue;

         remove_file(path.buf);
         continue;
      }

      auto drashpath = format_string(arena, "%/%", files.buf, (char *)file);
      auto drash_filestat = exists(drashpath.buf);

      //
      // @Todo: Highlight the missing directories, and ask the user if they want to create those directories or provide an alternative path
      //
      if (drash_filestat.type == ft_dir) {
         if (!move_directory(arena, drashpath.buf, info.path, false)) {
            if (errno == ENOENT) {
               fprintf(stderr, "Can't restore '%s' to its original location because a directory doesn't exists in:\n", file);
               fprintf(stderr, "- '%s'\n", info.path);
               continue;
            }

            report_error(arena, "Failed to restore '%'", file);
            continue;
         }
      } else if (!move_file(arena, drashpath.buf, info.path, false)) {
         if (errno == ENOENT) {
            fprintf(stderr, "Can't restore '%s' to its original location because a directory doesn't exists in:\n", file);
            fprintf(stderr, "- '%s'\n", info.path);
            continue;
         }

         report_error(arena, "Failed to restore '%'", file);
         continue;
      }

      remove_file(path.buf);
   }
}

void Drash::remove(Arena_Allocator *arena, int argc, char **argv) const {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing argment 'file|directory' name\n");
      return;
   }

   for (int i = 0; i < argc; i++) {
      const char *filename = argv[i];
      auto drashfile = format_string(arena, "%/%", files.buf, (char *)filename);
      auto ex_res = exists(drashfile.buf);

      if (!ex_res.found) {
         fprintf(stderr, "File does not exists in the drashcan '%s'\n", filename);
         continue;
      }

      if (ex_res.type == ft_dir) {
         if (!remove_dir(arena, drashfile.buf)) continue;
      } else remove_file(drashfile.buf);

      auto infopath = format_string(arena, "%/%.info", metadata.buf, (char *)filename);
      remove_file(infopath.buf);

      auto lastfile_path = format_string(arena, "%/%", metadata.buf, (char *)"last");
      auto lastfile = fopen(lastfile_path.buf, "r");
      if (!lastfile) return;

      auto last_file = File {
         .fd = lastfile,
         .path = lastfile_path.buf
      };

      auto content = last_file.read_into_string(arena);
      if (match_string(filename, content.buf)) remove_file(lastfile_path.buf);
   }
}

#endif // DRASH_H
