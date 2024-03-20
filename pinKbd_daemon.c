#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gpiod.h>
#include <unistd.h>
#include <inttypes.h>
//how many lines to read from pisound gpio for encoders
#define PISOUND_ENC_NUM_LINES 18
//how many lines to read from pisound gpio for buttons
#define PISOUND_BUT_NUM_LINES 17
//how many lines to read from the Raspberry Pi for buttons
#define RPI_BUT_NUM_LINES 5
//var and function to catch SIGTERM when the program is killed
volatile sig_atomic_t done = 0;
static void term(int signum){
    done = 1;
}
//function that writes bits to the fd to simulate a keypress
static void emit(int fd, int type, int code, int val){
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    write(fd, &ie, sizeof(ie));
}
//event struct that holds gpiod request with lines to watch and gpiod event and event buffers
typedef struct _pinKbd_EVENT{
    struct gpiod_line_request* event_request;
    unsigned int* watched_lines; //the array of lines that are being watched by the event_request
    unsigned int* line_values; //the current values of the watched lines, dont forget encoder has lines in pairs
    unsigned char* final_values; //final values for encoders (one value per encoder), if final_value == 180 encoder CW, if 120 - CCW
    unsigned char* final_values_last; //final values for encoders that where last time of checking, for comparing with final_values, to update a tick only when the value changed
    unsigned int num_of_watched_lines; //how many lines are watched by this event
    struct gpiod_edge_event_buffer* edge_event_buffer; //the buffer that has the line events, its size should be equal to num_of_watched_lines
    unsigned int watch_buttons; //if == 1 this event watches buttons, otherwise - encoders. usefull when there is an event in the buffer and its
    //necessary to get the corresponding encoder or button (usually encoders or buttons index in the array).
    unsigned int chip_num; //chip number for this event in the PINKBD_GPIO_COMM chips array
}PINKBD_EVENT;

//struct that holds all of the gpio communication
typedef struct _pinKbd_GPIO_COMM{
    struct gpiod_chip** chips;
    unsigned int num_of_chips;
    //the events that watch the lines asigned to them
    PINKBD_EVENT** pin_events;
    unsigned int num_of_pin_events;
}PINKBD_GPIO_COMM;

//clean everything function
static void pinKbd_clean(int* uinput_fd, PINKBD_GPIO_COMM* pinKbd_obj){
    //clean the uinput emmiter
    if(uinput_fd){
	ioctl(*uinput_fd, UI_DEV_DESTROY);
	close(*uinput_fd);
    }
    
    if(!pinKbd_obj)return;

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
	    free(curr_event);
	}
	free(pinKbd_obj->pin_events);
    }
    free(pinKbd_obj);
}

