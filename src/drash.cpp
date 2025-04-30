#ifndef DRASH_H
#define DRASH_H

#include "strings.cpp"
#include "file_system.cpp"

#define DIR_PERM 0740

struct Drash_Info {
   char *path = NULL;
   File_Type type = unknown;
};

struct Drash {
   New_String files;    // Path to drash files directory
   New_String metadata; // Path to metadata about the drash files

   // Checks if metadata directory is empty or not
   bool drash_empty() const;

   // Parse the metadata for a single file
   Drash_Info parse_info(Fixed_Allocator *allocator, String *file_content) const;

   // Wipe out the drash directory
   void empty_drash(Fixed_Allocator *allocator) const;

   // List drash'd files
   void list_files(Fixed_Allocator *allocator) const;

   // Restore drash'd files
   void restore(Fixed_Allocator *allocator, const int argc, const char **argv) const;

   // Remove files in the drashcan
   void remove(Fixed_Allocator *allocator, int argc, const char **argv) const;
};

Drash init_drash() {
   const char *home_env = getenv("HOME");
   assert_err(home_env == NULL, "failed to get HOME environment variable");
   const u8 home_len = 30; // @Personal: I'm not gona have the HOME env greater than this.

   if (strlen(home_env) > home_len) {
      fprintf(stderr, "HOME environment variable too long: %zu\n", strlen(home_env));
      printf("max length %u characters\n", home_len);
      STOP;
   }

   char drash_dir[home_len+100] = {0};
   format_string(drash_dir, "%/%", home_env, (const char *)".local/share/Drash");

   int err = 0;
   err = mkdir(drash_dir, DIR_PERM);

   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   auto files = new_string(page_size);
   auto metadata = files.sub_string();

   format_string(files.buf, "%/files", drash_dir);
   err = mkdir(files.buf, DIR_PERM);
   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   format_string(metadata.buf, "%/metadata", drash_dir);
   err = mkdir(metadata.buf, DIR_PERM);
   if (errno != EEXIST) {
      assert_err(err != 0, "mkdir failed to creaet drash directory");
   }

   // Freaking C++ can't just simply assign a variable to a return value because of some wired implicit operator overloading
   Drash drash;
   drash.files.take_ref(&files);
   drash.metadata.take_ref(&metadata);
   return drash;
}

bool Drash::drash_empty() const {
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

Drash_Info Drash::parse_info(Fixed_Allocator *allocator, String *file_content) const {
   Drash_Info drash_info;
   file_content->skip(strlen("Path: ")); // Skip the prefix

   int count = 0;
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

   char type[type_len] = {0};
   for (int i = 0; (*file_content)[i] != '\n'; i++) {
      type[i] = (*file_content)[i];
   }

   if (strcmp(type, "file") != 0 && strcmp(type, "directory") != 0) {
      fprintf(stderr, "Error: Unknown 'Type' value: %s\n", type);
      STOP;
   }

   if (strcmp("file", type) == 0) drash_info.type = file;
   else drash_info.type = dir;

   return drash_info;
}

void Drash::empty_drash(Fixed_Allocator *allocator) const {
   auto dir = open_dir(metadata.buf);

   if (dir.is_empty()) {
      printf("Drashcan is already empty\n");
      return;
   }

   struct dirent *rdir;
   int count = 0;
   while ((rdir = readdir(dir.fd))) {
      if (rdir->d_name[0] == '.') continue;
      if (strcmp(rdir->d_name, "last") == 0) continue;
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

   assert(!remove_all(files.buf), "failed to remove 'drash' directory when init 'Drash' (struct)");
   assert_err(mkdir(files.buf, DIR_PERM) != 0, "failed to create drashd files directory");

   rewinddir(dir.fd); // Reset the position of the directory back to start.
   while ((rdir = readdir(dir.fd)) != NULL) {
      if (rdir->d_name[0] == '.') continue;

      auto path = format_string(allocator, "%/%", metadata.buf, rdir->d_name);
      unlink(path.buf);

      allocator->reset();
   }
}

void Drash::list_files(Fixed_Allocator *allocator) const {
   if (this->drash_empty()) {
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

      auto path = format_string(allocator, "%/%", metadata.buf, rdir->d_name);
      auto file = open_file(path.buf, "r");
      auto content = file.read_into_string(allocator);

      auto info = parse_info(allocator, &content);

      if (info.type == File_Type::file) {
         auto filename = file_basename(allocator, info.path);
         auto path = format_string("%/%", files.buf, filename.buf);

         auto file = open_file(path.buf, "r");
         auto file_len = file.file_length();

         const int kilobyte = 1000;
         const int megabyte = kilobyte*kilobyte;
         const int gigabyte = megabyte*1000;

         if (file_len > gigabyte)
            printf("- %s | %d Gb\n", info.path, file_len);
         else if (file_len > megabyte)
            printf("- %s | %d Mb\n", info.path, file_len);
         else if (file_len > kilobyte)
            printf("- %s | %d Kb\n", info.path, file_len);
         else
            printf("- %s | %d Bytes\n", info.path, file_len);

      } else printf("- %s/\n", info.path);
      allocator->reset();
   }
}

void Drash::restore(Fixed_Allocator *allocator, const int argc, const char **argv) const {
   if (this->drash_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing argment 'file|directory' name\n");
      return;
   }

   // Restore the last drash'd file/directory
   if (argv[0][0] == '-') {
      auto last = format_string(allocator, "%/last", metadata.buf);
      auto ex_res = exists(last.buf);

      if (!ex_res.found) {
         printf("Nothing to restore!\n");
         return;
      }

      auto file = open_file(last.buf, "r");
      auto filename = file.read_into_string(allocator);

      auto old_path = format_string(allocator, "%/%", files.buf, filename.buf);
      auto info_path = format_string(allocator, "%/%.info", metadata.buf, filename.buf);

      auto file_info = open_file(info_path.buf, "r");
      auto file_info_content = file_info.read_into_string(allocator);

      auto info = parse_info(allocator, &file_info_content);

      ex_res = exists(info.path);
      if (ex_res.found) {
         printf("File already exists: %s\n", info.path);
         return;
      }

      move_file(old_path.buf, info.path);
      remove_file(info_path.buf);
      remove_file(last.buf);

      return;
   }

   for (int i = 0; i < argc; i++) {

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

      if (info.type == File_Type::dir &&
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

void Drash::remove(Fixed_Allocator *allocator, int argc, const char **argv) const {
   if (this->drash_empty()) {
      printf("Drashcan is empty!\n");
      return;
   }

   if (argc == 0) {
      fprintf(stderr, "Missing argment 'file|directory' name\n");
      return;
   }

   for (int i = 0; i < argc; i++) {
      allocator->reset();

      const char *filename = argv[i];
      auto drashfile = format_string(allocator, "%/%", files.buf, (char *)filename);
      auto ex_res = exists(drashfile.buf);

      if (!ex_res.found) {
         fprintf(stderr, "File does not exists in the drashcan '%s'\n", filename);
         continue;
      }

      if (ex_res.type == dir) {
         if (!remove_all(drashfile.buf)) continue;
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
      if (content.cmp(filename)) remove_file(lastfile_path.buf);
   }
}

#endif // DRASH_H
