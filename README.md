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
- `thumbgen/README.md`
