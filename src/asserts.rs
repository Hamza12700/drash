use std::{fmt, process::exit};

use crate::colors::Colorize;

pub trait ErrContext<T, E> {
  fn with_context<F: FnOnce(&E)>(self, callback: F) -> T;
  fn context<F: FnOnce(&E)>(self, callback: F) -> Option<T>;
}

impl<T, E, R> ErrContext<T, E> for R
where
  R: Into<Result<T, E>>,
  E: fmt::Display,
{
  fn with_context<F: FnOnce(&E)>(self, callback: F) -> T {
    match self.into() {
      Ok(val) => val,
      Err(err) => {
        callback(&err);
        eprintln!("{}: {err}", "Error".bold().red());
        exit(1)
      }
    }
  }

  fn context<F: FnOnce(&E)>(self, callback: F) -> Option<T> {
    todo!()
  }
}
