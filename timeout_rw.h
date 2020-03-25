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
//      -> If neither are free, this function fails and sets errno to EBUSY
//  - Allocates and fills a timeout_rw context
//Returns the new context on success, NULL on error. errno is set to whatever
//the last C library function used in timeout_rw_init set it to
timeout_rw_ctx* timeout_rw_init();

//Frees a context, disabling all active timers and releasing the signal number.
//Gracefulyl returns if passed a NULL pointer.
void timeout_rw_deinit(timeout_rw_ctx *ctx);

//Exactly the same as the read() system call, but takes in a timeout_rw context
//and a number of milliseconds. If timeout_ms == 0, then this uses a blocking 
//read
int timeout_read(int fd, void *buf, size_t count, timeout_rw_ctx *ctx, unsigned timeout_ms);

//Exactly the same as the write() system call, but takes in a timeout_rw context
//and a number of milliseconds. If timeout_ms == 0, then this uses a blocking 
//write
int timeout_write(int fd, void *buf, size_t count, timeout_rw_ctx *ctx, unsigned timeout_ms);
#endif
