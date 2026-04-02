# pfd - process file descriptors

`pfd` shows open file descriptors for a process in a readable table.

It is similar to `lsof -p <pid>` but focuses on compact, practical output.

## Usage

```bash
pfd [PID]
```

If no PID is provided, `pfd` uses the current process.

## Output columns

- `FD` descriptor number
- `TYPE` file, socket, pipe, anon_inode, or other
- `TARGET` path or decoded socket endpoint information

## Examples

```bash
# inspect your shell process
pfd $$

# inspect PID 1234
pfd 1234
```
