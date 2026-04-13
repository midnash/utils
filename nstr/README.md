# nstr - classified binary strings extractor

nstr extracts printable strings from a file and classifies each one (path, url, ip, version, error, uuid, text).

## Usage

```bash
nstr [OPTIONS] FILE
```

## Options

- -m, --min <N>: minimum string length (default: 4)
- -h, --help: show help

## Example

```bash
nstr /bin/ls
nstr --min 6 sample.bin
```
