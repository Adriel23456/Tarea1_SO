# Tarea1_SO

Objetivo principal:
- Implementar un servicio(Daemon) secundario en alguna plataforma de Linux con el fin de brindar un servicio al usuario.

Sinopsis:
- La ejecucion de aplicaciones o procesos en segundo plano provee algunas ventajas en el uso de las mismas, por ejemplo, se puede utilizar como un WebServer. En este proyecto se desarrollara un Daemon Linux utilizando SysVinit o Systemd. El Daemon implementara la funcionalidad de un servidor web cuya funcion principal sera el procesamiento de imagenes. Se debera considerar lo siguiente:
- El servidor/cliente sera desarrollaro en el lenguaje de programacion C.
- Se utilizaran conceptos relacionados con procesos e hilos (socket, bind, listen).
- Para el intercambio de informacion entre los clientes y el servidor se utilizara el protocolo HTTP o TCP.

Desarrollo:
1-Servidor:
    - Modulo que realiza todo el procesamiento de los archivos.
    - Debera ser capaz de aceptar los archivos (imagenes de cualquier tamaño y formato jpg, jpeg, png y gif), considera obtener los valores matematicos PUROS de las imagenes y aplicar cualquier operacion a esos valores y luego recontruir el archivo, que se procesaran de acuerdo con el tamaño de los mismos, es decir, se procesara primero los archivos mas pequeños primeros (debe existir un hilo escuchando).
    - El servidor tendra dos funciones principales:
        - La primera es poder tomar una imagen y aplicarle un histograma de ecualizacion, el cual consiste en mejorar el contraste de los colores de una imagen. La imagen se guardara en una ruta predeterminada que se define en el archivo de configuracion.
        - La segunda funcion es clasificar imagenes de acuerdo con el color predominante en la misma, es decir, habra tres directorios con los nombres de 'verdes', 'rojas', 'azules, en los cuales se almacenaran las imagenes con mayor cantidad de color, segun corresponda.
    - El puerto que utilizara el servidor por defecto es 1717, sin embargo, este puede cambiar especificandolo en el archivo de configuracion. Cada vez que inicie el servidor actualizara dicho valor.
    - Existira un archivo de configuracion que estara en el directorio: '/etc/server/config.conf'. Tambien se debera crear un archivo de registro (log), donde se almacene la actividad del servidor, o sea, que guarde peticiones de los clientes a tal archivo, estado de la ejecucion y hora de la misma.
    - El servidor debe iniciar cuando arranca el sistema (asegurandose de incorporar todas las dependencias de red) y la implementacion del mismo quedara a diseño de los creadores, puede utilizar SYsVinit o Systemd service, se debe justificar la eleccion.
    - El nombre del servidor sera: ImageServer y debera implementar las funciones de start, stop, status y restart. Las cuales consisten en iniciar, parar, reiniciar (debe cargar los datos de configuracion) y ver el estado del servidor en cualquier momento desde la consola.
    - Se debe asegurar que el Servidor trabaje como se espera, con comprobacion usando el comando 'top' o 'ps' de que de verdad es un daemon.
    - El servidor debe ejecutarse en diferente maquina que el cliente, y por ende, el cliente se ejecutara en una maquina virtual o un contenedor.
=========================================
(Ejemplo de archivo de configuracion)
Puerto:1719
DirColores:/jason/carpeta1/carpeta2
DirHisto:/jason/dir1/dir2
DirLog:/carpeta1
=========================================

=========================================
(Ejemplo de ejecuciones del servidor)
------------------------------------
#service ImageService stop
Stopping ImageService... done

#/etc/init.d/ImageServer stop
Stopping ImageService... done

#systemctl stop ImageService
------------------------------------
#service ImageService restart
Restarting ImageService... done

#/etc/init.d/ImageServer restart
Restarting ImageService... done

#systemctl restart ImageService
------------------------------------
#service ImageService start
Starting ImageService... done

#/etc/init.d/ImageServer start
Starting ImageService... done

#systemctl start ImageService
------------------------------------
#service ImageService status
daemon: ImageService is running (pid 17039)

#/etc/init.d/ImageServer status
daemon: ImageService is running (pid 17039)

#systemctl status ImageService
------------------------------------
=========================================
2-Cliente:
    - Es una aplicacion simple, donde la principal idea es un mecanismo para que el usuario elija la imagen que desea analizar y con ello enviarla al servidor para que proceda con el filtrado de la misma.
    - El cliente enviara imagenes de manera secuencial hasta que el usuario escriba 'Exit'. No se requiere una interfaz grafica, pero se debe considerar la configuracion basica como ip y puerto y otros parametros.
3-Indicaciones generales:
    - Todo se realizara en el lenguaje de programacion de C y se puede utilizar cualquier biblioteca de la misma.
    - Se debe implementar en linux
    - El cliente debe ser ejecutado en una maquina virtual (PERO si se ejecuta el servidor en una maquina virtual en la nube, entonces, el cliente simplemente se ejecuta localmente)
    - Sobre la ecualizacion del histograma, este toma en cuenta el histograma de los colores, para posteriormente calcular la frecuencia y con ello hacer la transformacion de cada pixel. Para el mapeo del nuevo pixel se puede utilizar el siguiente mapeo:
    Nuevo_pixel = FrecuenciaAcumulada(pixel) * 255 / (ancho*alto)


Plan de desarrollo:
1- Montar la aplicacion grafica del cliente la cual nos permita preparar un set de MULTIPLES imagenes listas.
2- Hacer que nuestra aplicacion de cliente extraiga las propiedades binarias de nuestras imagenes y que las tenga listas en memoria para mandar hacer el servidor.
3- Hacer un cliente/servidor y un protocolo que nos permita mandar UNA imagen a la vez, mandandolo por trozos de su parte binaria.
4- Tener la parte del servidor la cual simplemente va a estar siempre listo para recibir las imagenes y asegurarse que sea capaz de recibir una imagen a la vez secuencialmente. SIEMPRE ESCUCHANDO.
5- Montarle al servidor la capacidad para aplicarle a las imagenes sus dos funciones, histograma y clasificar
6- Montarle al servidor la funcionalidad de que sea un servicio de tipo daemon en Linux y sus funciones extras
7- Hacer el testing con todo 'local' pero asegurandose que todo movimiento de informacion SIEMPRE sea por 'https:1717' y que el servidor si sea un daemon
8- Montar el servidor en una maquina virtual
9- Modificar la configuracion del programa de cliente para que mande las imagenes por la red a la maquina virtual en la nube

*Al mismo tiempo:*
1- Investigar y justificar las decisiones sobre librerias y sobre como implementar un daemon en Linux
2- Preparacion general de la documentacion y de las preguntas que vamos a estar realizando