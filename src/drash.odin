package main

import "core:sys/posix"
import "core:sys/linux"
import "core:strings"
import "core:os"
import "core:fmt"

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
    if filename == "." || filename == ".." || filename == "last" do continue;

    fullpath := fmt.tprintf("%s/%s", drash.metadata, filename);
    fd, errno := linux.open(strings.clone_to_cstring(fullpath, context.temp_allocator), {}); // Open_Flags 'RDONLY' is default
    assert(errno == .NONE);
    defer linux.close(fd);

    st: linux.Stat;
    errno = linux.fstat(fd, &st);
    assert(errno == .NONE);

    buffer := make([]u8, st.size);
    _, errno = linux.read(fd, buffer[:]);
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

    append(&drash_files, Drash_Info{
      path = path,
      name = get_basename(path),
      size = st.size,
      type = type,
    });
  }

  return drash_files;
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
  if err == os.EEXIST { return drash; }
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

drash_restore :: proc(drash: ^Drash, args: []string) {}
drash_list :: proc(drash: ^Drash, args: []string) {}
drash_cat :: proc(drash: ^Drash, args: []string) {}
drash_empty :: proc(arena: ^Arena, drash: ^Drash, files: []string) {}
