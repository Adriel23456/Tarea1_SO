#!/bin/bash
set -e

################################################################################
# Image Server - Setup Script (with automatic TLS key generation)
# - Installs deps
# - Creates assets structure + default config.json (all in assets/)
# - Generates TLS cert/key for localhost OR a custom host/IP (self-signed)
# - Enables/disables tls_enabled in assets/config.json
# - Builds the server
#
# Usage (non-interactive examples):
#   ./setup.sh --install-deps
#   ./setup.sh --init
#   ./setup.sh --gen-tls-local
#   ./setup.sh --gen-tls mydomain.com
#   ./setup.sh --gen-tls 203.0.113.10
#   ./setup.sh --enable-tls
#   ./setup.sh --disable-tls
#   ./setup.sh --build
#   ./setup.sh --full-local         # deps + init + TLS localhost + enable + build
#   ./setup.sh --full-custom myhost # deps + init + TLS for host/IP + enable + build
################################################################################

# ----------------------------- OS detection -----------------------------------
if [ -f /etc/os-release ]; then
  . /etc/os-release
  OS_ID="$ID"
  OS_NAME="$NAME"
else
  OS_ID="unknown"
  OS_NAME="unknown"
fi

echo "=== Server setup ==="
echo "Detected OS: $OS_NAME"

# ----------------------------- Constants --------------------------------------
CONFIG_PATH="assets/config.json"
TLS_DIR_DEFAULT="assets/tls"
LOG_FILE_DEFAULT="assets/log.txt"

# ----------------------------- Helpers ----------------------------------------
is_ip() {
  # very simple IPv4 check
  local ip="$1"
  [[ "$ip" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]
}

have_jq() {
  command -v jq >/dev/null 2>&1
}

ensure_dirs() {
  mkdir -p assets \
           assets/histogram \
           assets/colors/red \
           assets/colors/green \
           assets/colors/blue \
           assets/tls
  [ -f "$LOG_FILE_DEFAULT" ] || echo "== Image Server Log ==" > "$LOG_FILE_DEFAULT"
}

write_default_config_if_missing() {
  if [ ! -f "$CONFIG_PATH" ]; then
    cat > "$CONFIG_PATH" <<'JSON'
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
JSON
    echo "Created $CONFIG_PATH"
  else
    echo "$CONFIG_PATH already exists, skipping..."
  fi
}

update_config_tls_enabled() {
  local value="$1" # 0 or 1
  if [ ! -f "$CONFIG_PATH" ]; then
    echo "Config not found. Creating default one..."
    write_default_config_if_missing
  fi

  if have_jq; then
    tmp="$(mktemp)"
    jq ".server.tls_enabled = $value" "$CONFIG_PATH" > "$tmp" && mv "$tmp" "$CONFIG_PATH"
    echo "Updated tls_enabled = $value in $CONFIG_PATH"
  else
    # Fallback: simple in-place replace (expects tls_enabled line to exist)
    # This is a best-effort replacement.
    sed -i -E 's/"tls_enabled"[[:space:]]*:[[:space:]]*[01]/"tls_enabled": '"$value"'/g' "$CONFIG_PATH"
    echo "Updated tls_enabled = $value in $CONFIG_PATH (sed fallback)"
  fi
}

install_dependencies() {
  case "$OS_ID" in
    ubuntu|debian)
      sudo apt update
      sudo apt install -y build-essential uuid-dev libssl-dev libjson-c-dev
      ;;
    fedora)
      sudo dnf install -y gcc libuuid-devel openssl-devel json-c-devel make
      ;;
    arch)
      sudo pacman -Syu --noconfirm base-devel util-linux-libs openssl json-c
      ;;
    *)
      echo "Install manually: gcc, make, libuuid-dev, libssl-dev, libjson-c-dev"
      ;;
  esac
  echo "Dependencies installed successfully!"
}

init_project() {
  ensure_dirs
  write_default_config_if_missing
  echo "Project directories and config are ready."
}

openssl_supports_addext() {
  # Returns 0 if -addext is supported
  openssl req -help 2>&1 | grep -q -- "-addext"
}

