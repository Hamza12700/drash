package main

import "core:sys/posix"
import "core:sys/linux"
import "core:strings"
import "core:os"
import "core:fmt"
import "core:mem"

Drash :: struct {
  files:    string,
  metadata: string,
}

Drash_Info :: struct {
  path: string,
  name: string,
  size: uint,
  type: File_Type,
}

parse_drash_info :: proc(drash: ^Drash) -> [dynamic]Drash_Info {
  drash_files: [dynamic]Drash_Info;
  info_dir := posix.opendir(strings.clone_to_cstring(drash.metadata));
  assert(info_dir != nil);
  defer posix.closedir(info_dir);

  for {
    rdir := posix.readdir(info_dir);
    if rdir == nil do break;

    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." do continue;

    fullpath := fmt.tprintf("%s/%s", drash.metadata, filename);
    info_fd, errno := linux.open(strings.clone_to_cstring(fullpath, context.temp_allocator), {}); // Open_Flags 'RDONLY' is default
    assert(errno == .NONE);
    defer linux.close(info_fd);

    st: linux.Stat;
    errno = linux.fstat(info_fd, &st);
    assert(errno == .NONE);

    buffer := make([]u8, st.size);
    _, errno = linux.read(info_fd, buffer[:]);
    assert(errno == .NONE);

    buffer = buffer[len("Path: "):]; // Skip the prefix

    // Add the path until the newline-character
    path := string(buffer[:strings.index(string(buffer), "\n")]);
    buffer = buffer[len(path)+1:]; // Skip the Path including the newline-character

    // Skip the prefix and add the filetype until the newline character
    filetype := string(buffer[len("Type: "):strings.index(string(buffer), "\n")]);

    if filetype != "file" && filetype != "directory" {
      unreachable();
      //continue;
    }

    type: File_Type = filetype == "file" ? .Regular : .Directory;

    drash_fd: linux.Fd;
    drash_fd, errno = linux.open(fmt.ctprintf("%s/%s", drash.files, get_basename(path)), {});
    assert(errno == .NONE);
    defer linux.close(drash_fd);

    errno = linux.fstat(drash_fd, &st);
    assert(errno == .NONE);

    append(&drash_files, Drash_Info{
      path = path,
      name = get_basename(path),
      size = st.size,
      type = type,
    });
  }

  return drash_files;
}

check_file_in_drash :: proc(drash: ^Drash, filename: string) -> (int, bool) {
  drash_files := parse_drash_info(drash);
  for drash_info, i in drash_files {
    if drash_info.name == filename do return i, true;
  }
  return -1, false;
}

init_drash :: proc() -> Drash {
  home_env_buffer: [100]u8;
  home := os.get_env(home_env_buffer[:], "HOME"); 
  assert(home != "");

  buffer: [100]u8;
  drash_dirpath := fmt.bprintf(buffer[:], "%s/.local/share/Drash", home);

  drash := Drash{
    files = fmt.aprintf("%s/files", drash_dirpath),
    metadata = fmt.aprintf("%s/metadata", drash_dirpath),
  };

  err := os.make_directory(drash_dirpath);
  if err == os.EEXIST do return drash;
  assert(err == .NONE);

  err = os.make_directory(drash.files);
  assert(err == .NONE);

  err = os.make_directory(drash.metadata);
  assert(err == .NONE);

  return drash;
}

drash_remove :: proc(#no_alias arena: ^Arena, drash: ^Drash, args: []string) {
  drash_files_info := parse_drash_info(drash);
  for drash_info in drash_files_info {
    for filename in args {
      if filename == drash_info.name {
        drash_filepath := fmt.tprintf("%s/%s", drash.files, filename);
        remove_files(arena, drash_filepath);

        drash_info_filepath := fmt.ctprintf("%s/%s.info", drash.metadata, filename);
        errno := linux.unlink(drash_info_filepath);
        if errno != .NONE {
          fmt.printf("Failed to remove '%s' because: %s\n", filename, errno);
          continue;
        }
      }
    } 
  }
}

drash_empty :: proc(#no_alias arena: ^Arena, drash: ^Drash) {
  drash_files_info := parse_drash_info(drash);
  if len(drash_files_info) == 0 {
    fmt.println("Drashcan is empty!");
    return;
  }

  remove_files(arena, drash.files);
  err := os.make_directory(drash.files);
  if err != .NONE {
    fmt.printf("Failed to create directory '%s' because: %s\n",
      get_basename(drash.files), err);
  }

  remove_files(arena, drash.metadata);
  err = os.make_directory(drash.metadata);
  if err != .NONE {
    fmt.printf("Failed to create directory '%s' because: %s\n",
      get_basename(drash.metadata), err);
  }
}

drash_cat :: proc(drash: ^Drash, files: []string) {
  drash_files := parse_drash_info(drash);
  for file in files {
    for drash_info in drash_files {
      if file == drash_info.name {
        free_all(context.temp_allocator);

        fd, errno := linux.open(fmt.ctprintf("%s/%s", drash.files, file), {});
        if errno != .NONE {
          fmt.println("Failed to open '%s' because: %s\n", file, errno);
          continue;
        }
        defer linux.close(fd);

        buffer := make([]u8, drash_info.size, context.temp_allocator);
        _, errno = linux.read(fd, buffer[:]);
        if errno != .NONE {
          fmt.println("Failed to read '%s' because: %s\n", file, errno);
          continue;
        }

        fmt.printf("\n-- %s: --\n\n", file);
        fmt.printf("%s", buffer);
      }
    }
  }
}

drash_restore :: proc(drash: ^Drash, files: []string) {
  drash_files := parse_drash_info(drash);
  for file in files {
    idx, found := check_file_in_drash(drash, file);
    if !found {
      fmt.printf("No file exists in the drashcan: '%s'\n", file);
      continue;
    }

    drash_filepath := fmt.ctprintf("%s/%s", drash.files, file);
    errno := linux.rename(drash_filepath, strings.clone_to_cstring(drash_files[idx].path, context.temp_allocator));
    if errno != .NONE {
      fmt.printf("Failed to move '%s' because: %s\n", file, errno);
      continue;
    }

    file_infopath := fmt.ctprintf("%s/%s.info", drash.metadata, file);
    errno = linux.unlink(file_infopath);
    if errno != .NONE {
      fmt.printf("Failed to remove '%s' because: %s\n", file, errno);
      continue;
    }

    fmt.println("Restore:", drash_files[idx].path);
  }
}

drash_list :: proc(drash: ^Drash) {
  using mem;

  drash_files := parse_drash_info(drash);
  if len(drash_files) == 0 {
    fmt.println("Drashcan is empty!");
    return;
  }

  fmt.println();
  for drash_file in drash_files {
    filesize := drash_file.size;
    if      filesize > Gigabyte do filesize /= Gigabyte;
    else if filesize > Megabyte do filesize /= Megabyte;
    else if filesize > Kilobyte do filesize /= Kilobyte;

    fmt.printf("- %s | %d\n", drash_file.path, filesize);
  }
}
