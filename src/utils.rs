use inquire::Confirm;
use std::path::Path;

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
