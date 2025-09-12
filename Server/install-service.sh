#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# ImageService - Instalador de servicio systemd
# Requisitos previos:
#   - Ejecutar desde la raíz del proyecto (donde está ./image-server y ./assets/)
#   - El binario debe estar compilado (usa: ./setup.sh --build)
#
# Realiza:
#   1) Crea usuario/grupo 'imageserver' (si no existe)
#   2) Crea /opt/ImageServer y /etc/ImageServer
#   3) Copia binario a /usr/local/bin/
#   4) Copia assets a /opt/ImageServer/ (idempotente)
#   5) Copia config a /etc/ImageServer/config.json (si no existe; si existe, respeta)
#   6) Ajusta permisos
#   7) Instala unidad systemd (Type=simple, foreground)
#   8) daemon-reload + enable + start + status
# ------------------------------------------------------------

SERVICE_NAME="ImageService"
SERVICE_USER="imageserver"
SERVICE_GROUP="imageserver"

BIN_SRC="./image-server"
BIN_DST="/usr/local/bin/image-server"

ASSETS_SRC="./assets"
APP_DIR="/opt/ImageServer"
CONFIG_DIR="/etc/ImageServer"
CONFIG_DST="${CONFIG_DIR}/config.json"

UNIT_PATH="/etc/systemd/system/${SERVICE_NAME}.service"

# ---------- Helpers ----------
need_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Este script debe ejecutarse con sudo/root." >&2
    exit 1
  fi
}

check_prereqs() {
  if ! command -v systemctl >/dev/null 2>&1; then
    echo "No se encontró systemd/systemctl en este sistema." >&2
    exit 1
  fi
  if [[ ! -x "${BIN_SRC}" ]]; then
    echo "No se encontró el binario ${BIN_SRC}. Compila primero: ./setup.sh --build" >&2
    exit 1
  fi
  if [[ ! -d "${ASSETS_SRC}" ]]; then
    echo "No se encontró el directorio ${ASSETS_SRC} con los assets." >&2
    exit 1
  fi
}

create_user_and_dirs() {
  # 1) Usuario/grupo de servicio
  if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd -r -s /usr/sbin/nologin "${SERVICE_USER}"
    echo "Usuario ${SERVICE_USER} creado."
  else
    echo "Usuario ${SERVICE_USER} ya existe, ok."
  fi

  # 2) Directorios
  mkdir -p "${APP_DIR}" "${CONFIG_DIR}"
  echo "Directorios ${APP_DIR} y ${CONFIG_DIR} listos."
}

install_binary_and_assets() {
  # 3) Binario
  install -m 0755 "${BIN_SRC}" "${BIN_DST}"
  echo "Binario copiado a ${BIN_DST}"

  # 4) Assets -> /opt/ImageServer
  #    - Copia completa si /opt/ImageServer/assets no existe
  #    - Si existe, actualiza contenidos, sin borrar extras
  if [[ ! -d "${APP_DIR}/assets" ]]; then
    cp -r "${ASSETS_SRC}" "${APP_DIR}/"
    echo "Assets copiados a ${APP_DIR}/assets"
  else
    cp -r "${ASSETS_SRC}/." "${APP_DIR}/assets/"
    echo "Assets actualizados en ${APP_DIR}/assets"
  fi

  # 5) Config -> /etc/ImageServer/config.json (solo si NO existe)
  if [[ ! -f "${CONFIG_DST}" ]]; then
    if [[ -f "${ASSETS_SRC}/config.json" ]]; then
      cp "${ASSETS_SRC}/config.json" "${CONFIG_DST}"
      echo "Config copiada a ${CONFIG_DST}"
    else
      # fallback: generar config mínima
      cat > "${CONFIG_DST}" <<'JSON'
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
      echo "Config generada en ${CONFIG_DST}"
    fi
  else
    echo "Config existente en ${CONFIG_DST} (se respeta, no se sobrescribe)."
  fi

  # 6) Permisos
  chown -R "${SERVICE_USER}:${SERVICE_GROUP}" "${APP_DIR}"
  chown "${SERVICE_USER}:${SERVICE_GROUP}" "${CONFIG_DST}"
  echo "Permisos aplicados a ${APP_DIR} y ${CONFIG_DST}"
}

write_systemd_unit() {
  # 7) Unidad systemd (foreground, Type=simple)
  cat > "${UNIT_PATH}" <<EOF
[Unit]
Description=${SERVICE_NAME} - Image processing server
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
ExecStart=${BIN_DST} --config ${CONFIG_DST} --foreground
WorkingDirectory=${APP_DIR}
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
Restart=on-failure
RestartSec=2
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

  echo "Unidad systemd escrita en ${UNIT_PATH}"
}

enable_and_start() {
  # 8) Recargar + habilitar + iniciar + estado
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}"
  systemctl start "${SERVICE_NAME}"
  systemctl status "${SERVICE_NAME}" --no-pager || true
}

# ---------- Main ----------
need_root
check_prereqs
create_user_and_dirs
install_binary_and_assets
write_systemd_unit
enable_and_start

echo ""
echo "Instalación completada."
echo "Binario:   ${BIN_DST}"
echo "App dir:   ${APP_DIR}"
echo "Config:    ${CONFIG_DST}"
echo "Servicio:  ${SERVICE_NAME} (systemd)"
echo ""
echo "Comandos útiles:"
echo "  sudo systemctl restart ${SERVICE_NAME}"
echo "  sudo systemctl status ${SERVICE_NAME}"
echo "  sudo journalctl -u ${SERVICE_NAME} -f"