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

      auto drash_files = format_string("%/files", drash_dir.buf);
      err = mkdir(drash_files.buf, DIR_PERM);
      if (errno != EEXIST) {
         assert_err(err != 0, "mkdir failed to creaet drash directory");
      }

      auto metadata_files = format_string("%s/metadata", drash_dir.buf);
      err = mkdir(metadata_files.buf, DIR_PERM);
      if (errno != EEXIST) {
         assert_err(err != 0, "mkdir failed to creaet drash directory");
      }

      free(drash_dir.buf);
      files = drash_files;
      metadata = metadata_files;
   }

   // @ToDo: Confirm before wiping the files!
   void empty_drash() {
      auto command = format_string("rm -rm %", files.buf);
      assert_err(system(command.buf) != 0, "failed to remove drashd files");
      assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");
   }
};
