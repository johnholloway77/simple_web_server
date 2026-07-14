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
approximately 60,000 req/s on the development machine versus less than 1,000 req/s 
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
gmake valgrind-build  # symbols-only build (no ASan) →  ./simple_server_valgrind
gmake clean
gmake test-all
```

Object files live in `build/release/`, `build/debug/`, and `build/valgrind/`
so the builds never collide — switching between them does not require a
`gmake clean`.

```sh
gmake clean        # remove all binaries and build/ objects
gmake clean-obj    # remove only object files (keep binaries)
```

### Dependencies

| Package | Purpose |
|---|---|
| `libmagic` | MIME-type detection fallback (optional, on by default) |
| `criterion` | Unit testing framework |
| `doxygen` | Documentation generation (optional) |
| `graphviz` | Call/dependency diagrams for docs (optional) |
| `llvm` / `clang-format` | Code formatting (optional) |
| `cppcheck` / `cpplint` | Static analysis (optional) |

Install on FreeBSD:

```sh
pkg install doxygen graphviz llvm cppcheck py39-cpplint criterion
```

`libmagic` is part of the FreeBSD base system. Pass `USE_LIBMAGIC=0` to
disable it (OmniOS/illumos users may need to do this):

```sh
gmake USE_LIBMAGIC=0
```

---

## Source Layout

```
server_revision/
├── main.c                          # Entry point: signal setup, poll() event loop
│
├── flags/
│   ├── flags.h                     # Flag bit definitions (C_FLAG, V_FLAG, etc.)
│   └── setFlags.c                  # Command-line argument parsing
│
├── sockets/
│   ├── socket.h                    # get_listener_v4/v6 declarations
│   ├── get_listener_v4.c           # Non-blocking IPv4 listening socket
│   └── get_listener_v6.c           # Non-blocking IPv6 listening socket (IPV6_V6ONLY)
│
├── client_conn/
│   ├── connections.h               # Client struct, state/response enums, function decls
│   ├── connections.c               # add_to_lists(), close_conn()
│   ├── accept_new_conn.c           # accept_new_conn(): drain listener, set O_NONBLOCK
│   ├── do_read.h                   # do_read() declaration
│   ├── do_read.c                   # recv() loop, header detection, parse→resolve→build
│   ├── do_write.h                  # do_write() declaration
│   └── do_write.c                  # send() loop, graceful shutdown(SHUT_WR)
│
├── requests/
│   ├── parse_request.h             # Request struct, http_method/version enums, parse_request() decl
│   ├── parse_request.c             # HTTP request-line parser and validator
│   ├── resolve_path.h              # ResolvedPath struct, resolve_path() decl
│   ├── resolve_path.c              # URI→filesystem mapping, MIME detection, CGI routing
│   └── close_resolve_path.c        # close_resolve_path_ptr(): fclose wrapper
│
├── response/
│   ├── build_response.h            # build_okay_response() and build_error_response() decls
│   ├── build_okay_response.c       # 200 OK: static files, directory listings, CGI execution
│   ├── build_error_response.c      # 4xx/5xx error responses
│   └── version_info.h              # SERVER_VERSION macro
│
├── debug/
│   └── debug.h                     # DBG/DBG_DEC/DBG_DO macros (no-op in release)
│
└── test_cases/
    ├── test_parse_request.c        # Criterion suite: parse_request() (~40 cases)
    ├── test_resolve_path.c         # Criterion suite: resolve_path()
    ├── test_build_error.c          # Criterion suite: build_error_response()
    └── test_build_okay_response.c  # Criterion suite + integration: build_okay_response()
