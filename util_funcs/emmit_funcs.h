#pragma once
typedef void APP_EMMIT_KEYPRESS;
//convert input string (for ex KEY_W) to code
//for all possible code take a look at https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
//entered key shortcuts are up to Relatie axes
int app_emmit_convert_to_enum(const char* in_string);
//emmits a keypress per keybit in the keypress struct.
int app_emmit_emmit_keypress(int uinput_fd, APP_EMMIT_KEYPRESS* keypress);
//initiates the emmit_keypress struct and returns it as void struct app_emmit_keypress
APP_EMMIT_KEYPRESS* app_emmit_init_keypress(int* key_nums, int key_size, int invert);
//init the uinput_fd for the keypresses, keybits - possible keypresses that might be emmited later
//device_name is just a name for the virtual device, can be anything
int app_emmit_init_input(int* uinput_fd, const char* device_name, int* keybits, int keybit_size);
void app_emmit_clean(int uinput_fd);
//cleans the emmit_keypress struct
void app_emmit_clean_keypress(APP_EMMIT_KEYPRESS* keypress);
