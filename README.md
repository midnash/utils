# utils - small command-line utilities

This repository contains a set of focused Linux CLI tools:

- `ff` - fast file finder with sane defaults
- `rn` - regex-based bulk rename tool
- `lip` - clean local IP address printer
- `pfd` - readable process file descriptor viewer
- `git-summary` - repository stats (commits, contributors, dates, hot files)
- `git-size` - largest git objects, sorted
- `hex` - readable hex dump with ASCII and color hints
- `magic` - identify file type by magic bytes
- `sym` - readable ELF symbol table viewer with demangling
- `deps` - shared library dependency tree renderer
- `elf` - human-readable ELF header/segment/section inspector
- `rpath` - print ELF RPATH/RUNPATH entries
- `socheck` - report unresolved shared library dependencies
- `epoch` - convert Unix timestamps and date/time strings
- `port` - show listening sockets and owning processes
- `jwt` - decode and pretty-print JWT header/payload
- `b64` - base64 encode/decode from args or stdin
- `thumbgen` - extract a video thumbnail via FFmpeg C API (optional build target)

## Project layout

- `ff/` source and README for file finder
- `rn/` source and README for bulk rename
- `lip/` source and README for local IP printing
- `pfd/` source and README for process file descriptors
- `git-summary/` source and README for repository stats
- `git-size/` source and README for git object size inspection
- `hex/` source and README for readable hex dumps
- `magic/` source and README for magic-byte type detection
- `sym/` source and README for ELF symbol inspection
- `deps/` source and README for shared library dependency trees
- `elf/` source and README for ELF structure inspection
- `rpath/` source and README for RPATH/RUNPATH printing
- `socheck/` source and README for unresolved dependency checks
- `epoch/` source and README for timestamp/date conversion
- `port/` source and README for socket-to-process lookup
- `jwt/` source and README for JWT inspection
- `b64/` source and README for base64 encode/decode
- `thumbgen/` source and README for frame extraction

## Build

Requirements:

- CMake 3.16+
- C++17 compiler (g++ or clang++)

```bash
cmake -S . -DCMAKE_BUILD_TYPE=Release
make
```

Produced binaries:

- `bin/ff`
- `bin/rn`
- `bin/lip`
- `bin/pfd`
- `bin/git-summary`
- `bin/git-size`
- `bin/hex`
- `bin/magic`
- `bin/sym`
- `bin/deps`
- `bin/elf`
- `bin/rpath`
- `bin/socheck`
- `bin/epoch`
- `bin/port`
- `bin/jwt`
- `bin/b64`
- `bin/thumbgen` (only when FFmpeg dev libraries are installed)

## Quick examples

```bash
# file finder
./bin/ff -e cpp

# bulk rename
./bin/rn --dry-run 's/\.jpeg$/.jpg/' *.jpeg

# list local IPs
./bin/lip

# show current shell open descriptors
./bin/pfd $$

# quick repository summary
./bin/git-summary .

# top 20 largest git objects
./bin/git-size .

# readable hex dump
./bin/hex CMakeLists.txt

# identify file type from signature
./bin/magic /bin/ls README.md

# inspect ELF symbol tables
./bin/sym --imports /bin/ls

# show dependency tree
./bin/deps --recurse /bin/ls

# inspect ELF metadata
./bin/elf /bin/ls

# print rpath/runpath entries
./bin/rpath /bin/ls

# check unresolved shared libs
./bin/socheck /usr/bin

# timestamp/date conversion
./bin/epoch 1713043200

# process listening on a port
./bin/port 22

# inspect JWT payload
./bin/jwt eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0In0.sig

# base64 encode/decode
./bin/b64 'hello world'
```

## Tool docs

Each tool has dedicated documentation:

- `ff/README.md`
- `rn/README.md`
- `lip/README.md`
- `pfd/README.md`
- `git-summary/README.md`
- `git-size/README.md`
- `hex/README.md`
- `magic/README.md`
- `sym/README.md`
- `deps/README.md`
- `elf/README.md`
- `rpath/README.md`
- `socheck/README.md`
- `epoch/README.md`
- `port/README.md`
- `jwt/README.md`
- `b64/README.md`
- `thumbgen/README.md`
