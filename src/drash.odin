package main

import "core:sys/posix"
import "core:os"
import "core:fmt"

Drash :: struct {
  files:    string,
  metadata: string,
}

Drash_Info :: struct {
  path: string,
  type: DrashFile_Info
}

DrashFile_Info :: enum {
  File,
  Directory,
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

  {
    err = os.make_directory(drash.files);
    assert(err == .NONE);

    err = os.make_directory(drash.metadata);
    assert(err == .NONE);
  }

  return drash;
}

drash_restore :: proc(drash: ^Drash, args: ^[]string) {}
drash_remove :: proc(drash: ^Drash, args: ^[]string) {}
drash_list :: proc(drash: ^Drash, args: ^[]string) {}
drash_cat :: proc(drash: ^Drash, args: ^[]string) {}