static struct gpiod_line_request* request_input_line(struct gpiod_chip* chip,
						     const unsigned int* offsets,
						     unsigned int num_lines,
						     const char* consumer, unsigned int debounce){
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
    if(debounce==1)
	gpiod_line_settings_set_debounce_period_us(settings, 10000);
    
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
			     struct gpiod_chip* curr_chip, unsigned int* lines_for_req, unsigned int lines_num, unsigned int control_num, const char* consumer,
			     unsigned int debounce, unsigned int buttons){
    pinKbd_obj->pin_events = realloc(pinKbd_obj->pin_events, sizeof(PINKBD_EVENT*) * num_of_events);
    if(!(pinKbd_obj->pin_events)){
	printf("Could not realloc pin_events\n");
	return -1;
    }
    PINKBD_EVENT* curr_event = malloc(sizeof(PINKBD_EVENT));
    if(!curr_event){
	printf("Could not malloc event\n");
	return -1;
    }
    pinKbd_obj->pin_events[num_of_events - 1] = curr_event;
    curr_event->event_request = request_input_line(curr_chip, lines_for_req, lines_num, consumer, debounce);
    if(!(curr_event->event_request)){
	printf("Could not create line request\n");
	return -1;
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
    memset(curr_event->line_values, 0, sizeof(unsigned int) * lines_num);
    memset(curr_event->final_values, 0, sizeof(unsigned char) * control_num);
    memset(curr_event->final_values_last, 0, sizeof(unsigned char) * control_num);
        
    return 0;
}
//initiate the PINKBD_GPIO_COMM struct,
//encoder_lines - pin numbers for encoders, dont forget that one encoder has two pins, so this array is in pairs
static PINKBD_GPIO_COMM* pinKbd_init(unsigned int num_of_chips, const char** const chip_paths,
				     const unsigned int* encoder_lines, unsigned int num_of_encoders, const unsigned int* encoder_chip_nums,
				     const unsigned int* button_lines, unsigned int num_of_buttons, const unsigned int* button_chip_nums){
    PINKBD_GPIO_COMM* pinKbd_obj = malloc(sizeof(PINKBD_GPIO_COMM));
    if(!pinKbd_obj)return NULL;
    if(num_of_chips <= 0)return NULL;
    if(!chip_paths)return NULL;
    //initialize to 0
    pinKbd_obj->pin_events = NULL;
    pinKbd_obj->num_of_pin_events = 0;
    
    pinKbd_obj->num_of_chips = num_of_chips;
    pinKbd_obj->chips = malloc(sizeof(struct gpiod_chip*) * num_of_chips);
    if(!(pinKbd_obj->chips)){
	printf("Could not create the chip array\n");
	pinKbd_clean(NULL, pinKbd_obj);
	return NULL;
    }
    //initiate the gpio chips on the struct
    for(int i = 0; i<num_of_chips; i++){
	pinKbd_obj->chips[i] = gpiod_chip_open(chip_paths[i]);
	if(!(pinKbd_obj->chips[i])){
	    printf("Could not create the %s chip\n",chip_paths[i]);
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}
    }

    //Create the event structs
    //First go through the chips and for each one go through the encoders and buttons of that chip
    //When found an encoder or a button for the chip add its lines to the offset array from which to create the gpiod_line_request
    unsigned int num_of_events = 0;
    pinKbd_obj->pin_events = malloc(sizeof(PINKBD_EVENT*));
    if(!(pinKbd_obj->pin_events)){
	printf("Could not malloc pin_events \n");
	pinKbd_clean(NULL, pinKbd_obj);
	return NULL;
    }
    for(int i = 0; i < pinKbd_obj->num_of_chips; i++){
	struct gpiod_chip* curr_chip = pinKbd_obj->chips[i];
	if(!curr_chip)continue;
	unsigned int chip_in_encoders_num = 0; //how many encoders are on this chip
	unsigned int chip_in_btns_num = 0; //how many buttons are on this chip
	unsigned int* encoders_lines_for_req = malloc(sizeof(unsigned int)); //array with the line numbers for the encoders for this chip
	unsigned int* btns_lines_for_req = malloc(sizeof(unsigned int)); //array with the line numbers for the buttons for this chip
	if(!encoders_lines_for_req || !btns_lines_for_req){
	    printf("Failed events initialization \n");
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}
	//go through encoders
	unsigned int encoders_lines_iterator = 0; //encoders have two pin numbers so increase this +2 when an encoder is found
	for(int enc_i = 0; enc_i < num_of_encoders; enc_i++){
	    if(encoder_chip_nums[enc_i] == i){
		chip_in_encoders_num += 1;
		encoders_lines_for_req = realloc(encoders_lines_for_req, sizeof(unsigned int) * (chip_in_encoders_num * 2));
		if(!encoders_lines_for_req){
		    printf("Failed events initialization - could not realloc encoders_lines \n");
		    pinKbd_clean(NULL, pinKbd_obj);
		    return NULL;
		}
		encoders_lines_for_req[encoders_lines_iterator] = encoder_lines[enc_i * 2];
		encoders_lines_for_req[encoders_lines_iterator + 1] = encoder_lines[(enc_i * 2) + 1];
		encoders_lines_iterator += 2;
	    }
	}
	//go through the buttons
	for(int btn_i = 0; btn_i < num_of_buttons; btn_i++){
	    if(button_chip_nums[btn_i] == i){
		chip_in_btns_num += 1;
		btns_lines_for_req = realloc(btns_lines_for_req, sizeof(unsigned int) * (chip_in_btns_num));
		if(!btns_lines_for_req){
		    printf("Failed events initialization - could not realloc btns_lines \n");
		    pinKbd_clean(NULL, pinKbd_obj);
		    return NULL;		    
		}
		btns_lines_for_req[chip_in_btns_num - 1] = button_lines[btn_i];
	    }
	}
	//if there where encoders with this chip create request, event etc.
	if(chip_in_encoders_num > 0 && encoders_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, encoders_lines_for_req, chip_in_encoders_num * 2, chip_in_encoders_num, "pinKbd_line_watch", 0, 0) == -1){
		pinKbd_clean(NULL, pinKbd_obj);
		return NULL;
	    }
	}
	//if there where buttons with this chip create request, event etc.
	if(chip_in_btns_num > 0 && btns_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, btns_lines_for_req, chip_in_btns_num, chip_in_btns_num, "pinKbd_line_watch", 0, 1) == -1){
		pinKbd_clean(NULL, pinKbd_obj);
		return NULL;
	    }	    
	}
	if(chip_in_encoders_num <= 0)
	    free(encoders_lines_for_req);
	if(chip_in_btns_num <= 0)
	    free(btns_lines_for_req);
    }
    if(num_of_events <= 0)
	free(pinKbd_obj->pin_events);
    pinKbd_obj->num_of_pin_events = num_of_events;
    printf("Created %d events \n", pinKbd_obj->num_of_pin_events);
    return pinKbd_obj;
}

