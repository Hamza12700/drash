package main

import "core:sys/linux"
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
filestat :: proc(path: string, alloc: mem.Allocator) -> (File_Info, linux.Errno) {
  path := path;
  fullpath : string;
  if path[0] != '/' { // Path is not an absolute-path
    cwd, err := get_working_directory(alloc);
    if err != .NONE do return {}, err;

    if path[len(path)-1] == '/' do path = path[:len(path)-1];
    fullpath = fmt.aprintf("%s/%s", cwd, path, allocator=alloc);
  } else {
    fullpath = path; 
  }

  st: linux.Stat;
  err := linux.lstat(strings.clone_to_cstring(fullpath, alloc), &st);
  if err != .NONE do return {}, err;

  type : File_Type;
  switch (st.mode & linux.S_IFMT) {
  case linux.S_IFBLK:  type = .Block_Device
  case linux.S_IFCHR:  type = .Character_Device
  case linux.S_IFDIR:  type = .Directory
  case linux.S_IFIFO:  type = .Named_Pipe
  case linux.S_IFLNK:  type = .Symlink
  case linux.S_IFREG:  type = .Regular
  case linux.S_IFSOCK: type = .Socket
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

get_working_directory :: proc(alloc: mem.Allocator) -> (string, linux.Errno) {
  // Maximum path-length on most Linux-Systems
	PATH_MAX :: 4096;
	buf := make([dynamic]u8, PATH_MAX, alloc);

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

// Recursively remove files/directories
remove_files :: proc(arena: ^Arena, filepath: string) {
  temp := arena_temp_begin(arena);
  defer arena_temp_end(temp);

  filepath_cstring := strings.clone_to_cstring(filepath, context.temp_allocator);
  fileinfo, errno := filestat(filepath, context.temp_allocator);
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
  dirfd: linux.Fd;
  dirfd, errno = linux.open(filepath_cstring, {.DIRECTORY});
  if errno != .NONE {
    fmt.printf("Failed to open directory '%s' because: %s\n", filepath, errno);
    return;
  }
  defer linux.close(dirfd);

  dirents_buffer: [8192]u8;
  for {
    bytes_read, err := linux.getdents(dirfd, dirents_buffer[:]);
    if err != .NONE {
      fmt.printf("Failed to read directory '%s' because: %s\n", filepath, err);
      return;
    }
    if bytes_read == 0 do break;

    offset: int;
    for dirent in linux.dirent_iterate_buf(dirents_buffer[:bytes_read], &offset) {
      filename := linux.dirent_name(dirent);
      if filename == "." || filename == ".." do continue;

      fullpath := fmt.tprintf("%s/%s", filepath, filename);
      file_type := File_Type.Regular;
      if dirent.type == .DIR {
        file_type = .Directory;
      } else if dirent.type == .UNKNOWN {
        fileinfo, errno := filestat(fullpath, context.temp_allocator);
        assert(errno == .NONE); // This should never happen because `getdents` returns valid files
        file_type = fileinfo.type;
      }

      if file_type == .Directory {
        remove_files(arena, fullpath);
      } else if file_type == .Regular {
        errno = linux.unlink(strings.clone_to_cstring(fullpath, context.temp_allocator));
        if errno != .NONE {
          fmt.printf("Failed to remove file '%s' because: %s\n", filename, errno);
          continue;
        }
      } else {
        fmt.eprintln("Skipping file '%s' because unknown filetype '%s'\n", filename, file_type);
        continue;
      }
    }
  }

  // Lastly, remove the parent/root directory
  errno = linux.rmdir(filepath_cstring);
  if errno != .NONE {
    fmt.printf("Failed to remove file '%s' because %s\n", filepath, errno);
    return;
  }
}
