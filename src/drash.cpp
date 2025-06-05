#ifndef DRASH_H
#define DRASH_H

#include "strings.cpp"
#include "file_system.cpp"

#define DIR_PERM 0740

struct Drash_Info {
   New_String path = {};
   File_Type type = ft_unknown;
};

struct Drash {
   New_String files;    // Path to drash files directory
   New_String metadata; // Path to metadata about the drash files

   Drash_Info parse_info(Allocator allocator, New_String *file_content);

   void empty_drash(Allocator allocator);
   void list_files(Allocator allocator);
   void cat(Allocator allocator, const char *filename);
   void restore(Allocator allocator, int argc, char **argv);
   void remove(Allocator allocator, int argc, char **argv);
   bool is_empty();
};

Drash init_drash(Allocator allocator) {
   const char *home_env = getenv("HOME");
   assert_err(home_env == NULL, "failed to get HOME environment variable");

   const u8 home_len = 30; // @Personal: I'm not gona have the HOME env greater than this.
   if (strlen(home_env) > home_len) {
      fprintf(stderr, "HOME environment variable too long: %zu\n", strlen(home_env));
      printf("max length %u characters\n", home_len);
      raise(SIGINT);
   }

   char drash_dir[150] = {0};
   format_string(drash_dir, "%/%", home_env, (const char *)".local/share/Drash"); // @Cleanup: Just put it in the HOME/.drash? Why it needs be in .local/share?

   Drash drash = {};
   drash.files = alloc_string(allocator, sizeof(drash_dir)+50);
   format_string(&drash.files, "%/files", drash_dir);

   drash.metadata = alloc_string(allocator, sizeof(drash_dir)+50);
   format_string(&drash.metadata, "%/metadata", drash_dir);

   auto filestat = exists(drash_dir);
   if (filestat.type != ft_dir) {
      fprintf(stderr, "'%s' isn't a directory\n", drash_dir);
      raise(SIGINT);
   }

   if (filestat.found) return drash;

   makedir(drash_dir, DIR_PERM);
   makedir(drash.files.buf, DIR_PERM);
   makedir(drash.metadata.buf, DIR_PERM);
   return drash;
}

bool Drash::is_empty() {
   auto dir = open_dir(metadata.buf);
   struct dirent *rdir;
   int count = 0;

   while ((rdir = readdir(dir.fd))) {
      count += 1;

      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      if (match_string(rdir->d_name, "last")) {
         count -= 1;
         continue;
      }
   }

   if (count > 2) return false;
   return true;
}

Drash_Info Drash::parse_info(Allocator allocator, New_String *file_content) {
   Drash_Info drash_info = {};
   file_content->skip(strlen("Path: ")); // Skip the prefix

   int count = 0;
   while ((*file_content)[count] != '\n') count += 1;

   drash_info.path = alloc_string(allocator, count);
   for (int i = 0; i < count; i++) {
      drash_info.path[i] = (*file_content)[i];
   }

   file_content->skip(count+1); // Skip the Path including the newline character.
   file_content->skip(strlen("Type: ")); // Skip the prefix

   const u8 type_len = strlen("directory")+1; // Max that 'Type' can hold.
   if (strlen(file_content->buf) > type_len) {
      fprintf(stderr, "Error: 'Type' feild is invalid: %s\n", file_content->buf);
      abort();
   }

   char type[20] = {0};
   file_content->remove(file_content->len()-1); // Remove the newline-character
   snprintf(type, type_len, "%s", file_content->buf);

   if (!match_string(type, "file") && !match_string(type, "directory")) {
      fprintf(stderr, "Error: Unknown 'Type' value: %s\n", type);
      abort();
   }

   if (match_string("file", type)) drash_info.type = ft_file;
   else drash_info.type = ft_dir;
   return drash_info;
}

void Drash::cat(Allocator allocator, const char *filename) {
   if (!filename) {
      fprintf(stderr, "No filename provided!\n");
      return;
   }
   auto file = file_basename(allocator, filename);
   auto fullpath = format_string(allocator, "%/%", files.buf, file.buf);
   auto filestat = exists(fullpath.buf);
   if (!filestat.found) {
      fprintf(stderr, "No filename '%s' in the drashcan!\n", file.buf);
      return;
   }

   if (filestat.type == ft_dir) {
      fprintf(stderr, "The provided filename is a directory\n");
      return;
   }
   if (filestat.type != ft_file) {
      fprintf(stderr, "Unknown filetype: '%s'\n", file.buf);
      return;
   }

   auto drashfile = open_file(fullpath.buf, "r");
   auto content = drashfile.read_into_string(allocator);
   write(STDOUT_FILENO, content.buf, content.cap);
}

