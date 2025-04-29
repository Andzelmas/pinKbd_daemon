# pinKbd_daemon
Daemon that converts chip for Raspberry Pi GPIO pin info to keyboard events.

The name of the chip can be entered in the pin_config.json filename. So this can be used for the Rpi own GPIO or some other brand.

### Install
Need to be able sudo /usr/bin/pinKbd_daemon without a password.
Otherwise the service wont be able to access the /dev/uinput/

make

make install

make install will (among other things) copy the config file to /etc/pin_config.json. Modify this file directly or the pin_config.json file in the download directory and copy it to /etc/

### Uninstall
make uninstall

or make clean_all to remove the compiled file in the pinKbd_daemon directory too

### Shortcuts
To add lines and what shortcuts they emmit modify pin_config.json (it has a simple default configuration)

For possible keys to emmit take a look at https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

For encoders there must be two lines. First line keypress array is for the clock-wise rotation of the encoder, and the second line keypress array is for the counter clock-wise rotation.

For buttons just one line with a single keypress array for it.
