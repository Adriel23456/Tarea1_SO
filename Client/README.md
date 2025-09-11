# Image Processing Client

A modern GTK4-based client application for the Image Processing Server daemon. This client provides a clean, efficient interface for loading, managing, and **sending images over the network (TCP / HTTP-like framing)** to the server for processing.

## Features

- **Modern GTK4 Interface**: Clean and responsive UI with modern design elements
- **Image Management**: Load and manage multiple images (jpg, jpeg, png, gif)
- **Server Configuration**: Easy-to-edit JSON configuration for server connection
- **Sequential Upload**: Sends images **one at a time**, in the order they were loaded
- **Chunked Transfer**: Configurable chunk size for robust transfers over TCP
- **Processing Headers**: Sends processing intent (histogram / color_classification / both)
- **Retries & Timeouts**: Connection retry policy and timeout settings
- **Progress Feedback**: Live text progress messages during upload

## Prerequisites

- Linux-based operating system (Ubuntu, Debian, Fedora, Arch, etc.)
- GCC, pkg-config
- GTK4 development libraries
- **json-c**, **uuid**, **OpenSSL (client-side TLS optional)**

### Packages

**Ubuntu/Debian**
```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev libjson-c-dev libssl-dev uuid-dev
````

**Fedora**

```bash
sudo dnf install -y gcc gtk4-devel pkg-config json-c-devel openssl-devel libuuid-devel
```

**Arch**

```bash
sudo pacman -S --noconfirm base-devel gtk4 pkg-config json-c openssl util-linux-libs
```

> ⚠️ The server currently listens on **plain TCP** (no TLS). The client supports `"protocol": "https"`, but **use `"http"` for now** until server-side TLS is added.

## Installation

### Quick Setup

```bash
# Install deps (Ubuntu/Debian shortcut)
make install-deps

# Create assets and default files (non-destructive)
make setup

# Build
make

# Run
./image-client
```

### Manual Build (without Makefile)

```bash
mkdir -p assets
gcc src/main.c src/gui.c src/dialogs.c src/network.c \
    `pkg-config --cflags --libs gtk4 json-c` -luuid -lssl -lcrypto \
    -Wall -Wextra -g -o image-client
./image-client
```

## Project Structure

```
image-client/
├── src/
│   ├── main.c           # Application entry point
│   ├── gui.c            # Main GUI implementation
│   ├── gui.h            # GUI header file
│   ├── dialogs.c        # Dialog implementations
│   ├── dialogs.h        # Dialog headers
│   ├── network.c        # Network (TCP) implementation + optional TLS client
│   └── network.h        # Network interface + NetConfig
├── assets/
│   ├── connection.json  # Client & server connection config
│   └── credits.txt      # Application credits
├── Makefile
└── README.md
```

## Configuration

`assets/connection.json` (created by `make setup` if missing):

```json
{
  "server": {
    "host": "localhost",
    "port": 1717,
    "protocol": "http"
  },
  "client": {
    "chunk_size": 65536,
    "connect_timeout": 10,
    "max_retries": 3,
    "retry_backoff_ms": 500
  }
}
```

You can edit it directly from the app: **Configuration → Save**.

## Usage

### Loading Images

1. Click **Load** to select one or more images (jpg, jpeg, png, gif).
2. They’ll appear in the list with their file sizes.

### Sending Images

1. Click **Send Images**.
2. Select processing type:

   * **Histogram Equalization**
   * **Color Classification**
   * **Both**
3. The client connects to the server, performs a handshake (server issues a UUID), and uploads the image in **binary chunks**.
4. After completion, the client notifies the server with the **image format** (jpg/png/jpeg/gif).

> Images are sent **sequentially**: one completes before the next begins.

### Credits

Click **Credits**.

### Exit

Click **Exit** or close the window.

## Wire Protocol (overview)

All messages have a fixed header:

```c
typedef struct {
  uint8_t  type;        // MessageType
  uint32_t length;      // payload length (network order)
  char     image_id[37];// UUID string or "" (null-terminated)
} MessageHeader;
```

Main flow per image:

1. **Client → Server**: `MSG_HELLO`
2. **Server → Client**: `MSG_IMAGE_ID_RESPONSE` (header.image\_id = UUID)
3. **Client → Server**: `MSG_IMAGE_INFO` (filename, total\_size, total\_chunks, processing\_type, format)
4. **Client → Server**: multiple `MSG_IMAGE_CHUNK` with raw bytes
5. **Client → Server**: `MSG_IMAGE_COMPLETE` (payload = `"jpg"`/`"png"`/`"jpeg"`/`"gif"`)

TCP guarantees order & integrity; no per-chunk ACK necessary.

## Makefile Targets

* `make` – Build the app
* `make run` – Build and run
* `make clean` – Remove build files
* `make setup` – Create assets & defaults (non-destructive)
* `make rebuild` – Clean + setup + build
* `make install-deps` – Install dependencies (Ubuntu/Debian)
* `make help` – Show targets

## Troubleshooting

* **Connection failed**: Ensure the server is running on `host:port` and `protocol` is `"http"` (for now).
* **TLS handshake mismatch**: If the server has no TLS, use `"protocol": "http"`.
* **Undefined references to SSL/uuid**: Install `libssl-dev` and `uuid-dev` and ensure `-lssl -lcrypto -luuid` in the Makefile.
* **GTK errors**: `pkg-config --modversion gtk4` must return a valid version.

## License

Developed for the Systems Operations course. All rights reserved.