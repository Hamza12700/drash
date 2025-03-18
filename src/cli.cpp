#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "strings.cpp"
#include "assert.c"
#include "types.cpp"
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

const Command commands[] = {
   {.name = "list", .desc = "List Drash'd files", .action = Command::List },
   {.name = "remove", .desc = "Remove files from the Drash/can", .action = Command::Remove },
   {.name = "empty", .desc = "Empty the Drash/can", .action = Command::Empty },
   {.name = "restore", .desc = "Restore removed files", .action = Command::Restore },
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
      char lbuf[15] = {0};
      char sbuf[5] = {0};

      int x = 0;
      for (uint i = 0; i < strlen(name) - 1; i++) {
         const char name_char = name[i];

         if (name_char != '|') {
            lbuf[i] = name_char;
            continue;
         }

         sbuf[x] = name[i+1];
         x++;
      }

      if (strcmp(str, lbuf) == 0 || strcmp(str, sbuf) ==  0) return true;
      return false;
   }
};

const Option options[] = {
   { .name = "force|f", .desc = "Force remove file" , .action = Option::Force },
   { .name = "help|h", .desc = "Display the help message" , .action = Option::Help },
   { .name = "version|v", .desc = "Print the version" , .action = Option::Version },
};

void print_help() {
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

void handle_opts(const char **argv, const int argc) {
   const char *arg = argv[0] += 1;
   if (arg[0] == '-') arg += 1;

   // @NOTE: Check files and directories if they exists so that way I don't have to
   // do error checking in every case for the command line option.

   for (const auto opt : options) {
      if (!opt.cmp(arg)) {
         continue;
      }

      switch (opt.action) {
         case Option::Help: return print_help();
         case Option::Version: {
            printf("drash version: %s\n", VERSION);
            return;
         }

         case Option::Force: {
            if (argc == 1) {
               fprintf(stderr, "Missing argument file(s)\n");
               return;
            }

            // Skip the current argument
            for (int i = 1; i < argc; i++) {
               const char *path = argv[i];

               struct stat sb;
               assert_err(lstat(path, &sb) != 0, "lstat failed");

               if ((sb.st_mode & S_IFMT) == S_IFREG) {
                  assert_err(unlink(path) != 0, "failed to remove file");
                  continue;
               }

               // @Incomplete: Implement a function that will delete files and directories recursively
               auto string = format_string("rm -rf %", path);

               assert_err(system(string.buf) != 0, "system command failed");
            }

            return;
         }
      }
   }

   fprintf(stderr, "Unkonwn option: %s\n", arg);
   return;
}

bool handle_commands(const char **argv, const uint argc, const Drash *drash) {
   for (uint i = 0; i < argc; i++) {
      const char *arg = argv[i];

      for (auto cmd : commands) {
         if (strcmp(arg, cmd.name) == 0) {
            switch (cmd.action) {
               case Command::List: {
                  printf("TODO: LIST");
                  return true;
               }

               case Command::Restore: {
                  printf("TODO: Restore");
                  return true;
               }

               case Command::Empty: {
                  drash->empty_drash();
                  return true;
               }

               case Command::Remove: {
                  printf("TODO: Remove");
                  return true;
               }
            }
         }
      }
   }

   return false;
}
