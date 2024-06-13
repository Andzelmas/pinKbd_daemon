#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gpiod.h>
#include <unistd.h>
#include <inttypes.h>
#include <dirent.h>
#include <json-c/json.h>
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
    uint64_t* line_timestamps; //timestamps of the line value writting, useful for button debouncing
    int* intrf_value; //values for lines that are used by the interface
    unsigned int num_of_watched_lines; //how many lines are watched by this event
    struct gpiod_edge_event_buffer* edge_event_buffer; //the buffer that has the line events, its size should be equal to num_of_watched_lines
    unsigned int watch_buttons; //if == 1 this event watches buttons, otherwise - encoders. usefull when there is an event in the buffer and its
    //necessary to get the corresponding encoder or button (usually encoders or buttons index in the array).
    unsigned int chip_num; //chip number for this event in the PINKBD_GPIO_COMM chips array
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
    //clean the uinput emmiter
    if(pinKbd_obj->uinput_fd){
	ioctl(pinKbd_obj->uinput_fd, UI_DEV_DESTROY);
	close(pinKbd_obj->uinput_fd);
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
	    if(curr_event->line_timestamps)free(curr_event->line_timestamps);
	    if(curr_event->intrf_value)free(curr_event->intrf_value);
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
//initiate the PINKBD_GPIO_COMM struct,
//encoder_lines - pin numbers for encoders, dont forget that one encoder has two pins, so this array is in pairs
static PINKBD_GPIO_COMM* pinKbd_init(unsigned int num_of_chips, const char** const chip_paths,
				     const unsigned int* encoder_lines, unsigned int num_of_encoders, const unsigned int* encoder_chip_nums,
				     const unsigned int* button_lines, unsigned int num_of_buttons, const unsigned int* button_chip_nums){
    PINKBD_GPIO_COMM* pinKbd_obj = malloc(sizeof(PINKBD_GPIO_COMM));
    if(!pinKbd_obj)return NULL;
    
    //Initiate the uinput setup
    //-----------------------------------
    pinKbd_obj->uinput_fd = 0;
    struct uinput_setup usetup;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(!fd){
	printf("Could not open file to emmit the keyboard events\n");
	pinKbd_clean(pinKbd_obj);
	return NULL;
    }
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
    ioctl(fd, UI_SET_KEYBIT, KEY_EQUAL);
    ioctl(fd, UI_SET_KEYBIT, KEY_MINUS);
    ioctl(fd, UI_SET_KEYBIT, KEY_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFT);
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
    ioctl(fd, UI_SET_KEYBIT, KEY_UP);
    ioctl(fd, UI_SET_KEYBIT, KEY_DOWN);
    ioctl(fd, UI_SET_KEYBIT, KEY_ENTER);
    
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; // sample vendor
    usetup.id.product = 0x5678; //sample product
    strcpy(usetup.name, "Pisound-Micro Pin to Keypress Device");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    pinKbd_obj->uinput_fd = fd;
    //---------------------------------------------------------
    if(num_of_chips <= 0)return NULL;
    if(!chip_paths)return NULL;
    //initialize to 0
    pinKbd_obj->pin_events = NULL;
    pinKbd_obj->num_of_pin_events = 0;
    
    pinKbd_obj->num_of_chips = num_of_chips;
    pinKbd_obj->chips = malloc(sizeof(struct gpiod_chip*) * num_of_chips);
    if(!(pinKbd_obj->chips)){
	printf("Could not create the chip array\n");
	pinKbd_clean(pinKbd_obj);
	return NULL;
    }
    //initiate the gpio chips on the struct
    for(int i = 0; i<num_of_chips; i++){
	pinKbd_obj->chips[i] = gpiod_chip_open(chip_paths[i]);
	if(!(pinKbd_obj->chips[i])){
	    printf("Could not create the %s chip\n",chip_paths[i]);
	    pinKbd_clean(pinKbd_obj);
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
	pinKbd_clean(pinKbd_obj);
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
	    pinKbd_clean(pinKbd_obj);
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
		    pinKbd_clean(pinKbd_obj);
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
		    pinKbd_clean(pinKbd_obj);
		    return NULL;		    
		}
		btns_lines_for_req[chip_in_btns_num - 1] = button_lines[btn_i];
	    }
	}
	//if there where encoders with this chip create request, event etc.
	if(chip_in_encoders_num > 0 && encoders_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, encoders_lines_for_req, chip_in_encoders_num * 2, chip_in_encoders_num, "pinKbd_line_watch", 0, 0) == -1){
		pinKbd_clean(pinKbd_obj);
		return NULL;
	    }
	}
	//if there where buttons with this chip create request, event etc.
	if(chip_in_btns_num > 0 && btns_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, btns_lines_for_req, chip_in_btns_num, chip_in_btns_num, "pinKbd_line_watch", 0, 1) == -1){
		pinKbd_clean(pinKbd_obj);
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
//chip_num - on what chip the encoder or button was activated
//control_num - the number of encoder or button on the chip
//control_value - the value of the control, 0 - for button release, 1 - push, 1 - for encoder CW rotation, -1 for CCW
//inc_mult how many times the events should be invoked
static int pinKbd_invoke_control(PINKBD_GPIO_COMM* pinKbd_obj, unsigned int chip_num, unsigned int control_num, int control_value, unsigned int inc_mult, unsigned int button){
    if(!pinKbd_obj)return -1;
    int fd = pinKbd_obj->uinput_fd;
    if(!fd)return -1;
    if(inc_mult <= 0) return -1;
    //TODO these cases should be cleanedup and implemented a system which finds the control_num in a conf file and finds the corresponding keypress it should emmit
    //on key press and key realese, or in case of encoders what it should on CW and CCW ticks
    for(int i = 0; i < inc_mult; i++){
	if(chip_num == 0){
	    if(button == 1){
		switch(control_num){
		case 0:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_W, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_W, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 1:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_E, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_E, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 2:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_R, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_R, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 3:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_T, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_T, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 4:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_Y, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_Y, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		}
	    }
	    
	}

	if(chip_num == 1){
	    //encoders on chip 1
	    if(button == 0){
		switch(control_num){
		case 0:
		    if(control_value > 0){			
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 1:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_2, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_2, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_2, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_2, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 2:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_3, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_3, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_3, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_3, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 3:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_4, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_4, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_4, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_4, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 4:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_5, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_5, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_5, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_5, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 5:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_6, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_6, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_6, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_6, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 6:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_7, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_7, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_7, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_7, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 7:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_8, 1);
			emit(fd, EV_KEY, KEY_EQUAL, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_8, 0);
			emit(fd, EV_KEY, KEY_EQUAL, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_8, 1);
			emit(fd, EV_KEY, KEY_MINUS, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_8, 0);
			emit(fd, EV_KEY, KEY_MINUS, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 8:
		    if(control_value > 0){
			emit(fd, EV_KEY, KEY_UP, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_UP, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);			
		    }
		    if(control_value < 0){
			emit(fd, EV_KEY, KEY_DOWN, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_DOWN, 0);		
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;		    
		}		
	    }
	    //buttons on chip 1
	    else if(button == 1){
		switch(control_num){
		case 0:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 1:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_2, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_2, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 2:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_3, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_3, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 3:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_4, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_4, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 4:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_5, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_5, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 5:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_6, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_6, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 6:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_7, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_7, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 7:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_8, 1);
			emit(fd, EV_KEY, KEY_RIGHT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_8, 0);
			emit(fd, EV_KEY, KEY_RIGHT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 8:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_ENTER, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_ENTER, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 9:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_LEFT, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_LEFT, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 10:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_LEFTMETA, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_LEFTMETA, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 11:
		    //TODO the bottom of the three buttons does not have any emmit right now
		    //the idea was to use this as a shift (for example while holding to increase/decrease values faster etc.)
		    //actual shift would not work, now reserved A
		    break;
		case 12:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_S, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_S, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 13:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_D, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_D, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 14:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_F, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_F, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 15:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_G, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_G, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;
		case 16:
		    if(control_value == 0){
			emit(fd, EV_KEY, KEY_H, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    if(control_value == 1){
			emit(fd, EV_KEY, KEY_H, 0);
			emit(fd, EV_SYN, SYN_REPORT, 0);
		    }
		    break;	
		}		
	    }
	}
    }

    return 0;
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
				    pinKbd_invoke_control(pinKbd_obj, curr_event->chip_num, control_num, curr_event->intrf_value[control_num],
							  abs(curr_event->intrf_value[control_num])/enc_sens, 0);
				    curr_event->intrf_value[control_num] = 0;
				}
			    }
			}
			//do for buttons
			else if(curr_event->watch_buttons == 1){
			    curr_event->intrf_value[control_num] += 1;
			    if(curr_event->intrf_value[control_num] > 0){
				pinKbd_invoke_control(pinKbd_obj, curr_event->chip_num, control_num, curr_event->final_values[control_num], 1, 1);
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


//--------------------------------------------------
//JSON funcs
typedef void JSONHANDLE; //void handle for json object, solater these functions can be made into a lib
//read a file_path to a string
static char* app_json_read_to_buffer(const char* file_path){
    char* ret_string = NULL;
    FILE* fp;
    fp = fopen(file_path, "r");
    if(!fp){
	return ret_string;
    }

    ret_string = (char*)malloc(sizeof(char)*JSONFILESIZE+1);
    if(!ret_string){
	goto clean;		
    }
    size_t read_num = fread(ret_string, sizeof(char), JSONFILESIZE, fp);
    int total_read = read_num;

    char* temp_string = NULL;
    while(read_num>=JSONFILESIZE){
	temp_string = realloc(ret_string, (total_read+read_num)*sizeof(char)+1);
	if(!temp_string){
	    goto clean;
	}
	ret_string = temp_string;
	read_num = fread(ret_string+total_read, sizeof(char), JSONFILESIZE, fp);
	total_read+=read_num;	 
    }
    temp_string = realloc(ret_string, total_read*sizeof(char)+1);
    //dont forget the null terminator
    temp_string[total_read] = '\0';
    if(!temp_string){
	goto clean;
    }
    ret_string = temp_string;

clean:
    if(fp)fclose(fp);
    return ret_string;
}

//make a json_object from a file_path
static JSONHANDLE* app_json_tokenise_path(char* file_path){
    char* buffer = NULL;
    struct json_object* parsed_fp = NULL;
    
    buffer = app_json_read_to_buffer(file_path);
    if(!buffer){
	return NULL;
    }
    parsed_fp = json_tokener_parse(buffer);
    free(buffer);
    if(!parsed_fp){
	return NULL;
    }

    return (JSONHANDLE*)parsed_fp;
}

//iterate json file and run proc_func for each json object that is not json_type_object (so a string, array, int etc.)
//proc_func can use the found_json_obj to do something with that object data
static int app_json_iterate_objs_run_callback(JSONHANDLE* in_handle,
				       const char* json_name, const char* json_parent, const char* top_name,
				       void* arg,
				       void(*proc_func)(void*, const char* js_name, const char* parent_name,
							const char* top_node_name, JSONHANDLE* found_handle)){
    struct json_object* parsed_fp = (struct json_object*) in_handle;

    int return_val = 0;

    if(!parsed_fp){
	return -1;
    }
    //iterate through the parsed_fp
    struct json_object_iterator it;
    struct json_object_iterator itEnd;

    it = json_object_iter_begin(parsed_fp);
    itEnd = json_object_iter_end(parsed_fp);
    
    unsigned int iter = 0;
    while (!json_object_iter_equal(&it, &itEnd)) {
	const char* cur_name =  json_object_iter_peek_name(&it);
	struct json_object* rec_obj = NULL;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_get_type(rec_obj)!=json_type_object && json_object_get_type(rec_obj)!=json_type_null){
	    iter+=1;
	    (*proc_func)(arg, json_name, json_parent, top_name, (JSONHANDLE*)rec_obj);
	}
	json_object_iter_next(&it);
    }
    //now go through the objects inside this json object
    //and call this function recursevily
    it = json_object_iter_begin(parsed_fp);
    while(!json_object_iter_equal(&it, &itEnd)){
	const char* cur_name =  json_object_iter_peek_name(&it);	
	struct json_object* rec_obj = NULL;
	const char* parent_name = json_name;
	if(iter<=0)parent_name = json_parent;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_get_type(rec_obj)==json_type_object)
	    app_json_iterate_objs_run_callback((JSONHANDLE*)rec_obj, cur_name, parent_name, top_name,
					  arg, (*proc_func));
	json_object_iter_next(&it);
    }
    
    return return_val;
}
//--------------------------------------------------

int main(){
    //Initiate struct for SIGTERM handling
    //----------------------------------
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(struct sigaction));
    sig_action.sa_handler = term;
    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);
    
    //---------------------------------------------
    //TODO the labels should be in a config file along with lines for buttons and encoders
    //find chip path from a label
    char* chip_rpi_path = pinKbd_return_path_from_label("pinctrl-bcm2711");
    if(!chip_rpi_path){
	printf("could not find raspberry chip \n");
	return -1;
    }
    char* chip_pisound_path = pinKbd_return_path_from_label("pisound-micro-gpio");
    if(!chip_pisound_path){
	printf("could not find pisound chip path \n");
	if(chip_rpi_path)free(chip_rpi_path);
	return -1;
    }
    //TODO again, the lines for buttons and encoders should be in a config file
    //Initiate the gpio
    //---------------------------------------------------
    PINKBD_GPIO_COMM* pinKbd_obj = pinKbd_init(2, (const char*[2]){chip_rpi_path, chip_pisound_path},
					       (const unsigned int[18]){6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}, 9,
					       (const unsigned int[18]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
					       (const unsigned int[22]){24,25,26,27,28,29,30,31,32,34,35,36,0,3,2,4,5,24,23,22,27,17}, 22,
					       (const unsigned int[22]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0});
    free(chip_rpi_path);
    free(chip_pisound_path);
    
    if(!pinKbd_obj){
	return -1;
    }
    
    while(!done){
	pinKbd_update_values(pinKbd_obj, 4);
    }
    
    //clean everything here
    pinKbd_clean(pinKbd_obj);

    printf("pinKbd_dameon shutting down, done cleanup\n");
    return 0;
}
