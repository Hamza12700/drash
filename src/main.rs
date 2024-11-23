mod colors;
mod drash;
mod utils;

use colors::Colorize;
use drash::{ConfigError, Drash};
use std::{
  fs,
  path::{Path, PathBuf},
  process::exit,
};

use clap::{Parser, Subcommand};
use inquire::{min_length, Confirm, MultiSelect};
use tabled::{settings::Style, Table};

/// Put files into drash so you can restore them later
#[derive(Debug, Parser)]
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

#[derive(Subcommand, Debug)]
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
    return false;
  }
}

fn main() -> anyhow::Result<()> {
  let drash = Drash::new()
    .inspect_err(|err| match err {
      ConfigError::EnvError(var_error) => {
        eprintln!("{}", "Failed to read env variable".bold());
        eprintln!("{}: {var_error}", "Error".bold().red());
      }
      ConfigError::IoError(error) => {
        eprintln!("{}", "Failed to create directory".bold());
        eprintln!("{}: {error}", "Error".bold().red());
      }
    })
    .expect("Failed to initialize the `Drash` struct");

  let args = Args::parse();
  if args.is_none() {
    let entries = fs::read_dir(".")?;
    let mut path_vec: Vec<String> = Vec::with_capacity(3);

    for path in entries {
      let path = path?;
      let path = path.path();

      // Remove `./` from the start of the string
      let striped_path = path.strip_prefix("./")?;
      path_vec.push(striped_path.to_str().unwrap_or("None").to_string())
    }
    if path_vec.is_empty() {
      println!("Nothing in the current directory to drash");
      return Ok(());
    }

    let ans = MultiSelect::new("Select files to drash", path_vec)
      .with_validator(min_length!(1, "At least choose one file"))
      .prompt()?;

    for file in ans {
      match drash.put_file(file) {
        Ok(_) => (),
        Err(err) => {
          eprintln!("{}: {err}", "Error".red().bold());
          continue;
        }
      };
    }

    return Ok(());
  }

  if let Some(files) = &args.files {
    if args.force {
      for file in files {
        if file.is_symlink() {
          fs::remove_file(&file)?;
          return Ok(());
        }

        let file_name = file.display().to_string();
        if !file.exists() {
          eprintln!("file not found: '{}'", file_name.bold());
          continue;
        }

        match file.is_dir() {
          true => fs::remove_dir_all(file)?,
          false => fs::remove_file(file)?,
        };
        continue;
      }

      println!("Removed: {} files", files.len());
      return Ok(());
    }

    for file in files {
      match drash.put_file(file) {
        Ok(_) => (),
        Err(err) => {
          eprintln!("{}: {err}", "Error".red().bold());
          continue;
        }
      };
    }

    return Ok(());
  }

  if let Some(Commands::List) = &args.commands {
    let mut original_paths = drash.list_file_paths()?;
    if original_paths.len() == 0 {
      println!("Nothing is the drashcan");
      return Ok(());
    }

    original_paths.sort_unstable_by(|a, b| match (a.file_type.as_str(), b.file_type.as_str()) {
      ("directory", "dir") | ("file", "file") => a.path.cmp(&b.path),
      ("directory", "file") => std::cmp::Ordering::Less,
      ("file", "directory") => std::cmp::Ordering::Greater,
      _ => std::cmp::Ordering::Equal,
    });

    let table_style = Style::psql();
    println!(
      "{}",
      Table::new(&original_paths).with(table_style).to_string()
    );
    println!("\nTotal entries: {}", original_paths.len());
  }

  if let Some(Commands::Remove { search_file }) = &args.commands {
    if let Some(search) = search_file {
      if search != "-" {
        let files = drash.search_file_path("Select files to remove:")?;

        for file in files {
          let file_path = Path::new(&file);
          match drash.remove_file(file_path) {
            Ok(_) => (),
            Err(err) => {
              eprintln!("{}: {err}", "Error".bold().red());
              continue;
            }
          };
        }
        return Ok(());
      }

      let last_file = fs::read_dir(&drash.files_path)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          if let Err(err) = drash.remove_file(file.path()) {
            eprintln!("Error: {}", err);
            exit(1);
          }
        }
        None => eprintln!("Drashcan is empty"),
      }

      return Ok(());
    }

    let mut empty = true;
    let entries = fs::read_dir(&drash.info_path)?;
    let mut real_path: Vec<String> = Vec::new();

    for entry in entries {
      let path = entry?;
      let content = fs::read_to_string(&path.path())?;
      let file_info: Box<[&str]> = content.lines().collect();

      let file_path = match file_info.get(1) {
        Some(path) => {
          empty = false;
          path.trim_start_matches("Path=")
        }
        None => continue,
      };

      real_path.push(file_path.to_string());
    }
    if empty {
      println!("Nothing is the drashcan");
      return Ok(());
    }

    let files = drash.search_file_path("Select files to remove:")?;

    for path in files {
      let path = Path::new(&path);
      match drash.remove_file(path) {
        Ok(_) => (),
        Err(err) => {
          eprintln!("{}: {err}", "Error".red().bold());
          continue;
        }
      };
    }

    return Ok(());
  }

  if let Some(Commands::Empty { yes }) = &args.commands {
    let files = fs::read_dir(&drash.files_path)?;
    let file_entries = files.count();
    if file_entries == 0 {
      println!("Drashcan is alraedy empty");
      return Ok(());
    }

    if *yes {
      fs::remove_dir_all(&drash.files_path)?;
      fs::remove_dir_all(&drash.info_path)?;

      fs::create_dir(&drash.files_path)?;
      fs::create_dir(&drash.info_path)?;

      match file_entries {
        n if n > 1 => println!("Removed: {file_entries} files"),
        _ => println!("Removed: {file_entries} file"),
      }
      return Ok(());
    }

    let user_input = Confirm::new("Empty the drash directory?")
      .with_default(true)
      .prompt()?;

    if !user_input {
      return Ok(());
    }

    fs::remove_dir_all(&drash.files_path)?;
    fs::remove_dir_all(&drash.info_path)?;

    fs::create_dir(&drash.files_path)?;
    fs::create_dir(&drash.info_path)?;
  }

  if let Some(Commands::Restore {
    overwrite,
    search_file,
  }) = &args.commands
  {
    if let Some(search) = search_file {
      if search != "-" {
        let files = drash.search_file_path("Select files restore:")?;

        for file in files {
          let file_path = Path::new(&file);
          if !drash.restore_file(file_path, *overwrite)? {
            println!("Skipped {:?}", file_path.file_name().unwrap());
            continue;
          }
        }

        return Ok(());
      }

      let last_file = fs::read_dir(&drash.files_path)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          if !drash.restore_file(file.path(), *overwrite)? {
            println!("Skipped {}", file.path().display())
          } else {
            println!("Restored {:?}", file.file_name())
          }
        }
        None => eprintln!("Drashcan is empty"),
      };

      return Ok(());
    }

    let original_paths = drash.list_file_paths()?;
    if original_paths.len() == 0 {
      println!("Drashcan is empty");
      return Ok(());
    }

    let restore_paths = MultiSelect::new("Select files to restore", original_paths.into_vec())
      .with_validator(min_length!(1, "At least choose one file to restore"))
      .prompt()?;

    for entry in restore_paths.iter() {
      let path = Path::new(&entry.path);

      if !drash.restore_file(path, *overwrite)? {
        println!("Skipped {}", entry.path);
        continue;
      }
    }

    if restore_paths.len() > 1 {
      println!("Restored {} files", restore_paths.len())
    } else {
      println!("Restored 1 file")
    }
    return Ok(());
  }

  Ok(())
}
