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

### Shortcuts
To add lines and what shortcuts they emmit modify pin_config.json (it has a simple default configuration)

For possible keys to emmit take a look at https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

Entered key shortcuts are up to Relative axes
