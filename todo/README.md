# todo - TODO/FIXME scanner

`todo` scans a codebase for TODO/FIXME/HACK/XXX comments and prints a sorted report.

## Usage

```bash
todo [OPTIONS] [PATH]
```

## Options

- `-t, --tag <TAG>` filter by one tag
- `-h, --help` show help

## Examples

```bash
todo
todo -t FIXME src
```
