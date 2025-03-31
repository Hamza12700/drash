# Drash

A simple Command-Line Interface app, a better alternative to the Linux `rm`
command. It keeps track of the original file-path of the removed file, and puts
it into a directory, making it easy to recovery them later if you accidentally
deleted the wrong file.

This same functionality is used by KDE, GNOME, and XFCE [freedesktop-trashcan](https://www.freedesktop.org/wiki/Specifications/trash-spec/).

## Installation:

Clone the repo into your local machine:
```bash
git clone https://github.com/hamza12700/drash
```

Compile the binary
```bsah
cd drash

# 'opt' for release build
make opt
```

This binary gets put into the `build` directory.

## Usage

### Drash files

Put file(s) into drashcan:
```
drash foo
```

Pass `--force/-f` flag to delete a file permanently:
```
drash -f foo
```

### SubCommands

#### List

List all the removed files:
```
drash list
```

#### Restore

Restore the last removed file:
```
drash restore -
```

Restore a file matching the file name:
```
drash restore <FILE_NAME>
```

#### Empty

Empty the drashcan:
```
drash empty
```
