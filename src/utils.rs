use inquire::Confirm;
use std::{
  fs,
  path::PathBuf,
};

pub fn check_path(path: &PathBuf, drash_files: &PathBuf, drash_info_dir: &PathBuf) -> bool {
  if path.exists() {
    let file_name = path.file_name().unwrap().to_str().unwrap();
    let ans = Confirm::new(format!("File already exists: {file_name}").as_str())
      .with_default(false)
      .with_help_message("Do you want to overwrite it?")
      .prompt()
      .unwrap();

    if !ans {
      return true;
    }

    let file_name = path.file_name().unwrap().to_str().unwrap();
    fs::rename(&drash_files.join(file_name), path).unwrap();
    fs::remove_file(&drash_info_dir.join(format!("{}.drashinfo", file_name))).unwrap();
  }

  return false;
}
