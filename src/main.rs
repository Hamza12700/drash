mod cli;
mod colors;
mod drash;
mod utils;

use colors::Colorize;
use drash::Drash;
use std::path::PathBuf;

use clap::{Parser, Subcommand};

/// Put files into drash so you can restore them later
#[derive(Parser)]
#[command(version)]
struct Args {
  /// Files to drash
  files: Option<Vec<PathBuf>>,

  /// Force remove file
  #[arg(short, long)]
  force: bool,

  #[command(subcommand)]
  commands: Option<Commands>,
}

#[derive(Subcommand)]
enum Commands {
  /// List files in the drashcan
  List,

  /// Remove selected files in the drashcan
  Remove {
    /// remove searched file or use `-` to remove the last drashed file
    search_file: Option<String>,
  },

  /// Empty drashcan
  Empty {
    /// remove all files without giving a warning
    #[arg(short, long)]
    yes: bool,
  },

  /// Restore drashed files
  Restore {
    /// restore file without asking to replace it with an existing one
    #[arg(short, long)]
    overwrite: bool,

    /// restore searched file or use `-` to restore the last drashed file
    search_file: Option<String>,
  },
}

impl Args {
  /// Check if files or subcommands are empty
  fn is_none(&self) -> bool {
    if self.files.is_none() && self.commands.is_none() {
      return true;
    }
    false
  }
}

fn main() {
  let drash = Drash::new();

  let args = Args::parse();
  if args.is_none() {
    cli::drash_cwd(&drash);
    return;
  }

  if let Some(files) = &args.files {
    if args.force {
      drash.force_remove_file(files);
      return;
    }

    for file in files {
      if let Err(err) = drash.put_file(file) {
        eprintln!("{}: {err}", "Error".red().bold());
        continue;
      }
    }
    return;
  }

  if let Some(cmds) = args.commands {
    match cmds {
      Commands::List => cli::list_drash_files(&drash),
      Commands::Remove { search_file } => cli::remove_files_from_drash(&drash, search_file),
      Commands::Empty { yes } => cli::empty_drash(&drash, yes),
      Commands::Restore {
        overwrite,
        search_file,
      } => cli::restore_files(&drash, search_file, overwrite),
    }
  }
}
