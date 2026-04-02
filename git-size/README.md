# git-size - largest git objects

`git-size` shows git object sizes sorted largest-first, useful for finding accidentally committed large files.

## Usage

```bash
git-size [OPTIONS] [REPO_PATH]
```

## Options

- `-n, --top <N>` number of rows to print (default: 20)
- `-h, --help` show help

## Example

```bash
git-size --top 30 .
```
