# Simple Web Server — Phase 1 (Poll Revision)

This program was designed for and developed on FreeBSD. It is the group
project from CS631 Advanced Programming in the Unix Environment, as taught
by Jan Schaumann. The objective is to write a simple web server that speaks
a limited subset of HTTP/1.0 as defined in
[RFC 1945](https://www.rfc-editor.org/rfc/rfc1945.html). For more
information on the assignment specifics, see the
[assignment page](https://stevens.netmeister.org/631/f23-group-project.html).

This implementation uses a single-process `poll()`-based event loop instead
of the original fork-per-connection model, which was benchmarked at
approximately 60,000 req/s on the development machine versus ~20,000 req/s
for the fork model.

> **Important:** This is not a production server. It has limited
> functionality, probable bugs, and security issues. It is made purely for
> educational purposes and is **not** recommended for internet-facing use.

---

## Building

This project requires GNU make. On FreeBSD, install it with
`pkg install gmake` and use `gmake` in place of `make`:

```sh
gmake          # release build  →  ./simple_server
gmake debug    # debug build    →  ./simple_server_debug
gmake clean
gmake test-all
```

Object files live in `build/release/` and `build/debug/` so the two builds
never collide — switching between them does not require a `gmake clean`.

```sh
gmake clean        # remove all binaries and build/ objects
gmake clean-obj    # remove only object files
```

### Dependencies

| Package | Purpose |
|---|---|
| `libmagic` | MIME-type detection fallback |
| `criterion` | Unit testing framework |
| `doxygen` | Documentation generation (optional) |
| `graphviz` | Call/dependency diagrams (optional) |
| `llvm` / `clang-format` | Code formatting (optional) |
| `cppcheck` / `cpplint` | Static analysis (optional) |

Install on FreeBSD:

```sh
pkg install doxygen graphviz llvm cppcheck py39-cpplint criterion
```

---

## Running

```sh
./simple_server [options]
```

Once started the server responds to requests from a browser, cURL, or
telnet. With default settings it listens on port 8080 on all interfaces.

### Flags

| Flag | Argument | Description |
|---|---|---|
| `-c` | `dir` | Allow CGI execution from the given directory. |
| `-v` | — | Verbose mode: do not daemonize, log to stdout. |
| `-bind4` | `address` | Bind the IPv4 listener to a specific address (e.g. `192.168.1.10` or `0.0.0.0`). Default: all IPv4 addresses. |
| `-bind6` | `address` | Bind the IPv6 listener to a specific address (e.g. `::1` or `::`). Default: all IPv6 addresses. |
| `-l` | `file` | Log all requests to the given file. |
| `-p` | `port` | Listen on the given port. Default: 8080. |

`-bind4` and `-bind6` are independent — you can specify one, both, or
neither. If neither is given both listeners bind to the wildcard address.
Addresses are validated with `inet_pton` at startup; an invalid address
exits immediately with an error message.

### Example

```sh
# Verbose mode on all interfaces, port 8000
./simple_server -v -p 8000

# Bind IPv4 to a specific address, IPv6 to loopback only
./simple_server -bind4 192.168.1.10 -bind6 ::1 -p 8080

# Enable CGI, log requests
./simple_server -c ./cgi-bin -l ./server.log
```

---

## Testing

Unit tests use the [Criterion](https://github.com/Snaipe/Criterion) framework.
Each test suite links only the unit under test — never `main.c` — so a
break in one unit never blocks another suite from running.

```sh
gmake test-parse          # parse_request suite (39 tests)
gmake test-parse-asan     # ...under AddressSanitizer
gmake test-resolve        # resolve_path suite
gmake test-resolve-asan   # ...under AddressSanitizer
gmake test-all            # all suites
gmake test-all-asan       # all suites under ASan
```

Output defaults to `-j1 --quiet` (failures only, deterministic order).
Override with `TEST_RUN_FLAGS`:

```sh
gmake test-parse TEST_RUN_FLAGS='-j1 --verbose'
```

---

## Code Quality

```sh
gmake check          # format check + static analysis
gmake format         # auto-format with clang-format
gmake format-check   # check formatting without modifying files
gmake lint           # cppcheck + cpplint
gmake fix            # apply formatting
```

---

## Documentation

```sh
gmake docs           # generate Doxygen docs with call graphs (requires Graphviz)
gmake docs-no-graphs # generate without diagrams (faster)
gmake docs-init      # create initial Doxyfile
ggmake docs-clean     # remove generated docs
```

Generated output is at `docs/html/index.html`.

---

## Stopping the Server

The server daemonizes by default (suppressed by `-v`). To stop it:

```sh
sockstat | grep 8080
# jholloway simple_server 31863 3  tcp4  *:8080  *:*
kill 31863
```

---

## Memory Management

Memory safety is validated with both Valgrind and AddressSanitizer as part
of normal development. Per-connection allocations (`in_buf`, `out_buf`,
`file_ptr`) are freed in `close_conn` on every connection close. Valgrind
confirms zero definitely-lost blocks across single and multi-request runs.

---

## Use of AI Tools (Claude)

This project uses Claude as a knowledgeable reviewer and tutor, in a
specific and limited capacity:

**Claude IS used for:**
- Makefile improvements and build system structure
- Code review: identifying bugs, security issues, performance problems
- Documentation assistance and structuring
- Explaining systems programming concepts and tradeoffs
- Interpreting profiling and benchmark data (DTrace, Valgrind, `hey`)
- Criterion test suite design (tests are written by Claude, implementation
  is written independently by the developer)
- Formatting guidance (`.clang-format`, BSD style)

**Claude is NOT used for:**
- Writing implementation code — all `.c` source is human-written
- Making design decisions — those are made independently
- Shortcuts — problem-solving and implementation remain the developer's work

The goal is to use AI as a senior engineer or TA would be used: review,
explanation, and a second pair of eyes. All implementation decisions and
code remain the developer's own, for better and worse.

For more on this approach, see:
[Benchmarking and Rebuilding an Old Web Server — Part 1](https://jholloway.dev/posts/benchmarking-and-rebuilding-an-old-web-server---part-1/)
