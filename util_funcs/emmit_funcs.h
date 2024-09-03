#pragma once
//convert input string (for ex KEY_W) to code
//for all possible code take a look at https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
//entered key shortcuts are up to Relatie axes
int app_emmit_convert_to_enum(const char* in_string);
//emmits a keypress per keybit in keybits. Val is what value to send (1 or 0)
//emmit_invert - if 1 the same keybits will be sent with inverted value after the first emmit (in essence sending press and release key signals)
int app_emmit_emmit_keypress(int uinput_fd, int* keybits, int keybit_size, int val, unsigned int emmit_invert);
//init the uinput_fd for the keypresses, keybits - possible keypresses that might be emmited later
//device_name is just a name for the virtual device, can be anything
int app_emmit_init_input(int* uinput_fd, const char* device_name, int* keybits, int keybit_size);
void app_emmit_clean(int uinput_fd);
