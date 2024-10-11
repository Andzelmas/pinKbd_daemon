#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gpiod.h>
#include <inttypes.h>
#include <dirent.h>
#include "util_funcs/json_funcs.h"
#include "util_funcs/emmit_funcs.h"
//size of the single buffer read, when reading a file to buffer
#define JSONFILESIZE 1024
//how many lines to read from pisound gpio for encoders
#define PISOUND_ENC_NUM_LINES 18
//how many lines to read from pisound gpio for buttons
#define PISOUND_BUT_NUM_LINES 17
//how many lines to read from the Raspberry Pi for buttons
#define RPI_BUT_NUM_LINES 5
//for button to register a push the timestamp difference of the values must be this ns or greater
#define DEBOUNCE_NS 20000000
//the timeout for the gpiod wait line request function (in nanoseconds)
#define GPIODWAIT_TIMEOUT 500000

//var and function to catch SIGTERM when the program is killed
volatile sig_atomic_t done = 0;
static void term(int signum){
    done = 1;
}

//event struct that holds gpiod request with lines to watch and gpiod event and event buffers
typedef struct _pinKbd_EVENT{
    struct gpiod_line_request* event_request;
    unsigned int* watched_lines; //the array of lines that are being watched by the event_request
    unsigned int* line_values; //the current values of the watched lines, dont forget encoder has lines in pairs
    unsigned char* final_values; //final values for encoders (one value per encoder), if final_value == 180 encoder CW, if 120 - CCW
    unsigned char* final_values_last; //final values for encoders that where last time of checking, for comparing with final_values, to update a tick only when the value changed
    uint64_t* line_timestamps; //timestamps of the line value writting, useful for button debouncing
    int* intrf_value; //values for lines that are used by the interface
    unsigned int num_of_watched_lines; //how many lines are watched by this event
    struct gpiod_edge_event_buffer* edge_event_buffer; //the buffer that has the line events, its size should be equal to num_of_watched_lines
    unsigned int watch_buttons; //if == 1 this event watches buttons, otherwise - encoders. usefull when there is an event in the buffer and its
    //necessary to get the corresponding encoder or button (usually encoders or buttons index in the array).
    unsigned int chip_num; //chip number for this event in the PINKBD_GPIO_COMM chips array
    APP_EMMIT_KEYPRESS** event_keypresses; //the keypresses structs per line (for encoders the array size is num_of_watched_lines too, but even numbers mean CW keypresses and odd numbers - CCW presses)
}PINKBD_EVENT;

//struct that holds all of the gpio communication
typedef struct _pinKbd_GPIO_COMM{
    int uinput_fd;
    struct gpiod_chip** chips;
    unsigned int num_of_chips;
    //the events that watch the lines asigned to them
    PINKBD_EVENT** pin_events;
    unsigned int num_of_pin_events;
}PINKBD_GPIO_COMM;

//clean everything function
static void pinKbd_clean(PINKBD_GPIO_COMM* pinKbd_obj){
    if(!pinKbd_obj)return;
    app_emmit_clean(pinKbd_obj->uinput_fd);
    if(pinKbd_obj->chips){
	for(int i = 0; i < pinKbd_obj->num_of_chips; i++){
	    if(!(pinKbd_obj->chips[i]))
		continue;
	    gpiod_chip_close(pinKbd_obj->chips[i]);
	}
	free(pinKbd_obj->chips);
    }

    //clean the events
    if(pinKbd_obj->pin_events){
	for(int i = 0; i < pinKbd_obj->num_of_pin_events; i++){
	    PINKBD_EVENT* curr_event = pinKbd_obj->pin_events[i];
	    if(!curr_event)continue;
	    if(curr_event->edge_event_buffer)gpiod_edge_event_buffer_free(curr_event->edge_event_buffer);
	    if(curr_event->event_request)gpiod_line_request_release(curr_event->event_request);
	    if(curr_event->watched_lines)free(curr_event->watched_lines);
	    if(curr_event->line_values)free(curr_event->line_values);
	    if(curr_event->final_values)free(curr_event->final_values);
	    if(curr_event->final_values_last)free(curr_event->final_values_last);
	    if(curr_event->line_timestamps)free(curr_event->line_timestamps);
	    if(curr_event->intrf_value)free(curr_event->intrf_value);
	    //clean the event_keypresses structs
	    if(curr_event->event_keypresses){
		for(int j = 0; j < curr_event->num_of_watched_lines; j++){
		    app_emmit_clean_keypress(curr_event->event_keypresses[j]);
		}
		free(curr_event->event_keypresses);
	    }
	    free(curr_event);
	}
	free(pinKbd_obj->pin_events);
    }
    free(pinKbd_obj);
}

