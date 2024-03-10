#compiler name
CC=gcc
#name of the binary filename
FILE=pinKbd_daemon

MAIN_SRC=pinKbd_daemon.c

create_pinKbd_daemon:
	$(CC) -g -x c -o $(FILE) $(MAIN_SRC)
install:
	sudo cp pinKbd_daemon /usr/bin/
	mkdir -p ~/.config/systemd/user/
	cp pinKbd_daemon.service ~/.config/systemd/user/pinKbd_daemon.service
	systemctl --user start pinKbd_daemon.service
	systemctl --user enable pinKbd_daemon.service
uninstall:
	sudo rm /usr/bin/pinKbd_daemon
	systemctl --user stop pinKbd_daemon.service
	systemctl --user disable pinKbd_daemon.service
	rm ~/.config/systemd/user/pinKbd_daemon.service
clean_local:
	rm pinKbd_daemon
clean_all: clean_local uninstall
