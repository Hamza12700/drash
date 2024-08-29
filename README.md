# Drash

Drash is a Command Line Interface (CLI) application that serves as a better alternative to the Linux `rm` command. It records the original path of the file which was removed. It puts the deleted file into a temp directory (`~/.local/share/Drash`), making it easy to recovery them later if you accidentally deleted the wrong file.

This same functionality is used by KDE, GNOME, and XFCE [freedesktop-trashcan](https://www.freedesktop.org/wiki/Specifications/trash-spec/).

## Why?

I wanted the experience using
[trash-cli](https://github.com/andreafrancia/trash-cli), but with more
features. Like being able to search files in the drashcan and recovering them.
Put files into the drashcan by searching for them in the current working
directory.

## Installation:

Clone the repo into your local machine:
```bash
git clone https://github.com/hamza12700/drash
```

Use [Cargo](https://doc.rust-lang.org/cargo/) to  build and install the binary into your `$PATH`.
```bsah
cd drash
cargo install --path .
```

## Usage

### Drash files

Put a file intot the drashcan:
```
drash foo
```

Pass `--force/-f` flag to delete a file without storing it in the drashcan:
```
drash foo -f
```

Pass no arguments and options to put files into drashcan by search for them:
```
drash
```

### SubCommands

#### List

List files in the drashcan:
```
drash list
```

#### Restore

Restore a file from drashcan using indices, also supports range and comma separated values:
```
drash restore
```

Restore the last drashed file:
```
drash restore -
```

Restore a file using fuzzy searching:
```
drash restore <FILE_NAME>
```

Pass `--overwrite/-o` flag to overwrite the existing file that same path:
```
drash restore <FILE_NAME> --overwrite
```

#### Empty

Empty the drashcan:
```
drash empty
```

Pass `--yes/-y` pass to not show the confirm prompt:
```
drash empty -y
```

## Bugs

If you discover a bug please report them [here](https://github.com/Hamza12700/drash/issues/).

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
