#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "timeout_rw.h"

//Set up a dummy hander which does nothing
static void dummy_sig_handler(int s) {return;}

//Performs the following initialization steps for allowing timeout_read and 
//timeout_write:
//  - Initializes a new timer object
//  - Installs a signal handler for SIGUSR1 if it is free. Otherwise, SIGUSR2.
//      -> If neither are free, this function fails and sets errno to EAGAIN
//  - Allocates and fills a timeout_rw context
//Returns the new context on success, NULL on error. errno is set to whatever
//the last C library function used in timeout_rw_init set it to
timeout_rw_ctx* timeout_rw_init() {
    int selected_sig_no = -1;
    struct sigaction sa;
    struct sigevent sigev;
    timeout_rw_ctx *ret;
    
    //Test the waters: is SIGUSR1 free?
    sigaction(SIGUSR1, NULL, &sa);
    if (sa.sa_handler == NULL) {
        selected_sig_no = SIGUSR1;
        goto timeout_rw_init_make_ctx;
    }
    
    //Test the waters: is SIGUSR2 free?
    sigaction(SIGUSR2, NULL, &sa);
    if (sa.sa_handler == NULL) {
        selected_sig_no = SIGUSR2;
    } else {
        //No signals are free. Return NULL.
        errno = EBUSY;
        return NULL;
    }
    
timeout_rw_init_make_ctx:

    ret = malloc(sizeof(timeout_rw_ctx));
    if (!ret) return NULL;
    
    //Save the signal number
    ret->signo = selected_sig_no;
    
    //Tell timer_create which signal we want to use
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = selected_sig_no;
    
    //Create a clock in ret->clk which targets selected_sig_no
    int rc = timer_create(CLOCK_MONOTONIC, &sigev, &ret->clk);
    if (rc < 0) goto err_free_ctx;
    
    //Now register the signal handler
    sa.sa_handler = dummy_sig_handler;
    sa.sa_flags = 0; //Can not use SA_RESTART!
    sigemptyset(&sa.sa_mask);
    
    rc = sigaction(selected_sig_no, &sa, NULL);
    if (rc < 0) goto err_delete_timer;
    
    //Nice job! The signal is registered and we can start using the context
    return ret;


    int tmp;
    
err_delete_timer:
    //Preserve errno
    tmp = errno;
    timer_delete(ret->clk);
    errno = tmp;

err_free_ctx:
    //Preserve errno
    tmp = errno;
    free(ret);
    errno = tmp;
    return NULL;
}

//Frees a context, disabling all active timers and releasing the signal number.
//Gracefulyl returns if passed a NULL pointer.
void timeout_rw_deinit(timeout_rw_ctx *ctx) {
    //Gracefully return if context is NULL
    if (ctx == NULL) return;
    
    timer_delete(ctx->clk);
    
    //Deregister the signal handler
    struct sigaction sa = {
        .sa_handler = SIG_DFL
    };
    int rc = sigaction(ctx->signo, &sa, NULL);
    if (rc < 0) {
        //All we can do is print a message
        fprintf(stderr, "A terrible error has occurred: can't reset the signal handler properly\n");
    }
    free(ctx);
}

static struct itimerspec stop_timer = {
    .it_interval = {.tv_sec = 0, .tv_nsec = 0},
    .it_value = {.tv_sec = 0, .tv_nsec = 0}
};

//Exactly the same as the read() system call, but takes in a timeout_rw context
//and a number of milliseconds. If timeout_ms == 0, then this uses a blocking 
//read
int timeout_read(int fd, void *buf, size_t count, timeout_rw_ctx *ctx, unsigned timeout_ms) {
    int ret;
    
    if (timeout_ms == 0) {
        ret = read(fd, buf, count);
        //Save read() return value and errno to make it more flexible for user
        //to check timeouts without worrying about errno
        ctx->last_errno = errno;
        ctx->last_ret = ret;
        return ret;
    }
    
    struct itimerspec timeout_val = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value = {.tv_sec = (timeout_ms/1000), .tv_nsec = (timeout_ms%1000)*1000000}
    };
    
    //Set the alarm clock
    timer_settime(ctx->clk, 0, &timeout_val, NULL);
    
    //Perform the read
    ret = read(fd, buf, count);
    
    //Save read() return value and errno to make it more flexible for user
    //to check timeouts without worrying about errno
    ctx->last_errno = errno;
    ctx->last_ret = ret;
    
    //Just make sure to cancel the timer if necessary
    if (ret >= 0 || errno == EINTR) {
        timer_settime(ctx->clk, 0, &stop_timer, NULL);
    }
    
    return ret;
}

//Exactly the same as the write() system call, but takes in a timeout_rw context
//and a number of milliseconds. If timeout_ms == 0, then this uses a blocking 
//write
int timeout_write(int fd, void *buf, size_t count, timeout_rw_ctx *ctx, unsigned timeout_ms) {
    int ret;
    
    if (timeout_ms == 0) {
        ret = write(fd, buf, count);
        
        //Save write() return value and errno to make it more flexible for user
        //to check timeouts without worrying about errno
        ctx->last_errno = errno;
        ctx->last_ret = ret;
        
        return ret;
    }
    
    struct itimerspec timeout_val = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value = {.tv_sec = (timeout_ms/1000), .tv_nsec = (timeout_ms%1000)*1000}
    };
    
    //Set the alarm clock
    timer_settime(ctx->clk, 0, &timeout_val, NULL);
    
    //Perform the write
    ret = write(fd, buf, count);
    
    //Save write() return value and errno to make it more flexible for user
    //to check timeouts without worrying about errno
    ctx->last_errno = errno;
    ctx->last_ret = ret;
    
    //Just make sure to cancel the timer if necessary
    if (ret >= 0 || errno == EINTR) {
        timer_settime(ctx->clk, 0, &stop_timer, NULL);
    }
    
    return ret;
}

//Returns 1 if the last call to timeout_read/timeout_write timed out. Note that
//this returns 0 if it was successful, had an error, or encountered EOF
int timed_out(timeout_rw_ctx *ctx) {
    return (ctx->last_ret < 0) && (ctx->last_errno == EINTR);
}

//Returns 1 if the last call to timeout_read/timeout_write encountered EOF. 
//Returns 0 otherwise
int reached_eof(timeout_rw_ctx *ctx) {
    return (ctx->last_ret == 0);
}

//Returns 1 if last call to timeout_read/timeout_write transferred more than
//zero bytes
int successful_transfer(timeout_rw_ctx *ctx) {
    return (ctx->last_ret > 0);
}
