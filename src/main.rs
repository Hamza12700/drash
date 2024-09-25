use clap::{Parser, Subcommand};
use drash::{colors::Colorize, utils};
use inquire::{min_length, Confirm, MultiSelect};
use std::{
  env, fmt, fs,
  io::{self, Write},
  path::{Path, PathBuf},
  process::exit,
};
use tabled::{settings::Style, Table, Tabled};

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

impl fmt::Display for FileList {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{}", self.path)
  }
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
  //// Path to the *drashed* files
  files_path: PathBuf,

  /// Path to the *drashed* info files
  info_path: PathBuf,
}

enum ConfigError {
  EnvError(env::VarError),
  IoError(io::Error),
}

impl From<env::VarError> for ConfigError {
  fn from(err: env::VarError) -> Self {
    ConfigError::EnvError(err)
  }
}

impl From<io::Error> for ConfigError {
  fn from(err: io::Error) -> Self {
    ConfigError::IoError(err)
  }
}

impl Drash {
  /// Create new **Drash** struct containing path to:
  /// - Drash files
  /// - Drash files info
  fn new() -> Result<Self, ConfigError> {
    let home = env::var("HOME")?;
    let drash_dir = Path::new(&home).join(".local/share/Drash");
    let drash_files = Path::new(&drash_dir).join("files");
    let drash_info_dir = Path::new(&drash_dir).join("info");

    if !drash_dir.exists() {
      fs::create_dir(&drash_dir)?;
      fs::create_dir(&drash_files)?;
      fs::create_dir(&drash_info_dir)?;
    }

    Ok(Self {
      files_path: drash_files,
      info_path: drash_info_dir,
    })
  }

  /// List original file paths in the 'drashcan' and returning `FileList` struct containing the
  /// original file path and file-type
  fn list_file_paths(&self) -> io::Result<Box<[FileList]>> {
    let info_files = fs::read_dir(&self.info_path)?;
    let mut file_paths: Vec<FileList> = Vec::with_capacity(1);

    for file in info_files {
      let file = file?;
      let file_content = fs::read_to_string(file.path())?;
      let file_content: Box<[&str]> = file_content.lines().collect();

      let original_path = file_content[1].trim_start_matches("Path=");
      let file_type = file_content[2].trim_start_matches("FileType=");
      file_paths.push(FileList {
        path: original_path.to_string(),
        file_type: file_type.to_string(),
      });
    }

    Ok(file_paths.into_boxed_slice())
  }

  /// Put file into drashcan
  fn put_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
    let path = path.as_ref();

    // Do not store the symlink, instead delete it and return
    if path.is_symlink() {
      fs::remove_file(path)?;
      println!("{}", "Removed symlink".bold());
      return Ok(());
    }
    let mut file_name = path.display().to_string();

    // Reture an error if file not found
    if !path.exists() {
      return Err(io::Error::new(
        io::ErrorKind::NotFound,
        format!("file not found: '{}'", file_name.bold()),
      ));
    }

    // Remove trailing forward slash
    if file_name.ends_with("/") {
      file_name.pop();
    }

    // Get the file in current working directory
    let current_file = env::current_dir()?.join(&file_name);

    // Create file in append mode for storing info about the file and it original path
    let mut buffer = fs::OpenOptions::new()
      .write(true)
      .append(true)
      .create(true)
      .open(Path::new(&self.info_path).join(format!("{}.drashinfo", file_name)))?;

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
    fs::rename(path, Path::new(&self.files_path).join(file_name))?;

    Ok(())
  }

  /// Remove file in the `drashcan`
  fn remove_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
    let original_file_path = path.as_ref();
    let file_name = original_file_path.file_name().unwrap().to_str().unwrap();
    let file_info = format!("{file_name}.drashinfo");

    // Check if path is a directory or a file
    match &self.files_path.join(file_name).is_dir() {
      true => {
        fs::remove_dir_all(&self.files_path.join(file_name))?;
        fs::remove_file(&self.info_path.join(file_info))?;
      }
      false => {
        fs::remove_file(&self.files_path.join(file_name))?;
        fs::remove_file(&self.info_path.join(file_info))?;
      }
    }

    Ok(())
  }

  /// Restore a file to its original path
  fn restore_file<P: AsRef<Path>>(&self, path: P, overwrite: bool) -> io::Result<bool> {
    let path = path.as_ref();
    let file_name = path.file_name().unwrap();
    let file_info = format!("{}.drashinfo", file_name.to_str().unwrap());

    // Check if the same file exists in path
    let check = utils::check_overwrite(path, overwrite);
    match check {
      true => {
        fs::rename(&self.files_path.join(file_name), path)?;
        fs::remove_file(&self.info_path.join(file_info))?;
        Ok(true)
      }
      false => Ok(false),
    }
  }

  /// Search for files for their original file path stored in the drashcan
  fn search_file_path(&self, msg: &str) -> anyhow::Result<Vec<String>> {
    let file_info = fs::read_dir(&self.info_path)?;
    let mut path_vec: Vec<String> = Vec::new();

    // Push the original path value in `path_vec`
    for file in file_info {
      let file = file?;
      let file_info = fs::read_to_string(file.path())?;
      let file_info: Box<[&str]> = file_info.split("\n").collect();
      let file_path = file_info[1].trim_start_matches("Path=");
      if file_path.is_empty() {
        continue;
      }
      path_vec.push(file_path.to_string())
    }

    // Prompt the user to choose files
    let ans = MultiSelect::new(msg, path_vec)
      .with_validator(min_length!(1, "At least choose one file"))
      .prompt()?;

    Ok(ans)
  }
}

fn main() -> anyhow::Result<()> {
  let drash = match Drash::new() {
    Ok(val) => val,
    Err(err) => match err {
      ConfigError::EnvError(var_error) => {
        eprintln!("{}", "Failed to read env variable".bold());
        eprintln!("{}: {var_error}", "Error".bold().red());
        exit(1);
      }
      ConfigError::IoError(io_error) => {
        eprintln!("{}", "Failed to create directories".bold());
        eprintln!("{}: {io_error}", "Error".bold().red());
        exit(1);
      }
    },
  };

  let args = Args::parse();
  if args.is_none() {
    let entries = fs::read_dir(".")?;
    let mut path_vec: Vec<String> = Vec::new();
    for path in entries {
      let path = path?;
      let path = path.path();
      // Remove `./` from the start of the string
      path_vec.push(
        path
          .display()
          .to_string()
          .trim_start_matches("./")
          .to_string(),
      )
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