```

Build artifacts go into `build/release/`, `build/debug/`, and
`build/valgrind/` (not committed). The output binaries are placed in the
project root: `simple_server`, `simple_server_debug`,
`simple_server_valgrind`.

---

## Architecture

The server is a single-process, single-thread `poll()` event loop. There
are no worker threads and no forked child processes for request handling
(CGI still forks, as required by the CGI spec).

**Connection lifecycle:**

```
READING → (PROCESSING) → SENDING_HEADER / SENDING_BODY → CLOSING
```

All states map directly to the `client_state` enum in `connections.h`.
`PROCESSING` is transient — it occurs inside `do_read()` while the response
is being built and is not visible to the poll loop.

**Per-connection state (`Client` struct in `connections.h`):**

| Field | Purpose |
|---|---|
| `in_buf` / `input_length` / `input_capacity` | Heap buffer accumulating the inbound request |
| `out_buf` / `output_length` / `output_sent` | Heap buffer holding the complete outbound response |
| `file_ptr` / `file_size` / `body_sent` | Open file being streamed as response body |
| `fd` | Socket file descriptor |
| `state` | Current lifecycle state |
| `resp_val` | Response code determined during parsing |
| `client_addr` | Peer address string (IPv4 or IPv6) |

The `pfds[]` and `clients[]` arrays are kept parallel and grow together via
`realloc()` (geometric doubling). Slot removal is done by swapping the
removed entry with the last active entry so the arrays stay dense.

**Request handling pipeline (all inside `do_read()`):**

1. `recv()` loop drains the non-blocking socket into `in_buf`
2. Scan for `\r\n\r\n` to detect end of headers
3. `parse_request()` — validates method, version, path; detects traversal
4. `resolve_path()` — maps URI to filesystem; detects MIME type
5. `build_okay_response()` or `build_error_response()` — writes full
   response into `out_buf`
6. Transition poll event mask to `POLLOUT`, client state to `SENDING_HEADER`

`do_write()` then drains `out_buf` to the socket via a `send()` loop and
calls `shutdown(SHUT_WR)` on completion.

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
Each test suite links only the units under test — never `main.c` — so a
break in one unit never blocks another suite from running.

```sh
gmake test-parse               # parse_request suite (~40 tests)
gmake test-parse-asan          # ...under AddressSanitizer
gmake test-resolve             # resolve_path suite
gmake test-resolve-asan        # ...under AddressSanitizer
gmake test-build-error         # build_error_response suite
gmake test-build-error-asan    # ...under AddressSanitizer
gmake test-build-okay          # build_okay_response unit + integration tests
gmake test-build-okay-asan     # ...under AddressSanitizer
gmake test                     # all unit suites (no memcheck)
gmake test-all                 # all unit suites + memcheck (Valgrind)
gmake test-all-asan            # all suites under ASan
```

The `test-build-okay` target has two layers:

- **Unit layer** — calls `resolve_path()` → `build_okay_response()` directly,
  in-process, no sockets. Catches struct-ownership bugs between the two
  functions.
- **Integration layer** — forks and execs a real server binary, fires actual
  HTTP requests via curl over a real socket, sends `SIGTERM`, and requires a
  clean graceful exit. The binary used is controlled by `SIMPLE_SERVER_BIN`
  (defaults to `./simple_server`; `test-build-okay-asan` points it at
  `./simple_server_debug`).

Output defaults to `-j1 --verbose`. Override with `TEST_RUN_FLAGS`:

```sh
gmake test-parse TEST_RUN_FLAGS='-j1 --quiet'
```

### Memory leak testing

```sh
gmake valgrind-build   # build symbols-only binary (no ASan — they conflict)
gmake memcheck         # fire 500 mixed requests, assert zero leaks on shutdown
gmake memcheck VALGRIND_REQUESTS=2000   # override request count
```

`memcheck` starts the server under Valgrind, fires a mix of file, directory,
and 404 requests, sends `SIGTERM`, then asserts that Valgrind reports zero
definitely-lost, indirectly-lost, and error-summary counts.

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
gmake docs-clean     # remove generated docs
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

Or send `SIGTERM` / `SIGINT` — both are handled gracefully (the event loop
exits cleanly and frees all allocations).

---

## Memory Management

Memory safety is validated with both Valgrind and AddressSanitizer as part
of normal development. Per-connection allocations (`in_buf`, `out_buf`,
`file_ptr`) are freed in `close_conn()` on every connection close. Valgrind
confirms zero definitely-lost blocks across single and multi-request runs
(`gmake memcheck`).

---

## Performance & Roadmap

### Achieved

Benchmarked with `wrk -t8 -d30s` on the development laptop:

| Phase | Implementation | Concurrency | Req/sec | Latency (avg) |
|---|---|---|---|---|
| Baseline | fork-per-connection | c8 | ~277 | ~17 ms |
| **Phase 1** | **poll() event loop** | **c8** | **~111,000** | **56 µs** |
| Phase 1 | poll() event loop | c200 | ~114,000 | 9.4 ms |
| Phase 1 | poll() event loop | c1000 | ~111,000 | 23 ms |
| nginx (reference) | event-driven, multi-worker | c200 | ~124,000 | 1.6 ms |
| nginx (reference) | event-driven, multi-worker | c1000 | ~108,000\* | 9.5 ms |

\* nginx produced 284,000 read errors at c1000; the poll() server had 11 timeouts and zero
read errors at the same load. The baseline fork server could not sustain concurrent load —
`hey -c100` recorded 5,936 "connection refused" errors against 100 successful responses.

### Planned

| Phase | Focus | Target | Key technique |
|---|---|---|---|
| Phase 2 | Platform-native I/O multiplexing | >125,000 req/sec | kqueue (FreeBSD/macOS), epoll (Linux), event ports (illumos); poll() fallback |
| Phase 3 | Zero-copy file transfer | >150,000 req/sec | sendfile(2) on FreeBSD/Linux/macOS, sendfilev() on illumos |
| Phase 4 | Memory arenas | uncapped | Per-connection arena allocators; eliminate per-request malloc/free |

### Cross-platform I/O multiplexing (Phase 2)

The poll() loop will be replaced by a thin backend abstraction that selects
the best available mechanism at compile time:

| Platform | Backend |
|---|---|
| FreeBSD / macOS | `kqueue(2)` + `kevent(2)` |
| Linux | `io_uring(2)`, falling back to `epoll(7)` |
| illumos | event ports (`port_create` / `port_associate`) |
| everything else | `poll(2)` — current implementation, always available |

### Zero-copy sendfile (Phase 3)

Static file serving will bypass the `fread()` → `out_buf` → `send()` path:

| Platform | API |
|---|---|
| FreeBSD | `sendfile(2)` with `sf_hdtr` (combined header + body in one call) |
| Linux | `sendfile(2)` / `splice(2)` |
| macOS | `sendfile(2)` (Darwin variant) |
| illumos | `sendfilev(3EXT)` |
| fallback | current read + send path |

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
