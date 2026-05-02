# C-Python Client-Server University Project

> **Final project for Laboratorio 2** — A multi-threaded, mixed-language client-server application that processes text streams through a producer-consumer pipeline backed by a concurrent hash table.

## Overview

This project demonstrates a **C + Python** client-server system where:

- **Python server** listens for TCP connections, distributes incoming text to **two named pipes** based on connection type
- **C archive process** reads from both pipes, parses words, and maintains a **hash table** that counts word occurrences
- **C clients** read text files, connect via TCP, and stream words to the server

The result is a **producer-consumer / reader-writer pipeline** that safely processes concurrent text ingestion and lookup using threads, semaphores, mutexes, and POSIX named pipes (FIFOs).

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  client1.c (single-message, type A)                          │
│  client2.c (multi-message,  type B)                          │
└────────────┬──────────────┬──────────────────────────────────┘
             │ TCP :55116   │ TCP :55116
             ▼              ▼
┌──────────────────────────────────────────────────────────────┐
│                 Python Server (server.py)                    │
│                                                              │
│  ┌─ ConnectionA ──► "capolet" FIFO ──┐                       │
│  │                                   │                       │
│  └─ ConnectionB ──► "caposc" FIFO  ──┤                       │
│                                      │                       │
│  Spawns ./archivio as subprocess     │                       │
│  Manages shutdown via SIGTERM        │                       │
└──────────────────────────────────────┼───────────────────────┘
                                       │
                     Named Pipes (FIFOs)
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────┐
│               C Archive Process (archivio)                   │
│                                                              │
│  ┌─ Reader Master Thread ──┬─ Reader Consumer Threads ──►    │
│  │  (reads "capolet")      │  (lookup in hash table,         │
│  │                         │   write to lettori.log)         │
│  └─ Writer Master Thread ──┼─ Writer Consumer Threads ──►    │
│     (reads "caposc")       │  (insert into hash table,       │
│                            │   count occurrences)            │
│                                                              │
│  Bounded buffer (size 10) with semaphore-based               │
│  producer-consumer synchronization                           │
│                                                              │
│  Readers-writers lock (readers-priority)                     │
│  for concurrent hash table access                            │
│                                                              │
│  Signal handler thread: SIGINT → print distinct word count   │
│                          SIGTERM → graceful shutdown         │
└──────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
.
├── server.py                    PyInstaller-compiled executable (Python 3)
├── server_files/
│   ├── server.py                Server entry point
│   ├── ServerProtocol.py        Abstract base class for connection handling
│   ├── ConnectionA.py           Single-message connection handler (type A)
│   ├── ConnectionB.py           Multi-message connection handler (type B)
│   └── PipeManager.py           Named pipe (FIFO) wrapper
├── archivio.c                   Main archive process — threading, IPC, signal handling
├── hashTableManager.c           Hash table operations (add, count, create, destroy entries)
├── hashTableManager.h           Header for hash table functions
├── rw_lock.c                    Readers-writers lock (readers-priority)
├── rw_lock.h                    Header for readers-writers lock
├── xerrori.c                    Error-checking wrappers for system calls
├── xerrori.h                    Header for error wrappers
├── client1.c                    Single-message client (type A) — one connection per line
├── client2.c                    Multi-message client (type B) — one persistent connection per file
├── makefile                     Build rules (gcc, pthreads, librt)
├── file1.txt / file2.txt / file3.txt  Sample input files (Italian dictionary words)
├── images/
│   ├── server.jpeg              High-level server flow diagram
│   └── connection_and_pipe.jpeg Connection & pipe flow diagram
└── .gitignore
```

---

## How It Works

### 1. Server (`server.py`)

The Python server:
- Listens on **`127.0.0.1:55116`** with a **ThreadPoolExecutor**
- On connection, reads **1 byte** to determine the connection type:
  - `'a'` → **ConnectionA**: one string, close — writes to `"capolet"` FIFO
  - `'b'` → **ConnectionB**: multiple strings until a zero-length marker — writes to `"caposc"` FIFO
- Spawns the `archivio` C process as a subprocess
- On `KeyboardInterrupt` (Ctrl+C): closes the FIFOs, sends **SIGTERM** to the archive process, waits for it, cleans up

### 2. Clients

Both clients connect to `127.0.0.1:55116`.

| Client | Connection Type | Behavior |
|--------|---------------|----------|
| `client1.c` | A (single-message) | Opens a file, reads lines via `getline`, opens a fresh TCP connection for **each line**, sends connection type + length + string, closes |
| `client2.c` | B (multi-message) | Takes multiple files via argv, creates one thread per file, opens **one** TCP connection per thread, sends all lines consecutively, signals end with a length of **0**, then closes |

Both send:
- **1 byte**: connection type (`'a'` or `'b'`)
- **2 bytes**: string length as a `short` in **network byte order** (htons)
- **N bytes**: the string itself (null-terminated)

> **Why `short`?** Strings are capped at 2048 characters, so 2 bytes (16 bits) is more than enough.

### 3. Archive Process (`archivio.c`)

This is the core processing engine, written in C.

#### Threading Model

| Thread Type | Count | Role |
|------------|-------|------|
| Writer Master | 1 | Reads `"caposc"` FIFO, parses tokens, pushes into bounded buffer |
| Reader Master | 1 | Reads `"capolet"` FIFO, parses tokens, pushes into bounded buffer |
| Writer Consumers | N (configurable, default 3) | Pop from buffer, insert into hash table (count occurrences) |
| Reader Consumers | N (configurable, default 3) | Pop from buffer, look up in hash table, log to `lettori.log` |
| Signal Handler | 1 | Waits for SIGTERM / SIGINT via `sigwait`, orchestrates graceful shutdown |

#### Producer-Consumer Buffer

A **bounded buffer** of size 10 connects each master thread to its consumers. Synchronization uses:
- **POSIX unnamed semaphores** (`full_slots`, `empty_slots`) for availability signaling
- **`pthread_mutex_t`** for mutual exclusion when consumers pop from the buffer

#### Hash Table Access (Readers-Writers Lock)

Hash table operations are protected by a **readers-priority readers-writers lock** (`rw_lock.c`):

- Multiple readers can access simultaneously
- A writer needs exclusive access (waits until all readers finish)
- New readers can still enter while a writer is waiting (readers-priority)

#### Signal Handling

- **SIGINT** (from `pkill -SIGINT`): prints number of **distinct strings** added to the hash table to stderr
- **SIGTERM** (from the Python server on shutdown):
  1. Joins the master threads (they exit when the FIFO is closed)
  2. Master threads set `termination_code = -1` and post the semaphore
  3. Each consumer wakes, checks the code, posts the next consumer, and exits
  4. Signal handler joins all consumer threads
  5. Prints distinct count to stdout
  6. Returns to `main()` for memory cleanup

> The termination code is a **dedicated variable**, not written to the buffer. This avoids issues with the `char *` buffer type and potential conflicts with token separators in `strtok`.

### 4. Hash Table Manager (`hashTableManager.c`)

Wraps POSIX `hsearch` / `hcreate` with:
- `aggiungi(s)`: insert or increment word count
- `conta(s)`: look up word count
- `crea_entry(s, n)` / `distruggi_entry(e)`: entry lifecycle helpers

When a new string is inserted, the first byte of the source string is set to `'\0'` as a sentinel so the writer consumer can increment the distinct-count counter.

---

## Building & Running

### Prerequisites

- **GCC** with C11 support
- **Python 3** (for the server — the repo includes a pre-built executable, but you can rebuild from sources)
- POSIX-compatible system (Linux, macOS with some adaptation)

### Build

```bash
make
```

This compiles `archivio`, `client1`, and `client2`.

### Run

Start the server with a thread pool, optional reader/writer counts, and optional valgrind:

```bash
./server.py 5 -r 2 -w 4
```

| Flag | Description | Default |
|------|-------------|---------|
| `thread` (positional) | Number of threads in the server's ThreadPoolExecutor | — |
| `-r / --readers_number` | Number of reader consumer threads | `3` |
| `-w / --writers_number` | Number of writer consumer threads | `3` |
| `-v / --valgrind` | Run `archivio` under valgrind for leak checking | off |

Then run clients in another terminal:

```bash
# Multi-message client (type B): multiple files, one connection per file
./client2 file1.txt file2.txt

