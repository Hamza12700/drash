use colored::Colorize;
use std::{
  fs,
  io::{self, Write},
  path::PathBuf,
};

pub fn check_path(path: &PathBuf, drash_files: &PathBuf, drash_info_dir: &PathBuf) -> bool {
  if path.exists() {
    println!("\n{}: {:?}", "File already exists".bold(), path.file_name().unwrap());
    print!("Do you want overwrite it? {}: ", "(Y/n)".bold());
    io::stdout().flush().unwrap();

    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    let input = input.trim();
    if input == "n" {
      return true;
    }

    let file_name = path.file_name().unwrap().to_str().unwrap();
    fs::rename(&drash_files.join(file_name), path).unwrap();
    fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name))).unwrap();
  }

  return false;
}
