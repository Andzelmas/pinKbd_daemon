#compiler name
CC=gcc
#name of the binary filename
FILE=pinKbd_daemon
#additional libraries
LIBS=-lgpiod
MAIN_SRC=pinKbd_daemon.c

#Remote dir for the source code
PI_DIR = ~/Audio/Source/pinKbd_daemon/

create_pinKbd_daemon:
	$(CC) -g -x c -o $(FILE) $(MAIN_SRC) $(LIBS)
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

.PHONY: rsync_src
pi_build: rsync_src
	ssh pi_daw make -C $(PI_DIR)
pi_run:
	ssh pi_daw -t "cd $(PI_DIR) && sudo ./pinKbd_daemon"
pi_run_valgrind:
	ssh pi_daw -t "cd $(PI_DIR) && sudo valgrind --leak-check=full --log-file=val_log ./pinKbd_daemon"
rsync_src:
	rsync -va * pi_daw:$(PI_DIR)