void Drash::empty_drash(Allocator allocator) {
   auto dir = open_dir(files.buf);
   if (dir.is_empty()) {
      printf("Drashcan is already empty\n");
      return;
   }

   struct dirent *rdir = NULL;
   int count = 0;
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      count += 1;
   }

   printf("\nTotal entries: %d\n", count);
   printf("Empty drash directory? [Y/n]: ");

   char input[5];
   fgets(input, sizeof(input), stdin);

   if (strcmp(input, "n\n") == 0) {
      printf("Canceled\n");
      return;
   }

   remove_dir(allocator, files.buf);
   makedir(files.buf, DIR_PERM);
   remove_files(allocator, metadata.buf);
}

void Drash::list_files(Allocator allocator) {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   auto dir = open_dir(metadata.buf);
   printf("\nDrash'd Files:\n\n");

   struct dirent *rdir;
   while ((rdir = readdir(dir.fd)) != NULL) {
      uint mem_usage = 0;

      // We can't simply compare the first character of 'd_name' because if a file starts with a '.' then it gets skipped.
      // That's because 'd_name[256]' holds every file names with some sort-of padding between them.
      //
      // Call to 'readdir' mutates the 'd_name' pointer to point to next filename null-terminated string.

      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      if (match_string(rdir->d_name, "last")) continue;

      auto path = format_string(allocator, "%/%", metadata.buf, rdir->d_name);
      auto file = open_file(path.buf, "r");
      auto content = file.read_into_string(allocator);

      auto info = parse_info(allocator, &content);
      mem_usage += (path.cap + content.cap + info.path.cap);

      if (info.type == ft_file) {
         auto filename = file_basename(allocator, &info.path);
         auto drashpath = format_string(allocator, "%/%", files.buf, filename.buf);
         mem_usage += filename.cap + drashpath.cap;

         auto file = open_file(drashpath.buf, "r");
         auto filelen = file.file_length();

         const int kilobyte = 1000;
         const int megabyte = kilobyte*kilobyte;
         const int gigabyte = megabyte*1000;

         if (filelen > gigabyte) {
            float size = (float)filelen/gigabyte;
            printf("- %s | %.0fG\n", info.path.buf, size);

         } else if (filelen > megabyte) {
            float size = (float)filelen/megabyte;
            printf("- %s | %.0fM\n", info.path.buf, size);

         } else if (filelen > kilobyte) {
            float size = (float)filelen/kilobyte;
            printf("- %s | %.1fK\n", info.path.buf, size);

         } else {
            printf("- %s | %ld\n", info.path.buf, filelen);
         }

      } else printf("- %s/\n", info.path.buf);

      allocator.reset(mem_usage);
   }
}

