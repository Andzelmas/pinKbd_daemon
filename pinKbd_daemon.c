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
    unsigned int num_of_watched_lines; //how many lines are watched by this event
    struct gpiod_edge_event_buffer* edge_event_buffer; //the buffer that has the line events, its size should be equal to num_of_watched_lines
    unsigned int watch_buttons; //if == 1 this event watches buttons, otherwise - encoders. usefull when there is an event in the buffer and its
    //necessary to get the corresponding encoder or button (usually encoders or buttons index in the array).
}PINKBD_EVENT;
//encoder struct
typedef struct _pinKbd_ENCODER{
    struct gpiod_chip* chip; //the chip this encoder uses for the gpio lines
    unsigned int lines[2]; //encoder pin numbers for the two channels
    unsigned int curr_edge_0; //current edge (0 or 1) for the lines[0]
    unsigned int curr_edge_1; //current edge (0 or 1) for the lines[1]
    unsigned char final_num; //the 8-bits that hold the sequence of edge values, from this its possible to know if encoder rotates CW or CCW
    unsigned char final_num_last; //the last 8-bit sequence that had the edge values to compare to final_num so encoder tick is announced only when final_num changes
}PINKBD_ENCODER;
//button struct
typedef struct _pinKbd_BUTTON{
    struct gpiod_chip* chip; //the chip this button uses for the gpio lines
    unsigned int line; //button pin number
}PINKBD_BUTTON;