gen_selfsigned_cert_local() {
  # Generates localhost cert in assets/tls
  mkdir -p "$TLS_DIR_DEFAULT"
  local KEY="$TLS_DIR_DEFAULT/server.key"
  local CRT="$TLS_DIR_DEFAULT/server.crt"

  if openssl_supports_addext; then
    openssl req -x509 -newkey rsa:2048 \
      -keyout "$KEY" \
      -out "$CRT" \
      -days 365 -nodes \
      -subj "/CN=localhost" \
      -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
  else
    # Fallback with a temporary config file
    local CNF="$(mktemp)"
    cat > "$CNF" <<'CFG'
[ req ]
default_bits       = 2048
distinguished_name = dn
x509_extensions    = v3_req
prompt             = no

[ dn ]
CN = localhost

[ v3_req ]
subjectAltName = @alt_names

[ alt_names ]
DNS.1 = localhost
IP.1  = 127.0.0.1
CFG
    openssl req -x509 -newkey rsa:2048 \
      -keyout "$KEY" \
      -out "$CRT" \
      -days 365 -nodes -config "$CNF"
    rm -f "$CNF"
  fi

  chmod 600 "$KEY" || true
  echo "Generated TLS key/cert for localhost at $TLS_DIR_DEFAULT"
}

gen_selfsigned_cert_for_host() {
  # $1 = host or IP
  local HOST="$1"
  mkdir -p "$TLS_DIR_DEFAULT"
  local KEY="$TLS_DIR_DEFAULT/server.key"
  local CRT="$TLS_DIR_DEFAULT/server.crt"

  if is_ip "$HOST"; then
    # IP
    if openssl_supports_addext; then
      openssl req -x509 -newkey rsa:2048 \
        -keyout "$KEY" \
        -out "$CRT" \
        -days 365 -nodes \
        -subj "/CN=$HOST" \
        -addext "subjectAltName=IP:$HOST"
    else
      local CNF="$(mktemp)"
      cat > "$CNF" <<CFG
[ req ]
default_bits       = 2048
distinguished_name = dn
x509_extensions    = v3_req
prompt             = no

[ dn ]
CN = $HOST

[ v3_req ]
subjectAltName = @alt_names

[ alt_names ]
IP.1  = $HOST
CFG
      openssl req -x509 -newkey rsa:2048 \
        -keyout "$KEY" \
        -out "$CRT" \
        -days 365 -nodes -config "$CNF"
      rm -f "$CNF"
    fi
  else
    # DNS name
    if openssl_supports_addext; then
      openssl req -x509 -newkey rsa:2048 \
        -keyout "$KEY" \
        -out "$CRT" \
        -days 365 -nodes \
        -subj "/CN=$HOST" \
        -addext "subjectAltName=DNS:$HOST"
    else
      local CNF="$(mktemp)"
      cat > "$CNF" <<CFG
[ req ]
default_bits       = 2048
distinguished_name = dn
x509_extensions    = v3_req
prompt             = no

[ dn ]
CN = $HOST

[ v3_req ]
subjectAltName = @alt_names

[ alt_names ]
DNS.1 = $HOST
CFG
      openssl req -x509 -newkey rsa:2048 \
        -keyout "$KEY" \
        -out "$CRT" \
        -days 365 -nodes -config "$CNF"
      rm -f "$CNF"
    fi
  fi

  chmod 600 "$KEY" || true
  echo "Generated TLS key/cert for '$HOST' at $TLS_DIR_DEFAULT"
}

enable_tls_and_info() {
  update_config_tls_enabled 1
  echo "TLS enabled in $CONFIG_PATH. Place/use certs in $(jq -r '.server.tls_dir' "$CONFIG_PATH" 2>/dev/null || echo "$TLS_DIR_DEFAULT")"
}

disable_tls_and_info() {
  update_config_tls_enabled 0
  echo "TLS disabled in $CONFIG_PATH"
}

build_project() {
  if [ -f Makefile ]; then
    make clean
    make setup
    make
    if [ -f image-server ]; then
      echo "✓ Build successful!"
      echo "Run with: ./image-server"
    else
      echo "✗ Build failed. Please check errors above."
      exit 1
    fi
  else
    echo "Makefile not found. Ensure files are present."
    exit 1
  fi
}

