package main

import "core:fmt"
import "core:os"
import "core:sys/posix" // This link with `libc'
import "core:strings"
import "base:runtime"
import "core:mem"

VERSION :: "2.0.0"

PATH_MAX :: PAGE_SIZE

main :: proc() {
  using fmt;

  arena: Arena;
  temp_arena: Arena;
  context.allocator = arena_allocator(&arena);
  context.temp_allocator = arena_allocator(&temp_arena);

  // Converting the '[]cstring' (null-terminated strings) to '[]string' (proper string-type)
  args := make([]string, len(runtime.args__))
  for _, i in args {
    args[i] = string(runtime.args__[i])
  }

  if len(args) == 1 {
    println("Missing argument file(s)");
    return;
  }

  args = args[1:];
  drash := init_drash();

  if args[0][0] == '-' {
    handle_opts(&temp_arena, &drash, &args);
    return;
  }

  // Save the lsat filename
  filepath := args[len(args)-1];
  if len(filepath) >= posix.PATH_MAX {
    println("Filename '%s...' is too long", filepath[:10]);
    println("Max Path length is %d", posix.PATH_MAX);

  } else {
    filestat, errno := filestat(filepath);
    if errno == .NONE {
      if filestat.type == .Symlink {
        err := os.remove(filepath);
        if err != .NONE {
          printf("Failed to remove the symlink because '%s'\n", err);
        }
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

    fileinfo, errno := filestat(arg);
    if errno != .NONE {
      printf("File not found: '%s'\n", arg); 
      continue;
    }

    if fileinfo.type == .Symlink {
      err := os.remove(arg);
      assert(err == .NONE);
      continue;
    }

    metadata_path := tprintf("%s/%s.info", drash.metadata, fileinfo.name);
    if os.exists(metadata_path) {} // nocheckin

    type: string;
    {
      buffer: [20]u8;
      if fileinfo.type == .Directory do type = bprintf(buffer[:], "directory");
      else { type = bprintf(buffer[:], "file"); }
    }

    metadata := tprintf("Path: %s\nType: %s\n", fileinfo.fullpath, type);
    err := os.write_entire_file_or_err(metadata_path, transmute([]u8) metadata);
    if err != .NONE {
      printf("failed to write file '%s' because %s\n", metadata_path, err); 
    }

    drash_path := tprintf("%s/%s", drash.files, fileinfo.name);
    if err = os.rename(arg, drash_path); err != .NONE {
      printf("Failed to move the file '%s' because '%s'\n", fileinfo.name, err);
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
    name = "version|v",
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
  if arg == "" {
    fmt.println("Invalid option format");
    os.exit(1);
  }

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

handle_opts :: proc(arena: ^Arena, drash: ^Drash, args: ^[]string) {
  action := parse_options(args);
  files := args[1:]; // Skip the option argument

  switch  action {
  case .Help:    display_help();
  case .Version: fmt.println("drash version: ", VERSION);
  case .Force:   force_remove_files(arena, &files);
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

force_remove_files :: proc(arena: ^Arena, args: ^[]string) {
  if len(args) == 0 {
    fmt.println("Missing argument file(s)");
    return;
  }

  for filepath in args {
    remove_files(arena, filepath);
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
  context.temp_allocator = mem.Allocator{arena_allocator_proc, arena};

  checkpoint := arena_checkpoint(arena);
  fileinfo, errno := filestat(filepath, context.temp_allocator);
  if errno != .NONE {
    fmt.println("File not found:", filepath);
    return;
  }

  if fileinfo.type == .Symlink {
    if err := os.remove(filepath); err != .NONE {
      fmt.printf("Failed to remove '%s' because: %s\n", fileinfo.name, err);
      return;
    }
  }

  // If not file or symlink then it's probably a directory
  filepath_cstring := strings.clone_to_cstring(filepath, context.temp_allocator);

  dir := posix.opendir(filepath_cstring);
  assert(dir != nil);
  defer posix.closedir(dir);

  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." { continue; }

    fullpath := fmt.tprintf("%s/%s", filepath, filename);
    fileinfo, errno := filestat(fullpath, context.temp_allocator);
    assert(errno == .NONE); // This should never happen because `readdir` returns valid files

    if fileinfo.type == .Directory {
      remove_files(arena, fileinfo.fullpath);
    } else {
      err := os.remove(fileinfo.fullpath);
      if err != .NONE {
        fmt.printf("Failed to remove file '%s' because: %s\n", fileinfo.name, err);
        continue;
      }
    }
  }

  posix.rewinddir(dir);
  for rdir := posix.readdir(dir); rdir != nil; rdir = posix.readdir(dir) {
    filename := strings.truncate_to_byte(string(rdir.d_name[:]), 0);
    if filename == "." || filename == ".." do continue;

    fullpath := fmt.tprintf("%s/%s", filepath, filename);

    err := os.remove_directory(fullpath);
    if err != .NONE {
      fmt.printf("Failed to remove file '%s' because %s\n", filename, err);
      continue;
    }
  }

  // Lastly, remove the parent/root directory
  err := os.remove_directory(filepath);
  if err != .NONE {
    fmt.printf("Failed to remove file '%s' because %s\n", filepath, err);
    return;
  }

  arena_restore(arena, checkpoint);
}