void Drash::restore(Allocator allocator, const int argc, char **argv) {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing arugment 'file|directory' name\n");
      return;
   }

   // Restore the last drash'd file/directory
   if (argv[0][0] == '-') {
      auto last = format_string(allocator, "%/last", metadata.buf);
      auto last_filestat = exists(last.buf);
      if (!last_filestat.found) {
         printf("Nothing to restore!\n");
         return;
      }

      auto file = open_file(last.buf, "r");
      auto filename = file.read_into_string(allocator);
      {
         auto filelen = filename.len();
         if (filename[filelen-1] == '/') filename.remove(filelen - 1);
      }

      auto info_path = format_string(allocator, "%/%.info", metadata.buf, filename.buf);
      auto file_info = open_file(info_path.buf, "r");
      auto file_info_content = file_info.read_into_string(allocator);

      auto info = parse_info(allocator, &file_info_content);
      auto filestat = exists(info.path.buf);

      if (filestat.found) {
         if (filestat.type == ft_dir) {
            printf("Directory already exists: %s\n", info.path.buf);
         } else if (filestat.type == ft_file) {
            printf("File already exists: %s\n", info.path.buf);
         } else {
            fprintf(stderr, "Unknown filetype encounter: %s\n", info.path.buf);
            return;
         }

         printf("Would like to overwrite it? [Y/n]: ");

         char input[5];
         fgets(input, sizeof(input), stdin);
         if (strcmp(input, "n\n") == 0) {
            printf("Canceled\n");
            return;
         }

         if (filestat.type == ft_dir) remove_dir(allocator, info.path.buf);
         else remove_file(info.path.buf);

         auto drashfile = format_string(allocator, "%/%", files.buf, (char *)filename.buf);
         if (filestat.type == ft_file) move_file(allocator, drashfile.buf, info.path.buf);
         else move_directory(allocator, drashfile.buf, info.path.buf);

         remove_file(info_path.buf);
         remove_file(last.buf);
         return;
      }

      auto drashfile = format_string(allocator, "%/%", files.buf, (char *)filename.buf);
      if (filestat.type == ft_file) move_file(allocator, drashfile.buf, info.path.buf);
      else move_directory(allocator, drashfile.buf, info.path.buf);
      remove_file(info_path.buf);
      remove_file(last.buf);
      return;
   }

   for (int i = 0; i < argc; i++) {
      const char *file = argv[i];
      auto path = format_string(allocator, "%/%.info", metadata.buf, (char *)file);

      if (!file_exists(path.buf)) {
         printf("No file or directory named: %s\n", file);
         continue;
      }

      auto info_file = open_file(path.buf, "r");
      auto content = info_file.read_into_string(allocator);
      auto info = parse_info(allocator, &content);
      auto filestat = exists(info.path.buf);

      if (filestat.found) {
         if (filestat.type == ft_dir) {
            printf("Directory already exists: %s\n", info.path.buf);
         } else if (filestat.type == ft_file) {
            printf("File already exists: %s\n", info.path.buf);
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

         if (filestat.type == ft_dir) remove_dir(allocator, info.path.buf);
         else remove_file(info.path.buf);

         auto drashpath = format_string(allocator, "%/%", files.buf, (char *)file);
         auto drash_filestat = exists(drashpath.buf);
         if (drash_filestat.type == ft_dir) {
            if (!move_directory(allocator, drashpath.buf, info.path.buf)) continue;
         } else if (!move_file(allocator, drashpath.buf, info.path.buf)) continue;

         remove_file(path.buf);
         continue;
      }

      auto drashpath = format_string(allocator, "%/%", files.buf, (char *)file);
      auto drash_filestat = exists(drashpath.buf);

      //
      // @Todo: Highlight the missing directories, and ask the user if they want to create those directories or provide an alternative path
      //
      if (drash_filestat.type == ft_dir) {
         if (!move_directory(allocator, drashpath.buf, info.path.buf, false)) {
            if (errno == ENOENT) {
               fprintf(stderr, "Can't restore '%s' to its original location because a directory doesn't exists in:\n", file);
               fprintf(stderr, "- '%s'\n", info.path.buf);
               continue;
            }

            fprintf(stderr, "[Error]: Failed to restore '%s'\n", file);
            fprintf(stderr, "- '%s'\n", strerror(errno));
            continue;
         }
      } else if (!move_file(allocator, drashpath.buf, info.path.buf, false)) {
         if (errno == ENOENT) {
            fprintf(stderr, "Can't restore '%s' to its original location because a directory doesn't exists in:\n", file);
            fprintf(stderr, "- '%s'\n", info.path.buf);
            continue;
         }

         fprintf(stderr, "[Error]: Failed to restore '%s'\n", file);
         fprintf(stderr, "- '%s'\n", strerror(errno));
         continue;
      }

      remove_file(path.buf);

      auto filename = file_basename(allocator, file);
      auto lastfile_path = format_string(allocator, "%/last", metadata.buf);
      auto lastfile_info = exists(lastfile_path.buf);
      if (!lastfile_info.found) return;

      auto lastfile = open_file(lastfile_path.buf, "r");
      auto lastfile_name = lastfile.read_into_string(allocator);
      auto filelen = lastfile_name.len();
      if (lastfile_name[filelen-1] == '/') lastfile_name.remove(filelen - 1);

      if (match_string(&lastfile_name, &filename)) remove_file(lastfile_path.buf);
   }
}

void Drash::remove(Allocator allocator, int argc, char **argv) {
   if (this->is_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing argument 'file|directory' name\n");
      return;
   }

   for (int i = 0; i < argc; i++) {
      const char *filename = argv[i];
      auto drashfile = format_string(allocator, "%/%", files.buf, (char *)filename);
      auto ex_res = exists(drashfile.buf);

      if (!ex_res.found) {
         fprintf(stderr, "File does not exists in the drashcan '%s'\n", filename);
         continue;
      }

      if (ex_res.type == ft_dir) {
         if (!remove_dir(allocator, drashfile.buf)) continue;
      } else remove_file(drashfile.buf);

      auto infopath = format_string(allocator, "%/%.info", metadata.buf, (char *)filename);
      remove_file(infopath.buf);

      auto lastfile_path = format_string(allocator, "%/%", metadata.buf, (char *)"last");
      auto lastfile = fopen(lastfile_path.buf, "r");
      if (!lastfile) return;

      auto last_file = File {
         .fd = lastfile,
         .path = lastfile_path.buf
      };

      auto content = last_file.read_into_string(allocator);
      if (match_string(filename, content.buf)) remove_file(lastfile_path.buf);
   }
}

#endif // DRASH_H
