# Image Processing Server

Concurrent **TCP/TLS** server for receiving and processing images. It implements a **framed binary protocol** (fixed header + payload), assigns a **UUID** per image, saves incoming files, and runs **two processing pipelines**:

* **Dominant color classification** (red/green/blue)
* **Histogram equalization** (contrast enhancement)

Supports **PNG/JPG/JPEG** as static images and **GIF (including animated)** with per-frame processing and GIF output.

---

## Features

* **Thread-per-connection** concurrency (pthreads)
* **TCP or TLS**: optional TLS via OpenSSL (self-signed certs can be generated with `setup.sh`)
* **JSON config**: `assets/config.json` (port, TLS, output paths)
* **Thread-safe logging**: `assets/log.txt` (mutex-protected)
* **Binary protocol**: common header + `MSG_*` messages
* **Processing**:

  * **Dominant color** (R/G/B) → copies the image/animation to the matching color folder
  * **Histogram equalization** (RGB) → writes output to the `histogram` folder
* **Animated GIF**: decode with **stb\_image**, write with **gif.h** (per frame, respects delays)
* **Auto-created assets**: directory tree and default `config.json` via `setup.sh` or `make setup`
* **Auto-download external headers**: `stb_image*.h` and `gif.h` (Makefile scripts)

---

## Requirements

* Linux
* `gcc`, `make`
* Dev packages:

  * `uuid-dev` (libuuid)
  * `libssl-dev` (OpenSSL)
  * `libjson-c-dev` (JSON-C)
* The Makefile auto-downloads **stb\_image.h**, **stb\_image\_write.h**, and **gif.h** when missing.

### Package installation

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
├── assets/                       # Auto-created
│   ├── incoming/                 # Received files: <uuid>_<filename>
│   ├── histogram/                # Histogram outputs
│   ├── colors/
│   │   ├── red/                  # Dominant color (red)
│   │   ├── green/                # Dominant color (green)
│   │   └── blue/                 # Dominant color (blue)
│   ├── tls/                      # TLS cert/key (self-signed)
│   ├── log.txt                   # Server logs
│   └── config.json               # Configuration
├── src/
│   ├── main.c                    # Boot: config, logging, start_server()
│   ├── server.c / server.h       # Accept loop; thread-per-connection; protocol flow
│   ├── connection.c / .h         # Conn abstraction (TCP/TLS) + framing helpers
│   ├── image_processing.c / .h   # PNG/JPG/JPEG (static): color & histogram
│   ├── gif_processing.c / .h     # GIF (animated/static): color & histogram per frame
│   ├── config.c / .h             # JSON-C loading + directory creation
│   ├── logging.c / .h            # Thread-safe logging
│   ├── utils.c / .h              # mkdir -p, endian helpers, read file, etc.
│   ├── protocol.h                # Messages + processing types
│   ├── stb_image.h               # (downloaded if missing)
│   ├── stb_image_write.h         # (downloaded if missing)
│   └── gif.h                     # (downloaded if missing)
├── Makefile
├── setup.sh                      # Interactive/CLI setup: deps, init, TLS, build
├── download_stb.sh               # Downloads stb_image*.h
└── download_gif.sh               # Downloads gif.h
```

---

## Build & Run

### Option A: Interactive `setup.sh` (recommended)

```bash
cd Server
./setup.sh
```

Menu:

1. **Full setup** (deps + init + TLS for localhost + enable TLS + build)
2. **Full setup** (TLS for custom host/IP + enable TLS + build)
3. Install dependencies only
4. Initialize project (dirs + default config)
5. Generate TLS for localhost and **enable TLS**
6. Generate TLS for custom host/IP and **enable TLS**
7. **Build** only
8. Exit

### Option B: Non-interactive `setup.sh`

Examples:

```bash
# Dependencies only
./setup.sh --install-deps

# Initialize (create assets/ and default config.json if missing)
./setup.sh --init

# TLS for localhost and enable it
./setup.sh --gen-tls-local
./setup.sh --enable-tls

# TLS for specific host/IP and enable it
./setup.sh --gen-tls example.com
./setup.sh --enable-tls

# Build
./setup.sh --build

# Full (localhost)
./setup.sh --full-local

