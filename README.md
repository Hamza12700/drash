# Drash
Put files/directories to drash.

Drash is a CLI app that trashes files and directories and stores the path of that file where it was removed from.

Command line arguments:

```
Commands:
  list     List files in the drashcan
  empty    Empty drashcan
  restore  Restore drashed files
  help     Print this message or the help of the given subcommand(s)

Arguments:
  [FILES]...  Files to drash

Options:
  -h, --help     Print help
  -V, --version  Print version
```

## Usage
Drash a file or directory:

```
drash foo
```

List drashed files:

```
drsah list
┌────┬───────────┬─────────────────────────────────────┐
│ id │ file_type │ path                                │
├────┼───────────┼─────────────────────────────────────┤
│ 0  │ file      │ /home/hamza/personal/drash/main/baz │
│ 1  │ file      │ /home/hamza/personal/drash/main/for │
│ 2  │ file      │ /home/hamza/personal/drash/main/bar │
└────┴───────────┴─────────────────────────────────────┘
```

Restore a single drashed file:

```
drash restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 2
```

Restore multiple drashed files separated by `,`, also support range:

```
drash restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 0-1, 2
```

Restore multiple drashed files overwriting the existing files:

```
drash restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 0-1, 2

File already exists: "check"
Do you want overwrite it? (Y/n): n
```

Or pass `--overwrite` or `-o` flag to not prompt you:

```
drash restore --overwrite
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 0-2
```

Remove all files from the drash directory:

```
drash empty
Would empty the following drash directories:
  - /home/hamza/.local/share/Drash
  - Entries 3
Proceed? (Y/n): y
```

Use `--yes` or `-y` flag to remove all files in the drashcan directory without asking for confirmation:

```
drash empty -y
Removed: 3 files
```

# Why?
In Linux, deleting file or a directory using `rm -r` is irreversible. Now, I
don't know about you but I make a lot of typos when I'm typing (skill issue, I
know). **Drash** cli app written in RUST (BTW) solves this issue, by storing
files in user's `.local/share` directory and keeping track of the original path
where the file was removed.
