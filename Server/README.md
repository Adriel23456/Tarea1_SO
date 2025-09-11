# Image Processing Server

A minimal, sequential **TCP server** that receives images from the Image Processing Client, logs events, and writes incoming files to disk. It implements the handshake & framed messaging protocol required by the client uploader.

> **Scope (current)**: receive-only, sequential (one connection at a time), non-daemon. Processing (histogram / color classification), daemonization, and TLS will be added in later milestones.

## Features

- **TCP Listener** on default port `1717`
- **Handshake**: on `MSG_HELLO`, issues a **UUID** (image ID) back to the client
- **Chunked Receive**: accepts `MSG_IMAGE_INFO`, `MSG_IMAGE_CHUNK`*N*, `MSG_IMAGE_COMPLETE`
- **File Writing**: stores images under `assets/incoming/<uuid>_<filename>`
- **Logging**: appends human-readable entries to `assets/log.txt`
- **Robust framing**: fixed header + payload in network byte order

## Prerequisites

- Linux-based OS
- GCC, make
- **libuuid** development package

### Packages

**Ubuntu/Debian**
```bash
sudo apt update
sudo apt install -y uuid-dev build-essential
````

**Fedora**

```bash
sudo dnf install -y libuuid-devel gcc make
```

**Arch**

```bash
sudo pacman -S --noconfirm util-linux-libs base-devel
```

## Install & Build

```bash
cd Server
./setup.sh      # installs deps, creates assets/, builds
./image-server  # start the server
```

Manual steps:

```bash
# Create directories
mkdir -p assets assets/incoming
[ -f assets/log.txt ] || echo "== Image Server Log ==" > assets/log.txt

# Build
make
./image-server
```

## Project Structure

```
Server/
├── src/
│   └── main.c          # TCP server implementation
├── assets/
│   ├── incoming/       # Received files: <uuid>_<filename>
│   └── log.txt         # Server logs
├── Makefile
└── setup.sh
```

## Run

1. Start the server:

   ```bash
   ./image-server
   ```

   Logs are written to `assets/log.txt`, e.g.:

   ```
   [YYYY-MM-DD HH:MM:SS] Server starting on port 1717
   [YYYY-MM-DD HH:MM:SS] Listening...
   [YYYY-MM-DD HH:MM:SS] Accepted connection from 127.0.0.1:54000
   [YYYY-MM-DD HH:MM:SS] HELLO -> new image id = 0a1b2c3d-...
   [YYYY-MM-DD HH:MM:SS] IMAGE_INFO: id=... file=Azul.png size=... chunks=... proc=... fmt=png
   [YYYY-MM-DD HH:MM:SS] IMAGE_COMPLETE: id=... file=Azul.png fmt=png chunks=... remaining=0
   [YYYY-MM-DD HH:MM:SS] Connection closed
   ```

2. Start the client and send images:

   * In the client config, set `"protocol": "http"` and point `host`/`port` to the server.
   * Click **Send Images** in the client.

## Protocol

All frames begin with:

```c
typedef struct {
  uint8_t  type;        // MessageType
  uint32_t length;      // payload length (network order)
  char     image_id[37];// UUID string or "" (null-terminated)
} MessageHeader;
```

**MessageType:**

* `MSG_HELLO` (client → server)
* `MSG_IMAGE_ID_RESPONSE` (server → client; sets `header.image_id`)
* `MSG_IMAGE_INFO` (client → server; payload = `ImageInfo`)
* `MSG_IMAGE_CHUNK` (client → server; payload = raw bytes)
* `MSG_IMAGE_COMPLETE` (client → server; payload = `"jpg"`/`"png"`/`"jpeg"`/`"gif"`)
* `MSG_ERROR` (server → client; reserved)

**ImageInfo payload:**

```c
typedef struct {
  char     filename[MAX_FILENAME];
  uint32_t total_size;     // network order
  uint32_t total_chunks;   // network order
  uint8_t  processing_type;// 1=histogram, 2=color, 3=both
  char     format[10];     // "jpg","jpeg","png","gif"
} ImageInfo;
```

## Logging

* File: `assets/log.txt`
* Format: `[YYYY-MM-DD HH:MM:SS] <message>`
* Sample events:

  * Server start & listen
  * Connection accept/close
  * UUID issuance on HELLO
  * IMAGE\_INFO summary
  * IMAGE\_COMPLETE summary

## Makefile Targets

* `make` – Build the server
* `make setup` – Create assets and log file
* `make rebuild` – Clean + setup + build
* `make clean` – Remove build artifacts
* `make help` – List targets

## Troubleshooting

* **Client connects but transfer fails**: Ensure client `protocol` is `"http"` (server is TCP-only right now).
* **No files in `assets/incoming/`**: Check `assets/log.txt` to confirm `IMAGE_INFO` and `IMAGE_COMPLETE` were received.
* **Port already in use**: Stop prior instances or change the client/server port consistently.
* **UUID errors**: Verify `uuid-dev` / `libuuid-devel` is installed and linked.

## Roadmap

* [ ] Server-side TLS (OpenSSL) to support `"https"` from the client
* [ ] Multi-client concurrency (thread-per-connection or event loop)
* [ ] Queue by size (smallest-first) for processing stage
* [ ] Daemonization (SysVinit or systemd unit) with `start|stop|status|restart`
* [ ] Processing modules: histogram equalization & color classification
* [ ] Final ACK after `IMAGE_COMPLETE`

## License

Developed for the Systems Operations course. All rights reserved.