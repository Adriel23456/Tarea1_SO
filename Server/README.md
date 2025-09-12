# Image Processing Server

Concurrent **image processing server** (TCP/TLS) with a framed binary protocol, per-connection threads, and a **priority scheduler** (small files first). Images are received **fully in memory** and then processed by a background worker:

* **Dominant color classification** (red/green/blue)
* **Histogram equalization** (contrast enhancement)
* **Static PNG/JPG/JPEG** and **animated GIF** (per-frame processing and GIF writing)

Outputs are written to:

* `assets/colors/<red|green|blue>/`
* `assets/histogram/`

---

## Features

* **Thread-per-connection** (pthreads)
* **Dedicated worker** with **min-heap** (size-ascending priority)
* **TCP or TLS** (OpenSSL; optional self-signed certs)
* **JSON configuration** (`assets/config.json`)
* **Thread-safe logging** to `assets/log.txt`
* **Framed protocol**: fixed header + payload
* **Animated GIF**: decode with `stb_image`, write with `gif.h`

---

## Requirements

* Linux
* `gcc`, `make`
* Dev packages:

  * Ubuntu/Debian: `build-essential uuid-dev libssl-dev libjson-c-dev`
  * Fedora: `gcc make libuuid-devel openssl-devel json-c-devel`
  * Arch: `base-devel util-linux-libs openssl json-c`

---

## Layout

```
Server/
├── assets/
│   ├── histogram/
│   ├── colors/{red,green,blue}/
│   ├── tls/
│   ├── log.txt
│   └── config.json
├── src/
│   ├── main.c, server.c/.h, connection.c/.h, scheduler.c/.h
│   ├── image_processing.c/.h, gif_processing.c/.h
│   ├── config.c/.h, logging.c/.h, utils.c/.h, daemon.c/.h
│   ├── protocol.h, stb_image*.h, gif.h
├── Makefile
├── setup.sh
└── install-service.sh
```

---

## Build

### Option 1: Interactive script

```bash
./setup.sh
```

Guides you through installing deps, initializing `assets/`, generating TLS, and building.

### Option 2: Non-interactive

```bash
./setup.sh --install-deps
./setup.sh --init
./setup.sh --gen-tls-local && ./setup.sh --enable-tls   # optional
./setup.sh --build
```

### Option 3: Makefile

```bash
make setup
make
make rebuild
make clean
```

---

## Configuration (`assets/config.json`)

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

* Change **port**: `server.port`
* Enable **TLS**: `server.tls_enabled = 1` (or `./setup.sh --enable-tls`)
* Adjust **output paths** as needed

---

## Run in console (foreground)

```bash
./image-server
```

* With `tls_enabled=1`, it listens over TLS using `assets/tls/server.crt` and `server.key`.
* Logs are written to `assets/log.txt`.

---

## Install as a systemd service

**Automated installation**:

```bash
sudo ./install-service.sh
```

This installs the binary at `/usr/local/bin/image-server`, assets at `/opt/ImageServer/assets`, config at `/etc/ImageServer/config.json`, and creates the `ImageService.service` unit.

**Manage the service**:

```bash
sudo systemctl stop ImageService
sudo systemctl restart ImageService
sudo systemctl start ImageService
sudo systemctl status ImageService
```

**Service logs**:

```bash
sudo journalctl -u ImageService -f
```

---

## Verify the process with `top` / `ps`

Get PID(s):

```bash
pidof image-server
ps aux | grep image-server
ps -p "$(pidof image-server)" -o pid,ppid,cmd
```

Monitor in real time:

```bash
top -p "$(pidof image-server)"
```

(Press `q` to quit `top`.)

---

## Inspect outputs and logs

List classification and histogram folders:

```bash
ls -lh /opt/ImageServer/assets/colors/red
ls -lh /opt/ImageServer/assets/colors/green
ls -lh /opt/ImageServer/assets/colors/blue
ls -lh /opt/ImageServer/assets/histogram
```

To eliminate the content of this files we have:

```bash
sudo systemctl stop ImageService

sudo find /opt/ImageServer/assets/colors/red \
           /opt/ImageServer/assets/colors/green \
           /opt/ImageServer/assets/colors/blue \
           /opt/ImageServer/assets/histogram \
     -mindepth 1 -exec rm -rf {} +

sudo find /opt/ImageServer/assets/colors/red \
          /opt/ImageServer/assets/colors/green \
          /opt/ImageServer/assets/colors/blue \
          /opt/ImageServer/assets/histogram -mindepth 1 | wc -l

sudo systemctl start ImageService
```

Open an output file (GUI environment):

```bash
# Open the first red-classified image with the system's default viewer
xdg-open "/opt/ImageServer/assets/colors/red/$(ls -1 /opt/ImageServer/assets/colors/red | head -n1)" 2>/dev/null || true

# Or open a specific file (replace FILE_NAME)
xdg-open "/opt/ImageServer/assets/colors/red/FILE_NAME"
```

View the server log file:

```bash
tail -f /opt/ImageServer/assets/log.txt
```

---

## Protocol (summary)

**Header**:

```c
typedef struct {
  uint8_t  type;         // MessageType
  uint32_t length;       // payload (big-endian)
  char     image_id[37]; // UUID
} MessageHeader;
```

**Messages**:

```c
typedef enum {
  MSG_HELLO = 1,
  MSG_IMAGE_ID_REQUEST,
  MSG_IMAGE_ID_RESPONSE,
  MSG_IMAGE_INFO,
  MSG_IMAGE_CHUNK,
  MSG_IMAGE_COMPLETE,
  MSG_ACK,
  MSG_ERROR
} MessageType;
```

**`ImageInfo` payload**:

```c
#define MAX_FILENAME 256
typedef struct {
  char     filename[MAX_FILENAME];
  uint32_t total_size;      // big-endian
  uint32_t total_chunks;    // big-endian
  uint8_t  processing_type; // 1=HIST, 2=COLOR, 3=BOTH
  char     format[10];      // "jpg","jpeg","png","gif"
} ImageInfo;
```

---

## Troubleshooting

* **TLS issues**: verify `assets/tls/server.crt` and `server.key`, and `tls_enabled=1`.
* **No outputs**: ensure the client sends `processing_type > 0`; check `assets/log.txt`.
* **`bind: Address already in use`**: another process is using the port; change `server.port` or stop that process.
* **Permissions**: if the service cannot write to `/opt/ImageServer/assets`, fix ownership:

  ```bash
  sudo chown -R imageserver:imageserver /opt/ImageServer
  ```

---

## License

Academic project for **Operating Systems** coursework. All rights reserved.