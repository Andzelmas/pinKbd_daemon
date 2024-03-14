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
//how many lines to read from pisound gpio
#define PISOUND_NUM_LINES 3
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

//clean everything function
//TODO create a convenient struct and clean not only chips but lines etc.
//and send here the single struct not a variable per chip, line etc
static void clean(int* uinput_fd, struct gpiod_chip* rpi_chip, struct gpiod_chip* pisound_chip){
    ioctl(*uinput_fd, UI_DEV_DESTROY);
    close(*uinput_fd);

    if(rpi_chip)
	gpiod_chip_close(rpi_chip);
    if(pisound_chip)
	gpiod_chip_close(pisound_chip);
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
    //gpiod_line_settings_set_debounce_period_us(settings, 1000);
    
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
    //TODO user should be able to change chip names in a cofig file
    //(maybe same one where keyboard shortcuts are changed)
    const char* pisound_chipath = "/dev/gpiochip2";
    struct gpiod_chip* pisound_chip = NULL;
    pisound_chip = gpiod_chip_open(pisound_chipath);
    if(!pisound_chip){
	printf("cant find the chip\n");
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    const unsigned int pisound_offsets[PISOUND_NUM_LINES] = {6, 7, 34};
    struct gpiod_line_request* pisound_lines = request_input_line(pisound_chip, pisound_offsets, PISOUND_NUM_LINES, "pisound_mult_lines_watch");
    if(!pisound_lines){
	printf("no line \n");
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    struct gpiod_edge_event_buffer* event_buffer = NULL;
    struct gpiod_edge_event* event = NULL;
    int event_buf_size = PISOUND_NUM_LINES;
    event_buffer = gpiod_edge_event_buffer_new(event_buf_size);
    if(!event_buffer){
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    
    //sleep after initiate so that the system has time to register the device this can be deleted
    //when the emit will emit events only when encoders or buttons are used
    //sleep(1);
    while(!done){
	//TODO only for testing, should emit only when encoder or button is used
	/*
	emit(fd, EV_KEY, KEY_1, 1);
	emit(fd, EV_SYN, SYN_REPORT, 0);
	emit(fd, EV_KEY, KEY_1, 0);
	emit(fd, EV_SYN, SYN_REPORT, 0);
	*/
	if(gpiod_line_request_wait_edge_events(pisound_lines, 0)){
	    int ret = gpiod_line_request_read_edge_events(pisound_lines, event_buffer, event_buf_size);
	    if(ret > 0){
		for(int i = 0; i < ret; i++){
		    event = gpiod_edge_event_buffer_get_event(event_buffer, i);
		    unsigned int offset = gpiod_edge_event_get_line_offset(event);
		    unsigned int type = gpiod_edge_event_get_event_type(event);
		    uint64_t timestamp = gpiod_edge_event_get_timestamp_ns(event);
		    printf("Line %d type %d timestamp %" PRIu64 "\n", offset, type, timestamp);
		}
	    }
	}
    }
    
    //clean everything here
    clean(&fd, NULL, pisound_chip);

    printf("the pinKbd service is shutting down\n");
    return 0;
}
