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

There's only two dependencies: `g++` and `make`:
```bsah
cd drash

# 'opt' for release build
make opt
```

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
