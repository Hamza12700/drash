#ifndef FILE_SYS
#define FILE_SYS

#include <dirent.h>

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
