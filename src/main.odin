package main

import "core:fmt"
import "core:os"
import "core:strings"
import "core:sys/linux"
import "base:runtime"

VERSION :: "2.0.0"
PATH_MAX :: int(PAGE_SIZE)

main :: proc() {
  using fmt;
  arena: Arena;
  temp_arena: Arena;
  context.allocator = arena_allocator(&arena, PAGE_SIZE * 4); // Doesn't need to be huge
  context.temp_allocator = arena_allocator(&temp_arena);      // Defaults to 1-Megabyte of memory

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
    handle_opts(&temp_arena, &drash, args);
    return;
  }

  for arg in args {
    free_all(context.temp_allocator);
    
    if len(arg) >= PATH_MAX {
      println("Filename '%s...' is too long", arg[:10]);
      println("Max Path length is %d", PATH_MAX);
      continue;
    }

    fileinfo, errno := filestat(arg);
    if errno != .NONE {
      printf("File not found: '%s'\n", arg); 
      continue;
    }

    if fileinfo.type == .Symlink {
      errno := linux.unlink(strings.clone_to_cstring(arg, context.temp_allocator));
      assert(errno == .NONE);
      printf("Removed Symlink: %s\n", arg);
      continue;
    }

    metadata_path := tprintf("%s/%s.info", drash.metadata, fileinfo.name);
    if os.exists(metadata_path) {
      unreachable(); // nocheckin
    }

    type: string;
    {
      buffer: [20]u8;
      if fileinfo.type == .Directory {
        type = bprintf(buffer[:], "directory"); 
      } else {
        type = bprintf(buffer[:], "file"); 
      }
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
  Empty,
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
    name = "empty",
    desc = "Wipe-out all the files in the drashcan",
    action = .Empty
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

parse_options :: proc(args: []string) -> Option_Action {
  arg := args[0][1:];
  if arg == "" {
    fmt.println("Invalid option format");
    os.exit(1);
  }
  if arg[0] == '-' do arg = arg[1:];

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

handle_opts :: proc(arena: ^Arena, drash: ^Drash, args: []string) {
  action := parse_options(args);
  files := args[1:]; // Skip the option argument
  switch  action {
  case .Help:    display_help();
  case .Version: fmt.println("drash version: ", VERSION);
  case .Force:   force_remove_files(arena, files);
  case .Restore: drash_restore(drash, files);
  case .Empty:   drash_empty(arena, drash);
  case .Remove:  drash_remove(arena, drash, files);
  case .List:    drash_list(drash);
  case .Cat:     drash_cat(drash, files);
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

force_remove_files :: proc(arena: ^Arena, args: []string) {
  if len(args) == 0 {
    fmt.println("Missing argument file(s)");
    return;
  }
  for filepath in args {
    remove_files(arena, filepath);
  }
}
