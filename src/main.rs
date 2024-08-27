use clap::{Parser, Subcommand};
use colored::Colorize;
use inquire::{
  formatter::MultiOptionFormatter, list_option::ListOption, min_length, validator::Validation,
  Confirm, MultiSelect,
};
use tabled::{settings::Style, Table, Tabled};
use utils::{check_overwrite, fuzzy_find_files};
mod utils;
use std::{
  env,
  error::Error,
  fs::{self},
  io::{self, Write},
  path::{Path, PathBuf},
  process::exit,
  rc::Rc,
};

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

#[derive(Tabled)]
struct FileList {
  file_type: String,
  path: String,
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

struct Drash {
  /// Drash home directory
  dir: PathBuf,

  //// Drashed files directory
  files: PathBuf,

  /// Drashed files info directory
  info: PathBuf,
}

impl Drash {
  /// Create new **Drash** struct containing path to:
  /// - Dirash directory
  /// - Drash files
  /// - Drash files info
  fn new() -> Self {
    let home = match env::var("HOME") {
      Ok(path) => path,
      Err(err) => {
        eprintln!("Failed to get HOME environment variable: {}", err);
        exit(1);
      }
    };
    let drash_dir = Path::new(&home).join(".local/share/Drash");
    let drash_files = Path::new(&drash_dir).join("files");
    let drash_info_dir = Path::new(&drash_dir).join("info");

    Self {
      dir: drash_dir,
      files: drash_files,
      info: drash_info_dir,
    }
  }

  /// Create directories for **Drash** to store removed files
  fn create_directories(&self) -> io::Result<()> {
    if !self.dir.exists() {
      fs::create_dir(&self.dir)?;
      fs::create_dir(&self.files)?;
      fs::create_dir(&self.info)?;
    }

    Ok(())
  }

  /// Put file into drashcan
  fn put_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
    let path = path.as_ref();

    // Do not store the symlink, instead delete it and return
    if path.is_symlink() {
      fs::remove_file(path)?;
      return Ok(());
    }
    let mut file_name = path.display().to_string();

    // Reture an error if file not found
    if !path.exists() {
      return Err(io::Error::new(
        io::ErrorKind::NotFound,
        format!("file not found: '{}'", file_name),
      ));
    }

    // Remove trailing forward slash
    if file_name.ends_with("/") {
      file_name.pop();
    }
    // If contains a forward slash, extract the part of the string containing the file name
    if file_name.contains("/") {
      let path = file_name.split("/").last();
      file_name = path.unwrap().to_string();
    }

    // Get the file in current working directory
    let current_file = env::current_dir()?.join(path);

    // Create file in append mode for storing info about the file and it original path
    let mut buffer = fs::OpenOptions::new()
      .write(true)
      .append(true)
      .create(true)
      .open(Path::new(&self.info).join(format!("{}.drashinfo", file_name)))?;

    buffer.write_all(b"[Drash Info]\n")?;

    // Write the original `Path` value
    let formatted_string = format!("Path={}\n", current_file.display());
    buffer.write_all(formatted_string.as_bytes())?;

    // Write what file type it is:
    // - File
    // - Directory
    buffer.write_all(b"FileType=")?;
    if path.is_dir() {
      buffer.write_all(b"directory\n")?;
    } else {
      buffer.write_all(b"file\n")?;
    }

    // Move the file to drashed files directory
    fs::rename(path, Path::new(&self.files).join(file_name))?;

    Ok(())
  }
}

