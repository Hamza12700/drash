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
};

File open_file(const char *file_path, const char *modes) {
   FILE *file = fopen(file_path, modes);

   if (file == NULL) fatal_error("failed to open file: %", file_path);

   return File {
      .file = file,
      .path = file_path
   };
}

bool file_exists(const char *path, bool follow_symlink = false) {
   struct stat st;
   int err = 0;

   if (follow_symlink) err = stat(path, &st);
   else err = lstat(path, &st);

   if (err != 0) return false;
   return true;
}

bool file_is_symlink(const char *path) {
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

   auto char_array = Array::make(allocator, path->nlen());
   uint file_idx;

   for (int i = path->len(); (*path)[i] != '/'; i--) {
      file_idx++;
   }

   char_array = path;
   file_idx += 1;
   char_array.skip(file_idx);

   return char_array.to_string();
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
