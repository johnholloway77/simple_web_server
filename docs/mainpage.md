# FreeBSD Web Server Documentation {#mainpage}

## Overview

This is an HTTP/1.0 web server designed for FreeBSD as part of CS631
Advanced Programming in the Unix Environment coursework. It implements a
subset of HTTP/1.0 as defined in
[RFC 1945](https://www.rfc-editor.org/rfc/rfc1945.html).

The server runs as a **single process using a `poll()`-based event loop**.
There are no forked child processes for request handling — all connections
are served concurrently within one process across parallel non-blocking
sockets. CGI execution still forks, as required by the CGI specification.

**⚠️ Important:** This is an educational project, not a production server.
It has limited functionality and should not be exposed to the internet.

---

## Key Features

- **Single-process poll() event loop** — non-blocking I/O, no fork per connection
- **Dual-stack IPv4/IPv6** — simultaneous listeners on both protocol versions
- **Dynamic connection array** — grows via realloc as connections arrive
- **HTTP/1.0 and HTTP/1.1 accepted** — responses always sent as HTTP/1.0
- **Content-Length on all responses** — full headers per RFC 1945
- **MIME type detection** — extension table with libmagic fallback
- **Directory listings** — HTML browsing when no index.htm/index.html exists
- **CGI execution** — fork/exec in `build_okay_response.c`; pipes stdout, waits for exit, assembles response
- **Daemon mode** — background operation, suppressed by `-v`
- **SIGTERM/SIGINT handled** — clean event-loop exit, all memory freed
- **Memory safety** — Valgrind-clean and AddressSanitizer-clean

---

## Architecture

### Event Loop

`main()` creates one IPv4 and one IPv6 listening socket (both
`O_NONBLOCK`), allocates parallel `pollfd` and `Client` arrays, then
enters the `poll()` loop:

```
while (running) {
    poll(pfds, fd_count, -1);

    for each fd:
        if listener fd  → accept_new_conn()   // drain all pending accepts
        if POLLERR/HUP  → close_conn()
        if POLLIN  + READING        → do_read()
        if POLLOUT + SENDING_HEADER
                   or SENDING_BODY  → do_write()
        if CLOSING                  → close_conn()
}
```

### Connection State Machine

Each connection follows a linear path through the `client_state` enum:

```
READING ──► (PROCESSING) ──► SENDING_HEADER ──► SENDING_BODY ──► CLOSING
   │                                                                  ▲
   └──────────────────── error / EOF ───────────────────────────────►┘
```

`PROCESSING` is transient — it occurs inside `do_read()` while the
response is being built and is never observed by the poll loop.

### Request Handling Pipeline

All of the following happens inside a single `do_read()` call once
`"\r\n\r\n"` is detected in the input buffer:

```
recv() loop → append to in_buf
     │
     ▼
parse_request()      validate method, version, path, traversal check
     │
     ▼
resolve_path()       URI → filesystem: sets rp.path_type (IS_FILE/IS_DIR/IS_CGI)
     │
     ├─ success → build_okay_response()   dispatches on path_type:
     │                IS_FILE → serve file bytes
     │                IS_DIR  → generate HTML directory listing
     │                IS_CGI  → fork/exec script, pipe stdout, assemble response
     └─ error   → build_error_response()  assemble 4xx/5xx into out_buf
                          │
                          ▼
              pfds[i].events = POLLOUT
              client.state   = SENDING_HEADER
```

`do_write()` then drains `out_buf` via `send()` and issues
`shutdown(SHUT_WR)` on completion.

### Module Dependency

```
                        ┌───────────┐
                        │  main.c   │
                        │ poll loop │
                        └─────┬─────┘
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
          ▼                   ▼                   ▼
    ┌──────────┐      ┌──────────────┐     ┌───────────┐
    │  flags/  │      │ client_conn/ │     │ sockets/  │
    │ setFlags │      │ connections  │     │get_listener│
    └──────────┘      │ accept_conn  │     └───────────┘
                      │ do_read      │
                      │ do_write     │
                      └──────┬───────┘
                             │
                   ┌─────────┴──────────┐
                   │                    │
                   ▼                    ▼
            ┌──────────┐        ┌──────────────┐
            │requests/ │        │  response/   │
            │parse_req │        │ build_okay   │
            │resolve_  │        │ build_err    │
            │  path    │        └──────────────┘
            └──────────┘
```

---

## File Structure

```
server_revision/
├── main.c                          Entry point: signal setup, poll() loop
│
├── flags/
│   ├── flags.h                     Flag bit definitions (C_FLAG, V_FLAG, …)
│   └── setFlags.c                  Command-line argument parsing
│
├── sockets/
│   ├── socket.h                    get_listener_v4/v6 declarations
│   ├── get_listener_v4.c           Non-blocking IPv4 listening socket
│   └── get_listener_v6.c           Non-blocking IPv6 listening socket
│
├── client_conn/
│   ├── connections.h               Client struct, enums, function declarations
│   ├── connections.c               add_to_lists(), close_conn()
│   ├── accept_new_conn.c           Drain listener, set O_NONBLOCK, record peer addr
│   ├── do_read.h                   do_read() declaration
│   ├── do_read.c                   recv() loop → parse → resolve → build response
│   ├── do_write.h                  do_write() declaration
│   └── do_write.c                  send() loop, shutdown(SHUT_WR) on completion
│
├── requests/
│   ├── parse_request.h             Request struct, http_method/version enums
│   ├── parse_request.c             HTTP request-line parser and validator
│   ├── resolve_path.h              ResolvedPath struct, resolve_path() declaration
│   ├── resolve_path.c              URI→filesystem, MIME detection, CGI routing
│   └── close_resolve_path.c        close_resolve_path_ptr() — fclose wrapper
│
├── response/
│   ├── build_response.h            build_okay_response() / build_error_response() decls
│   ├── build_okay_response.c       200 OK: static files, directory listings, CGI execution
│   ├── build_error_response.c      4xx/5xx error responses
│   └── version_info.h              SERVER_VERSION macro
│
├── debug/
│   └── debug.h                     DBG/DBG_DEC/DBG_DO — no-op in release builds
│
└── test_cases/
    ├── test_parse_request.c        Criterion suite: parse_request() (~40 cases)
    ├── test_resolve_path.c         Criterion suite: resolve_path()
    ├── test_build_error.c          Criterion suite: build_error_response()
    └── test_build_okay_response.c  Criterion suite + integration: build_okay_response()
```

Build output goes into `build/release/`, `build/debug/`, and
`build/valgrind/`. Binaries are placed in the project root:
`simple_server`, `simple_server_debug`, `simple_server_valgrind`.

---

## Building

GNU make is required. On FreeBSD use `gmake`:

```sh
gmake                 # release build  →  ./simple_server
gmake debug           # ASan + -DDEBUG →  ./simple_server_debug
gmake valgrind-build  # symbols only   →  ./simple_server_valgrind
gmake clean
```

### Dependencies

| Package | Purpose |
|---|---|
| `libmagic` | MIME-type detection fallback (base system on FreeBSD) |
| `criterion` | Unit testing framework |
| `doxygen` | This documentation |
| `graphviz` | Call/dependency diagrams (optional) |
| `llvm` | clang-format for code formatting (optional) |
| `cppcheck` / `cpplint` | Static analysis (optional) |

```sh
pkg install doxygen graphviz llvm cppcheck py39-cpplint criterion
```

---

## Usage

```sh
./simple_server [options]
```

| Flag | Argument | Description |
|---|---|---|
| `-c` | `dir` | Enable CGI execution from the given directory |
| `-v` | — | Verbose: skip daemonizing, log to stdout |
| `-bind4` | `addr` | Bind IPv4 listener to a specific address (default: all) |
| `-bind6` | `addr` | Bind IPv6 listener to a specific address (default: all) |
| `-l` | `file` | Log all requests to a file |
| `-p` | `port` | Listen port (default: 8080; root required below 1024) |

```sh
# Verbose mode, all interfaces, port 8000
./simple_server -v -p 8000

# Specific addresses
./simple_server -bind4 192.168.1.10 -bind6 ::1 -p 8080

# CGI + logging
./simple_server -c ./cgi-bin -l ./server.log
```

### Stopping the server

```sh
sockstat | grep 8080
kill <pid>      # SIGTERM triggers a clean event-loop exit
```

---

## Testing

```sh
gmake test-parse              # parse_request suite
gmake test-resolve            # resolve_path suite
gmake test-build-error        # build_error_response suite
gmake test-build-okay         # build_okay_response unit + integration
gmake test                    # all unit suites
gmake test-all                # all suites + Valgrind memcheck
gmake test-all-asan           # all suites under AddressSanitizer
gmake memcheck                # start server under Valgrind, fire 500 requests
```

---

## Performance & Roadmap

### Achieved

Benchmarked with `wrk` on the development laptop (`wrk -t8 -d30s`):

| Phase | Implementation | Concurrency | Req/sec | Latency (avg) |
|---|---|---|---|---|
| Baseline | fork-per-connection | c8 | ~277 | ~17 ms |
| **Phase 1** | **poll() event loop** | **c8** | **~111,000** | **56 µs** |
| Phase 1 | poll() event loop | c200 | ~114,000 | 9.4 ms |
| Phase 1 | poll() event loop | c1000 | ~111,000 | 23 ms |
| nginx (reference) | event-driven, multi-worker | c200 | ~124,000 | 1.6 ms |
| nginx (reference) | event-driven, multi-worker | c1000 | ~108,000\* | 9.5 ms |

\* nginx produced 284,000 read errors at c1000; our server had 11 timeouts and zero read errors.

**Notable:** the poll() server matches nginx throughput at high concurrency on this machine
and beats it at c1000 on error-free delivery, despite being single-process and having no
sendfile or kernel-tuned worker model. The baseline fork server couldn't accept connections
fast enough to sustain load — `hey -c100` showed 5,936 "connection refused" errors against
100 successful responses.

### Planned

| Phase | Focus | Target | Key technique |
|---|---|---|---|
| Phase 2 | Platform-native I/O multiplexing | >125,000 req/sec | kqueue (FreeBSD/macOS), epoll (Linux), event ports (illumos); poll() fallback |
| Phase 3 | Zero-copy file transfer | >150,000 req/sec | sendfile(2) on FreeBSD/Linux/macOS, sendfilev() on illumos |
| Phase 4 | Memory arenas | uncapped | Per-connection arena allocators; eliminate per-request malloc/free |

### Cross-platform I/O multiplexing strategy (Phase 2)

The event-loop backend will be selected at compile time based on platform:

```
FreeBSD / macOS  →  kqueue(2) + kevent(2)
Linux            →  io_uring(2) with epoll(7) fallback
illumos          →  event ports (port_create / port_associate)
everything else  →  poll(2)   (current implementation, always available)
```

A thin abstraction layer (`event_backend.h`) will expose a uniform
interface so `main.c` and `do_read.c`/`do_write.c` are unaffected by the
backend in use.

### Zero-copy sendfile strategy (Phase 3)

Static file serving will bypass the `read()` → `out_buf` → `send()`
pipeline and hand the file descriptor directly to the kernel:

```
FreeBSD   →  sendfile(2)            (sf_hdtr for combined header+body send)
Linux     →  sendfile(2)            (splice(2) as fallback for pipes)
macOS     →  sendfile(2)            (Darwin variant, same fd-based API)
illumos   →  sendfilev(3EXT)
fallback  →  current read+send path (always available)
```

---

## Security

- Path traversal blocked in `parse_request()` (`../` detection)
- Absolute-path URIs rejected (path must start with `/`)
- CGI query string length capped at `PATH_MAX` before fork
- All string operations use bounds-checked functions (`strlcpy`, `snprintf`)
- AddressSanitizer and Valgrind used continuously during development

---

## References

- [RFC 1945 — HTTP/1.0](https://www.rfc-editor.org/rfc/rfc1945.html)
- [Stevens CS631 Assignment](https://stevens.netmeister.org/631/f23-group-project.html)
- [poll(2) — FreeBSD man page](https://man.freebsd.org/cgi/man.cgi?query=poll&sektion=2)
- [FreeBSD Developer's Handbook](https://docs.freebsd.org/en/books/developers-handbook/)

---

**Version:** Phase 1 (poll revision)
**Course:** CS631 Advanced Programming in the Unix Environment
**Platform:** FreeBSD (also builds on Linux, macOS, Illumos)
