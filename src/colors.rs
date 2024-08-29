pub trait Colorize {
  fn red(self) -> String;
  fn bold(self) -> String;
}

impl<T: AsRef<str>> Colorize for T {
  fn red(self) -> String {
    format!("\x1b[31m{}\x1b[0m", self.as_ref())
  }

  fn bold(self) -> String {
    format!("\x1b[1m{}\x1b[0m", self.as_ref())
  }
}