static struct gpiod_line_request* request_input_line(struct gpiod_chip* chip,
						     const unsigned int* offsets,
						     unsigned int num_lines,
						     const char* consumer){
    if(!chip)return NULL;

    struct gpiod_request_config* reg_cfg = NULL;
    struct gpiod_line_request* request = NULL;
    struct gpiod_line_settings* settings = NULL;
    struct gpiod_line_config* line_cfg = NULL;

    settings = gpiod_line_settings_new();
    if(!settings)return NULL;

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    
    line_cfg = gpiod_line_config_new();
    if(!line_cfg)
	goto clean;
    for(int i = 0; i < num_lines; i++){
	int ret = gpiod_line_config_add_line_settings(line_cfg, &offsets[i], 1, settings);
	if(ret)
	    goto clean;
    }
    if(consumer){
	reg_cfg = gpiod_request_config_new();
	if(!reg_cfg)
	    goto clean;
	gpiod_request_config_set_consumer(reg_cfg, consumer);
    }
    request = gpiod_chip_request_lines(chip, reg_cfg, line_cfg);

clean:
    if(settings)
	gpiod_line_settings_free(settings);
    if(line_cfg)
	gpiod_line_config_free(line_cfg);
    if(reg_cfg)
	gpiod_request_config_free(reg_cfg);

    return request;
}
//initiate the event struct
//lines_num - how many pins are watched for this event, for encoders two lines per encoder
//control_num - how many encoders or buttons are watched, for buttons lines_num == control_num, for encoders lines_num *2 == control_num
static int pinKbd_init_event(PINKBD_GPIO_COMM* pinKbd_obj, unsigned int num_of_events,
			     struct gpiod_chip* curr_chip, unsigned int* lines_for_req, APP_EMMIT_KEYPRESS** keypresses, unsigned int lines_num, unsigned int control_num,
			     const char* consumer, unsigned int buttons){
    if(num_of_events <= 0)return -1;
    if(!(pinKbd_obj->pin_events) && num_of_events > 1){
	printf("pin_events not initialised but should be more that one, error \n");
	return -1;
    }
    if(!(pinKbd_obj->pin_events) && num_of_events == 1)pinKbd_obj->pin_events = malloc(sizeof(PINKBD_EVENT*) * num_of_events);
    if(!(pinKbd_obj->pin_events)){
	printf("could not create the pin_events array \n");
	return -1;
    }
    if(pinKbd_obj->pin_events && num_of_events > 1){
	PINKBD_EVENT** temp_events = realloc(pinKbd_obj->pin_events, sizeof(PINKBD_EVENT*) * num_of_events);
	if(!(temp_events)){
	    printf("Could not realloc pin_events\n");
	    return -2;
	}
	pinKbd_obj->pin_events = temp_events;
    }
    pinKbd_obj->pin_events[num_of_events - 1] = NULL;
    PINKBD_EVENT* curr_event = malloc(sizeof(PINKBD_EVENT));
    if(!curr_event){
	printf("Could not malloc event\n");
	return -2;
    }

    curr_event->event_request = request_input_line(curr_chip, lines_for_req, lines_num, consumer);
    if(!(curr_event->event_request)){
	free(curr_event);
	printf("Could not create line request\n");
	return -2;
    }

    curr_event->num_of_watched_lines = lines_num;
    curr_event->watched_lines = lines_for_req;
    curr_event->watch_buttons = buttons;
    curr_event->chip_num = 0;
    curr_event->edge_event_buffer = NULL;
    curr_event->final_values = NULL;
    curr_event->final_values_last = NULL;
    curr_event->line_values = NULL;
    curr_event->edge_event_buffer = gpiod_edge_event_buffer_new(curr_event->num_of_watched_lines);
    curr_event->event_keypresses = keypresses;
    
    pinKbd_obj->pin_events[num_of_events - 1] = curr_event;
    
    //which chip this is in the chips array
    for(int chp_i = 0; chp_i < pinKbd_obj->num_of_chips; chp_i++){
	if(!(pinKbd_obj->chips[chp_i]))continue;
	if(pinKbd_obj->chips[chp_i] == curr_chip){
	    curr_event->chip_num = chp_i;
	    break;
	}
    }
    if(!(curr_event->edge_event_buffer)){
	printf("Could not create edge_event_buffer for the event\n");
	return -1;
    }
    curr_event->line_values = malloc(sizeof(unsigned int) * lines_num);
    if(!(curr_event->line_values)){
	printf("Failed to malloc line_values array for the event\n");
	return -1;
    }
    curr_event->final_values = malloc(sizeof(unsigned char) * control_num);
    if(!(curr_event->final_values)){
	printf("Failed to malloc final_values array for the event\n");
	return -1;
    }
    curr_event->final_values_last = malloc(sizeof(unsigned char) * control_num);
    if(!(curr_event->final_values_last)){
	printf("Failed to malloc final_values_last for the event\n");
	return -1;
    }
    curr_event->line_timestamps = malloc(sizeof(uint64_t) * control_num);
    if(!(curr_event->line_timestamps)){
	printf("Failed to malloc line_timestamps for the event\n");
	return -1;
    }
    curr_event->intrf_value = malloc(sizeof(int) * control_num);
    if(!(curr_event->intrf_value)){
	printf("Failed to malloc intrf_value for the event\n");
	return -1;
    }      
    memset(curr_event->line_values, 1, sizeof(unsigned int) * lines_num);
    memset(curr_event->final_values, 1, sizeof(unsigned char) * control_num);
    memset(curr_event->final_values_last, 1, sizeof(unsigned char) * control_num);
    memset(curr_event->line_timestamps, 0, sizeof(uint64_t) * control_num);
    memset(curr_event->intrf_value, 0, sizeof(int) * control_num);
    return 0;
}

