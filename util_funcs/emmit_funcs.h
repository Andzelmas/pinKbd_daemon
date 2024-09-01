#pragma once
//convert input string (for ex KEY_W) to code
//for all possible code take a look at https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
//entered key shortcuts are up to Relatie axes
int app_emmit_convert_to_enum(const char* in_string);
