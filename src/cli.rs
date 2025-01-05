use std::{fs, path::Path, process::exit};

use inquire::{min_length, Confirm, MultiSelect};
use tabled::{settings::Style, Table};

use crate::{colors::Colorize, drash::Drash};

/// List files in the **Drash-Can**
pub fn list_drash_files(drash: &Drash) {
  let mut original_paths = drash.list_file_paths();
  if original_paths.len() == 0 {
    println!("Nothing is the drashcan");
    return;
  }

  original_paths.sort_unstable_by(|a, b| match (a.file_type.as_str(), b.file_type.as_str()) {
    ("directory", "dir") | ("file", "file") => a.path.cmp(&b.path),
    ("directory", "file") => std::cmp::Ordering::Less,
    ("file", "directory") => std::cmp::Ordering::Greater,
    _ => std::cmp::Ordering::Equal,
  });

  let table_style = Style::psql();
  println!("{}", Table::new(&original_paths).with(table_style));
  println!("\nTotal entries: {}", original_paths.len());
}

/// Select files to **Drash** in current working directory
pub fn drash_cwd(drash: &Drash) {
  let entries = fs::read_dir(".").expect("failed to read current working directory");
  let mut path_vec: Vec<String> = Vec::with_capacity(3);

  for path in entries {
    let path = path.expect("failed to get directory entry");
    let path = path.path();

    // Remove `./` from the start of the string
    let striped_path = path
      .strip_prefix("./")
      .expect("failed to remove './' prefix");
    if let Some(str_path) = striped_path.to_str() {
      path_vec.push(str_path.to_string());
    }
  }

  if path_vec.is_empty() {
    println!("Nothing in the current working directory to drash");
    return;
  }

  let ans = MultiSelect::new("Select files to drash", path_vec)
    .with_validator(min_length!(1, "At least choose one file"))
    .prompt()
    .unwrap();

  for file in ans {
    if let Err(err) = drash.put_file(file) {
      eprintln!("{}: {err}", "Error".red().bold());
      continue;
    }
  }
}

/// Remove file from **Drash**
pub fn remove_files_from_drash(drash: &Drash, search_pattern: Option<String>) {
  if let Some(search) = search_pattern {
    if search != "-" {
      let files = drash.search_file_path("Select files to remove:");

      for file in files {
        let file_path = Path::new(&file);
        if let Err(err) = drash.remove_file_from_drash(file_path) {
          eprintln!("{}: {err}", "Error".bold().red());
          continue;
        }
      }
      return;
    }

    let last_file = fs::read_dir(&drash.files_path)
      .expect("failed to read Drash directory")
      .flatten()
      .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

    match last_file {
      Some(file) => {
        if let Err(err) = drash.remove_file_from_drash(file.path()) {
          eprintln!("Error: {}", err);
          exit(1);
        }
      }
      None => eprintln!("Drashcan is empty"),
    }

    return;
  }

  let mut empty = true;
  let entries = fs::read_dir(&drash.info_path).expect("failed to read Info directory");
  let mut real_path: Vec<String> = Vec::new();

  for entry in entries {
    let path = entry.expect("failed to get directory entry");
    let content = fs::read_to_string(path.path()).expect("failed to read file into string");
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
    return;
  }

  let files = drash.search_file_path("Select files to remove:");

  for path in files {
    let path = Path::new(&path);
    if let Err(err) = drash.remove_file_from_drash(path) {
      eprintln!("{}: {err}", "Error".red().bold());
      continue;
    }
  }
}

/// Empty the **Drash-Can**
pub fn empty_drash(drash: &Drash, yes: bool) {
  let files = fs::read_dir(&drash.files_path).expect("failed to read Drash directory");
  let file_entries = files.count();
  if file_entries == 0 {
    println!("Drashcan is alraedy empty");
    return;
  }

  if yes {
    fs::remove_dir_all(&drash.files_path).expect("failed to remove Drash directory");
    fs::remove_dir_all(&drash.info_path).expect("failed to remove Info directory");

    fs::create_dir(&drash.files_path).expect("failed to create Drash directory");
    fs::create_dir(&drash.info_path).expect("failed to create Info directory");

    match file_entries {
      n if n > 1 => println!("Removed: {file_entries} files"),
      _ => println!("Removed: {file_entries} file"),
    }
    return;
  }

  let user_input = Confirm::new("Empty the drash directory?")
    .with_default(true)
    .prompt()
    .unwrap();

  if !user_input {
    return;
  }

  fs::remove_dir_all(&drash.files_path).expect("failed to remove Drash directory");
  fs::remove_dir_all(&drash.info_path).expect("failed to remove Info directory");

  fs::create_dir(&drash.files_path).expect("failed to create Drash directory");
  fs::create_dir(&drash.info_path).expect("failed to create Info directory");
}

pub fn restore_files(drash: &Drash, search_file: Option<String>, overwrite: bool) {
  if let Some(search) = search_file {
    if search != "-" {
      let files = drash.search_file_path("Select files restore:");

      for file in files {
        let file_path = Path::new(&file);
        if !drash.restore_file(file_path, overwrite) {
          println!("Skipped {:?}", file_path.file_name().unwrap());
          continue;
        }
      }

      return;
    }

    let last_file = fs::read_dir(&drash.files_path)
      .expect("failed to read Drash directory")
      .flatten()
      .max_by_key(|x| x.metadata().unwrap().modified().unwrap());

    match last_file {
      Some(file) => {
        if !drash.restore_file(file.path(), overwrite) {
          println!("Skipped {}", file.path().display())
        } else {
          println!("Restored {:?}", file.file_name())
        }
      }
      None => eprintln!("Drashcan is empty"),
    };

    return;
  }

  let original_paths = drash.list_file_paths();
  if original_paths.len() == 0 {
    println!("Drashcan is empty");
    return;
  }

  let restore_paths = MultiSelect::new("Select files to restore", original_paths.into_vec())
    .with_validator(min_length!(1, "At least choose one file to restore"))
    .prompt()
    .unwrap();

  for entry in restore_paths.iter() {
    let path = Path::new(&entry.path);

    if !drash.restore_file(path, overwrite) {
      println!("Skipped {}", entry.path);
      continue;
    }
  }

  if restore_paths.len() > 1 {
    println!("Restored {} files", restore_paths.len())
  } else {
    println!("Restored 1 file")
  }
}