fn main() -> anyhow::Result<()> {
  let drash = Drash::new();
  if let Err(err) = drash.create_directories() {
    eprintln!("Failed to create directory: {}", err);
    exit(1);
  }

  let args = Args::parse();
  if args.is_none() {
    let selected_files = fuzzy_find_files(".", None)?;

    for item in selected_files.selected_items.iter() {
      let selected_file_name = item.output().to_string();
      let file = Path::new(&selected_file_name);

      match drash.put_file(file) {
        Ok(_) => (),
        Err(err) => {
          eprintln!("Error: {}", err);
          continue;
        }
      };
    }

    if selected_files.selected_items.len() > 1 {
      println!("Removed {} files", selected_files.selected_items.len());
    } else {
      println!("\nRemoved 1 file");
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
          eprintln!("Error: {}", err);
          continue;
        }
      };
    }

    return Ok(());
  }

  if let Some(Commands::List) = &args.commands {
    let mut empty = true;
    let entries = fs::read_dir(&drash.info)?;
    let mut real_path: Vec<FileList> = Vec::new();

    for entry in entries {
      let entry = entry?;
      let file_info: Rc<_> = fs::read_to_string(&entry.path())?
        .lines()
        .map(|line| line.to_string())
        .collect();

      let file_path = match file_info.get(1) {
        Some(path) => {
          empty = false;
          path.trim_start_matches("Path=").to_string()
        }
        None => {
          eprintln!("{}", "Fialed to get file path by index".red().bold());
          continue;
        }
      };

      let file_type = file_info[2].trim_start_matches("FileType=").to_string();
      real_path.push(FileList {
        file_type,
        path: file_path,
      });
    }
    if empty {
      println!("Nothing is the drashcan");
      return Ok(());
    }

    real_path.sort_unstable_by(|a, b| match (a.file_type.as_str(), b.file_type.as_str()) {
      ("directory", "dir") | ("file", "file") => a.path.cmp(&b.path),
      ("directory", "file") => std::cmp::Ordering::Less,
      ("file", "directory") => std::cmp::Ordering::Greater,
      _ => std::cmp::Ordering::Equal,
    });

    let table_style = Style::psql();
    println!("{}", Table::new(&real_path).with(table_style).to_string());
    println!("\nTotal entries: {}", real_path.len());
  }

  if let Some(Commands::Remove { search_file }) = &args.commands {
    if let Some(search) = search_file {
      if search != "-" {
        let selected_files = fuzzy_find_files(&drash.info, Some(search))?;

        for file_skim in selected_files.selected_items.iter() {
          let file = file_skim.output().to_string();
          let file_path = Path::new(&file);
          let file_name = file_path.file_name().unwrap();

          fs::remove_file(&drash.files.join(file_name))?;
          fs::remove_file(
            &drash.info.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
          )?;
        }
        let total_files = selected_files.selected_items.len();

        if total_files > 1 {
          println!("\nRemoved: {total_files} files");
        } else {
          println!("\nRemoved: 1 file");
        }
        return Ok(());
      }

      let last_file = fs::read_dir(&drash.files)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          let path = file.path();
          let file_name = file.file_name();
          let file_name = Path::new(&file_name);

          let file_info =
            fs::read_to_string(&drash.info.join(format!("{}.drashinfo", file_name.display())))?;

          if path.is_dir() {
            fs::remove_dir_all(&drash.files.join(&file_name))?;
          } else {
            fs::remove_file(&drash.files.join(&file_name))?;
          }
          fs::remove_file(
            &drash.info.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
          )?;

          let file_path: Rc<_> = file_info.split("\n").collect();
          let file_path = file_path[1].trim_start_matches("Path=");

          println!("Remvoed: {}", file_path)
        }
        None => eprintln!("Drashcan is empty"),
      }

      return Ok(());
    }

    let mut empty = true;
    let entries = fs::read_dir(&drash.info)?;
    let mut real_path: Vec<String> = Vec::new();

    for path in entries {
      let path = path?;
      let file_info: Rc<_> = fs::read_to_string(&path.path())?
        .lines()
        .map(|line| line.to_string())
        .collect();

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

    let list_formatter: MultiOptionFormatter<'_, String> = &|a| {
      if a.len() == 1 {
        return format!("{} file removed", a.len());
      }
      format!("{} files removed", a.len())
    };

    let validate = |a: &[ListOption<&String>]| -> Result<Validation, Box<dyn Error + Send + Sync>> {
      if a.len() < 1 {
        return Ok(Validation::Invalid("At select one file to remove".into()));
      }
      Ok(Validation::Valid)
    };

    let file_paths = MultiSelect::new("Select files to remove:", real_path)
      .with_formatter(list_formatter)
      .with_validator(validate)
      .prompt()?;

    for path in file_paths {
      let path = Path::new(&path);
      let file_name = path.file_name().unwrap().to_str().unwrap();
      if drash.files.join(file_name).is_dir() {
        fs::remove_dir_all(&drash.files.join(file_name))?;
      } else {
        fs::remove_file(&drash.files.join(file_name))?;
      }
      fs::remove_file(&drash.info.join(format!("{}.drashinfo", file_name)))?;
    }

    return Ok(());
  }

  if let Some(Commands::Empty { yes }) = &args.commands {
    let files = fs::read_dir(&drash.files)?;
    let file_entries = files.count();
    if file_entries == 0 {
      println!("Drashcan is alraedy empty");
      return Ok(());
    }

    if *yes {
      fs::remove_dir_all(&drash.files)?;
      fs::remove_dir_all(&drash.info)?;

      fs::create_dir(&drash.files)?;
      fs::create_dir(&drash.info)?;

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

    fs::remove_dir_all(&drash.files)?;
    fs::remove_dir_all(&drash.info)?;

    fs::create_dir(&drash.files)?;
    fs::create_dir(&drash.info)?;
  }

  if let Some(Commands::Restore {
    overwrite,
    search_file,
  }) = &args.commands
  {
    if let Some(search) = search_file {
      if search != "-" {
        let selected_files = fuzzy_find_files(&drash.info, Some(search))?;
        let mut total_files = 0;

        for file_skim in selected_files.selected_items.iter() {
          let file = file_skim.output().to_string();
          let file_path = Path::new(&file);
          let file_name = file_path.file_name().unwrap().to_str().unwrap();

          let check = check_overwrite(file_path, *overwrite);
          if check {
            fs::rename(&drash.files.join(file_name), file_path)?;
            fs::remove_file(&drash.info.join(format!("{}.drashinfo", file_name)))?;
            total_files += 1;
          } else {
            println!("Skipped {file_name}");
            continue;
          }
        }

        if total_files == 0 {
          println!("\nDidn't restore any file");
        }
        return Ok(());
      }

      let last_file = fs::read_dir(&drash.files)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          let file_name = file.file_name();
          let file_name = Path::new(&file_name);

          let file_info =
            fs::read_to_string(&drash.info.join(format!("{}.drashinfo", file_name.display())))?;

          let file_path: Rc<_> = file_info.split("\n").collect();
          let file_path = file_path[1].trim_start_matches("Path=");

          let check = check_overwrite(file_path, *overwrite);
          if check {
            fs::rename(&drash.files.join(&file_name), file_path)?;
            fs::remove_file(
              &drash.info.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
            )?;
            println!("Restored: {}", file_name.display());
          } else {
            println!("Did not restored: {}", file_name.display());
          }
        }
        None => eprintln!("Drashcan is empty"),
      }

      return Ok(());
    }

    let mut path_entries: Vec<String> = Vec::new();
    let mut empty = true;
    let paths = fs::read_dir(&drash.info)?;

    for path in paths {
      let path = path?.path();
      let file_info = fs::read_to_string(&path)?;
      let file_info: Rc<_> = file_info.split("\n").collect();
      let file_path = file_info[1].trim_start_matches("Path=").to_string();
      if !file_path.is_empty() {
        empty = false;
      }
      path_entries.push(file_path);
    }
    if empty {
      println!("Drashcan is empty");
      return Ok(());
    }

    let restore_paths = MultiSelect::new("Select files to restore", path_entries)
      .with_validator(min_length!(1, "At least choose one file to restore"))
      .prompt()?;

    for entry in restore_paths {
      let path = Path::new(&entry);
      let file_name = path.file_name().unwrap().to_str().unwrap();

      let check = check_overwrite(path, *overwrite);
      if check {
        fs::rename(&drash.files.join(file_name), path)?;
        fs::remove_file(&drash.info.join(format!("{}.drashinfo", file_name)))?;
        println!("Restored: {file_name}");
      } else {
        println!("Skipped {}", file_name);
        continue;
      }
    }
    return Ok(());
  }

  Ok(())
}
