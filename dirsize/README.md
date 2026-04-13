# dirsize - sorted directory size report

`dirsize` shows immediate child sizes for a directory, sorted descending, with an inline bar chart.

## Usage

```bash
dirsize [OPTIONS] [PATH]
```

## Options

- `-a, --all` include hidden entries
- `-n, --top <N>` show top N rows
- `-h, --help` show help

## Examples

```bash
dirsize
dirsize -n 20 ~
```
