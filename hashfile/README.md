# hashfile - multi-hash file fingerprint

hashfile computes md5, sha1, sha256, sha512, and blake2 hashes for one file.

## Usage

```bash
hashfile [OPTIONS] FILE
```

## Options

- --check <HASH>: verify against any computed hash
- --check <ALGO:HASH>: verify one specific algorithm
- -h, --help: show help

## Examples

```bash
hashfile archive.tar
hashfile --check sha256:abcdef... archive.tar
```
