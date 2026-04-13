# sym - readable symbol table viewer

`sym` prints ELF symbols from `.symtab` and `.dynsym` with optional C++ demangling,
plus import/export filtering.

## Usage

```bash
sym [OPTIONS] FILE
```

## Options

- `-D, --dyn` show only `.dynsym`
- `--imports` show only undefined/import symbols
- `--exports` show only defined/export symbols
- `--no-demangle` disable C++ demangling
- `-h, --help` show help

## Examples

```bash
sym /bin/ls
sym --imports /usr/bin/curl
sym -D --exports ./libfoo.so
```