//filter the paths that are chip paths, copied from libgpiod tools-common.h
static int chip_dir_filter(const struct dirent *entry)
{
	struct stat sb;
	int ret = 0;
	char *path;
	unsigned int path_len = strlen(entry->d_name)+6;
	path = malloc(sizeof(char)*path_len);
	if(!path)return 0;
	snprintf(path, path_len, "/dev/%s", entry->d_name);

	if ((lstat(path, &sb) == 0) && (!S_ISLNK(sb.st_mode)) &&
	    gpiod_is_gpiochip_device(path))
		ret = 1;

	free(path);

	return ret;
}
//clean the return dirent struct
static void dirent_clean_dirents(unsigned int num, struct dirent **entries){
    if(num <= 0)return;
    for(int j = 0; j < num; j++){
	free(entries[j]);
    }
    free(entries);
}
//returns path in /dev/ of the chip with chip_label, altered from libgpiod tools-common.h
static char* pinKbd_return_path_from_label(const char* chip_label){
    char* ret_path = NULL;
    int num_chips = 0;
    struct dirent **entries;
    num_chips = scandir("/dev/", &entries, chip_dir_filter, alphasort);
    if(num_chips < 0)return NULL;
    for(int i = 0; i < num_chips; i++){
	unsigned int path_len = strlen(entries[i]->d_name)+6;
	char* chip_path = malloc(sizeof(char)*path_len);
	if(!chip_path)continue;
	snprintf(chip_path, path_len, "/dev/%s", entries[i]->d_name);
	struct gpiod_chip* cur_chip = gpiod_chip_open(chip_path);
	if(!cur_chip){
	    free(chip_path);
	    continue;
	}
	
	struct gpiod_chip_info* info = gpiod_chip_get_info(cur_chip);
	if(!info){
	    free(chip_path);
	    gpiod_chip_close(cur_chip);
	    continue;
	}
	
	if(strcmp(gpiod_chip_info_get_label(info), chip_label) == 0){
	    ret_path = chip_path;
	    gpiod_chip_info_free(info);
	    gpiod_chip_close(cur_chip);
	    break;
	}
	free(chip_path);
	gpiod_chip_info_free(info);
	gpiod_chip_close(cur_chip);
    }
    dirent_clean_dirents(num_chips, entries);
    return ret_path;
}

