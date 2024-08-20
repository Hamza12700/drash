use clap::{Parser, Subcommand};
use colored::Colorize;
use inquire::{
  formatter::MultiOptionFormatter, list_option::ListOption, min_length, validator::Validation,
  Confirm, MultiSelect,
};
use skim::{
  prelude::{SkimItemReader, SkimOptionsBuilder},
  Skim,
};
use tabled::{settings::Style, Table, Tabled};
use utils::check_path;
mod utils;
use std::{
  collections::HashMap,
  env,
  error::Error,
  fs,
  io::{self, Cursor, Write},
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

  /// Remove selected files in the drashcan
  Remove {
    /// remove the last drashed file. Use `-` to delete it
    last: Option<String>,
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

    /// restore files interactively
    #[arg(short, long)]
    interactive: bool,
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
    let mut files = String::new();
    let options = SkimOptionsBuilder::default()
      .height(Some("50%"))
      .multi(true)
      .build()?;

    let entries = fs::read_dir(".")?;
    for entry in entries {
      let entry = entry?;
      let path = entry.path();
      if let Some(file_name) = path.file_name().and_then(|name| name.to_str()) {
        files.push_str(file_name);
        files.push('\n');
      }
    }

    let skim_item_reader = SkimItemReader::default();
    let items = skim_item_reader.of_bufread(Cursor::new(files));
    let selected_files = Skim::run_with(&options, Some(items))
      .map(|out| out.selected_items)
      .unwrap_or_else(|| Vec::new());

    for item in selected_files.iter() {
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

    if selected_files.len() > 1 {
      println!("Removed {} files", selected_files.len());
    } else {
      println!("\nRemoved {} file", selected_files.len());
    }

    return Ok(());
  }

  if let Some(files) = &args.files {
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
        file_type,
        path: file_path,
      });
    }
    if empty {
      println!("Nothing is the drashcan");
      return Ok(());
    }

    let table_style = Style::psql();
    let table = Table::new(real_path).with(table_style).to_string();
    println!("{table}");
  }

  if let Some(Commands::Remove { last }) = &args.commands {
    if let Some(_) = last {
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
            fs::remove_dir(&drash_files.join(&file_name))?;
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
        fs::remove_dir(&drash_files.join(file_name))?;
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
    interactive,
  }) = &args.commands
  {
    let mut len = 0;
    let mut files: HashMap<usize, PathBuf> = HashMap::new();
    let mut empty = true;
    let mut path_entries: Vec<String> = Vec::new();
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
          if !*interactive {
            let mut file_type = line.trim_start_matches("FileType=");
            if file_type == "file" {
              file_type = "F"
            } else {
              file_type = "D"
            }
            println!("  {idx}:{file_type} - {path_value}");
            files.insert(idx, Path::new(&path_value).to_path_buf());
          } else {
            path_entries.push(path_value.to_string());
          }
        }
      }
      len = idx;
    }
    if empty {
      println!("Drashcan is empty");
      return Ok(());
    }

    if *interactive {
      let formatter: MultiOptionFormatter<'_, String> = &|a| {
        if a.len() <= 1 {
          return format!("{} file restored", a.len());
        }
        format!("{} files restored", a.len())
      };

      let restore_paths = MultiSelect::new("Select files to restore", path_entries)
        .with_validator(min_length!(1, "At least choose one file to restore"))
        .with_formatter(formatter)
        .prompt()?;

      for entry in restore_paths {
        let path = Path::new(&entry);
        let file_name = path.file_name().unwrap().to_str().unwrap();

        fs::rename(&drash_files.join(file_name), path)?;
        fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
      }

      return Ok(());
    }

    print!("What file to restore [0..{len}]: ");
    io::stdout().flush()?;
    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input)?;

    let user_input = user_input.trim();
    if !user_input.contains(",") && !user_input.contains("-") {
      let idx: usize = user_input.parse()?;
      let path = files.get(&idx).unwrap();
      let file_name = path.file_name().unwrap().to_str().unwrap();

      match *overwrite {
        true => {
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
        }
        false => {
          check_path(path, &drash_files, &drash_info_dir);
          fs::rename(&drash_files.join(file_name), path)?;
          fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name)))?;
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
