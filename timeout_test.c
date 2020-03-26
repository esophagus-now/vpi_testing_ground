#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "timeout_rw.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./fifo_watch FIFO_FILEPATH\n");
        return -1;
    }
    
    timeout_rw_ctx *ctx = timeout_rw_init();
    if (ctx == NULL) {
        perror("timeout_init failed");
        return -1;
    }
    
    int fd = open(argv[1], TIMEOUT_RW_OPEN_FLAGS);
    if (fd < 0) {
        char msg[80];
        sprintf(msg, "Could not open [%s]", argv[1]);
        perror(msg);
        timeout_rw_deinit(ctx);
        return -1;
    }
    
    while (1) {
        char buf[80];
        int len;
        
        len = timeout_read(fd, buf, 80, ctx, 1000);
        
        if (timed_out(ctx)) {
            puts("\t[Time out]");
            continue;
        } else if (reached_eof(ctx)) {
            puts("Reach end of file");
            break;
        } else if (len < 0) {
            fprintf(stderr, "timeout_read failed: %s\n", timeout_rw_strerror(ctx));
            break;
        }
        
        write(STDOUT_FILENO, buf, len);
    }
    
    timeout_rw_deinit(ctx);
    close(fd);
}
