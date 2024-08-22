# Drash

Drash is a Command Line Interface (CLI) application that serves as a better alternative to the Linux `rm` command. It records the original path of the file which was removed. It puts the deleted file into a temp directory (`~/.local/share/Drash`), making it easy to recovery them later if you accidentally deleted the wrong file.

This same functionality is used by KDE, GNOME, and XFCE [freedesktop-trashcan](https://www.freedesktop.org/wiki/Specifications/trash-spec/).

## Installation:

Clone the repo into your local machine:
```bash
git clone https://github.com/hamza12700/drash
```

Use [cargo](https://doc.rust-lang.org/cargo/) to  build and install the binary into your `$PATH`.
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

Delete a file without storing it in the drashcan:
```
# Pass --force or -f
drash foo -f
```

Pass no arguments and options to put files into drashcan in fuzzy find mode:
```
# Will open fuzzy search menu
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

Restore a file by overwriting the existing file:
```
# Or use any other method shown above
drash restore <FILE_NAME> --overwrite
```

#### Empty

Empty the drashcan:
```
drash empty
```

Pass `--yes or -y` pass to not show the confirm prompt:
```
drash empty -y
```

## Bugs

If you discover a bug please report them [here](https://github.com/Hamza12700/drash/issues/).

> [!NOTE]
> Please note that this application is still in early stage. If you encounter any bugs the chances are I've are already fixed them in the `main` branch because I'm actively working on this. I would recommend trying out this CLI application by cloning it from the `main` branch and building it from source.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

All pull requests should be submitted to the "main" branch.