# Full (custom host/IP)
./setup.sh --full-custom example.com
```

### Option C: Makefile

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

* If **TLS is enabled** (`"tls_enabled": 1`), the server listens with TLS (uses `assets/tls/server.crt` and `server.key`).
* If TLS is disabled, it listens on **plain TCP**.

Logs live in `assets/log.txt`.

---

## Configuration (`assets/config.json`)

Default example:

```json
{
  "server": {
    "port": 1717,
    "tls_enabled": 0,
    "tls_dir": "assets/tls"
  },
  "paths": {
    "log_file": "assets/log.txt",
    "incoming_dir": "assets/incoming",
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
* **Customize paths** as needed.

---

## Protocol

**Common header** (`length` is big-endian):

```c
typedef struct {
  uint8_t  type;           // MessageType
  uint32_t length;         // payload length (big-endian)
  char     image_id[37];   // UUID "8-4-4-4-12" + '\0' (or "" when applicable)
} MessageHeader;
```

**Messages (`MessageType`)**:

```c
typedef enum {
  MSG_HELLO = 1,              // Client -> Server
  MSG_IMAGE_ID_REQUEST,       // (reserved)
  MSG_IMAGE_ID_RESPONSE,      // Server -> Client (uuid in header.image_id)
  MSG_IMAGE_INFO,             // Client -> Server (payload = ImageInfo)
  MSG_IMAGE_CHUNK,            // Client -> Server (payload = raw bytes)
  MSG_IMAGE_COMPLETE,         // Client -> Server (payload = "jpg"/"png"/"jpeg"/"gif")
  MSG_ACK,                    // Server -> Client (final ACK; per-chunk ACK optional)
  MSG_ERROR                   // Server -> Client (text)
} MessageType;
```

**`ImageInfo` payload**:

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

**Processing types**:

```c
typedef enum {
  PROC_HISTOGRAM = 1,
  PROC_COLOR_CLASSIFICATION = 2,
  PROC_BOTH = 3
} ProcessingType;
```

**Typical flow**:

1. Client sends `MSG_HELLO`.
2. Server responds with `MSG_IMAGE_ID_RESPONSE` and a new `image_id` (UUID).
3. Client sends `MSG_IMAGE_INFO` (payload `ImageInfo`).
4. Client sends `MSG_IMAGE_CHUNK` *N* (raw bytes).
5. Client sends `MSG_IMAGE_COMPLETE` (optional string payload with the format).
6. Server processes according to `processing_type` and replies with `MSG_ACK`.

---

## Image Processing

### Static (PNG/JPG/JPEG)

* **Color classification**:

  * Computes global sums of **R, G, B** and picks the dominant channel.
  * Saves an exact copy of the input image to one of:

    * `assets/colors/red/`
    * `assets/colors/green/`
    * `assets/colors/blue/`
  * File name: `<uuid>_<filename>`.

* **Histogram equalization**:

  * Per channel (R, G, B) using CDF.
  * Saves to `assets/histogram/<uuid>_<filename>` (same format as input).

### GIF (including animated)

* Decodes all frames with `stbi_load_gif_from_memory`.
* **Color classification**:

  * Global RGB sum across all frames → writes an **animated GIF** to the dominant color folder.
* **Histogram equalization**:

  * Per-frame (RGB; alpha preserved) → writes an **animated GIF** to `histogram/`.
* **Delays**:

  * Heuristic ms→centiseconds (detects multiples of 10 ≥ 20).
  * Clamps: min 2 cs (20 ms), max 5000 cs (50 s).

---

## TLS (optional)

`setup.sh` can generate **self-signed** certs:

* **localhost**:

  ```bash
  ./setup.sh --gen-tls-local
  ./setup.sh --enable-tls
  ```
* **Custom host/IP**:

  ```bash
  ./setup.sh --gen-tls example.com
  ./setup.sh --enable-tls
  ```

Files:

* `assets/tls/server.crt`
* `assets/tls/server.key`

> When **TLS is enabled**, the client must connect using **TLS**.

---

## Sample Logs

```
[YYYY-MM-DD HH:MM:SS] Server starting (plain TCP) on port 1717
[YYYY-MM-DD HH:MM:SS] Listening with image processing enabled...
[YYYY-MM-DD HH:MM:SS] Accepted connection from 127.0.0.1:56342
[YYYY-MM-DD HH:MM:SS] HELLO -> new image id = 0a1b2c3d-...
[YYYY-MM-DD HH:MM:SS] IMAGE_INFO: id=... file=photo.png size=... chunks=... proc=2 fmt=png
[YYYY-MM-DD HH:MM:SS] IMAGE_COMPLETE: id=... file=photo.png fmt=png chunks=... remaining=0
[YYYY-MM-DD HH:MM:SS] Color classification: saved to assets/colors/green/... (dominant: green)
[YYYY-MM-DD HH:MM:SS] Connection closed
```

---

## Development Notes

* **STB (single-header)**:

  * `#define STB_IMAGE_IMPLEMENTATION`
  * `#define STB_IMAGE_WRITE_IMPLEMENTATION`
  * **Only once** in the whole project (done in `image_processing.c`).
* `gif_processing.c` does **not** define STB implementations; it only uses the APIs.
* To avoid `-Wstringop-truncation`, `server.c` uses **safe copies** with `strnlen + memcpy` and manual `'\0'`.
* `delays` from `stbi_load_gif_from_memory` are released with `free(delays)`.

---

## Makefile Targets

* `make` – Download external headers if missing and build
* `make setup` – Create `assets/`, `log.txt`, and default `config.json`
* `make rebuild` – `clean` + `setup` + `make`
* `make clean` – Remove `obj/` and binary
* `make clean-all` – Also remove downloaded headers (`stb_*.h`, `gif.h`)
* `make help` – List targets and features

---

## Troubleshooting

* **TLS handshake fails**: check `assets/tls/server.crt` and `server.key`, and ensure `tls_enabled=1`.
* **Client uses https but server is on TCP**: enable TLS (`tls_enabled=1`) or use plain TCP.
* **No output in color/histogram folders**:

  * Ensure the client sent `processing_type > 0`.
  * Check `assets/log.txt` for load/save errors.
* **`bind: Address already in use`**: another process is using the port; stop it or change the port.

---

## Roadmap
* [ ] Daemonization (SysVinit or systemd unit) with start|stop|status|restart
* [ ] Processing modules: histogram equalization & color classification

## License

Academic project for the **Operating Systems** course. All rights reserved.