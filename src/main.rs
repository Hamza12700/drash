use clap::Parser;
use std::{
  env, fs,
  io::Write,
  path::{Path, PathBuf},
  process::exit,
};

/// Put files/directories into drash
#[derive(Debug, Parser)]
#[command(version)]
struct Args {
  /// File to drash
  file: Option<PathBuf>,

  /// List files/directories in the drash
  #[arg(short, long)]
  list: bool,

  /// Empty drash
  #[arg(short, long)]
  empty: bool,
}

fn main() -> anyhow::Result<()> {
  let drash_dir = Path::new(&env::var("HOME")?).join(".local/share/Drash");
  let drash_files = Path::new(&drash_dir).join("files");
  let drash_info_dir = Path::new(&drash_dir).join("info");

  if !drash_dir.exists() {
    fs::create_dir(&drash_dir).inspect_err(|err| {
      eprintln!("Failed to create directory at {:?}", drash_dir);
      eprintln!("Error message: {err}");
      exit(1);
    })?;

    fs::create_dir(Path::new(&drash_dir).join("info")).inspect_err(|err| {
      eprintln!("Failed to create directory at {:?}", drash_info_dir);
      eprintln!("Error message: {err}");
      exit(1);
    })?;

    fs::create_dir(&drash_files).inspect_err(|err| {
      eprintln!("Failed to create directory at {:?}", drash_files);
      eprintln!("Error message: {err}");
      exit(1);
    })?;
  }
  let args = Args::parse();
  
  if let Some(file) = args.file.as_ref() {
    let current_file = env::current_dir()?.join(file);
    let mut buffer = fs::OpenOptions::new()
      .write(true)
      .append(true)
      .create(true)
      .open(Path::new(&drash_info_dir).join(format!("{}.drashinfo", file.display())))?;

    buffer.write_all(b"[Drash Info]\n")?;
    let formatted_string = format!("Path={}\n", current_file.display());
    buffer.write_all(formatted_string.as_bytes())?;
    fs::rename(file, Path::new(&drash_files).join(file))?;
  }

  if args.empty {
    !unimplemented!("need to work on this")
  }

  if args.list {
    unimplemented!("also need to work on this");
  }

  Ok(())
}
