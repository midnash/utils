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
- `dirsize` - sorted directory sizes with inline bar chart
- `caps` - readable Linux capability inspector for process/file
- `fstype` - print filesystem type, mount options, and device
- `inode` - inspect inode metadata and Linux inode flags
- `todo` - scan codebase for TODO/FIXME/HACK/XXX comments
- `inc` - resolve C/C++ #include dependencies without a compiler
- `palette` - print terminal 256/truecolor palettes and escape code refs
- `nstr` - classified strings extractor for binaries
- `entropy` - per-block Shannon entropy chart for files
- `bindiff` - byte-level binary diff with offset context
- `hashfile` - compute md5/sha1/sha256/sha512/blake2 in one pass
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
- `dirsize/` source and README for sorted size reports
- `caps/` source and README for Linux capability inspection
- `fstype/` source and README for filesystem metadata lookup
- `inode/` source and README for inode inspection
- `todo/` source and README for TODO-style comment scanning
- `inc/` source and README for include dependency analysis
- `palette/` source and README for terminal color palette output
- `nstr/` source and README for classified string extraction
- `entropy/` source and README for entropy analysis
- `bindiff/` source and README for binary comparison
- `hashfile/` source and README for multi-hash fingerprinting
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
- `bin/dirsize`
- `bin/caps`
- `bin/fstype`
- `bin/inode`
- `bin/todo`
- `bin/inc`
- `bin/palette`
- `bin/nstr`
- `bin/entropy`
- `bin/bindiff`
- `bin/hashfile`
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

# sorted directory sizes
./bin/dirsize -n 20 ~

# process/file capabilities
./bin/caps $$

# filesystem type and mount options
./bin/fstype /var/log

# inode metadata and flags
./bin/inode /etc/passwd

# scan TODO/FIXME/HACK/XXX comments
./bin/todo .

# include dependency tree
./bin/inc -r -I include src/main.cpp

# terminal palette preview
./bin/palette --codes

# classified strings from a binary
./bin/nstr /bin/ls

# entropy chart by block
./bin/entropy /bin/ls

# byte-level binary diff
./bin/bindiff old.bin new.bin

# compute all major hashes
./bin/hashfile /bin/ls
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
- `dirsize/README.md`
- `caps/README.md`
- `fstype/README.md`
- `inode/README.md`
- `todo/README.md`
- `inc/README.md`
- `palette/README.md`
- `nstr/README.md`
- `entropy/README.md`
- `bindiff/README.md`
- `hashfile/README.md`
- `thumbgen/README.md`
