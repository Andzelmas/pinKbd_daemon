# pinKbd_daemon
Daemon that converts Pisound-Micro and Raspberry Pi GPIO pin info to keyboard events

### Install
Need to be able sudo /usr/bin/pinKbd_daemon without a password.
Otherwise the service wont be able to access the /dev/uinput/

make

make install

### Uninstall
make uninstall

or make clean_all to remove the compiled file in the pinKbd_daemon directory too
