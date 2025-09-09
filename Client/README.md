Aqui se explicara la instalacion y ejecucion total del cliente ya sea todo local o con el servidor en la nube.

#TODO SE TRABAJARA EN UBUNTU

#Comandos de instalacion de dependencias
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev

#Comandos de ejecucion del proyecto
gcc hello.c `pkg-config --cflags --libs gtk4` -o hello
./hello

