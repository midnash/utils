# entropy - per-block Shannon entropy chart

entropy computes Shannon entropy for each block of a file and prints a chart to highlight high-entropy regions.

## Usage

```bash
entropy [OPTIONS] FILE
```

## Options

- -b, --block <N>: block size in bytes (default: 4096)
- -w, --width <N>: chart width (default: 40)
- -h, --help: show help

## Example

```bash
entropy sample.bin
entropy -b 1024 -w 60 firmware.bin
```
