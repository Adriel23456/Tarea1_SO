# Tarea1\_SO

## Objetivo principal

Implementar un servicio (*daemon*) en una plataforma Linux que brinde un servicio al usuario.

## Sinopsis

La ejecución de aplicaciones o procesos en segundo plano ofrece ventajas prácticas (p. ej., un **servidor web** o de procesamiento). En este proyecto se desarrollará un *daemon* en Linux utilizando **SysVinit** o **systemd**. El daemon expondrá la funcionalidad de un **servidor de procesamiento de imágenes**. Se deberá considerar:

* El **servidor/cliente** será desarrollado en **C**.
* Se utilizarán conceptos de **procesos** e **hilos** (socket, bind, listen).
* Para el intercambio de información entre clientes y servidor se utilizará **HTTP** o **TCP**.

---

## Desarrollo

### 1. Servidor

* Módulo que realiza **todo el procesamiento de archivos**.
* Debe **aceptar imágenes** de cualquier tamaño en formatos **jpg, jpeg, png y gif**; obtener los valores numéricos puros de la imagen, **aplicar operaciones** y **reconstruir** el archivo. El procesamiento será **prioritario por tamaño** (primero los archivos más pequeños). Debe existir **un hilo en escucha** permanente.
* Funciones principales:

  1. **Ecualización de histograma**: mejora el contraste de la imagen. La salida se guarda en una ruta predefinida en el archivo de configuración.
  2. **Clasificación por color predominante**: tres directorios *verdes*, *rojas* y *azules* almacenarán las imágenes según su color dominante.
* **Puerto por defecto**: `1717` (configurable en el archivo de configuración; el servidor debe leerlo en cada inicio).
* **Archivo de configuración**: residirá en el directorio indicado (ej.: `/etc/server/config.conf`). Debe existir además un **archivo de registro (log)** con las peticiones, estado de ejecución y marcas de tiempo.
* **Inicio al arrancar el sistema** (incorporando dependencias de red). La implementación puede ser con **SysVinit** o **systemd** (se debe **justificar la elección**).
* **Nombre del servicio**: `ImageServer`. Debe implementar `start`, `stop`, `status` y `restart` (iniciar, detener, **reiniciar recargando configuración** y ver el estado).
* **Verificación de daemon**: comprobar con `top` o `ps` que el proceso corre en segundo plano.
* **Despliegue**: el servidor debe ejecutarse en una máquina diferente al cliente (el cliente puede correr en una VM o contenedor).

#### Ejemplo de archivo de configuración

```text
Puerto:1719
DirColores:/jason/carpeta1/carpeta2
DirHisto:/jason/dir1/dir2
DirLog:/carpeta1
```

#### Ejemplo de ejecuciones del servidor

```bash
# SysV / service
service ImageService stop
service ImageService restart
service ImageService start
service ImageService status

# Script SysV directo
/etc/init.d/ImageServer stop
/etc/init.d/ImageServer restart
/etc/init.d/ImageServer start
/etc/init.d/ImageServer status

# systemd
sudo systemctl stop ImageService
sudo systemctl restart ImageService
sudo systemctl start ImageService
sudo systemctl status ImageService
```

---

### 2. Cliente

* Aplicación simple: permite al usuario **elegir la imagen** a analizar y **enviarla al servidor** para su procesamiento.
* Envía imágenes **secuencialmente** hasta que el usuario escriba `Exit`. No se requiere interfaz gráfica; sí **configuración básica** (IP, puerto y parámetros relevantes).

---

### 3. Indicaciones generales

* Todo se implementará en **C**, pudiendo usar **bibliotecas** del lenguaje/entorno.
* Se implementará en **Linux**.
* El **cliente** debe ejecutarse en una **máquina virtual** (o local si el servidor corre en una VM en la nube).
* Sobre la **ecualización de histograma**: se toma el histograma de los canales de color, se calcula la **frecuencia acumulada** y se transforma cada píxel. Mapeo sugerido:

```text
Nuevo_pixel = FrecuenciaAcumulada(pixel) * 255 / (ancho * alto)
```

---

## Gestión del servicio

### Comandos principales (systemd)

```bash
sudo systemctl stop ImageService
sudo systemctl restart ImageService
sudo systemctl start ImageService
sudo systemctl status ImageService
```

### Verificación del proceso (daemon)

```bash
# Ver PIDs y comando
pidof image-server
ps aux | grep image-server
ps -p "$(pidof image-server)" -o pid,ppid,cmd

# Monitoreo en tiempo real
top -p "$(pidof image-server)"
```

---

## Exploración de directorios y logs

### Contenido de carpetas de clasificación e histogramas

```bash
ls -lh /ruta/a/colors/rojas
ls -lh /ruta/a/colors/verdes
ls -lh /ruta/a/colors/azules
ls -lh /ruta/a/histogramas
```

### Abrir un archivo directamente (entorno gráfico)

```bash
# Abre el primer archivo listado en 'rojas' con el visor por defecto
xdg-open "/ruta/a/colors/rojas/$(ls -1 /ruta/a/colors/rojas | head -n1)" 2>/dev/null || true

# O abrir un archivo específico
xdg-open "/ruta/a/colors/rojas/NOMBRE_ARCHIVO"
```

### Ver el archivo de log del servidor

```bash
tail -f /ruta/a/log.txt
```

> Ajusta las rutas anteriores a las definidas en tu archivo de configuración.

---

## Plan de desarrollo

1. **Cliente (UI/CLI):** preparar un set de múltiples imágenes.
2. **Cliente (memoria):** extraer los **datos binarios** de cada imagen y mantenerlos listos en memoria.
3. **Protocolo cliente/servidor:** enviar **una imagen a la vez**, por **trozos** (chunks).
4. **Servidor (escucha):** aceptar conexiones y **recibir secuencialmente**; siempre **en escucha**.
5. **Servidor (procesamiento):** implementar **histograma** y **clasificación por color**.
6. **Servicio en Linux:** desplegar como **daemon** con funciones `start/stop/status/restart`.
7. **Pruebas locales:** todo “local”, asegurando transferencia por `https:1717` (si TLS) y ejecución como daemon.
8. **Despliegue:** montar el servidor en una **máquina virtual**.
9. **Cliente → nube:** configurar el cliente para enviar imágenes hacia la VM remota.

*En paralelo:*

1. **Investigación y justificación**: bibliotecas empleadas y diseño de daemon en Linux.
2. **Documentación**: preparación de la documentación y del banco de **preguntas**.