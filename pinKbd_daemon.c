#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

volatile sig_atomic_t done = 0;

static void term(int signum){
    done = 1;
}

int main(){
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(struct sigaction));
    sig_action.sa_handler = term;
    sigaction(SIGTERM, &sig_action, NULL);
    
    int i = 0;
    while(!done){
	if(i >= 500000000){
	    printf("pinKbd_daemon is running %d \n", i);
	    i = 0;
	}
	i += 1;
    }

    printf("the pinKbd service is shutting down\n");
    return 0;
}