show_help() {
cat <<EOF
Usage: $0 [OPTION] [HOST_OR_IP]

Options:
  --install-deps           Install system dependencies
  --init                   Create assets structure and default config.json
  --gen-tls-local          Generate self-signed TLS key/cert for localhost (SAN: localhost, 127.0.0.1)
  --gen-tls <host|ip>      Generate self-signed TLS key/cert for a custom host or IP
  --enable-tls             Set "tls_enabled": 1 in assets/config.json
  --disable-tls            Set "tls_enabled": 0 in assets/config.json
  --build                  Build the project (make clean/setup/build)
  --full-local             Deps + init + gen TLS (localhost) + enable TLS + build
  --full-custom <host|ip>  Deps + init + gen TLS (custom) + enable TLS + build
  --help                   Show this help

Interactive (no args): prompts a menu.
EOF
}

interactive_menu() {
  echo ""
  echo "Quick Setup for Image Server"
  echo ""
  echo "Choose an option:"
  echo "1) Full setup (deps + init + TLS for localhost + enable TLS + build)"
  echo "2) Full setup (deps + init + TLS for custom host/IP + enable TLS + build)"
  echo "3) Install dependencies only"
  echo "4) Initialize project (dirs + default config)"
  echo "5) Generate TLS for localhost and enable TLS"
  echo "6) Generate TLS for custom host/IP and enable TLS"
  echo "7) Build project only"
  echo "8) Exit"
  echo ""
  read -p "Enter choice [1-8]: " choice

  case "$choice" in
    1)
      install_dependencies
      init_project
      gen_selfsigned_cert_local
      enable_tls_and_info
      build_project
      ;;
    2)
      read -p "Enter host or IP for certificate (e.g., example.com or 203.0.113.10): " host
      install_dependencies
      init_project
      gen_selfsigned_cert_for_host "$host"
      enable_tls_and_info
      build_project
      ;;
    3)
      install_dependencies
      ;;
    4)
      init_project
      ;;
    5)
      init_project
      gen_selfsigned_cert_local
      enable_tls_and_info
      ;;
    6)
      read -p "Enter host or IP for certificate (e.g., example.com or 203.0.113.10): " host
      init_project
      gen_selfsigned_cert_for_host "$host"
      enable_tls_and_info
      ;;
    7)
      build_project
      ;;
    8)
      echo "Exiting..."
      exit 0
      ;;
    *)
      echo "Invalid choice"
      exit 1
      ;;
  esac
}

# ----------------------------- Main -------------------------------------------
case "${1:-}" in
  --install-deps)
    install_dependencies
    ;;
  --init)
    init_project
    ;;
  --gen-tls-local)
    init_project
    gen_selfsigned_cert_local
    ;;
  --gen-tls)
    if [ -z "$2" ]; then
      echo "Please provide a host or IP: $0 --gen-tls <host|ip>"
      exit 1
    fi
    init_project
    gen_selfsigned_cert_for_host "$2"
    ;;
  --enable-tls)
    update_config_tls_enabled 1
    ;;
  --disable-tls)
    update_config_tls_enabled 0
    ;;
  --build)
    build_project
    ;;
  --full-local)
    install_dependencies
    init_project
    gen_selfsigned_cert_local
    enable_tls_and_info
    build_project
    ;;
  --full-custom)
    if [ -z "$2" ]; then
      echo "Please provide a host or IP: $0 --full-custom <host|ip>"
      exit 1
    fi
    install_dependencies
    init_project
    gen_selfsigned_cert_for_host "$2"
    enable_tls_and_info
    build_project
    ;;
  --help)
    show_help
    ;;
  *)
    interactive_menu
    ;;
esac

echo ""
echo "================================================================"
echo "                        SETUP COMPLETE"
echo "================================================================"
echo "Config file: $CONFIG_PATH"
echo "TLS dir:     $TLS_DIR_DEFAULT"
echo "Run server:  ./image-server"
echo "Note: Client must use protocol \"https\" if TLS is enabled."