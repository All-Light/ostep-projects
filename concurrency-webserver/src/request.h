#ifndef __REQUEST_H__
#include <sys/time.h>

void request_handle(int fd, int log_fd, double start_time, int tid);

#endif // __REQUEST_H__
