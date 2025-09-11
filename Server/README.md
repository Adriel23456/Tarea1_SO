# Image Processing Server

A concurrent **TCP/TLS** image server with a **framed binary protocol**, per-connection threads, and a dedicated **priority scheduler** that processes **smaller files first**. Incoming images are received **entirely in memory** (no temporary disk copy) and then processed by a background worker. The server supports:

* **Dominant color classification** (red/green/blue)
* **Histogram equalization** (contrast enhancement)
* **Static PNG/JPG/JPEG** and **animated GIF** (per-frame processing with GIF output)

Outputs are written to `assets/colors/<red|green|blue>/` and `assets/histogram/`. There is **no** `assets/incoming/` directory anymore.

---

## What’s new (compared to earlier versions)

* **No more `assets/incoming/`:** received files are kept in memory only.
* **Priority queue (min-heap) + worker thread:** a scheduler thread “listens” and processes jobs ordered by **file size ascending**.
* **Connection threads** only receive data and enqueue jobs; all processing happens on the worker.

---

## Features

* **Thread-per-connection** (pthreads) for networking
* **Dedicated worker thread** with **min-heap** (smallest-first) scheduling
* **TCP or TLS** (OpenSSL; self-signed certs via `setup.sh`)
* **JSON config** (`assets/config.json`) for port, TLS and output paths
* **Thread-safe logging** to `assets/log.txt`
* **Framed protocol**: fixed header + payload messages
* **Animated GIF** support: decode with **stb\_image**, write with **gif.h**
* **Auto-download external headers**: `stb_image*.h`, `gif.h` (Makefile helpers)
* **Auto-create assets and default config** via `setup.sh` or `make setup`

---

## Requirements

* Linux
* `gcc`, `make`
* Dev packages:

  * `uuid-dev` (libuuid)
  * `libssl-dev` (OpenSSL)
  * `libjson-c-dev` (JSON-C)

The Makefile will fetch **stb\_image.h**, **stb\_image\_write.h**, and **gif.h** if missing.

### Package install

**Ubuntu/Debian**

```bash
sudo apt update
sudo apt install -y build-essential uuid-dev libssl-dev libjson-c-dev
```

**Fedora**

```bash
sudo dnf install -y gcc make libuuid-devel openssl-devel json-c-devel
```

**Arch**

```bash
sudo pacman -Syu --noconfirm base-devel util-linux-libs openssl json-c
```

---

## Project Structure

```
Server/
├── assets/                         # Auto-created
│   ├── histogram/                  # Histogram outputs
│   ├── colors/
│   │   ├── red/                    # Dominant color (red)
│   │   ├── green/                  # Dominant color (green)
│   │   └── blue/                   # Dominant color (blue)
│   ├── tls/                        # TLS cert/key (self-signed)
│   ├── log.txt                     # Server logs
│   └── config.json                 # Configuration (no 'incoming_dir')
├── src/
│   ├── main.c                      # Boot: config, logging, scheduler init, start_server()
│   ├── server.c / server.h         # Accept loop; thread-per-connection; protocol flow
│   ├── connection.c / .h           # Conn abstraction (TCP/TLS) + framing helpers
│   ├── scheduler.c / .h            # Min-heap (size-first) queue + worker thread
│   ├── image_processing.c / .h     # Static images: color & histogram; from memory
│   ├── gif_processing.c / .h       # GIF (animated): color & histogram per frame; from memory
│   ├── config.c / .h               # JSON-C loading + directory creation (no 'incoming_dir')
│   ├── logging.c / .h              # Thread-safe logging
│   ├── utils.c / .h                # mkdir -p, endian helpers, file utils
│   ├── protocol.h                  # Messages + processing types
│   ├── stb_image.h                 # (downloaded if missing)
│   ├── stb_image_write.h           # (downloaded if missing)
│   └── gif.h                       # (downloaded if missing)
├── Makefile
└── setup.sh                        # Interactive/CLI setup: deps, init, TLS, build
```

---

## Build & Run

### A) Interactive `setup.sh` (recommended)

```bash
cd Server
./setup.sh
```

Menu:

1. Full setup (deps + init + TLS localhost + enable TLS + build)
2. Full setup (deps + init + TLS custom host/IP + enable TLS + build)
3. Install dependencies only
4. Initialize project (dirs + default config)
5. Generate TLS for localhost and enable TLS
6. Generate TLS for custom host/IP and enable TLS
7. Build project only
8. Exit

### B) Non-interactive `setup.sh`

```bash
./setup.sh --install-deps
./setup.sh --init
./setup.sh --gen-tls-local && ./setup.sh --enable-tls
./setup.sh --gen-tls example.com && ./setup.sh --enable-tls
./setup.sh --build
./setup.sh --full-local
./setup.sh --full-custom example.com
```

### C) Makefile

```bash
make setup     # create assets/ and default config.json
make           # downloads headers if missing + builds
make rebuild   # clean + setup + build
make clean     # remove obj/ and binary
make help      # list targets
```

### Start the server

```bash
./image-server
```

* With `tls_enabled=1`, the server listens with TLS (uses `assets/tls/server.crt` and `server.key`).
* Otherwise, it listens on plain TCP.
* Logs go to `assets/log.txt`.

---

## Configuration (`assets/config.json`)

Default example (note: **no `incoming_dir`**):

```json
{
  "server": {
    "port": 1717,
    "tls_enabled": 0,
    "tls_dir": "assets/tls"
  },
  "paths": {
    "log_file": "assets/log.txt",
    "histogram_dir": "assets/histogram",
    "colors_dir": {
      "red": "assets/colors/red",
      "green": "assets/colors/green",
      "blue": "assets/colors/blue"
    }
  }
}
```