//update all encoder and button values
//first checks if any events happened at all, if yes goes through proper processes for encoders and buttons
static int pinKbd_update_values(PINKBD_GPIO_COMM* pinKbd_obj){
    if(!pinKbd_obj)return -1;
    if(!(pinKbd_obj->pin_events))return -1;
    if(pinKbd_obj->num_of_pin_events <= 0) return -1;

    for(int i = 0; i < pinKbd_obj->num_of_pin_events; i++){
	PINKBD_EVENT* curr_event = pinKbd_obj->pin_events[i];
	if(!curr_event)continue;
	if(!(curr_event->event_request))continue;
	//if event happened
	if(gpiod_line_request_wait_edge_events(curr_event->event_request, 0)){
	    //this function blocks until there is an event this is why first call the gpiod_line_request_wait_edge_events with 0ms
	    int ret = gpiod_line_request_read_edge_events(curr_event->event_request, curr_event->edge_event_buffer, curr_event->num_of_watched_lines);
	    if(ret > 0){
		for(int ev_i = 0; ev_i < ret; ev_i++){
		    struct gpiod_edge_event* event = gpiod_edge_event_buffer_get_event(curr_event->edge_event_buffer, ev_i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event);
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
		    //if this is an encoder, get the encoder number from the line index (since there are two lines per encoder)
		    if(curr_event->watch_buttons == 0){
			if(line_idx%2 != 0)line_idx -= 1;
			line_idx *= 0.5;
		    }
		    //TODO update line_values for encoders and buttons
		    //TODO for encoders do the bitwise operations and update in which direction a tick happened, will need a variable on event to store this info
		    //TODO these rotations will accumulate +1 for CW and -1 for CCW
		    //TODO new function pinKbd_invoke() will go through encoders and buttons and depending on the values and which encoder or button invoked
		    //will call an action (like emmit keyboard). Then this function will make the value 0. This way can control sensitivity - for example dont
		    //invoke until an encoder has +2 or -2 rotation (so two ticks in any direction). For buttons will need to watch the last and current value
		    //if they are the same do nothing if they changed do something (emmit keyboard) and change the last value to current. This way for buttons
		    //do something only when the button value changes - no need to emmit non stop 1 calls when the button is pushed, emmit 1 when pushed and
		    //0 when released.
		    //TODO still need to think where and how to debounce buttons
		    printf("Offset %d type_ %d\n", offset, type_);
		    if(curr_event->watch_buttons)
			printf("%d event is for a button %d in chip %d\n", i, line_idx, curr_event->chip_num);
		    if(!curr_event->watch_buttons)
			printf("%d event is for an encoder %d in chip %d\n", i, line_idx, curr_event->chip_num);
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
    
    //----------------------------------

    //Initiate the uinput setup
    //-----------------------------------
    struct uinput_setup usetup;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    //initiate the virtual device for emmiting the keypresses
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    //TODO What keys to use should be sourced from a file, where user can configure what
    //encoders and buttons produce what keypresses. If no such file just use these as defaults
    ioctl(fd, UI_SET_KEYBIT, KEY_1);
    ioctl(fd, UI_SET_KEYBIT, KEY_2);
    ioctl(fd, UI_SET_KEYBIT, KEY_3);
    ioctl(fd, UI_SET_KEYBIT, KEY_4);
    ioctl(fd, UI_SET_KEYBIT, KEY_5);
    ioctl(fd, UI_SET_KEYBIT, KEY_6);
    ioctl(fd, UI_SET_KEYBIT, KEY_7);
    ioctl(fd, UI_SET_KEYBIT, KEY_8);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTALT);
    ioctl(fd, UI_SET_KEYBIT, KEY_Q);
    ioctl(fd, UI_SET_KEYBIT, KEY_W);
    ioctl(fd, UI_SET_KEYBIT, KEY_E);
    ioctl(fd, UI_SET_KEYBIT, KEY_R);
    ioctl(fd, UI_SET_KEYBIT, KEY_T);
    ioctl(fd, UI_SET_KEYBIT, KEY_Y);
    ioctl(fd, UI_SET_KEYBIT, KEY_S);
    ioctl(fd, UI_SET_KEYBIT, KEY_D);
    ioctl(fd, UI_SET_KEYBIT, KEY_F);
    ioctl(fd, UI_SET_KEYBIT, KEY_G);
    ioctl(fd, UI_SET_KEYBIT, KEY_H);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTMETA);
    ioctl(fd, UI_SET_KEYBIT, KEY_A);
    
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; // sample vendor
    usetup.id.product = 0x5678; //sample product
    strcpy(usetup.name, "Pisound-Micro Pin to Keypress Device");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    //---------------------------------------------

    //Initiate the gpio
    //---------------------------------------------------
    PINKBD_GPIO_COMM* pinKbd_obj = pinKbd_init(2, (const char*[2]){"/dev/gpiochip0", "/dev/gpiochip2"},
					       (const unsigned int[18]){6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}, 9,
					       (const unsigned int[18]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
					       (const unsigned int[22]){24,25,26,27,28,29,30,31,32,33,34,35,0,1,2,3,4,17,27,22,23,24}, 22,
					       (const unsigned int[22]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0});
    if(!pinKbd_obj){
	pinKbd_clean(&fd, NULL);
	return -1;
    }
    
    while(!done){
	pinKbd_update_values(pinKbd_obj);
	//TODO only for testing, should emit only when encoder or button is used
	/*
	emit(fd, EV_KEY, KEY_1, 1);
	emit(fd, EV_SYN, SYN_REPORT, 0);
	emit(fd, EV_KEY, KEY_1, 0);
	emit(fd, EV_SYN, SYN_REPORT, 0);
	*/
	//TODO need a wrap function for all of this
/*
	if(gpiod_line_request_wait_edge_events(pisound_encoders_req, 0)){
	    int ret = gpiod_line_request_read_edge_events(pisound_encoders_req, event_encoders_buffer, event_encoders_buf_size);
	    if(ret > 0){
		for(int i = 0; i < ret; i++){
		    event_encoder = gpiod_edge_event_buffer_get_event(event_encoders_buffer, i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event_encoder);
		    unsigned int type = gpiod_edge_event_get_event_type(event_encoder);
		    unsigned int type_ = 0;
		    if(type == 1) type_ = 1;
		    if(offset % 2 == 0){
			if(type_ != c_A){
			    c_A = type_;
			    //printf("offset %d type %d type %d\n", offset, c_A, c_B);
			    final_num = final_num << 1;
			    final_num = final_num | c_A;
			    final_num = final_num << 1;
			    final_num = final_num | c_B;
			}
		    }
		    else{
			if(type_ != c_B){
			    c_B = type_;
			    //printf("offset %d type %d type %d\n", offset, c_A, c_B);
			    final_num = final_num << 1;
			    final_num = final_num | c_A;
			    final_num = final_num << 1;
			    final_num = final_num | c_B;
			}
		    }
		}
		if(final_num_last != final_num){
		    if(final_num == 180) enc_count += 1;
		    if(final_num == 120) enc_count -= 1;
		    if(enc_count >= 1){
			printf("CW Rotation \n");
			emit(fd, EV_KEY, KEY_LEFTSHIFT, 1);			
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_LEFTSHIFT, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
			enc_count = 0;
		    }
		    if(enc_count <= -1){
			printf("CCW Rotation \n");
			emit(fd, EV_KEY, KEY_LEFTALT, 1);			
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_LEFTALT, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
			enc_count = 0;
		    }
		    final_num_last = final_num;
		}
 	    }

	}
	if(gpiod_line_request_wait_edge_events(pisound_buttons_req, 0)){
	    int ret = gpiod_line_request_read_edge_events(pisound_buttons_req, event_but_buffer, event_but_buf_size);
	    if(ret > 0){
		for(int i = 0; i < ret; i++){
		    event_but = gpiod_edge_event_buffer_get_event(event_but_buffer, i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event_but);
		    unsigned int type = gpiod_edge_event_get_event_type(event_but);
		    uint64_t timestamp = gpiod_edge_event_get_timestamp_ns(event_but);
		    printf("Line %d type %d timestamp %" PRIu64 "\n", offset, type, timestamp);
		}
	    }	    
	}
	if(gpiod_line_request_wait_edge_events(rpi_buttons_req, 0)){
	    int ret = gpiod_line_request_read_edge_events(rpi_buttons_req, event_rpi_but_buffer, event_rpi_but_buf_size);
	    if(ret > 0){
		for(int i = 0; i < ret; i++){
		    event_rpi_but = gpiod_edge_event_buffer_get_event(event_rpi_but_buffer, i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event_rpi_but);
		    unsigned int type = gpiod_edge_event_get_event_type(event_rpi_but);
		    uint64_t timestamp = gpiod_edge_event_get_timestamp_ns(event_rpi_but);
		    printf("Line %d type %d timestamp %" PRIu64 "\n", offset, type, timestamp);
		}
	    }
	}
*/
    }
    
    //clean everything here
    pinKbd_clean(&fd, pinKbd_obj);

    printf("the pinKbd service is shutting down\n");
    return 0;
}
