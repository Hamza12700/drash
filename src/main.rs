use clap::Parser;
use std::{
  env, fs,
  path::{Path, PathBuf},
  process::exit,
};

/// Put files/directories into drash
#[derive(Debug, Parser)]
#[command(version)]
struct Args {
  file: PathBuf,
}

fn main() -> anyhow::Result<()> {
  let drash_dir = Path::new(&env::var("HOME")?).join(".local/share/Drash");
  if !drash_dir.exists() {
    fs::create_dir(&drash_dir).inspect_err(|err| {
      eprintln!("Failed to create directory at {:?}", drash_dir);
      eprintln!("Error message: {err}");
      exit(1);
    })?;
  }
  let args = Args::parse();
  fs::rename(&args.file, Path::new(&drash_dir).join(&args.file))?;

  Ok(())
}
