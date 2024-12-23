#include "tcp.h"
#include <pthread.h>
#include <unistd.h>

void *start_ftp(void *args);
void *klog(void *args);
void *krw_server(void *args);

int main(void) {
	for (;;) {
		pthread_t ftp = NULL;
		pthread_t klog_thread = NULL;
		pthread_create(&ftp, NULL, start_ftp, NULL);
		pthread_create(&klog_thread, NULL, klog, NULL);
		pthread_join(ftp, NULL);
		pthread_join(klog_thread, NULL);
		usleep(SLEEP_PERIOD);
	}
	return 0;
}
