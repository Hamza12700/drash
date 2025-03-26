#ifndef FILE_SYS
#define FILE_SYS

#include <dirent.h>
#include <sys/stat.h>

#include "strings.cpp"
#include "array.cpp"

struct File {
   FILE *file;
   const char *path;

   ~File() {
      int err = fclose(file);

      // Closing the file shouldn't fail and if it does then it's not a big deal.
      // Just report the error.
      if (err != 0) report_error("failed to close file: %", path);
   }

   FILE * operator* () {
      return file;
   }

   uint file_length() {
      fseek(file, 0, SEEK_END);
      uint file_size = ftell(file);
      rewind(file);

      return file_size;
   }

   // Read the entire contents of the file into a string.
   String read_into_string(Fixed_Allocator *allocator) {
      const uint file_len = file_length();

      auto buf = String::with_size(allocator, file_len);
      fread(buf.buf, sizeof(char), file_len, file);

      return buf;
   }
};

struct Directory {
   DIR *dir;
   const char *path;

   ~Directory() {
      int err = closedir(dir);

      // Closing the directory also shouldn't fail and if does then report the error.
      if (err != 0) report_error("failed to close directory: %", path);
   }

   DIR * operator* () {
      return dir;
   }

   // Resets the directory stream back to the beginning of the directory.
   bool is_empty() {
      uint count = 0;
      while (readdir(dir) != NULL) count += 1;

      if (count <= 2) {
         rewinddir(dir);
         return true;
      }

      rewinddir(dir);
      return false;
   }
};

File open_file(const char *file_path, const char *modes) {
   FILE *file = fopen(file_path, modes);

   if (file == NULL) fatal_error("failed to open file: %", file_path);

   return File {
      .file = file,
      .path = file_path
   };
}

bool file_exists(const char *path) {
   FILE *file = fopen(path, "r");

   if (file == NULL) return false;

   fclose(file);
   return true;
}

bool is_symlink(const char *path) {
   struct stat st;
   if (lstat(path, &st) != 0) return false;

   if ((st.st_mode & S_IFMT) == S_IFLNK) return true;
   return false;
}

bool is_file(const char *path) {
   struct stat st;
   if (lstat(path, &st) != 0) return false;

   if ((st.st_mode & S_IFMT) == S_IFREG) return true;
   return false;
}

bool is_dir(const char *path) {
   struct stat st;
   if (lstat(path, &st) != 0) return false;

   if ((st.st_mode & S_IFMT) == S_IFDIR) return true;
   return false;
}

String file_basename(Fixed_Allocator *allocator, const String *path) {
   bool contain_slash = false;
   for (uint i = 0; i < path->len(); i++) {
      if ((*path)[i] == '/') {
         contain_slash = true;
         break;
      }
   }

   if (!contain_slash) return *path;

   auto buffer = String::with_size(allocator, path->len());
   uint file_idx = 0;

   for (int i = path->len(); (*path)[i] != '/'; i--) {
      file_idx++;
   }

   buffer = path->buf;
   buffer.skip(buffer.len() - file_idx+1);

   return buffer;
}

Directory open_dir(const char *dir_path) {
   DIR *dir = opendir(dir_path);
   if (dir == NULL) fatal_error("failed to open directory: %", dir_path);

   return Directory {
      .dir = dir,
      .path = dir_path,
   };
}

#endif // FILE_SYS
