#ifndef FILE_SYS
#define FILE_SYS

#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

#include "strings.cpp"

struct File {
   FILE *fd;
   const char *path;

   ~File();
   long file_length();

   // Read the entire contents of the file into a string.
   New_String read_into_string(Arena *arena);
   New_String read_into_string();
};

File::~File() {
   if (fclose(fd) != 0)
      fprintf(stderr, "failed to close the file-descriptor of '%s'\n", path);
}

long File::file_length() {
   fseek(fd, 0, SEEK_END);
   long file_size = ftell(fd);
   rewind(fd);

   return file_size;
}

New_String File::read_into_string(Arena *arena) {
   const int filelen = file_length();
   auto buf = alloc_string(arena, filelen);
   fread(buf.buf, 1, filelen, fd);
   return buf;
}

New_String File::read_into_string() {
   const int filelen = file_length();
   auto buf = malloc_string(filelen);
   fread(buf.buf, 1, filelen, fd);
   return buf;
}

struct Directory {
   DIR *fd;
   const char *path;

   ~Directory();
   bool is_empty();
};

Directory::~Directory() { // @Temporary: Not checking the error-code
   if (closedir(fd) != 0)
      fprintf(stderr, "failed to close the (directory) file-descriptor of '%s'\n", path);
}

bool Directory::is_empty() {
   int count = 0;
   while (readdir(fd) != NULL) count += 1;

   if (count <= 2) {
      rewinddir(fd);
      return true;
   }

   rewinddir(fd);
   return false;
}

File open_file(const char *file_path, const char *modes) {
   FILE *file = fopen(file_path, modes);

   if (file == NULL) {
      fprintf(stderr, "failed to open file: %s\n", file_path);
      abort();
   }

   return File {
      .fd = file,
      .path = file_path
   };
}

bool file_exists(const char *path) {
   FILE *file = fopen(path, "r");

   if (file == NULL) return false;

   fclose(file);
   return true;
}

bool dir_exists(const char *path) {
   DIR *dir = opendir(path);
   if (dir == NULL) {
      closedir(dir);
      return false;
   }

   closedir(dir);
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

// Return's true on success otherwise false
bool remove_file(const char *path) {
   if (unlink(path) != 0) {
      fprintf(stderr, "failed to remove file '%s'\n", path);
      return false;
   }

   return true;
}

// Copies the (oldpath) file to file (newpath) and removes the oldpath
bool copy_move_file(Arena *arena, const char *oldpath, const char *newpath) {
   auto file = open_file(oldpath, "rb");
   auto file_content = file.read_into_string(arena);

   auto newfile = open_file(newpath, "wb");
   write(fileno(newfile.fd), file_content.buf, file_content.cap); // @Speed @Memory: After writing the file contents, the allocated space becomes garbage.
   return remove_file(oldpath);
}

// Move file and handle different filesystems.
// The 'display_err' doesn't get inherit from other function calls. It only get's applied to 'rename' syscall
bool move_file(Arena *arena, const char *oldpath, const char *newpath, bool display_err = true) {
   if (rename(oldpath, newpath) != 0) {
      if (errno != EXDEV) {
         if (display_err) {
            fprintf(stderr, "failed to move '%s' to '%s'\n", oldpath, newpath);
            return false;
         }
         return false;
      }

      return copy_move_file(arena, oldpath, newpath);
   }

   return true;
}

New_String file_basename(Arena *arena, New_String *path) {
   const int pathlen = path->len();

   if ((*path)[pathlen-1] == '/') {
      path->remove(pathlen-1);
   }

   bool contain_slash = false;
   for (int i = 0; i < pathlen; i++) {
      if ((*path)[i] == '/') {
         contain_slash = true;
         break;
      }
   }

   if (!contain_slash) return *path;

   auto buffer = alloc_string(arena, pathlen+1);
   int file_idx = 0;

   for (int i = strlen(path->buf); (*path)[i] != '/'; i--) {
      file_idx++;
   }

   buffer.assign_string(path->buf);
   buffer.skip(strlen(buffer.buf) - file_idx+1);

   return buffer;
}

New_String file_basename(Arena *arena, const char *path) {
   New_String tmp = {0};
   tmp.buf = (char *)path;
   tmp.cap = strlen(path)+1;
   tmp.arena = arena;
   tmp.flags |= Referenced;
   return file_basename(arena, &tmp);
}

Directory open_dir(const char *dir_path) {
   DIR *dir = opendir(dir_path);
   if (dir == NULL) {
      fprintf(stderr, "failed to open directory: %s\n", dir_path);
      abort();
   }

   return Directory {
      .fd = dir,
      .path = dir_path,
   };
}

enum File_Type : u8 {
   ft_file = 0,
   ft_dir,
   ft_lnk,
   ft_unknown,
};

struct Ex_Res {
   // Should be 16 bits wide? [en.wikipedia.org/wiki/Unix_file_types#Numeric]
   u16 perms = 0; // @Robustness: Validate if this is correct.

   bool found = false;
   File_Type type = ft_unknown;
};

// Check if file|directory exists
Ex_Res exists(const char *path) {
   struct stat st;
   Ex_Res ret;

   if (lstat(path, &st) != 0) return ret;

   ret.perms = st.st_mode & 07777; // Masks out the file-type upper bits and only store file-permission bits
   auto st_mode = st.st_mode & S_IFMT;

   switch (st_mode) {
      case S_IFREG: {
         ret.type = ft_file;
         ret.found = true;
         return ret;
      }

      case S_IFDIR: {
         ret.type = ft_dir;
         ret.found = true;
         return ret;
      }

      case S_IFLNK: {
         ret.type = ft_lnk;
         ret.found = true;
         return ret;
      }
   }

   return ret;
}

bool makedir(const char *dirpath, uint modes, bool panic = true) {
   if (mkdir(dirpath, modes) != 0) {
      if (panic) {
         fprintf(stderr, "failed to create directory at %s\n", dirpath);
         raise(SIGINT);
      }
      return false;
   }
   return true;
}

// Remove all files inside a directory
bool remove_files(Arena *arena, const char *dirpath) {
   auto dir = open_dir(dirpath);
   struct dirent *rdir;
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;

      auto fullpath = format_string(arena, "%/%", (char *)dirpath, rdir->d_name);
      auto filestat = exists(fullpath.buf);
      if (filestat.type == ft_file || filestat.type == ft_lnk) {
         if (!remove_file(fullpath.buf)) return false;
      } else if (!remove_files(arena, fullpath.buf)) return false;
   }

   return true;
}

