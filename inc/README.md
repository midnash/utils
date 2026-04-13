# inc - C/C++ include dependency finder

`inc` parses `#include` directives and resolves headers without invoking a compiler.

## Usage

```bash
inc [OPTIONS] FILE
```

## Options

- `-I <DIR>` add include search path
- `-r, --recurse` recurse include graph
- `-h, --help` show help

## Examples

```bash
inc main.cpp
inc -r -I include src/main.cpp
```
