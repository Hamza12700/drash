use anyhow::Ok;
use clap::Parser;
use std::{
  collections::HashMap,
  env, fs,
  io::{self, Write},
  path::{Path, PathBuf},
  process::exit,
};

/// Put files/directories into drash
#[derive(Debug, Parser)]
#[command(version)]
#[group(required = true)]
struct Args {
  /// Files to drash
  files: Vec<PathBuf>,

  /// List files/directories in the drash
  #[arg(short, long)]
  list: bool,

  /// Empty drash
  #[arg(short, long)]
  empty: bool,

  /// Restore from drash chosen files
  #[arg(short, long)]
  restore: bool,
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

  if args.files.len() >= 1 {
    let files: &Vec<PathBuf> = args.files.as_ref();
    for file in files {
      let mut file_name = file.display().to_string();
      if !file.exists() {
        eprintln!("file not found: '{}'", file_name);
        exit(1);
      }
      if file_name.ends_with("/") {
        file_name.pop();
      }
      let current_file = env::current_dir()?.join(&file_name);
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
      fs::rename(file, Path::new(&drash_files).join(file))?;
    }
  }

  if args.empty {
    let files = fs::read_dir(&drash_files)?;
    let file_entries = files.count();
    if file_entries == 0 {
      println!("Drashcan is alraedy empty");
      exit(0);
    }
    println!("Would empty the following drash directories:");
    println!("  - {}", &drash_dir.display());
    println!("  - Entries {file_entries}");
    print!("Proceed? (Y/n): ");
    io::stdout().flush()?;

    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input)?;
    if user_input.trim() == "n" {
      exit(0);
    }

    fs::remove_dir_all(&drash_files)?;
    fs::remove_dir_all(&drash_info_dir)?;

    fs::create_dir(&drash_files)?;
    fs::create_dir(&drash_info_dir)?;
  }

  if args.list {
    let mut idx = 0;
    let mut empty = true;
    let paths = fs::read_dir(&drash_info_dir)?;
    for path in paths {
      let path = path?;
      let file_info = fs::read_to_string(&path.path())?;
      let mut path_value = "";

      for line in file_info.lines() {
        if line.starts_with("Path=") {
          empty = false;
          path_value = line.trim_start_matches("Path=");
          idx += 1;
        } else if line.starts_with("FileType=") {
          let mut file_type = line.trim_start_matches("FileType=");
          if file_type == "file" {
            file_type = "F"
          } else {
            file_type = "D"
          }
          println!("{idx}:{file_type} - {path_value}");
        }
      }
    }
    if empty {
      println!("Nothing is the drashcan");
    }
  }

  if args.restore {
    let mut len = 0;
    let mut files: HashMap<usize, PathBuf> = HashMap::new();
    let paths = fs::read_dir(&drash_info_dir)?;

    for (idx, path) in paths.enumerate() {
      let path = path?;
      let file_info = fs::read_to_string(&path.path())?;

      for line in file_info.lines() {
        if line.starts_with("Path=") {
          let path_value = line.trim_start_matches("Path=");
          println!("  {idx} {path_value}");
          files.insert(idx, Path::new(path_value).to_path_buf());
        }
      }
      len = idx;
    }

    print!("What file to restore [0..{len}]: ");
    io::stdout().flush()?;
    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input)?;

    let user_input: usize = user_input.trim().parse()?;
    if user_input > len {
      eprintln!("Out of range 0..{len}");
      exit(1);
    }
    let file = files.get(&user_input).unwrap();
    let file_name = file.file_name().unwrap();
    fs::rename(&drash_files.join(file_name), file)?;
    fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name.to_str().unwrap())))?;
  }

  Ok(())
}
