#ifndef TIMEOUT_RW_H
#define TIMEOUT_RW_H 1


//Stores state information for this process
typedef struct _timeout_rw_ctx {
    timer_t clk;
    int signo;
} timeout_rw_ctx;

//Performs the following initialization steps for allowing timeout_read and 
//timeout_write:
//  - Initializes a new timer object
//  - Installs a signal handler for SIGUSR1 if it is free. Otherwise, SIGUSR2.
//      -> If neither are free, this function fails and sets errno to EAGAIN
//  - Allocates and fills a timeout_rw context
//Returns the new context on success, NULL on error. errno is set to whatever
//the last C library function used in timeout_rw_init set it to
timeout_rw_ctx* timeout_rw_init();

#endif