//initialize the PINKBD_GPIO_COMM struct from json config file
static PINKBD_GPIO_COMM* pinKbd_init_from_config(const char* config_path){
    JSONHANDLE* parsed_fp = app_json_tokenise_path(config_path);
    if(!parsed_fp)return NULL;
    
    PINKBD_GPIO_COMM* pinKbd_obj = malloc(sizeof(PINKBD_GPIO_COMM));
    if(!pinKbd_obj){
	app_json_clean_object(parsed_fp);
	return NULL;
    }
    pinKbd_obj->chips = NULL;
    pinKbd_obj->num_of_chips = 0;
    pinKbd_obj->pin_events = NULL;
    pinKbd_obj->num_of_pin_events = 0;
    pinKbd_obj->uinput_fd = 0;
    
    //------------------------------------------------------------
    //get all the possible keybits from the pin_config file
    JSONHANDLE** keys = malloc(sizeof(JSONHANDLE*));
    unsigned int keys_size = 0;
    int err_keys_array = app_json_iterate_and_find_obj(parsed_fp, "keys", &keys, &keys_size);
    int* keybit_array = malloc(sizeof(int));
    int keybit_size = 0;
    for(size_t j = 0; j < keys_size; j++){
	JSONHANDLE* cur_key = keys[j];
	size_t elem_size = 0;
	char** cur_key_elements = app_json_array_to_string_array(cur_key, &elem_size);
	if(!cur_key_elements)continue;
	for(size_t key_elem = 0; key_elem < elem_size; key_elem++){
	    char* cur_string = cur_key_elements[key_elem];
	    if(!cur_string)continue;

	    int cur_keybit = app_emmit_convert_to_enum(cur_string);
	    free(cur_string);
	    if(cur_keybit == -1)continue;
	    //add the keybit to the keybit_array, but only if its not already there
	    unsigned int found = 0;
	    for(size_t kb = 0; kb < keybit_size; kb ++){
		if(cur_keybit == keybit_array[kb]){
		    found = 1;
		    break;
		}
	    }
	    if(found == 1)continue;
	    int* temp_array = realloc(keybit_array, sizeof(int) * (keybit_size+1));
	    if(!temp_array)continue;
	    keybit_size += 1;	    
	    keybit_array = temp_array;
	    keybit_array[keybit_size - 1] = cur_keybit;
	}
	free(cur_key_elements);
    }
    if(keys)free(keys);
    //now init the emmit for all the keys
    if(keybit_array){
	if(keybit_size > 0){
	    int err = app_emmit_init_input(&(pinKbd_obj->uinput_fd), "pinKbd emmiter", keybit_array, keybit_size);
	}
	free(keybit_array);
    }
    //------------------------------------------------------------
    
    JSONHANDLE** chips = malloc(sizeof(JSONHANDLE*));
    unsigned int chips_size = 0;
    int err_chips = app_json_iterate_and_find_obj(parsed_fp, "chipname", &chips, &chips_size);
    //create placeholder for the chips array
    pinKbd_obj->chips = malloc(sizeof(struct gpiod_chip*) * chips_size);
    if(!(pinKbd_obj->chips)){
	printf("Could not create the chip array\n");
	pinKbd_clean(pinKbd_obj);
	return NULL;
    }
    pinKbd_obj->num_of_chips = chips_size;
    unsigned int num_of_events = 0; //total number of events to initiate on the pinKbd_obj->pin_events
    for(int i = 0; i < chips_size; i++){
	pinKbd_obj->chips[i] = NULL;
	JSONHANDLE* curr_chip = chips[i];
	if(!curr_chip)continue;
	char* chip_label = app_json_obj_to_string(curr_chip);
	if(!chip_label)continue;
	JSONHANDLE* chip_parent = app_json_iterate_and_return_parent(parsed_fp, curr_chip);
	
	//find chip path from a label
	char* chip_path = pinKbd_return_path_from_label(chip_label);
	if(!chip_path){
	    printf("could not find %s path \n", chip_label);
	    free(chip_label);
	    continue;
	}
	free(chip_label);
	
	//create the chip
	pinKbd_obj->chips[i] = gpiod_chip_open(chip_path);
	free(chip_path);	
	if(!(pinKbd_obj->chips[i])){
	    printf("Could not create the %s chip\n",chip_path);
	    continue;
	}
	
	//now find the encoder and button lines for curr_chip in the pin_config.json
	unsigned int enc_num = 0; //how many encoders on this chip
	unsigned int btn_num = 0; //how many buttons on this chip
	unsigned int* enc_line_nums = malloc(sizeof(unsigned int)); //array with the line numbers for the encoders for this chip
	unsigned int* btn_line_nums = malloc(sizeof(unsigned int)); //array with the line numbers for the buttons for this chip
	APP_EMMIT_KEYPRESS** enc_keypresses = malloc(sizeof(APP_EMMIT_KEYPRESS*)); //array with the keypress structs for the encoder lines - array size should be enc_num
	APP_EMMIT_KEYPRESS** btn_keypresses = malloc(sizeof(APP_EMMIT_KEYPRESS*)); //array with the keypress structs for the button lines - array size should be btn_num
	if(!enc_line_nums || !btn_line_nums || !enc_keypresses || !btn_keypresses){
	    goto clean_fail;
	}
	//get all the json obj with key word name
	JSONHANDLE** names = malloc(sizeof(JSONHANDLE*));
	unsigned int names_size = 0;
	int err_types = app_json_iterate_and_find_obj(chip_parent, "name", &names, &names_size);
	for(size_t j = 0; j < names_size; j++){
	    //name_parent is a button or encoder entry in the pin_config
	    JSONHANDLE* name_parent = app_json_iterate_and_return_parent(chip_parent, names[j]);
	    if(!name_parent)continue;
	    //now get the line_num and keys entries on the encoder or button
	    //since the lines and keys on the lines are in sequence no need to get the line_num parent in order to get the key array for that line
	    JSONHANDLE** line_nums = malloc(sizeof(JSONHANDLE*));
	    //if line_size == 2 this is an encoder, if line_size == 1 its a button
	    unsigned int line_size = 0;
	    app_json_iterate_and_find_obj(name_parent, "line_num", &line_nums, &line_size);
	    unsigned int button = 0;
	    if(line_size == 1) button = 1;
	    if(!line_nums)continue;
	    JSONHANDLE** key_arrays = malloc(sizeof(JSONHANDLE*));
	    unsigned int key_size = 0;
	    app_json_iterate_and_find_obj(name_parent, "keys", &key_arrays, &key_size);
	    //should be same number of line_nums and key arrays, if not the pin_config has an error for this encoder or button
	    if(!key_arrays || line_size != key_size){
		if(key_arrays)free(key_arrays);
		free(line_nums);
		continue;
	    }	    
	    for(size_t lin = 0; lin < line_size; lin++){
		JSONHANDLE* curr_line_num = line_nums[lin];
		JSONHANDLE* curr_key_array = key_arrays[lin];
		int line_num_int = 0;
		if(app_json_obj_to_int(curr_line_num, &line_num_int) == -1)continue;
		APP_EMMIT_KEYPRESS* line_keypress_struct = NULL;
		if(button == 0){
		    unsigned int* temp_enc = realloc(enc_line_nums, sizeof(unsigned int)*(enc_num+1));
		    if(!temp_enc)continue;
		    APP_EMMIT_KEYPRESS** temp_enc_keypresses = realloc(enc_keypresses, sizeof(APP_EMMIT_KEYPRESS*)*(enc_num+1));
		    if(!temp_enc_keypresses){
			free(temp_enc);
			continue;
		    }
		    enc_line_nums = temp_enc;
		    enc_line_nums[enc_num] = line_num_int;
		    enc_keypresses = temp_enc_keypresses;
		    enc_keypresses[enc_num] = NULL;
		    enc_num += 1;
		}
		if(button == 1){
		    unsigned int* temp_btn = realloc(btn_line_nums, sizeof(unsigned int)*(btn_num+1));
		    if(!temp_btn)continue;
		    APP_EMMIT_KEYPRESS** temp_btn_keypresses = realloc(btn_keypresses, sizeof(APP_EMMIT_KEYPRESS*)*(btn_num+1));
		    if(!temp_btn_keypresses){
			free(temp_btn);
			continue;
		    }
		    btn_line_nums = temp_btn;
		    btn_line_nums[btn_num] = line_num_int;
		    btn_keypresses = temp_btn_keypresses;
		    btn_keypresses[btn_num] = NULL;
		    btn_num += 1;
		}		
		//now get the strings from the key array and convert them to keybits that will be stored event_keypresses struct array for this line
		int* keybit_array = malloc(sizeof(int));
		if(!keybit_array)continue;
		int keybit_size = 0;
		
		size_t elem_size = 0;
		char** cur_key_elements = app_json_array_to_string_array(curr_key_array, &elem_size);
		if(!cur_key_elements)continue;		
		for(size_t key_elem = 0; key_elem < elem_size; key_elem++){
		    char* cur_key_string = cur_key_elements[key_elem];
		    if(!cur_key_string)continue;
		    int cur_keybit = app_emmit_convert_to_enum(cur_key_string);
		    free(cur_key_string);
		    if(cur_keybit == -1)continue;
		    int* temp_array = realloc(keybit_array, sizeof(int) * (keybit_size+1));
		    if(!temp_array)continue;
		    keybit_size += 1;		    
		    keybit_array = temp_array;
		    keybit_array[keybit_size - 1] = cur_keybit;		    
		}
		unsigned int invert = 0;
		if(button == 0)invert = 1;
		line_keypress_struct = app_emmit_init_keypress(keybit_array, keybit_size, invert);
		if(keybit_array)free(keybit_array);
		if(line_keypress_struct){
		    if(button == 0){
			enc_keypresses[enc_num-1] = line_keypress_struct;
		    }
		    if(button == 1){
			btn_keypresses[btn_num-1] = line_keypress_struct;
		    }
		}
		free(cur_key_elements);		
		
	    }
	    if(line_nums)free(line_nums);
	    if(key_arrays)free(key_arrays);
	}
	
	if(!names)goto clean_fail;
	//init the encoder events for this chip
	if(enc_num > 0){
	    num_of_events += 1;
	    int init_error = pinKbd_init_event(pinKbd_obj, num_of_events, pinKbd_obj->chips[i], enc_line_nums, enc_keypresses, enc_num, enc_num * 0.5, "pinKbd daemon line watch", 0);
	    if(init_error < 0)goto clean_fail;
	}
	//init the button events for this chip
	if(btn_num > 0){
	    num_of_events += 1;
	    int init_error = pinKbd_init_event(pinKbd_obj, num_of_events, pinKbd_obj->chips[i], btn_line_nums, btn_keypresses, btn_num, btn_num, "pinKbd daemon line watch", 1);
	    if(init_error < 0)goto clean_fail;
	}
	if(enc_num <= 0 && enc_line_nums){
	    free(enc_line_nums);
	    if(enc_keypresses)free(enc_keypresses);
	}
	if(btn_num <= 0 && btn_line_nums){
	    free(btn_line_nums);
	    if(btn_keypresses)free(btn_keypresses);
	}
	free(names);
	continue;
	
    clean_fail:
	if(enc_line_nums)free(enc_line_nums);
	if(btn_line_nums)free(btn_line_nums);
	if(enc_keypresses){
	    for(int en_press = 0; en_press < enc_num; en_press++){
		app_emmit_clean_keypress(enc_keypresses[en_press]);
	    }
	    free(enc_keypresses);
	}
	if(btn_keypresses){
	    for(int en_press = 0; en_press < btn_num; en_press++){
		app_emmit_clean_keypress(btn_keypresses[en_press]);
	    }
	    free(btn_keypresses);
	}
	gpiod_chip_close(pinKbd_obj->chips[i]);
	pinKbd_obj->chips[i] = NULL;
	if(names)free(names);
	
    }
    pinKbd_obj->num_of_pin_events = num_of_events;
    if(parsed_fp)app_json_clean_object(parsed_fp);    
    if(chips)free(chips);    

    return pinKbd_obj;
}

