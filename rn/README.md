# rn - regex bulk rename

`rn` renames files using a single regex substitution expression.

## Usage

```bash
rn [OPTIONS] 's/pattern/replacement/[flags]' FILE...
```

## Options

- `-n, --dry-run` show planned changes without renaming
- `-f, --force` overwrite destination if it already exists
- `-h, --help` show help

## Flags

- `g` replace all matches in each filename (default: first match only)
- `i` case-insensitive matching

## Examples

```bash
# rename .jpeg to .jpg
rn 's/\.jpeg$/.jpg/' *.jpeg

# preview first
rn --dry-run 's/_old/_new/g' *.txt
```
