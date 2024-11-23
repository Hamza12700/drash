use crate::{colors::Colorize, utils};
use std::{
  env, fmt, fs,
  io::{self, Write},
  path::{Path, PathBuf},
};

use inquire::{min_length, MultiSelect};
use tabled::Tabled;

pub struct Drash {
  //// Path to the *drashed* files
  pub files_path: PathBuf,

  /// Path to the *drashed* info files
  pub info_path: PathBuf,
}

#[derive(Tabled)]
pub struct FileList {
  pub file_type: String,
  pub path: String,
}

impl fmt::Display for FileList {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{}", self.path)
  }
}

impl Drash {
  /// Creates a new `Drash` struct containing path to:
  /// - Removed files
  /// - Metadata about the removed files
  pub fn new() -> Self {
    let home = env::var("HOME").expect("failed to get the HOME environment variable");
    let drash_dir = Path::new(&home).join(".local/share/Drash");
    let drash_files = Path::new(&drash_dir).join("files");
    let drash_info_dir = Path::new(&drash_dir).join("info");

    if !drash_dir.exists() {
      fs::create_dir(&drash_dir).expect("failed to create the drash home directory");
      fs::create_dir(&drash_files).expect("failed to create the drash files directory");
      fs::create_dir(&drash_info_dir).expect("failed to create the drash info directory");
    }

    Self {
      files_path: drash_files,
      info_path: drash_info_dir,
    }
  }

  /// List original file paths in the 'drashcan' and returning `FileList` struct containing the
  /// original file path and file-type
  pub fn list_file_paths(&self) -> io::Result<Box<[FileList]>> {
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
  pub fn put_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
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
      .open(format!(
        "{}/{}.drashinfo",
        &self.info_path.display(),
        file_name
      ))?;

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
  pub fn remove_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
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
  pub fn restore_file<P: AsRef<Path>>(&self, path: P, overwrite: bool) -> io::Result<bool> {
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

  /// Search for files for their original file path
  pub fn search_file_path(&self, msg: &str) -> anyhow::Result<Box<[String]>> {
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

    Ok(ans.into_boxed_slice())
  }
}
