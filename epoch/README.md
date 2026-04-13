# epoch - epoch/date converter

`epoch` converts between Unix timestamps and human-readable date/time strings.

## Usage

```bash
epoch [OPTIONS] [VALUE ...]
```

If no values are passed, it reads lines from stdin.

## Options

- `-e, --to-epoch` parse date/time input and print epoch seconds
- `-d, --from-epoch` parse epoch seconds and print formatted date/time
- `-u, --utc` use UTC (default: local time)
- `-f, --format <F>` custom format (strftime/strptime style)
- `-h, --help` show help

## Examples

```bash
epoch 1713043200
epoch --to-epoch '2026-04-13 12:00:00'
printf '1713043200\n' | epoch --utc
```
