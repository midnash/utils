# ff - fast file finder

`ff` is a fast, friendly file finder with sane defaults and `.gitignore` support.

## Usage

```bash
ff [OPTIONS] [PATTERN] [PATH]
```

## Options

- `-r, --regex` treat `PATTERN` as a regular expression
- `-i, --ignore-case` case-insensitive matching
- `-t, --type <TYPE>` filter by type: `f` file, `d` directory, `l` symlink, `x` executable
- `-e, --extension <EXT>` filter by extension (for example `cpp`, `rs`, `py`)
- `-H, --hidden` include hidden files and directories
- `-d, --max-depth <N>` limit search depth (`0` = immediate children)
- `-a, --absolute` print absolute paths
- `-c, --count` print total match count instead of paths
- `-0, --null` separate results with NUL byte (for `xargs -0`)
- `--no-ignore` ignore `.gitignore` rules
- `-h, --help` show help

## Examples

```bash
# list all non-hidden files recursively
ff

# files whose name contains "main"
ff main

# all .cpp files
ff -e cpp

# directories containing "src"
ff -t d src

# regex pattern
ff -r '\.test\.'

# one level deep, including hidden
ff -H -d 1
```
