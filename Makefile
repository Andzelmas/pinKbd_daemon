#compiler name
CC=gcc
#name of the binary filename
FILE=pinKbd_daemon

MAIN_SRC=pinKbd_daemon.c

create_pinKbd_daemon:
	$(CC) -g -x c -o $(FILE) $(MAIN_SRC)
