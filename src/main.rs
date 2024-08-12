use anyhow::Ok;
use clap::{Parser, Subcommand};
use colored::Colorize;
use inquire::Confirm;
use tabled::{settings::Style, Table, Tabled};
use utils::check_path;
mod utils;
use std::{
  collections::HashMap,
  env, fs,
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

  #[command(subcommand)]
  commands: Option<Commands>,
}

#[derive(Subcommand, Debug)]
enum Commands {
  /// List files in the drashcan
  List,

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
  },
}

#[derive(Tabled)]
struct FileList {
  id: usize,
  file_type: String,
  path: String,
}

impl Args {
  fn validate(&self) {
    if self.files.is_none() && self.commands.is_none() {
      eprintln!(
        "{}: one argument is required: {} or {}",
        "error".red().bold(),
        "<FILES>".bold(),
        "<COMMANDS>".bold()
      );
      eprintln!(
        "\n{}: {} <FILES> | <COMMANDS>\n",
        "Usage".bold().underline(),
        "drash".bold()
      );
      eprintln!("For more information, try '{}'", "--help".bold());
      exit(1);
    }
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
  args.validate();

  if let Some(files) = &args.files {
    for file in files {
      if file.is_symlink() {
        fs::remove_file(&file)?;
        exit(0);
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
    let mut idx = 0;
    let mut empty = true;
    let paths = fs::read_dir(&drash_info_dir)?;
    let mut real_path: Vec<FileList> = Vec::new();

    for path in paths {
      let path = path?;
      let file_info: Rc<_> = fs::read_to_string(&path.path())?
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
        id: idx,
        file_type,
        path: file_path,
      });
      idx += 1;
    }
    if empty {
      println!("Nothing is the drashcan");
      exit(0);
    }

    let table_style = Style::sharp();
    let table = Table::new(real_path).with(table_style).to_string();
    println!("{table}");
  }

  if let Some(Commands::Empty { yes }) = &args.commands {
    let files = fs::read_dir(&drash_files)?;
    let file_entries = files.count();
    if file_entries == 0 {
      println!("Drashcan is alraedy empty");
      exit(0);
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
      exit(0);
    }

    let user_input = Confirm::new("Empty the drash directory?")
      .with_default(true)
      .prompt()?;

    if !user_input {
      exit(0);
    }

    fs::remove_dir_all(&drash_files)?;
    fs::remove_dir_all(&drash_info_dir)?;

    fs::create_dir(&drash_files)?;
    fs::create_dir(&drash_info_dir)?;
  }

  if let Some(Commands::Restore { overwrite }) = &args.commands {
    let mut len = 0;
    let mut files: HashMap<usize, PathBuf> = HashMap::new();
    let mut empty = true;
    let paths = fs::read_dir(&drash_info_dir)?;

    for (idx, path) in paths.enumerate() {
      let path = path?.path();
      let file_info = fs::read_to_string(&path)?;
      let mut path_value = "";

      for line in file_info.lines() {
        if line.starts_with("Path=") {
          path_value = line.trim_start_matches("Path=");
        } else if line.starts_with("FileType=") {
          empty = false;
          let mut file_type = line.trim_start_matches("FileType=");
          if file_type == "file" {
            file_type = "F"
          } else {
            file_type = "D"
          }
          println!("  {idx}:{file_type} - {path_value}");
          files.insert(idx, Path::new(path_value).to_path_buf());
        }
      }
      len = idx;
    }
    if empty {
      println!("Drashcan is empty");
      exit(0);
    }

    print!("What file to restore [0..{len}]: ");
    io::stdout().flush()?;
    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input)?;

    let user_input = user_input.trim();
    if !user_input.contains(",") && !user_input.contains("-") {
      let idx: usize = user_input.parse()?;
      let path = files.get(&idx).unwrap();

      match *overwrite {
        true => {
          let file_name = path.file_name().unwrap().to_str().unwrap();
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        }
        false => {
          let _ = check_path(path, &drash_files, &drash_info_dir);
        }
      }
    } else if user_input.contains(",") && user_input.contains("-") {
      let user_input: Rc<_> = user_input.split(",").collect();
      if user_input[1].is_empty() {
        eprintln!("{}", "Invalid input".bold().red());
        eprintln!("example usage: {}", "0-range, X...".bold());
        exit(1);
      }

      let range = user_input[0];
      let range: Rc<_> = range.split("-").collect();
      if range[1].is_empty() {
        eprintln!("{}", "Invalid range sytax".bold().red());
        eprintln!("example range usage: {}", "0-range".bold());
        exit(1);
      }

      let start_idx: usize = range[0].parse()?;
      let end_idx: usize = range[1].parse()?;

      for idx in start_idx..=end_idx {
        if idx > len {
          eprintln!("Out of range: 0-{len}");
          continue;
        }

        let path = files.get(&idx).unwrap();
        if *overwrite {
          let file_name = path.file_name().unwrap().to_str().unwrap();
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        } else {
          let skip = check_path(path, &drash_files, &drash_info_dir);
          if skip {
            continue;
          }
        }
      }

      for idx in user_input.iter().skip(1) {
        let idx = idx.replace(" ", "");
        let idx: usize = idx.parse()?;
        if idx > len {
          eprintln!("Out of range 0-{len}");
          continue;
        }

        let path = files.get(&idx).unwrap();
        if *overwrite {
          let file_name = path.file_name().unwrap().to_str().unwrap();
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        } else {
          let skip = check_path(path, &drash_files, &drash_info_dir);
          if skip {
            continue;
          }
        }
      }
    } else if user_input.contains(",") {
      let user_input = user_input.replace(" ", "");
      let multi_index = user_input.split(",");
      for index in multi_index {
        let index: usize = index.parse()?;
        if index > len {
          eprintln!("Out of range 0-{len}");
          continue;
        }

        let path = files.get(&index).unwrap();
        if *overwrite {
          let file_name = path.file_name().unwrap().to_str().unwrap();
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        } else {
          let skip = check_path(path, &drash_files, &drash_info_dir);
          if skip {
            continue;
          }
        }
      }
    } else if user_input.contains("-") {
      let range: Rc<_> = user_input.split("-").collect();
      if range.len() != 2 {
        eprintln!("{}", "Invalid input".bold().red());
        eprintln!("Sytax to use range: 0-{len}");
        exit(1);
      }
      let start_idx: usize = range[0].parse()?;
      let end_idx: usize = range[1].parse()?;

      for idx in start_idx..=end_idx {
        if idx > len {
          eprintln!("Out of range 0-{len}");
          continue;
        }

        let path = files.get(&idx).unwrap();
        if *overwrite {
          let file_name = path.file_name().unwrap().to_str().unwrap();
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        } else {
          let skip = check_path(path, &drash_files, &drash_info_dir);
          if skip {
            continue;
          }
        }
      }
    }
  }

  Ok(())
}
