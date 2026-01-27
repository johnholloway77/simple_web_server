# FreeBSD Web Server Documentation {#mainpage}

## Overview

This is a high-performance HTTP/1.0 web server designed specifically for FreeBSD systems as part of the CS631 Advanced Programming in the Unix Environment coursework. The server implements a subset of the HTTP/1.0 protocol as defined in [RFC 1945](https://www.rfc-editor.org/rfc/rfc1945.html) and demonstrates advanced Unix systems programming concepts.

**⚠️ Important:** This is an educational project, not a production server. It has limited functionality and should not be exposed to the internet.

## Key Features

- **Dual-Stack IPv4/IPv6 Support** - Simultaneous listening on both protocol versions
- **Fork-per-Connection Model** - Each client connection handled in a separate process
- **CGI Script Execution** - Dynamic content generation with security validation
- **Directory Listing** - Automatic HTML directory browsing when no index.html exists
- **MIME Type Detection** - Automatic content-type detection using libmagic
- **Comprehensive Logging** - Request logging with timestamps and response codes
- **Daemon Mode** - Background operation with proper process management
- **Signal Handling** - Zombie process reaping with SIGCHLD handling
- **Memory Safety** - Valgrind-clean implementation with no memory leaks

## Architecture Overview

The server uses a modular design with clear separation of concerns:

### Core Components

- **main.c** - Server initialization, socket management, and main event loop
- **sockets/** - IPv4/IPv6 socket creation and connection handling
- **requests/** - HTTP request parsing and protocol validation
- **response/** - HTTP response generation and directory listings
- **cgi/** - CGI script execution with security controls
- **flags/** - Command-line argument processing
- **sig_handlers/** - Process signal management

### Process Model

1. **Main Process** - Listens on IPv4 and IPv6 sockets using select()
2. **Child Processes** - Fork per connection for request handling
3. **CGI Processes** - Separate processes for dynamic content execution

## Architecture Diagrams

### Request Processing Flow

```
┌─────────────┐    ┌──────────────┐    ┌─────────────────┐
│   Client    │    │ Main Process │    │ Child Process   │
│  (Browser)  │    │   (select)   │    │ (handleConn)    │
└──────┬──────┘    └──────┬───────┘    └─────────┬───────┘
       │                  │                      │
       │ HTTP Request     │                      │
       ├─────────────────►│                      │
       │                  │ accept()             │
       │                  ├─────────────────────►│
       │                  │ fork()               │
       │                  │◄─────────────────────┤
       │                  │                      │
       │                  │                      │ parseRequest()
       │                  │                      ├──────────┐
       │                  │                      │          │
       │                  │                      │◄─────────┘
       │                  │                      │
       │ HTTP Response    │                      │ send()
       │◄─────────────────────────────────────────┤
       │                  │                      │
       │                  │                      │ exit()
       │                  │                      ├──────────┐
       │                  │ SIGCHLD              │          │
       │                  │◄─────────────────────┘          │
       │                  │ waitpid()            │          │
       │                  ├─────────────┐        │          │
       │                  │             │        │          │
       │                  │◄────────────┘        │          X
       │                  │                      │
```

### Module Dependency Structure

```
                    ┌─────────────┐
                    │    main.c   │
                    │ (entry point│
                    │   select)   │
                    └──────┬──────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
    ┌──────────┐    ┌─────────────┐   ┌──────────┐
    │ flags/   │    │  sockets/   │   │sig_handlers/│
    │ setFlags │    │handleSocket │   │   reap.c   │
    └──────────┘    └──────┬──────┘   └──────────┘
                           │
                           ▼
                  ┌─────────────────┐
                  │    sockets/     │
                  │ handleConnection│
                  └────────┬────────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
         ┌──────────┐ ┌──────────┐ ┌──────────┐
         │requests/ │ │response/ │ │   cgi/   │
         │parseReq  │ │dirResponse│ │ cgiExe   │
         └────┬─────┘ └──────────┘ └──────────┘
              │
         ┌────┼────┐
         │    │    │
         ▼    ▼    ▼
    ┌────────┬────────┬────────┐
    │checkHttp checkMethod   │
    └─────────────────────────┘
```

### File Organization

```
server_revision/
├── main.c ...................... Server initialization & main loop
├── Makefile .................... Build configuration
├── setup.sh .................... Environment setup script
├── flags/
│   ├── flags.h ................. Command-line flag definitions
│   └── setFlags.c .............. Argument parsing & validation
├── sockets/
│   ├── socket.h ................ Socket interface definitions
│   ├── createSocket_v4.c ....... IPv4 socket creation
│   ├── createSocket_v6.c ....... IPv6 socket creation
│   ├── handleSocket.c .......... Connection acceptance
│   └── handleConnection.c ...... HTTP request processing
├── requests/
│   ├── requests.h .............. Request parsing interface
│   ├── parseRequest.c .......... HTTP request parser
│   ├── checkHttp.c ............. HTTP version validation
│   └── checkMethod.c ........... HTTP method validation
├── response/
│   ├── response.h .............. Response interface
│   ├── dirResponse.c ........... Directory listing generator
│   └── dirResponse.h ........... Directory response definitions
├── cgi/
│   ├── cgi.h ................... CGI interface
│   └── cgiExe.c ................ CGI script execution
└── sig_handlers/
    ├── reap.h .................. Signal handler interface
    └── reap.c .................. SIGCHLD handler (legacy)
```

## Security Features

- **Path Traversal Protection** - Blocks attempts to access files outside document root
- **Input Validation** - Comprehensive validation of HTTP methods and versions
- **CGI Security** - Command injection prevention and environment sanitization
- **Buffer Overflow Protection** - Safe string operations and bounds checking
- **Memory Management** - Proper cleanup and leak prevention

## Performance Characteristics

**Current Performance (Phase 0 Baseline):**
- ~2,450 requests/second (10.7x improvement from initial 229 req/sec)
- Low latency: ~3.3ms average response time
- Efficient MIME type detection with caching optimizations
- Memory-efficient operation with zero definitely lost bytes (Valgrind-verified)

**Planned Performance Improvements:**
- **Phase 1**: HTTP/1.0 protocol compliance → ~500 req/sec target
- **Phase 2**: Event-driven poll() implementation → ~10,000 req/sec target
- **Phase 3**: FreeBSD kqueue implementation → ~30,000 req/sec target
- **Phase 4**: Memory arena allocation → ~80,000 req/sec target
- **Phase 5**: Zero-copy sendfile() → ~100,000+ req/sec target

## Building and Installation

### Prerequisites
- FreeBSD 14.1+ (designed and tested on FreeBSD)
- Standard development tools (make, gcc/clang)
- libmagic for MIME type detection

### Platform Compatibility
While designed specifically for FreeBSD 14.1, this server should run on any POSIX-compliant Unix-like system with little to no modification. The code uses standard POSIX APIs and avoids FreeBSD-specific extensions where possible.

**Tested on:** FreeBSD 14.1
**Should work on:** Linux, macOS, OpenBSD, NetBSD, Solaris, and other POSIX systems

### Build Steps
```bash
# Quick setup with example content
./setup.sh

# Or build manually
make

# Clean build artifacts
make clean
```

### Directory Structure After Setup
```
├── cgi-bin/          # CGI scripts
├── cgi-data/         # CGI data storage
├── index.html        # Default index page
├── simple_server/    # Build output directory
├── subNoIndex/       # Example directory without index
└── subWithIndex/     # Example directory with index
```

## Usage

```bash
./simple_server [options]
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `-c dir` | Enable CGI execution from specified directory |
| `-d` | Debug mode: no daemon, single connection, stdout logging |
| `-l file` | Log all requests to specified file |
| `-p port` | Listen on specified port (default: 8080) |

### Example Usage

```bash
# Start in daemon mode on port 8080
./simple_server

# Debug mode with CGI and logging
./simple_server -d -c ./cgi-bin -l server.log -p 3000

# Production mode with CGI on port 80 (requires root)
sudo ./simple_server -c ./cgi-bin -l /var/log/webserver.log -p 80
```

### Accessing the Server

- **Web Browser**: http://localhost:8080
- **cURL**: `curl -v http://localhost:8080/`
- **Telnet**: `telnet localhost 8080` (for manual HTTP testing)

## Logging Format

All requests are logged in Common Log Format:
```
IP_ADDRESS TIMESTAMP "REQUEST_LINE" STATUS_CODE BYTES_SENT
```

Example:
```
127.0.0.1 2024-01-26T15:30:45Z "GET /index.html HTTP/1.1" 200 1024
::1 2024-01-26T15:30:46Z "GET /cgi-bin/hello.cgi HTTP/1.0" 200 256
```

## Process Management

### Starting as Daemon
```bash
./simple_server
# Server detaches and runs in background
```

### Finding and Stopping Daemon
```bash
# Find process ID
sockstat | grep 8080

# Stop server
kill <process_id>

# Or use pkill
pkill simple_server
```

## Testing and Verification

### Memory Testing with Valgrind
```bash
valgrind --leak-check=full --show-leak-kinds=all ./simple_server -d
```

### Performance Benchmarking
```bash
# Install hey benchmarking tool
# FreeBSD: pkg install hey

# Basic benchmark
hey -n 1000 -c 10 http://localhost:8080/

# Stress test
hey -n 10000 -c 100 -t 30 http://localhost:8080/
```

## Development Notes

This project demonstrates several advanced Unix programming concepts:

- **Socket Programming** - IPv4/IPv6 dual-stack implementation
- **Process Management** - Fork, exec, signal handling, zombie reaping
- **I/O Multiplexing** - select() for handling multiple sockets
- **Inter-Process Communication** - Pipes for CGI communication
- **Memory Management** - Malloc/free discipline with Valgrind verification
- **Security Programming** - Input validation and attack prevention
- **Performance Analysis** - Profiling and optimization techniques

## Learning Objectives Achieved

- Understanding of HTTP protocol internals
- Mastery of BSD socket programming
- Process lifecycle management in Unix
- Security-conscious C programming
- Performance profiling and optimization
- Professional debugging practices

## Contributing

This is an educational project. For learning purposes, examine the code to understand:
- How HTTP requests are parsed and validated
- How the fork-per-connection model works
- How CGI scripts are executed securely
- How memory management is handled safely

## References

- [RFC 1945 - HTTP/1.0](https://www.rfc-editor.org/rfc/rfc1945.html)
- [Stevens CS631 Assignment](https://stevens.netmeister.org/631/f23-group-project.html)
- [FreeBSD Developer's Handbook](https://docs.freebsd.org/en/books/developers-handbook/)
- [Secure Coding in C](https://www.securecoding.cert.org/)

---

**Version:** 0.1-baseline
**Course:** CS631 Advanced Programming in the Unix Environment
**Institution:** Stevens Institute of Technology
**Instructor:** Jan Schaumann