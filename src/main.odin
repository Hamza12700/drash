package main

import "core:fmt"
import "core:os"
import "core:sys/posix"
import "core:strings"

VERSION :: "2.0.0"

main :: proc() {
  using fmt;

  arena: Arena;
  context.allocator = arena_allocator(&arena);

  args := os.args;
  if len(args) == 1 {
    println("Missing argument file(s)");
    return;
  }

  args = args[1:];
  drash := init_drash();

  if args[0][0] == '-' {
    handle_opts(&drash, &args);
    return;
  }

  // Save the lsat filename
  filepath := args[len(args)-1];
  if len(filepath) >= posix.PATH_MAX {
    println("Filename '%s...' is too long", filepath[:10]);
    println("Max Path length is %d", posix.PATH_MAX);

  } else {
    filestat, err := os.lstat(filepath);
    if err == .NONE {
      if filestat.mode == os.File_Mode_Sym_Link {
        err = os.remove(filepath);
        printf("Failed to remove the symlink because '%s'\n", err);
      } else {
        last_filepath := tprintf("%s/last", drash.metadata);
        err := os.write_entire_file_or_err(last_filepath, transmute([]u8) filestat.name);
        if err != .NONE {
          printf("Failed to write to '%s' because %s\n", last_filepath, err);
        }
      }
    }  
  }

  for arg in args {
    free_all(context.temp_allocator);
    
    if len(arg) >= posix.PATH_MAX {
      println("Filename '%s...' is too long", arg[:10]);
      println("Max Path length is %d", posix.PATH_MAX);
      continue;
    }

    filestat, err := os.lstat(arg);
    if err != .NONE {
      printf("File not found: '%s'\n", arg); 
      continue;
    }

    if filestat.mode == os.File_Mode_Sym_Link {
      err = os.remove(arg);
      assert(err == .NONE);
      continue;
    }

    metadata_path := tprintf("%s/%s.info", drash.metadata, filestat.name);
    if os.exists(metadata_path) {} // nocheckin

    type: string;
    {
      buffer: [20]u8;
      if filestat.is_dir { type = bprintf(buffer[:], "directory"); }
      else { type = bprintf(buffer[:], "file"); }
    }

    metadata := tprintf("Path: %s\nType: %s\n", filestat.fullpath, type);
    err = os.write_entire_file_or_err(metadata_path, transmute([]u8) metadata);
    if err != .NONE {
      printf("failed to write file '%s' because %s\n", metadata_path, err); 
    }

    drash_path := tprintf("%s/%s", drash.files, filestat.name);
    if err = os.rename(arg, drash_path); err != .NONE {
      printf("Failed to move the file '%s' because '%s'\n", filestat.name, err);
      continue;
    }
  }
}

Option_Action :: enum {
  Force,
  Help,
  Version,
  List,
  Restore,
  Remove,
  Cat,
}

Option :: struct {
  name: string,
  desc: string,

  action: Option_Action,
}

OPTIONS :: []Option{
  {
    name = "force|f",
    desc = "Force remove file(s) (don't store it in the drashcan)",
    action = .Force
  },

  {
    name = "list|l",
    desc = "List files stored in the drashcan",
    action = .List
  },

  {
    name = "help|h",
    desc = "Display the help message",
    action = .Help
  },

  {
    name = "restore",
    desc = "Restore file(s) back to its original location",
    action = .Restore
  },

  {
    name = "remove|rm",
    desc = "Remove file(s) stored in the drashcan",
    action = .Remove
  },

  {
    name = "version",
    desc = "Display the version of the binary",
    action = .Version
  },

  {
    name = "cat",
    desc = "Print out the file contents",
    action = .Cat
  },
}

parse_options :: proc(args: ^[]string) -> Option_Action {
  arg := args[0][1:];
  if arg[0] == '-' { arg = arg[1:]; }

  for option in OPTIONS {
    name := option.name;
    fullname: [20]u8;
    prefix: [5]u8;

    {
      x := 0;
      is_prefix := false;

      for char, i in name {
        if !is_prefix {
          if char == '|' {
            is_prefix = true; 
            continue;
          }

          fullname[i] = u8(char);
          continue;
        }

        prefix[x] = u8(char); 
        x += 1;
      }
    }

    cmdname := strings.truncate_to_byte(string(fullname[:]), 0);
    cmdprefix := strings.truncate_to_byte(string(prefix[:]), 0);
    if (cmdname == arg || cmdprefix == arg) {
      return option.action; 
    }
  }

  fmt.println("No option found:", arg);
  os.exit(1);
}

handle_opts :: proc(drash: ^Drash, args: ^[]string) {
  action := parse_options(args);
  files := args[1:]; // Skip the option argument

  switch  action {
  case .Help:    display_help();
  case .Version: fmt.println("drash version: ", VERSION);
  case .Force:   force_remove_files(&files);
  case .Restore: drash_restore(drash, &files);
  case .Remove:  drash_remove(drash, &files);
  case .List:    drash_list(drash, &files);
  case .Cat:     drash_cat(drash, &files);
  }
}

display_help :: proc() {
  fmt.println("Usage: drash [OPTIONS] [FILES]..");

  fmt.println("\nOptions:\n");
  for opt in OPTIONS {
    fmt.printf("   %-10s",   opt.name);
    fmt.printf("   %-10s\n", opt.desc);
  }
}

force_remove_files :: proc(args: ^[]string) {
  if len(args) == 0 {
    fmt.println("Missing argument file(s)");
    return;
  }

  for filepath in args {
    remove_files(filepath);
  }
}

remove_files :: proc(filepath: string) {
  filestat, err := os.lstat(filepath);
  if err != .NONE {
    fmt.println("File not found:", filepath);
    return;
  }

  if filestat.mode == os.File_Mode_Sym_Link || !filestat.is_dir {
    if err = os.remove(filepath); err != .NONE {
      fmt.printf("Failed to remove '%s' because: %s\n", filestat.name, err);
      return;
    }
  }

  // If not file or symlink then it's probably a directory
  filepath_cstring := strings.clone_to_cstring(filepath);
  defer delete(filepath_cstring);

  dir := posix.opendir(filepath_cstring);
  defer posix.closedir(dir);

  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." { continue; }
    fullpath := fmt.aprintf("%s/%s", filepath, filename);
    fstat, err := os.lstat(fullpath);
    assert(err == .NONE);

    if fstat.is_dir {
      remove_files(fstat.fullpath);
    } else {
      err := os.remove(fstat.fullpath);
      if err != .NONE {
        fmt.printf("Failed to remove file '%s' because: %s\n", fstat.name, err);
        continue;
      }
    }
  }

  posix.rewinddir(dir);
  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." { continue; }

    fullpath := fmt.aprintf("%s/%s", filepath, filename);
    defer delete(fullpath);

    err := os.remove_directory(fullpath);
    if err != .NONE {
      fmt.printf("Failed to remove file '%s' because %s\n", filename, err);
      continue;
    }
  }

  // Lastly, remove the parent/root directory
  err = os.remove_directory(filepath);
  if err != .NONE {
    fmt.printf("Failed to remove file '%s' because %s\n", filepath, err);
    return;
  }
}
