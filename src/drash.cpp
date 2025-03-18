#ifndef DRASH_H
#define DRASH_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "fixed_allocator.cpp"
#include "strings.cpp"

#define DIR_PERM 0740

struct Drash {
   // Heap pointer to filepath of removed files
   String files;

   // Heap pointer to filepath of metadata about the removed files
   String metadata;

   //
   // NOTE:
   //
   // This struct will live entirety of the program, so malloc'ing memory is fine,
   // even if the memory doesn't get free'd then that's fine because the OS is gona handle that.
   //
   // - Hamza, 14 March 2025
   //

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
      DIR *metadata_dir = opendir(metadata.buf);
      assert_err(metadata_dir == NULL, "failed to open drash metadata directory");

      int count = 0;
      while (readdir(metadata_dir) != NULL) {
         count++;
      }

      if (count <= 2) {
         printf("Drashcan is already empty\n");
         closedir(metadata_dir);
         return;
      }

      closedir(metadata_dir);

      auto command = format_string("rm -rf %", files.buf);
      assert_err(system(command.buf) != 0, "failed to remove drashd files");
      assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");

      DIR *dir = opendir(metadata.buf);

      struct dirent *rdir;
      while ((rdir = readdir(dir)) != NULL) {
         if (rdir->d_name[0] == '.' || strcmp(rdir->d_name, "..") == 0) continue;

         auto path = format_string("%/%", metadata.buf, rdir->d_name);
         unlink(path.buf);
      }

      closedir(dir);
   }
};

#endif // DRASH_H
