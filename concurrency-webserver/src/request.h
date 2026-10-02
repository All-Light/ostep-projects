#ifndef __REQUEST_H__
#define MAX_STR_LEN 256
void request_handle(int fd, unsigned int buffer_size, char log_buffer[][MAX_STR_LEN], unsigned int* buffer_count, double program_start_time, double task_start_time, int tid);
#endif // __REQUEST_H__
