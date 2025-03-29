#ifndef DRASH_H
#define DRASH_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "strings.cpp"
#include "file_system.cpp"

#define DIR_PERM 0740

struct Drash_Info {
   // @Temporary: We should be using the 'String' type but thanks to C++ then
   // we have to deal with copy constructor, assignment and other shit.
   char *path = NULL;

   enum Type {
      None,
      File,
      Directory,
   };

   Type type = None;
};

struct Drash {
   String files; // Drashd files
   String metadata; // Info about drashd files

   Drash() {
      const char *home_env = getenv("HOME");
      assert_err(home_env == NULL, "failed to get HOME environment variable");
      const u8 home_len = 30;

      // @Personal: I'm not gona have the HOME env greater than 'home_len' chars!
      if (strlen(home_env) > home_len) {
         fprintf(stderr, "HOME environment variable too long: %zu\n", strlen(home_env));
         printf("max length %u characters\n", home_len);
         STOP;
      }

      //
      // @Speed:
      //
      // We know the length of the 'HOME' env variable which won't be larger than 'home_len'
      // but we're treating it as dynamic variable because we want the lifetime of this struct of the entire program.
      // We can't use a custom allocator because other part of the program could reset the memory at any time.
      //
      // - Hamza, 29 March 2025
      //

      auto drash_dir = format_string("%/%", home_env, (const char *)".local/share/Drash");

      int err = 0;
      err = mkdir(drash_dir.buf, DIR_PERM);

      if (errno != EEXIST) {
         assert_err(err != 0, "mkdir failed to creaet drash directory");
      }

      auto tmp_files = format_string("%/files", drash_dir.buf);
      err = mkdir(tmp_files.buf, DIR_PERM);
      if (errno != EEXIST) {
         assert_err(err != 0, "mkdir failed to creaet drash directory");
      }

      auto tmp_metadata = format_string("%/metadata", drash_dir.buf);
      err = mkdir(tmp_metadata.buf, DIR_PERM);
      if (errno != EEXIST) {
         assert_err(err != 0, "mkdir failed to creaet drash directory");
      }

      files.take_reference(&tmp_files);
      metadata.take_reference(&tmp_metadata);
   }

   // @TODO: Confirm before wiping out the files!
   void empty_drash() const {
      auto dir = open_dir(metadata.buf);

      if (dir.is_empty()) {
         printf("Drashcan is already empty\n");
         return;
      }

      printf("Empty drash directory? [Y/n]: ");

      char input[5];
      fgets(input, sizeof(input), stdin);

      if (strcmp(input, "n\n") == 0) {
         printf("Canceled\n");
         return;
      }

      auto command = format_string("rm -rf %", files.buf);
      assert_err(system(command.buf) != 0, "failed to remove drashd files");
      assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");

      struct dirent *rdir;
      while ((rdir = readdir(dir.fd)) != NULL) {
         if (rdir->d_name[0] == '.') continue;

         auto path = format_string("%/%", metadata.buf, rdir->d_name);
         unlink(path.buf);
      }
   }

   Drash_Info parse_info(Fixed_Allocator *allocator, String *file_content) const {
      Drash_Info drash_info;
      file_content->skip(strlen("Path: ")); // Skip the prefix

      uint count = 0;
      while ((*file_content)[count] != '\n') count += 1;

      drash_info.path = static_cast <char *>(allocator->alloc(count+1));
      const u8 type_len = strlen("directory")+1; // Max that 'Type' can hold.

      count = 0;
      while ((*file_content)[count] != '\n') {
         char line_char = (*file_content)[count];

         drash_info.path[count] = line_char;
         count += 1;
      }

      file_content->skip(count+1); // Skip the Path including the newline character.
      file_content->skip(strlen("Type: ")); // Skip the prefix

      if (file_content->len() > type_len) {
         fprintf(stderr, "Error: 'Type' feild is invalid: %s\n", file_content->buf);
         STOP;
      }

      count = 0;
      char type[type_len] = {0};

      while ((*file_content)[count] != '\n') {
         char line_char = (*file_content)[count];

         type[count] = line_char;
         count += 1;
      }

      if (strcmp(type, "file") != 0 && strcmp(type, "directory") != 0) {
         fprintf(stderr, "Error: Unknown 'Type' value: %s\n", type);
         STOP;
      }

      if (strcmp("file", type) == 0)
         drash_info.type = Drash_Info::File;
      else
         drash_info.type = Drash_Info::Directory;

      return drash_info;
   }

   bool is_empty() const {
      auto dir = open_dir(metadata.buf);

      if (dir.is_empty()) return true;
      return false;
   }

   // List drashd files
   void list_files(Fixed_Allocator *allocator) const {
      if (this->is_empty()) {
         printf("Drashcan is empty!\n");
         return;
      }

      auto dir = open_dir(metadata.buf);
      printf("\nDrash'd Files:\n\n");

      struct dirent *rdir;
      while ((rdir = readdir(dir.fd)) != NULL) {
         if (rdir->d_name[0] == '.') continue;

         auto path = format_string(allocator, "%/%", metadata.buf, rdir->d_name);
         auto file = open_file(path.buf, "r");
         auto content = file.read_into_string(allocator);

         auto info = parse_info(allocator, &content);

         if (info.type == Drash_Info::File)
            printf("- %s\n", info.path);
         else
            printf("- %s/\n", info.path);

         allocator->reset();
      }
   }

   void restore(Fixed_Allocator *allocator, const uint argc, const char **argv) const {
      if (this->is_empty()) {
         printf("Drashcan is empty!\n");
         return;
      }

      if (argc == 0) {
         fprintf(stderr, "Missing argment 'file|directory' name\n");
         return;
      }

      for (uint i = 0; i < argc; i++) {

         // Reset the allocator so that I don't have to do it again and again.
         allocator->reset();

         const char *file = argv[i];
         auto path = format_string(allocator, "%/%.info", metadata.buf, (char *)file);

         if (!file_exists(path.buf)) {
            printf("No file or directory named: %s\n", file);
            continue;
         }

         auto info_file = open_file(path.buf, "r");
         auto content = info_file.read_into_string(allocator);
         auto info = parse_info(allocator, &content);

         if (info.type == Drash_Info::Directory &&
            dir_exists(info.path))
         {
            printf("Directory already exists: %s\n", info.path);
            continue;
         }

         if (file_exists(info.path)) {
            printf("File already exists: %s\n", info.path);
            continue;
         }

         auto stored_path = format_string(allocator, "%/%", files.buf, (char *)file);
         int err = rename(stored_path.buf, info.path);

         if (err != 0) {
            report_error("failed to move '%' to '%'", stored_path.buf, info.path);
            continue;
         }

         remove_file(path.buf);
      }
   }
};

#endif // DRASH_H
