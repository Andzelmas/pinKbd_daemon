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
    //TODO unify everything into a single struct where encoders and buttons are built, now too much copy paste
    //ALSO 3 kinds of event_buffers, offsets etc... should be more elegant and clean
    const char* pisound_chipath = "/dev/gpiochip2";
    struct gpiod_chip* pisound_chip = NULL;
    pisound_chip = gpiod_chip_open(pisound_chipath);

    const char* rpi_chipath = "/dev/gpiochip0";
    struct gpiod_chip* rpi_chip = NULL;
    rpi_chip = gpiod_chip_open(rpi_chipath);
    if(!pisound_chip || !rpi_chip){
	printf("cant find the chip\n");
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    const unsigned int pisound_encoders_offsets[PISOUND_ENC_NUM_LINES] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    const unsigned int pisound_but_offsets[PISOUND_BUT_NUM_LINES] = {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0, 1, 2, 3, 4};
    const unsigned int rpi_but_offsets[RPI_BUT_NUM_LINES] = {17, 27, 22, 23, 24};
    struct gpiod_line_request* pisound_encoders_req = request_input_line(pisound_chip, pisound_encoders_offsets, PISOUND_ENC_NUM_LINES, "pisound_encoders_lines_watch", 0);
    struct gpiod_line_request* pisound_buttons_req = request_input_line(pisound_chip, pisound_but_offsets, PISOUND_BUT_NUM_LINES, "pisound_but_lines_watch", 0);
    struct gpiod_line_request* rpi_buttons_req = request_input_line(rpi_chip, rpi_but_offsets, RPI_BUT_NUM_LINES, "rpi_but_lines_watch", 1);
    if(!pisound_encoders_req || !pisound_buttons_req || !rpi_buttons_req){
	printf("no line \n");
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    struct gpiod_edge_event_buffer* event_encoders_buffer = NULL;
    struct gpiod_edge_event* event_encoder = NULL;
    int event_encoders_buf_size = PISOUND_ENC_NUM_LINES;
    struct gpiod_edge_event_buffer* event_but_buffer = NULL;
    struct gpiod_edge_event* event_but = NULL;
    int event_but_buf_size = PISOUND_BUT_NUM_LINES;
    struct gpiod_edge_event_buffer* event_rpi_but_buffer = NULL;
    struct gpiod_edge_event* event_rpi_but = NULL;
    int event_rpi_but_buf_size = RPI_BUT_NUM_LINES;   
    event_encoders_buffer = gpiod_edge_event_buffer_new(event_encoders_buf_size);
    event_but_buffer = gpiod_edge_event_buffer_new(event_but_buf_size);
    event_rpi_but_buffer = gpiod_edge_event_buffer_new(event_rpi_but_buf_size);
    if(!event_encoders_buffer || !event_but_buffer || !event_rpi_but_buffer){
	clean(&fd, NULL, pisound_chip);
	return -1;
    }
    
    //sleep after initiate so that the system has time to register the device this can be deleted
    //when the emit will emit events only when encoders or buttons are used
    //sleep(1);
    //TODO these variables should be per encoder
    //-----------------------------------------
    unsigned int l_A = 0;
    unsigned int c_A = 0;
    unsigned int l_B = 0;
    unsigned int c_B = 0;
    unsigned char final_num = 0;
    unsigned char final_num_last = 0;
    int enc_count = 0;
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
		    if(enc_count >= 2){
			printf("CW Rotation \n");
			emit(fd, EV_KEY, KEY_LEFTSHIFT, 1);			
			emit(fd, EV_KEY, KEY_1, 1);
			emit(fd, EV_SYN, SYN_REPORT, 0);
			emit(fd, EV_KEY, KEY_1, 0);
			emit(fd, EV_KEY, KEY_LEFTSHIFT, 0);			
			emit(fd, EV_SYN, SYN_REPORT, 0);
			enc_count = 0;
		    }
		    if(enc_count <= -2){
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
    }
    
    //clean everything here
    clean(&fd, NULL, pisound_chip);

    printf("the pinKbd service is shutting down\n");
    return 0;
}
