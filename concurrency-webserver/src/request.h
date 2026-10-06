#ifndef __REQUEST_H__
#include <stdbool.h>
#define MAX_STR_LEN 256
int request_handle(int fd, char log_buffer[][MAX_STR_LEN], unsigned int* buffer_count, bool logging_enabled, double program_start_time, double task_start_time, int tid);
#endif // __REQUEST_H__
