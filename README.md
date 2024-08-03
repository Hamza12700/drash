# Drash
Put files/directories to drash.

Drash is a CLI app that trashes files and directories and stores the path of that file where it was removed from.

Command line arguments:

```
Arguments:
  [FILES]...  File to drash

Options:
  -l, --list     List files/directories in the drash
  -e, --empty    Empty drash
  -r, --restore  Restore from drash chosen files
```

## Usage
Drash a file or directory:

```
drash foo
```

List drashed files:

```
# F is file and D is directory

drsah --list
0:F - /home/hamza/proptotyping/drash/check
1:D - /home/hamza/proptotyping/drash/foo
2:F - /home/hamza/proptotyping/drash/junk
```

Restore a drashed file:

```
drash --restore
  0 /home/hamza/proptotyping/drash/check
  1 /home/hamza/proptotyping/drash/foo
  2 /home/hamza/proptotyping/drash/junk
What file to restore [0..2]: 2
```

Remove all files from the drash directory:

```
drash --empty
Would empty the following drash directories:
  - /home/hamza/.local/share/Drash
Proceed? (Y/n): y
```
