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
# F means file and D means directory

drsah list
0:F - /home/hamza/proptotyping/drash/check
1:D - /home/hamza/proptotyping/drash/foo
2:F - /home/hamza/proptotyping/drash/junk
```

Restore a single drashed file:

```
drash restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 2
```

Restore multiple drashed files separated by ",", also support range:

```
drash restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 0-1, 2
```

Remove all files from the drash directory:

```
drash empty
Would empty the following drash directories:
  - /home/hamza/.local/share/Drash
  - Entries 3
Proceed? (Y/n): y
```
