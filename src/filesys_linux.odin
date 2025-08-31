package main

import "core:sys/linux"
import "core:sys/posix"
import "core:os"
import "core:strings"
import "core:mem"
import "core:fmt"

File_Info :: struct {
  fullpath: string, // allocated
  name:     string, // uses 'fullpath' as underlying data
  size:     uint,
  type:     File_Type,
}

File_Type :: enum {
  Regular,
  Directory,
  Symlink,
  Socket,
  Named_Pipe,
  Block_Device,
  Character_Device,
}

@(require_results)
filestat :: proc(path: string, allocator := context.allocator) -> (File_Info, linux.Errno) {
  using linux;

  fullpath : string;
  if path[0] != '/' { // Path is not an absolute-path
    cwd, err := get_working_directory(allocator);
    if err != .NONE do return {}, err;

    fullpath = fmt.aprintf("%s/%s", cwd, path, allocator=allocator);
  } else {
    fullpath = path; 
  }

  st: Stat = ---;
  err := lstat(strings.clone_to_cstring(fullpath, context.temp_allocator), &st);
  if err != .NONE do return {}, err;

  type : File_Type;
  switch (st.mode & S_IFMT) {
  case S_IFBLK:  type = .Block_Device
  case S_IFCHR:  type = .Character_Device
  case S_IFDIR:  type = .Directory
  case S_IFIFO:  type = .Named_Pipe
  case S_IFLNK:  type = .Symlink
  case S_IFREG:  type = .Regular
  case S_IFSOCK: type = .Socket
  }

  info := File_Info{
    fullpath = fullpath,
    name     = get_basename(fullpath),
    size     = st.size,
    type     = type,
  };

  return info, .NONE;
}

get_basename :: proc(path: string) -> string {
  if path == "" do return ".";

	is_separator :: proc(c: byte) -> bool {
		return c == '/'
	}

  path := path;
  if path[len(path)-1] == '/' do path = path[:len(path)-1];

	i := len(path)-1;
	for i >= 0 && !is_separator(path[i]) {
		i -= 1;
	}

	if i >= 0 {
		path = path[i+1:];
	}

	if path == "" {
		return "/";
	}

	return path;
}

get_working_directory :: proc(allocator: mem.Allocator) -> (string, linux.Errno) {
  // Maximum path-length on most Linux-Systems
	PATH_MAX :: 4096;
	buf := make([dynamic]u8, PATH_MAX, allocator);

	for {
    #no_bounds_check n, errno := linux.getcwd(buf[:]);
    if errno == .NONE {
      return string(buf[:n-1]), nil;
    }
    if errno != .ERANGE {
      return "", errno;
    }

    resize(&buf, len(buf)+PATH_MAX);
	}
}

// Recursively, remove files/directories
// 
// @NOTE:
// This function explicity takes the 'Arena' allocator data because it needs to perform
// operations that are only valid for that allocator and because the 'context.allocator'
// data is a 'rawptr (void *)' it can be type-casted to anything. To avoid this problem
// the function require's the allocator data to explicity passed in.
// 
// Because this function is recursive, every function-call saves the allocator state so
// when a recursive call is done it would reset the allocator to its previous state.
//
remove_files :: proc(arena: ^Arena, filepath: string) {
  temp := temp_begin(arena);
  defer temp_end(temp);
  allocator := arena_allocator_interface(temp.arena);

  filepath_cstring := strings.clone_to_cstring(filepath, allocator);
  fileinfo, errno := filestat(filepath, allocator);
  if errno != .NONE {
    fmt.println("File not found:", filepath);
    return;
  }

  #partial switch fileinfo.type {
  case .Symlink: {
    if err := linux.unlink(filepath_cstring); err != .NONE {
      fmt.printf("Failed to remove '%s' because: %s\n", fileinfo.name, err);
    }
    return;
  }

  case .Regular: {
    if err := linux.unlink(filepath_cstring); err != .NONE {
      fmt.printf("Failed to remove '%s' because: %s\n", fileinfo.name, err);
    }
    return;
  }
  }
  assert(fileinfo.type == .Directory); // Sanity check

  dir := posix.opendir(filepath_cstring);
  assert(dir != nil);
  defer posix.closedir(dir);

  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." do continue;

    fullpath := fmt.tprintf("%s/%s", filepath, filename);
    fileinfo, errno := filestat(fullpath, allocator);
    assert(errno == .NONE); // This should never happen because `readdir` returns valid files

    if fileinfo.type == .Directory {
      remove_files(arena, fileinfo.fullpath);
    } else {
      errno = linux.unlink(strings.clone_to_cstring(fileinfo.fullpath, allocator));
      if errno != .NONE {
        fmt.printf("Failed to remove file '%s' because: %s\n", fileinfo.name, errno);
        continue;
      }
    }
  }

  posix.rewinddir(dir);
  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." do continue;
    fullpath := fmt.ctprintf("%s/%s", filepath, filename);

    errno = linux.rmdir(fullpath);
    if errno != .NONE {
      fmt.printf("Failed to remove file '%s' because %s\n", filename, errno);
      continue;
    }
  }

  // Lastly, remove the parent/root directory
  errno = linux.rmdir(filepath_cstring);
  if errno != .NONE {
    fmt.printf("Failed to remove file '%s' because %s\n", filepath, errno);
    return;
  }
}