//update all encoder and button values
//first checks if any events happened at all, if yes goes through proper processes for encoders and buttons
//enc_sens - encoder sensitivity, the encoder must accumulate this many positive (for CW) or negative (for CCW) events to invoke its function
static int pinKbd_update_values(PINKBD_GPIO_COMM* pinKbd_obj, unsigned int enc_sens){
    if(!pinKbd_obj)return -1;
    if(!(pinKbd_obj->pin_events))return -1;
    if(pinKbd_obj->num_of_pin_events <= 0) return -1;

    for(int i = 0; i < pinKbd_obj->num_of_pin_events; i++){
	PINKBD_EVENT* curr_event = pinKbd_obj->pin_events[i];
	if(!curr_event)continue;
	if(!(curr_event->event_request))continue;
	int req_test = 0;
	req_test = gpiod_line_request_wait_edge_events(curr_event->event_request, GPIODWAIT_TIMEOUT);
	//if event happened
	if(req_test == 1){
	    //this function blocks until there is an event this is why first call the gpiod_line_request_wait_edge_events with 0ms
	    int ret = gpiod_line_request_read_edge_events(curr_event->event_request, curr_event->edge_event_buffer, curr_event->num_of_watched_lines);
	    if(ret > 0){
		for(int ev_i = 0; ev_i < ret; ev_i++){
		    struct gpiod_edge_event* event = gpiod_edge_event_buffer_get_event(curr_event->edge_event_buffer, ev_i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event);
		    uint64_t timestamp = gpiod_edge_event_get_timestamp_ns(event);
		    unsigned int type = gpiod_edge_event_get_event_type(event);
		    unsigned int type_ = 0;
		    if(type == 1) type_ = 1;
		    //find which button or encoder
		    int line_idx = -1;
		    for(int ln_i = 0; ln_i < curr_event->num_of_watched_lines; ln_i++){
			unsigned int curr_line = curr_event->watched_lines[ln_i];
			if(curr_line == offset){
			    line_idx = ln_i;
			    break;
			}
		    }
		    if(line_idx == -1){
			printf("line not found in event watched lines\n");
			continue;
		    }
		    unsigned int control_num = line_idx; //encoder or button index
		    //if this is an encoder, get the encoder number (since there are two lines per encoder)
		    if(curr_event->watch_buttons == 0){
			if(control_num%2 != 0)control_num -= 1;
			control_num *= 0.5;
		    }
		    //update line_values for encoders and buttons
		    //but only if the type_is different from the one saved
		    if(curr_event->line_values[line_idx] != type_){
			curr_event->line_values[line_idx] = type_;
			//update the final_values
			//for encoders we need to set first_val_idx and the second line idx value, since pins are in pairs for encoders
			if(curr_event->watch_buttons == 0){
			    unsigned int first_val_idx = line_idx;
			    unsigned int second_val_idx = line_idx + 1;
			    if(line_idx%2 != 0){
				first_val_idx = line_idx - 1;
				second_val_idx = line_idx;
			    }
			    curr_event->final_values[control_num] = curr_event->final_values[control_num] << 1;
			    curr_event->final_values[control_num] = curr_event->final_values[control_num] | curr_event->line_values[first_val_idx];
			    curr_event->final_values[control_num] = curr_event->final_values[control_num] << 1;
			    curr_event->final_values[control_num] = curr_event->final_values[control_num] | curr_event->line_values[second_val_idx];
			}
			//for buttons final_value will be the type_, if sufficient time has passed after the last event, to prevent bounce
			else if(curr_event->watch_buttons == 1){
			    uint64_t last_timestamp = curr_event->line_timestamps[control_num];
			    if(abs(timestamp - last_timestamp) >= DEBOUNCE_NS){
				curr_event->final_values[control_num] = type_;
			    }
			}
		    }
		    //check the final value
		    unsigned char final_val = curr_event->final_values[control_num];
		    unsigned int changed_val = 0;
		    if(final_val != curr_event->final_values_last[control_num]){
			//do for encoders
			if(curr_event->watch_buttons == 0){
			    if(final_val == 180 || final_val == 210 || final_val == 75 || final_val == 45){
				curr_event->intrf_value[control_num] += 1;
				changed_val = 1;
			    }

			    if(final_val == 120 || final_val == 225 || final_val == 135 || final_val == 30){
				curr_event->intrf_value[control_num] -= 1;
				changed_val = 1;
			    }
			    if(changed_val ==1){
				curr_event->line_timestamps[control_num] = timestamp;
				if(abs(curr_event->intrf_value[control_num]) >= enc_sens){
				    //for encoder find which keypress struct to get
				    unsigned int keypress_num = line_idx;
				    if(curr_event->intrf_value[control_num] > 0){
					if(line_idx%2 != 0) keypress_num = line_idx - 1;
				    }
				    if(curr_event->intrf_value[control_num] < 0){
					if(line_idx%2 == 0) keypress_num = line_idx + 1;
				    }
				    if(curr_event->intrf_value[control_num] != 0){
					for(int mult_iter = 0; mult_iter < abs(curr_event->intrf_value[control_num])/enc_sens; mult_iter++){
					    app_emmit_emmit_keypress(pinKbd_obj->uinput_fd, curr_event->event_keypresses[keypress_num], 0);
					}
				    }
				    curr_event->intrf_value[control_num] = 0;
				}
			    }
			}
			//do for buttons
			else if(curr_event->watch_buttons == 1){
			    curr_event->intrf_value[control_num] += 1;
			    if(curr_event->intrf_value[control_num] > 0){
				unsigned int unpress = 0;
				if(curr_event->final_values[control_num] == 1)unpress = 1;
				app_emmit_emmit_keypress(pinKbd_obj->uinput_fd, curr_event->event_keypresses[control_num], unpress);
				
				curr_event->intrf_value[control_num] = 0;
			    }
			    changed_val = 1;
			    curr_event->line_timestamps[control_num] = timestamp;
			}
			curr_event->final_values_last[control_num] = final_val;
		    }
		}
	    }
	}
    }
}

int main(){
    //Initiate struct for SIGTERM handling
    //----------------------------------
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(struct sigaction));
    sig_action.sa_handler = term;
    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);
      
    //use the config file to setup the pins
    PINKBD_GPIO_COMM* pinKbd_obj = pinKbd_init_from_config("/etc/pin_config.json");
    if(!pinKbd_obj)return -1;
  
    while(!done){
	pinKbd_update_values(pinKbd_obj, 4);
    }
    
    //clean everything here
    pinKbd_clean(pinKbd_obj);

    printf("pinKbd_dameon shutting down, done cleanup\n");

    return 0;
}
