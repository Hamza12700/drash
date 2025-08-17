package main

import "core:sys/linux"
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