//struct that holds all of the gpio communication
typedef struct _pinKbd_GPIO_COMM{
    struct gpiod_chip** chips;
    unsigned int num_of_chips;
    //array of the encoders
    PINKBD_ENCODER** encoders;
    unsigned int num_of_encoders;
    //array of the buttons
    PINKBD_BUTTON** buttons;
    unsigned int num_of_buttons;
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
    }
    //free the encoders and buttons
    if(pinKbd_obj->encoders){
	for(int i = 0; i < pinKbd_obj->num_of_encoders; i++){
	    if(pinKbd_obj->encoders[i])free(pinKbd_obj->encoders[i]);
	}
	free(pinKbd_obj->encoders);
    }
    if(pinKbd_obj->buttons){
	for(int i = 0; i < pinKbd_obj->num_of_buttons; i++){
	    if(pinKbd_obj->buttons[i])free(pinKbd_obj->buttons[i]);
	}
	free(pinKbd_obj->buttons);
    }

    //clean the events
    if(pinKbd_obj->pin_events){
	for(int i = 0; i < pinKbd_obj->num_of_pin_events; i++){
	    PINKBD_EVENT* curr_event = pinKbd_obj->pin_events[i];
	    if(!curr_event)continue;
	    if(curr_event->edge_event_buffer)gpiod_edge_event_buffer_free(curr_event->edge_event_buffer);
	    if(curr_event->event_request)gpiod_line_request_release(curr_event->event_request);
	    if(curr_event->watched_lines)free(curr_event->watched_lines);
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

static int pinKbd_init_event(PINKBD_GPIO_COMM* pinKbd_obj, unsigned int num_of_events,
			     struct gpiod_chip* curr_chip, unsigned int* lines_for_req, unsigned int lines_num, const char* consumer,
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
    curr_event->edge_event_buffer = gpiod_edge_event_buffer_new(curr_event->num_of_watched_lines);
    if(!(curr_event->edge_event_buffer)){
	printf("Could not create edge_event_buffer for the event\n");
	return -1;
    }
    return 0;
}

static PINKBD_GPIO_COMM* pinKbd_init(unsigned int num_of_chips, const char** const chip_paths,
				     const unsigned int* encoder_lines, unsigned int num_of_encoders, const unsigned int* encoder_chip_nums,
				     const unsigned int* button_lines, unsigned int num_of_buttons, const unsigned int* button_chip_nums){
    PINKBD_GPIO_COMM* pinKbd_obj = malloc(sizeof(PINKBD_GPIO_COMM));
    if(!pinKbd_obj)return NULL;
    if(num_of_chips <= 0)return NULL;
    if(!chip_paths)return NULL;
    //initialize to 0
    pinKbd_obj->encoders = NULL;
    pinKbd_obj->buttons = NULL;
    pinKbd_obj->pin_events = NULL;
    pinKbd_obj->num_of_encoders = 0;
    pinKbd_obj->num_of_buttons = 0;
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
    //initiate the encoders
    if(num_of_encoders > 0){
	pinKbd_obj->num_of_encoders = num_of_encoders;
	pinKbd_obj->encoders = malloc(sizeof(PINKBD_ENCODER*) * num_of_encoders);
	if(!(pinKbd_obj->encoders)){
	    printf("Could not create the encoders array\n");
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}
    }
    unsigned int get_enc_line = 0;
    for(int i = 0; i < num_of_encoders; i++){
	PINKBD_ENCODER* curr_enc = malloc(sizeof(PINKBD_ENCODER));
	unsigned int curr_chip_num = encoder_chip_nums[i];	
	if(!curr_enc || curr_chip_num >= pinKbd_obj->num_of_chips){
	    printf("Could not create the %d encoder or the chip number for this encoder exceeds the num_of_chips\n", i);
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}
	pinKbd_obj->encoders[i] = curr_enc;
	curr_enc->chip = pinKbd_obj->chips[curr_chip_num];
	curr_enc->lines[0] = encoder_lines[get_enc_line];
	curr_enc->lines[1] = encoder_lines[get_enc_line + 1];
	curr_enc->curr_edge_0 = 0;
	curr_enc->curr_edge_1 = 0;
	curr_enc->final_num = 0;
	curr_enc->final_num_last = 0;
	get_enc_line += 2;
    }
    //initiate the buttons
    if(num_of_buttons > 0){
	pinKbd_obj->num_of_buttons = num_of_buttons;
	pinKbd_obj->buttons = malloc(sizeof(PINKBD_BUTTON*) * num_of_buttons);
	if(!(pinKbd_obj->buttons)){
	    printf("Could not create the buttons array\n");
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}	
    }
    for(int i = 0; i < num_of_buttons; i++){
	PINKBD_BUTTON* curr_btn = malloc(sizeof(PINKBD_BUTTON));
	unsigned int curr_chip_num = button_chip_nums[i];	
	if(!curr_btn || curr_chip_num >= pinKbd_obj->num_of_chips){
	    printf("Could not create the %d button or the chip number for this encoder exceeds the num_of_chips\n", i);
	    pinKbd_clean(NULL, pinKbd_obj);
	    return NULL;
	}
	pinKbd_obj->buttons[i] = curr_btn;
	curr_btn->chip = pinKbd_obj->chips[curr_chip_num];
	curr_btn->line = button_lines[i];
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
	unsigned int encoders_lines_iterator = 0; //encoders have to pin numbers so increase this +2 when an encoder is found
	for(int enc_i = 0; enc_i < pinKbd_obj->num_of_encoders; enc_i++){
	    PINKBD_ENCODER* curr_enc = pinKbd_obj->encoders[enc_i];
	    if(!curr_enc)continue;
	    if(curr_enc->chip == curr_chip){
		chip_in_encoders_num += 1;
		encoders_lines_for_req = realloc(encoders_lines_for_req, sizeof(unsigned int) * (chip_in_encoders_num * 2));
		if(!encoders_lines_for_req){
		    printf("Failed events initialization - could not realloc encoders_lines \n");
		    pinKbd_clean(NULL, pinKbd_obj);
		    return NULL;
		}
		encoders_lines_for_req[encoders_lines_iterator] = curr_enc->lines[0];
		encoders_lines_for_req[encoders_lines_iterator + 1] = curr_enc->lines[1];
		encoders_lines_iterator += 2;
	    }
	}
	//go through the buttons
	for(int btn_i = 0; btn_i < pinKbd_obj->num_of_buttons; btn_i++){
	    PINKBD_BUTTON* curr_btn = pinKbd_obj->buttons[btn_i];
	    if(!curr_btn)continue;
	    if(curr_btn->chip == curr_chip){
		chip_in_btns_num += 1;
		btns_lines_for_req = realloc(btns_lines_for_req, sizeof(unsigned int) * (chip_in_btns_num));
		if(!btns_lines_for_req){
		    printf("Failed events initialization - could not realloc btns_lines \n");
		    pinKbd_clean(NULL, pinKbd_obj);
		    return NULL;		    
		}
		btns_lines_for_req[chip_in_btns_num - 1] = curr_btn->line;
	    }
	}
	//if there where encoders with this chip create request, event etc.
	if(chip_in_encoders_num > 0 && encoders_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, encoders_lines_for_req, chip_in_encoders_num * 2, "pinKbd_line_watch", 0, 0) == -1){
		pinKbd_clean(NULL, pinKbd_obj);
		return NULL;
	    }
	}
	//if there where buttons with this chip create request, event etc.
	if(chip_in_btns_num > 0 && btns_lines_for_req){
	    num_of_events += 1;
	    if(pinKbd_init_event(pinKbd_obj, num_of_events, curr_chip, btns_lines_for_req, chip_in_btns_num, "pinKbd_line_watch", 0, 0) == -1){
		pinKbd_clean(NULL, pinKbd_obj);
		return NULL;
	    }	    
	}
	if(chip_in_encoders_num <= 0)
	    free(encoders_lines_for_req);
	if(chip_in_btns_num <= 0)
	    free(btns_lines_for_req);
    }
    pinKbd_obj->num_of_pin_events = num_of_events;
    printf("Created %d events \n", pinKbd_obj->num_of_pin_events);
    return pinKbd_obj;
}

int main(){
    //Initiate struct for SIGTERM handling
    //----------------------------------
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(struct sigaction));
    sig_action.sa_handler = term;
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
    PINKBD_GPIO_COMM* pinKbd_obj = pinKbd_init(2, (const char*[2]){"/dev/gpiochip0", "/dev/gpiochip2"}, (const unsigned int[4]){6,7,8,9}, 2,
					       (const unsigned int[2]){1,1}, (const unsigned int[2]){23,24}, 1, (const unsigned int[2]){0, 0});
    if(!pinKbd_obj){
	pinKbd_clean(&fd, NULL);
	return -1;
    }
    
    
    //sleep after initiate so that the system has time to register the device this can be deleted
    //when the emit will emit events only when encoders or buttons are used
    //sleep(1);
    //TODO these variables should be per encoder
    //-----------------------------------------
    /*
    unsigned int l_A = 0;
    unsigned int c_A = 0;
    unsigned int l_B = 0;
    unsigned int c_B = 0;
    unsigned char final_num = 0;
    unsigned char final_num_last = 0;
    int enc_count = 0;
    */
    //-----------------------------------------
    while(!done){
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
