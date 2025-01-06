use crate::{colors::Colorize, utils};
use std::{
  env, fmt, fs,
  io::{self, Write},
  path::{Path, PathBuf},
};

use chrono::Local;
use inquire::{min_length, MultiSelect};
use tabled::Tabled;

/// Represents the paths for **Drashed** files and their **Info** files.
pub struct Drash {
  //// Path to the *drashed* files
  pub files_path: PathBuf,

  /// Path to the *drashed* info files
  pub info_path: PathBuf,
}

#[derive(Tabled)]
pub struct FileList {
  pub time: String,
  pub path: String,
  pub file_type: String,
}

pub struct FileInfo {
  pub path: PathBuf,
  pub filetype: String,
  pub time: String,
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

  /// Parse Drashed files metadata
  pub fn parse_file_info(&self) -> Box<[FileInfo]> {
    let mut parsed_files: Vec<FileInfo> = Vec::new();
    let dir_entries =
      fs::read_dir(&self.info_path).expect("failed to read drashed files directory");

    for entry in dir_entries {
      let file = entry.expect("failed to read directory entry");
      let file_content = fs::read_to_string(file.path()).expect("failed to read file into string");

      let lines: Box<[&str]> = file_content.lines().collect();

      // Skip the first line
      parsed_files.push(FileInfo {
        path: lines[1].trim_start_matches("Path=").into(),
        filetype: lines[2].trim_start_matches("FileType=").to_string(),
        time: lines[3].trim_start_matches("Time=").to_string(),
      });
    }

    parsed_files.into_boxed_slice()
  }

  /// Remove File without moving it into the *Drash* directory
  pub fn force_remove_file(&self, files: &Vec<PathBuf>) {
    for file in files {
      if file.is_symlink() {
        fs::remove_file(file).expect("failed to remove file");
        return;
      }

      let file_name = file.display().to_string();
      if !file.exists() {
        eprintln!("file not found: '{}'", file_name.bold());
        continue;
      }

      match file.is_dir() {
        true => fs::remove_dir_all(file).expect("failed to remove directory"),
        false => fs::remove_file(file).expect("failed to remove file"),
      };
    }

    if files.len() == 1 {
      println!("Removed file");
      return;
    }

    println!("Removed: {} files", files.len());
  }

  /// List files in the 'drashcan'
  pub fn list_file_paths(&self) -> Box<[FileList]> {
    let parsed_info = self.parse_file_info();
    let mut files = Vec::new();

    for file in parsed_info {
      files.push(FileList {
        path: file.path.display().to_string(),
        file_type: file.filetype,
        time: file.time,
      });
    }

    files.into_boxed_slice()
  }

  /// Put file into drashcan
  pub fn put_file<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
    let path = path.as_ref();

    // Do not store the symlink, instead delete it and exit
    if path.is_symlink() {
      fs::remove_file(path)?;
      let file_name = path.file_name().unwrap_or_default();
      println!(
        "Removed symlink: {}",
        file_name.to_str().unwrap_or_default()
      );
      return Ok(());
    }

    let mut file_name = path.file_name().unwrap().to_str().unwrap().to_string();

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
        self.info_path.display(),
        file_name
      ))?;

    buffer.write_all(b"[Drash-Info]\n")?;

    // Write the original `Path` value
    let formatted_string = format!("Path={}\n", current_file.display());
    buffer.write_all(formatted_string.as_bytes())?;

    // Write what file type it is:
    buffer.write_all(b"FileType=")?;
    if path.is_dir() {
      buffer.write_all(b"directory\n")?;
    } else {
      buffer.write_all(b"file\n")?;
    }

    buffer.write_all(b"Time=")?;
    let local_time = Local::now().format("%-d %b, %a %Y %H:%M").to_string();
    buffer.write_all(local_time.as_bytes())?;

    // Move the file to drashed files directory
    fs::rename(path, Path::new(&self.files_path).join(file_name))?;

    Ok(())
  }

  /// Remove file in the `drashcan`
  pub fn remove_file_from_drash<P: AsRef<Path>>(&self, path: P) -> io::Result<()> {
    let original_file_path = path.as_ref();
    let file_name = original_file_path.file_name().unwrap().to_str().unwrap();
    let file_info = format!("{file_name}.drashinfo");

    // Check if path is a directory or a file
    match &self.files_path.join(file_name).is_dir() {
      true => {
        fs::remove_dir_all(self.files_path.join(file_name))?;
        fs::remove_file(self.info_path.join(file_info))?;
      }
      false => {
        fs::remove_file(self.files_path.join(file_name))?;
        fs::remove_file(self.info_path.join(file_info))?;
      }
    }

    Ok(())
  }

  /// Restore a file to its original path
  pub fn restore_file<P: AsRef<Path>>(&self, path: P, overwrite: bool) -> bool {
    let path = path.as_ref();
    let file_name = path.file_name().unwrap();
    let file_info = format!("{}.drashinfo", file_name.to_str().unwrap());

    // Check if the same file exists in path
    let check = utils::check_overwrite(path, overwrite);
    if check {
      fs::rename(self.files_path.join(file_name), path).expect("failed to move file");
      fs::remove_file(self.info_path.join(file_info)).expect("failed to remove file");
      return true;
    }

    false
  }

  /// Search for files for their original file path
  pub fn search_file_path(&self, msg: &str) -> Box<[String]> {
    let file_info = fs::read_dir(&self.info_path).expect("failed to read Info directory");
    let mut path_vec: Vec<String> = Vec::new();

    // Push the original path value in `path_vec`
    for file in file_info {
      let file = file.expect("failed to get directory entry");
      let file_info = fs::read_to_string(file.path()).expect("failed to read file into string");
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
      .prompt()
      .unwrap();

    ans.into_boxed_slice()
  }
}

#[cfg(test)]
mod tests {
  use fs::File;

  use super::*;

  #[test]
  fn test_parse_file_info() {
    let drash = Drash::new();
    File::create_new("tmp").unwrap();

    drash.put_file("tmp").unwrap();
    let parsed_info = drash.parse_file_info();
    let cwd = env::current_dir().unwrap();

    fs::remove_file(&format!("{}/tmp", drash.files_path.display())).unwrap();
    fs::remove_file(&format!("{}/tmp.drashinfo", drash.info_path.display())).unwrap();

    assert_eq!(parsed_info[0].filetype, "file");
    assert_eq!(
      parsed_info[0].path,
      Path::new(&format!("{}/tmp", cwd.display()))
    );
  }
}
