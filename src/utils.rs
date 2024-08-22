use anyhow::Result;
use colored::Colorize;
use inquire::Confirm;
use skim::{
  prelude::{SkimItemReader, SkimOptionsBuilder},
  Skim, SkimOutput,
};
use std::{fs, io::Cursor, path::Path, rc::Rc};

pub fn check_overwrite<P: AsRef<Path>>(path: P, overwrite: bool) -> bool {
  let path = path.as_ref();
  if overwrite {
    return true;
  } else {
    if path.exists() {
      let file_name = path.file_name().unwrap().to_str().unwrap();
      let ans = Confirm::new(format!("File already exists: {file_name}").as_str())
        .with_default(false)
        .with_help_message("Do you want to overwrite it?")
        .prompt()
        .unwrap();

      return ans;
    }
    return true;
  }
}

/// Fuzzy find files in Drashed `Files/Info` files or in the specified path.
/// If fuzzy find is abort then the process exit with 0 code.
pub fn fuzzy_find_files<P: AsRef<Path>>(path: P, search_query: Option<&str>) -> Result<SkimOutput> {
  let mut files = String::new();
  let options = SkimOptionsBuilder::default()
    .height(Some("50%"))
    .multi(true)
    .query(search_query)
    .build()?;

  let path = path.as_ref();
  let path_str = path.to_str().unwrap();
  if path_str.contains("info") {
    let entries = fs::read_dir(path)?;

    for entry in entries {
      let entry = entry?;
      let path = entry.path();

      let file_info: Rc<_> = fs::read_to_string(&path)?
        .lines()
        .map(|line| line.to_string())
        .collect();

      let file_path = match file_info.get(1) {
        Some(f_path) => f_path.trim_start_matches("Path=").to_string(),
        None => {
          eprintln!(
            "{}",
            "Failed to get file path value by index: 1".red().bold()
          );
          continue;
        }
      };

      files.push_str(&file_path);
      files.push('\n');
    }
  } else {
    let entries = fs::read_dir(&path)?;
    for entry in entries {
      let entry = entry?;
      let path = entry.path();
      if let Some(file_name) = path.file_name().and_then(|name| name.to_str()) {
        files.push_str(file_name);
        files.push('\n');
      }
    }
  }

  let skim_item_reader = SkimItemReader::default();
  let items = skim_item_reader.of_bufread(Cursor::new(files));
  let selected_files = Skim::run_with(&options, Some(items)).unwrap();

  if selected_files.is_abort {
    println!("\n{}", "No files selected".bold());
    std::process::exit(0);
  }

  return Ok(selected_files);
}
