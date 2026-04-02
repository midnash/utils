# hex - readable hex dump

`hex` prints a readable hex dump with grouped bytes, ASCII view, and colorized byte classes.

## Usage

```bash
hex [OPTIONS] FILE
```

## Options

- `-w, --width <N>` bytes per row (default: 16)
- `-h, --help` show help

## Color meaning

- null bytes (`00`)
- printable bytes
- high bytes (`>= 0x80`)

## Example

```bash
hex -w 16 sample.bin
```
