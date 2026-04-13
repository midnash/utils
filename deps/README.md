# deps - shared library dependency tree

`deps` renders `ldd` output as a readable tree and can recurse into transitive dependencies.

## Usage

```bash
deps [OPTIONS] FILE
```

## Options

- `-r, --recurse` recurse into transitive dependencies
- `-h, --help` show help

## Examples

```bash
deps /bin/ls
deps --recurse ./build/bin/mytool
```
