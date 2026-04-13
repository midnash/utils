# socheck - unresolved .so dependency checker

`socheck` scans one ELF file or a directory tree and reports any libraries that
resolve to `not found` under current linker settings.

## Usage

```bash
socheck TARGET
```

## Example

```bash
socheck ./dist
socheck ./bin/myapp
```