# Single-message client (type A): one connection per line
./client1 file3.txt
```

Or use the Makefile shortcut:

```bash
make run_server
# python3 server.py 5 -r 2 -w 4 &
# sleep 2
# ./client2 file1.txt file2.txt
# sleep 1
# ./client1 file3.txt
# pkill -SIGINT -f python3
```

### Output Files

| File | Description |
|------|-------------|
| `lettori.log` | Reader consumers write `"<word> <count>"` for each word looked up |
| `server.log` | Server connection log (bytes written per connection) |
| `capolet` / `caposc` | Named pipes (created at runtime, cleaned up on exit) |

---

## Key Design Decisions

- **Semaphores over condition variables** for the bounded buffer: the condition to check is a simple integer, and the single-producer design doesn't need a mutex for the producer side
- **`strtok_r` over `strtok`**: avoids the global state issue of the standard `strtok`, making the code thread-safe by default
- **Termination code outside the buffer**: prevents conflicts between a sentinel value and actual string tokens (which could be any `char *`)
- **Readers-priority lock**: minimizes blocking for the more frequent read operations (log queries)
- **Client type A uses one connection per line**: while less efficient, it's simpler and avoids keeping state between messages
- **Client type B uses threads per file**: allows concurrent processing of multiple files while keeping one persistent connection per file

---

## Cleanup

```bash
make clean
```

---

## License

University project — for educational purposes.