bool remove_dir(Arena *arena, const char *dirpath) {
   auto dir = open_dir(dirpath);
   struct dirent *rdir;
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;

      auto fullpath = format_string(arena, "%/%", (char *)dirpath, rdir->d_name);
      auto filestat = exists(fullpath.buf);
      if (filestat.type == ft_file || filestat.type == ft_lnk) {
         if (!remove_file(fullpath.buf)) return false;
      } else if (!remove_dir(arena, fullpath.buf)) return false;
   }

   rewinddir(dir.fd);
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      auto fullpath = format_string(arena, "%/%", (char *)dirpath, rdir->d_name);

      if (rmdir(fullpath.buf) != 0) {
         fprintf(stderr, "failed to remove directory\n");
         perror("rmdir -");
         return false;
      }

      if (!remove_dir(arena, fullpath.buf)) return false;
   }

   if (rmdir(dirpath) != 0) { // Lastly, remove the parent directory
      fprintf(stderr, "failed to remove directory\n");
      perror("rmdir -");
      return false;
   }

   return true;
}

bool move_directory_copy(Arena *arena, const char *oldpath, const char *newpath) {
   auto dir = open_dir(oldpath);
   struct dirent *rdir;
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;

      auto old_filepath = format_string(arena, "%/%", (char *)oldpath, rdir->d_name);
      auto new_filepath = format_string(arena, "%/%", (char *)newpath, rdir->d_name);
      auto filestat = exists(old_filepath.buf);

      if (filestat.type == ft_file || filestat.type == ft_lnk) {
         if (!move_file(arena, old_filepath.buf, new_filepath.buf)) return false;
      } else {
         if (!makedir(new_filepath.buf, filestat.perms, false)) return false; // Create the new directory with same file-perms of the old-directory
         if (!move_directory_copy(arena, old_filepath.buf, new_filepath.buf)) return false;
      }
   }

   rewinddir(dir.fd);
   while ((rdir = readdir(dir.fd))) {
      if (match_string(rdir->d_name, ".") || match_string(rdir->d_name, "..")) continue;
      auto new_filepath = format_string(arena, "%/%", (char *)oldpath, rdir->d_name);

      if (rmdir(new_filepath.buf) != 0) {
         fprintf(stderr, "failed to remove directory\n");
         perror("rmdir -");
         return false;
      }

      if (!remove_dir(arena, new_filepath.buf)) return false;
   }

   if (rmdir(oldpath) != 0) { // Lastly, remove the parent directory
      fprintf(stderr, "failed to remove directory\n");
      perror("rmdir -");
      return false;
   }

   return true;
}

// Move direcotry and handle different filesystems.
// The 'display_err' doesn't get inherit from other function calls. It only get's applied to 'rename' syscall
bool move_directory(Arena *arena, const char *oldpath, const char *newpath, bool display_err = true) {
   if (rename(oldpath, newpath) != 0) {
      if (errno != EXDEV) {
         if (display_err) {
            report_error(arena, "failed to move '%' to '%'\n", oldpath, newpath);
            return false;
         }
         return false;
      }

      auto dirstat = exists(oldpath);
      makedir(newpath, dirstat.perms);
      return move_directory_copy(arena, oldpath, newpath);
   }

   return true;
}

#endif // FILE_SYS
