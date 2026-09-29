#ifndef __REQUEST_H__

void request_handle(int fd, int log_fd, double program_start_time, double task_start_time, int tid, pthread_mutex_t log_lock);
#endif // __REQUEST_H__
