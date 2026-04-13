# b64 - base64 encode/decode

`b64` encodes or decodes base64 from arguments or stdin.

## Usage

```bash
b64 [OPTIONS] [TEXT]
```

## Options

- `-d, --decode` decode input
- `-w, --wrap <N>` wrap encoded output every N chars (`0` = no wrap)
- `-h, --help` show help

## Examples

```bash
b64 'hello world'
echo 'aGVsbG8gd29ybGQ=' | b64 --decode
printf 'binary\0data' | b64
```
