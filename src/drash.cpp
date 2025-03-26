#ifndef DRASH_H
#define DRASH_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "strings.cpp"
#include "file_system.cpp"

#define DIR_PERM 0740

//
// @Temporary:
//
// I should be using the 'String' type but thanks to C++ then I have to deal with
// copy constructor, assignment and other shit.
//

struct Drash_Info {
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

      // @Personal: I'm not gona have the HOME env greater than 25 chars!
      if (strlen(home_env) > 25) {
         fprintf(stderr, "HOME environment variable too long or malformed, length: %zu\n", strlen(home_env));
         printf("max length 25 characters\n");
         exit(-1);
      }

      auto drash_dir = format_string("%/%", home_env, (const char *)".local/share/Drash");

      int err = 0;
      err = mkdir(drash_dir.buf, DIR_PERM);

      // Skip the error check if the directory already exists
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

      //
      // @Hack:
      //
      // We need the temporary string buffer but because the buffer only gets deallocated
      // when it's allocated by 'malloc' that's why we're setting the 'with_allocator' boolean flag to true.
      //
      // Maybe a adding a bit-flags field is good because then if something wants a reference to the buffer,
      // it would easy because then we can just set a bit-flag.
      //

      tmp_files.with_allocator = true;
      tmp_metadata.with_allocator = true;

      files.buf = tmp_files.buf;
      metadata.buf = tmp_metadata.buf;
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
      fgets(input, 5, stdin);

      if (strcmp(input, "n\n") == 0) {
         printf("Canceled\n");
         return;
      }

      auto command = format_string("rm -rf %", files.buf);
      assert_err(system(command.buf) != 0, "failed to remove drashd files");
      assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");

      struct dirent *rdir;
      while ((rdir = readdir(*dir)) != NULL) {
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
      char *type = static_cast <char *>(xcalloc(strlen("directory")+1, sizeof(char))); // This is the max that 'Type' prefix can get.

      count = 0;
      while ((*file_content)[count] != '\n') {
         char line_char = (*file_content)[count];

         drash_info.path[count] = line_char;
         count += 1;
      }

      file_content->skip(count+1); // Skip the Path including the newline character.
      file_content->skip(strlen("Type: ")); // Skip the prefix

      count = 0;
      while ((*file_content)[count] != '\n') {
         char line_char = (*file_content)[count];

         type[count] = line_char;
         count += 1;
      }

      if (strcmp(type, "file") == 0) drash_info.type = Drash_Info::File;
      else drash_info.type = Drash_Info::Directory;

      free(type);
      return drash_info;
   }

   // List drashd files
   void list_files(Fixed_Allocator *allocator) const {
      auto dir = open_dir(metadata.buf);
      if (dir.is_empty()) {
         printf("Drashcan is empty!\n");
         return;
      }

      printf("\nDrash'd Files:\n\n");

      struct dirent *rdir;
      while ((rdir = readdir(*dir)) != NULL) {
         if (rdir->d_name[0] == '.') continue;

         auto path = format_string(allocator, "%/%", metadata.buf, rdir->d_name);
         auto file = open_file(path.buf, "r");
         auto content = file.read_into_string(allocator);

         auto info = parse_info(allocator, &content);

         if (info.type == Drash_Info::File) {
            printf("- %s\n", info.path);
         } else {
            printf("- %s/\n", info.path);
         }

         allocator->reset();
      }
   }
};

#endif // DRASH_H