* **Change port**: `server.port`
* **Enable TLS**: `server.tls_enabled = 1` (or `./setup.sh --enable-tls`)
* **Customize output paths** as needed

---

## Protocol

**Header** (payload length is big-endian):

```c
typedef struct {
  uint8_t  type;           // MessageType
  uint32_t length;         // payload length (big-endian)
  char     image_id[37];   // UUID "8-4-4-4-12" + '\0' (or "" when unused)
} MessageHeader;
```

**Messages:**

```c
typedef enum {
  MSG_HELLO = 1,              // Client -> Server
  MSG_IMAGE_ID_REQUEST,       // (reserved)
  MSG_IMAGE_ID_RESPONSE,      // Server -> Client (uuid in header.image_id)
  MSG_IMAGE_INFO,             // Client -> Server (payload = ImageInfo)
  MSG_IMAGE_CHUNK,            // Client -> Server (payload = raw bytes)
  MSG_IMAGE_COMPLETE,         // Client -> Server (payload = "jpg"/"png"/"jpeg"/"gif")
  MSG_ACK,                    // Server -> Client (final ACK)
  MSG_ERROR                   // Server -> Client (text)
} MessageType;
```

**`ImageInfo` payload:**

```c
#define MAX_FILENAME 256
typedef struct {
  char     filename[MAX_FILENAME]; // base name
  uint32_t total_size;             // big-endian
  uint32_t total_chunks;           // big-endian
  uint8_t  processing_type;        // ProcessingType
  char     format[10];             // "jpg","jpeg","png","gif"
} ImageInfo;
```

**Processing types:**

```c
typedef enum {
  PROC_HISTOGRAM = 1,
  PROC_COLOR_CLASSIFICATION = 2,
  PROC_BOTH = 3
} ProcessingType;
```

**Typical flow:**

1. Client → `MSG_HELLO`
2. Server → `MSG_IMAGE_ID_RESPONSE` (UUID)
3. Client → `MSG_IMAGE_INFO` (`ImageInfo` payload)
4. Client → `MSG_IMAGE_CHUNK` × N
5. Client → `MSG_IMAGE_COMPLETE` (optional string payload = format)
6. Server → **enqueue job in memory**; worker processes by **size ascending**; server → `MSG_ACK`

---

## Processing Details

### Static images (PNG/JPG/JPEG) – in memory

* **Color classification:** computes global sums of **R, G, B** to pick the dominant channel; writes the **original image** to one of:

  * `assets/colors/red/`
  * `assets/colors/green/`
  * `assets/colors/blue/`
    with filename `<uuid>_<filename>`.

* **Histogram equalization:** per-channel (RGB) using CDF; writes to:

  * `assets/histogram/<uuid>_<filename>` (same format as input).

### GIF (animated) – in memory

* Decodes all frames with `stbi_load_gif_from_memory`.
* **Color classification:** global RGB sum across all frames → writes an **animated GIF** to the dominant color folder.
* **Histogram equalization:** per-frame RGB (alpha preserved) → writes an **animated GIF** to `histogram/`.
* **Delays:** heuristic to interpret ms→centiseconds (detect multiples of 10 ≥ 20); clamps to min **2 cs** (20 ms) and max **5000 cs** (50 s).

---

## Scheduler (smallest-first)

* A **worker thread** runs continuously and **waits** on a condition variable.
* Each completed upload is enqueued as a job containing:

  * In-memory buffer + size
  * `image_id`, `filename`, `format`, `processing_type`
* Jobs are stored in a **min-heap** keyed by `total_size` (ties break by filename), ensuring **small files are processed first**.

---

## TLS (optional)

Generate self-signed certs with `setup.sh`:

**localhost**

```bash
./setup.sh --gen-tls-local
./setup.sh --enable-tls
```

**custom host/IP**

```bash
./setup.sh --gen-tls example.com
./setup.sh --enable-tls
```

Files:

* `assets/tls/server.crt`
* `assets/tls/server.key`

> When TLS is enabled, clients must connect using TLS.

---

## Development Notes

* STB single-header implementations are defined **once** (in `image_processing.c`):

  * `#define STB_IMAGE_IMPLEMENTATION`
  * `#define STB_IMAGE_WRITE_IMPLEMENTATION`
* `gif_processing.c` **does not** define STB implementations; it only uses the APIs.
* Copies from user payloads avoid `-Wstringop-truncation` by using `strnlen + memcpy + manual '\0'`.
* Delays from `stbi_load_gif_from_memory` are freed with `free(delays)`.

---

## Make Targets

* `make` – download headers if needed and build
* `make setup` – create `assets/`, `log.txt`, and default `config.json`
* `make rebuild` – `clean` + `setup` + `make`
* `make clean` – remove `obj/` and binary
* `make clean-all` – also remove downloaded headers (`stb_*.h`, `gif.h`)
* `make help` – list targets and features

---

## Troubleshooting

* **TLS handshake fails:** verify `assets/tls/server.crt` and `server.key`, and `tls_enabled=1`.
* **Outputs missing in color/histogram folders:**

  * Ensure client sent `processing_type > 0`.
  * Check `assets/log.txt` for load/save errors.
* **`bind: Address already in use`:** another process is using the port; stop it or change `server.port`.

---

## Roadmap
* [ ] Daemonization (SysVinit or systemd unit) with start|stop|status|restart
* [ ] Processing modules: histogram equalization & color classification

## License

Academic project for the **Operating Systems** course. All rights reserved.