#ifndef FILE_SYS
#define FILE_SYS

#include <dirent.h>
#include <sys/stat.h>

#include "strings.cpp"

struct File {
   FILE *file;
   const char *path;

   ~File() {
      int err = fclose(file);
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
      if (err != 0) report_error("failed to close directory: %", path);
   }

   DIR * operator* () {
      return dir;
   }
};

File open_file(const char *file_path, const char *modes) {
   FILE *file = fopen(file_path, modes);

   if (file == NULL) {
      report_error("failed to open file: %", file_path);
   }

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

Directory open_dir(const char *dir_path) {
   DIR *dir = opendir(dir_path);

   if (dir == NULL) {
      report_error("failed to open directory: %", dir_path);
   }

   return Directory {
      .dir = dir,
      .path = dir_path,
   };
}

#endif // FILE_SYS
