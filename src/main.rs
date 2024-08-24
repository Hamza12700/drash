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
  io::Write,
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
  fn is_none(&self) -> bool {
    if self.files.is_none() && self.commands.is_none() {
      return true;
    }
    return false;
  }
}

fn main() -> anyhow::Result<()> {
  let drash_dir = Path::new(&env::var("HOME")?).join(".local/share/Drash");
  let drash_files = Path::new(&drash_dir).join("files");
  let drash_info_dir = Path::new(&drash_dir).join("info");

  if !drash_dir.exists() {
    if let Err(err) = fs::create_dir(&drash_dir) {
      eprintln!("Failed to create directory at {:?}", drash_dir);
      eprintln!("Error message: {err}");
      exit(1);
    }

    if let Err(err) = fs::create_dir(Path::new(&drash_dir).join("info")) {
      eprintln!("Failed to create directory at {:?}", drash_info_dir);
      eprintln!("Error message: {err}");
      exit(1);
    }

    if let Err(err) = fs::create_dir(&drash_files) {
      eprintln!("Failed to create directory at {:?}", drash_files);
      eprintln!("Error message: {err}");
      exit(1);
    }
  }
  let args = Args::parse();
  if args.is_none() {
    let selected_files = fuzzy_find_files(".", None)?;

    for item in selected_files.selected_items.iter() {
      let selected_file_name = item.output().to_string();
      let file = Path::new(&selected_file_name);

      if file.is_symlink() {
        fs::remove_file(&file)?;
        return Ok(());
      }

      let mut file_name = file.display().to_string();
      if !file.exists() {
        eprintln!("file not found: '{}'", file_name.bold());
        continue;
      }

      if file_name.ends_with("/") {
        file_name.pop();
      }
      if file_name.contains("/") {
        let paths = file_name.split("/").last();
        file_name = paths.unwrap().to_string();
      }
      let current_file = env::current_dir()?.join(&file);

      let mut buffer = fs::OpenOptions::new()
        .write(true)
        .append(true)
        .create(true)
        .open(Path::new(&drash_info_dir).join(format!("{}.drashinfo", file_name)))?;

      buffer.write_all(b"[Drash Info]\n")?;
      let formatted_string = format!("Path={}\n", current_file.display());
      buffer.write_all(formatted_string.as_bytes())?;
      buffer.write_all(b"FileType=")?;
      if file.is_dir() {
        buffer.write_all(b"directory\n")?;
      } else {
        buffer.write_all(b"file\n")?;
      }
      fs::rename(file, Path::new(&drash_files).join(file_name))?;
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
      if file.is_symlink() {
        fs::remove_file(&file)?;
        return Ok(());
      }

      let mut file_name = file.display().to_string();
      if !file.exists() {
        eprintln!("file not found: '{}'", file_name.bold());
        continue;
      }

      if file_name.ends_with("/") {
        file_name.pop();
      }
      if file_name.contains("/") {
        let paths = file_name.split("/").last();
        file_name = paths.unwrap().to_string();
      }
      // TODO: fix bug when drashing same files
      let current_file = env::current_dir()?.join(&file);

      let mut buffer = fs::OpenOptions::new()
        .write(true)
        .append(true)
        .create(true)
        .open(Path::new(&drash_info_dir).join(format!("{}.drashinfo", file_name)))?;

      buffer.write_all(b"[Drash Info]\n")?;
      let formatted_string = format!("Path={}\n", current_file.display());
      buffer.write_all(formatted_string.as_bytes())?;
      buffer.write_all(b"FileType=")?;
      if file.is_dir() {
        buffer.write_all(b"directory\n")?;
      } else {
        buffer.write_all(b"file\n")?;
      }
      fs::rename(file, Path::new(&drash_files).join(file_name))?;
    }
  }

  if let Some(Commands::List) = &args.commands {
    let mut empty = true;
    let entries = fs::read_dir(&drash_info_dir)?;
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
        let selected_files = fuzzy_find_files(&drash_info_dir, Some(search))?;

        for file_skim in selected_files.selected_items.iter() {
          let file = file_skim.output().to_string();
          let file_path = Path::new(&file);
          let file_name = file_path.file_name().unwrap();

          fs::remove_file(&drash_files.join(file_name))?;
          fs::remove_file(
            &drash_info_dir.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
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

      let last_file = fs::read_dir(&drash_files)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          let path = file.path();
          let file_name = file.file_name();
          let file_name = Path::new(&file_name);

          let file_info =
            fs::read_to_string(&drash_info_dir.join(format!("{}.drashinfo", file_name.display())))?;

          if path.is_dir() {
            fs::remove_dir_all(&drash_files.join(&file_name))?;
          } else {
            fs::remove_file(&drash_files.join(&file_name))?;
          }
          fs::remove_file(
            &drash_info_dir.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
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
    let entries = fs::read_dir(&drash_info_dir)?;
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
      if drash_files.join(file_name).is_dir() {
        fs::remove_dir_all(&drash_files.join(file_name))?;
      } else {
        fs::remove_file(&drash_files.join(file_name))?;
      }
      fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
    }

    return Ok(());
  }

  if let Some(Commands::Empty { yes }) = &args.commands {
    let files = fs::read_dir(&drash_files)?;
    let file_entries = files.count();
    if file_entries == 0 {
      println!("Drashcan is alraedy empty");
      return Ok(());
    }

    if *yes {
      fs::remove_dir_all(&drash_files)?;
      fs::remove_dir_all(&drash_info_dir)?;

      fs::create_dir(&drash_files)?;
      fs::create_dir(&drash_info_dir)?;

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

    fs::remove_dir_all(&drash_files)?;
    fs::remove_dir_all(&drash_info_dir)?;

    fs::create_dir(&drash_files)?;
    fs::create_dir(&drash_info_dir)?;
  }

  if let Some(Commands::Restore {
    overwrite,
    search_file,
  }) = &args.commands
  {
    if let Some(search) = search_file {
      if search != "-" {
        let selected_files = fuzzy_find_files(&drash_info_dir, Some(search))?;
        let mut total_files = 0;

        for file_skim in selected_files.selected_items.iter() {
          let file = file_skim.output().to_string();
          let file_path = Path::new(&file);
          let file_name = file_path.file_name().unwrap().to_str().unwrap();

          let check = check_overwrite(file_path, *overwrite);
          if check {
            fs::rename(&drash_files.join(file_name), file_path)?;
            fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
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

      let last_file = fs::read_dir(&drash_files)?
        .flatten()
        .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

      match last_file {
        Some(file) => {
          let file_name = file.file_name();
          let file_name = Path::new(&file_name);

          let file_info =
            fs::read_to_string(&drash_info_dir.join(format!("{}.drashinfo", file_name.display())))?;

          let file_path: Rc<_> = file_info.split("\n").collect();
          let file_path = file_path[1].trim_start_matches("Path=");

          let check = check_overwrite(file_path, *overwrite);
          if check {
            fs::rename(&drash_files.join(&file_name), file_path)?;
            fs::remove_file(
              &drash_info_dir.join(format!("{}.drashinfo", file_name.to_str().unwrap())),
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
    let paths = fs::read_dir(&drash_info_dir)?;

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
        fs::rename(&drash_files.join(file_name), path)?;
        fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
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
