# Drash

Drash is a nifty Command-Line Interface application which is a better
alternative to the Linux `rm` command. It records the original path of the
removed file, and puts it into a directory, making it easy to recovery
them later if you accidentally deleted the wrong file.

This same functionality is used by KDE, GNOME, and XFCE [freedesktop-trashcan](https://www.freedesktop.org/wiki/Specifications/trash-spec/).

## Why?

I was inspired by [trash-cli](https://github.com/andreafrancia/trash-cli), but
I wanted to create something with more bells
and whistles. Drash aims to enhance the *trash-cli* experience with additional
features like file searching and advance recovery capabilities.

With Drash, you can easily put files into the 'drashcan' by simply searching
for them in the current working directory. You can also search for and recover
previously deleted files.

## Installation:

Clone the repo into your local machine:
```bash
git clone https://github.com/hamza12700/drash
```

Use [Cargo](https://doc.rust-lang.org/cargo/) to build and install the binary.
```bsah
cd drash
cargo install --path .
```

## Usage

### Drash files

Put file(s) into drashcan:
```
drash foo
```

Pass `--force/-f` flag to delete a file permanently:
```
drash foo -f
```

Pass no arguments and options to remove multiple files in current directory:
```
drash
```

### SubCommands

#### List

List all the removed files:
```
drash list
```

#### Restore

Restore files by searching for them:
```
drash restore
```

Restore the last removed file:
```
drash restore -
```

Restore a file matching the file name:
```
drash restore <FILE_NAME>
```

Pass `--overwrite/-o` flag to overwrite the existing file:
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

If you discover any bugs please report them [here](https://github.com/Hamza12700/drash/issues/).

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
