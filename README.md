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

The project is a simple [Unity-Build](https://en.wikipedia.org/wiki/Unity_build) and the `Makefile` is extremely easy to modify/edit.

> [!NOTE]
> The `Makefile` is setup for my personal development workflow, before building the project in relase or debug make sure you take a look at the `Makefile` and understand what it's doing!

Compile the binary
```bsah
cd drash

# 'opt' for release build
make opt
```

Run `drash -help` to see the available commands
