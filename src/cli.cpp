#include "drash.cpp"

struct Command {
   const char *name;
   const char *desc;

   enum CmdAction {
      List,
      Remove, 
      Empty,
      Restore,
   };

   const CmdAction action;
};

static const Command commands[] = {
   {
      .name = "list",
      .desc = "List Drash'd files",
      .action = Command::List
   },

   {
      .name = "remove",
      .desc = "Remove files from the Drash/can",
      .action = Command::Remove
   },

   {
      .name = "empty",
      .desc = "Empty the Drash/can",
      .action = Command::Empty
   },

   {
      .name = "restore",
      .desc = "Restore removed files",
      .action = Command::Restore
   },
};


struct Option {
   const char *name;
   const char *desc;

   enum OptAction {
      Force,    // Remove the file without storing it in the drashcan
      Help,     // Help message. @NOTE: Add ability to show help for separate commands
      Version,  // Print version of the project
   };

   const OptAction action;

   // Compare a string with an option->name
   bool cmp(const char *str) const {
      char lbuf[20] = {0}; // Buffer to hold the long option name
      char sbuf[5] = {0};  // Buffer to hold the short option name

      for (u8 i = 0, x = 0; i < strlen(name) - 1; i++) {
         const char name_char = name[i];

         if (name_char != '|') {
            lbuf[i] = name_char;
            continue;
         }

         sbuf[x] = name[i+1];
         x++;
      }

      if (strcmp(str, lbuf) == 0 || strcmp(str, sbuf) ==  0) {
         return true;
      }

      return false;
   }
};

static const Option options[] = {
   {
      .name = "force|f",
      .desc = "Force remove file",
      .action = Option::Force
   },

   {
      .name = "help|h",
      .desc = "Display the help message",
      .action = Option::Help
   },

   {
      .name = "version|v",
      .desc = "Print the version",
      .action = Option::Version
   },
};

static void print_help() {
   printf("Usage: drash [OPTIONS] [FILES].. [SUB-COMMANDS]\n");
   printf("\nCommands:\n");
   for (const auto cmd : commands) {
      printf("   %-10s", cmd.name);
      printf("   %-10s\n", cmd.desc);
   }

   printf("\nOptions:\n");
   for (const auto opt : options) {
      printf("   %-10s", opt.name);
      printf("   %-10s\n", opt.desc);
   }
}

static void force_remove_files(uint argc, const char **argv) {
   if (argc == 1) {
      fprintf(stderr, "Missing argument file(s)\n");
      return;
   }

   for (uint i = 1; i < argc; i++) { // Skip the current argument
      const char *path = argv[i];

      auto file = exists(path);
      if (file.type == lnk) {
         remove_file(path);
         continue;
      } else if (!file.found) {
         fprintf(stderr, "file not found: %s\n", path);
         continue;
      }

      if (is_dir(path)) {
         // @Incomplete: Implement a function that will delete files and directories recursively
         auto buffer = format_string("/bin/rm -rf '%'", path);

         int err = system(buffer.buf);
         if (err != 0) {
            report_error("failed to remove directory: '%'", path);
            continue;
         }

         continue;
      }

      remove_file(path);
   }
}

void handle_opts(const char **argv, const int argc) {
   const char *arg = argv[0] += 1;
   if (arg[0] == '-') arg += 1;

   for (const auto opt : options) {
      if (!opt.cmp(arg)) continue;

      switch (opt.action) {
         case Option::Help: return print_help();

         case Option::Version: {
            printf("drash version: %s\n", VERSION);
            return;
         }

         case Option::Force: return force_remove_files(argc, argv);
      }
   }

   fprintf(stderr, "Unkonwn option: %s\n", arg);
}

bool handle_commands(const char **argv, uint argc, const Drash *drash, Fixed_Allocator *allocator) {
   for (uint i = 0; i < argc; i++) {
      const char *arg = argv[i];

      for (auto cmd : commands) {
         if (strcmp(arg, cmd.name) != 0) continue;

         // Skip the command name
         argv += 1;
         argc -= 1;

         switch (cmd.action) {
            case Command::List: {
               drash->list_files(allocator);
               return true;
            }

            case Command::Restore: {
               drash->restore(allocator, argc, argv);
               return true;
            }

            case Command::Empty: {
               drash->empty_drash(allocator);
               return true;
            }

            case Command::Remove: {
               drash->remove(allocator, argc, argv);
               return true;
            }
         }
      }
   }

   return false;
}
