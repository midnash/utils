# bindiff - byte-level binary diff

bindiff compares two binary files and reports byte-difference spans with offset context.

## Usage

```bash
bindiff [OPTIONS] FILE_A FILE_B
```

## Options

- -c, --context <N>: context bytes around each span (default: 8)
- -n, --max <N>: max spans to print (default: 100)
- -h, --help: show help

## Example

```bash
bindiff old.bin new.bin
bindiff -c 16 -n 20 a.out b.out
```
