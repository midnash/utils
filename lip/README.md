# lip - local IP printer

`lip` prints local interface IP addresses in a clean, table-style format.

## Usage

```bash
lip [--all]
```

By default, `lip` shows addresses from interfaces that are up and not loopback.

## Options

- `-a, --all` include loopback and down interfaces
- `-h, --help` show help

## Example

```bash
lip
# INTERFACE   FAMILY  ADDRESS
# eno1        ipv4    192.168.1.42
# eno1        ipv6    fe80::1234:5678:abcd:ef01
```
