#include "installer.h"
#include "net.h"
#include "http.h"
#include "server.h"
#include "util.h"


#define SERVER_PORT (12800)

int sceUserMainThreadPriority = 700;

size_t sceUserMainThreadStackSize = 512 * 1024;
size_t sceLibcHeapSize = 256 * 1024 * 1024;


int main(int argc, const char* const argv[]) 
{
	char* work_dir;
	char ip_address[16];
	int ret;

	//printf("Initializing user service...\n");
	ret = sceUserServiceInitialize(NULL);
	if (ret) {
		EPRINTF("User service initialization failed.\n");
		goto err;
	}

	work_dir = "/data";
	//printf("Working directory: %s\n", work_dir);

	//printf("Initializing AppInstUtil...\n");
	if (!app_inst_util_init()) {
		EPRINTF("AppInstUtil initialization failed.\n");
		goto err_user_service_terminate;
	}

	//printf("Initializing net...\n");
	if (!net_init()) {
		EPRINTF("Net initialization failed.\n");
		goto err_bgft_finalize;
	}

	ret = net_get_ipv4(ip_address, sizeof(ip_address));
	if (ret) {
		EPRINTF("Unable to get IP address: 0x%08X\n", ret);
		goto err_net_finalize;
	}

	//printf("Initializing HTTP/SSL...\n");
	 if (!http_init()) {
	 	EPRINTF("HTTP/SSL initialization failed.\n");
		goto err_net_finalize;
	 }

	//printf("Starting server...\n");
	if (!server_start(ip_address, SERVER_PORT, work_dir)) {
		EPRINTF("Server start failed.\n");
		goto err_http_finalize;
	}

	printf("Listening for incoming connections on %s:%d...\n", ip_address, SERVER_PORT);
	if (!server_listen()) {
		goto err_server_stop;
	}

err_server_stop:
	//printf("Stopping server...\n");
	server_stop();

err_http_finalize:
	//printf("Finalizing HTTP/SSL...\n");
	http_fini();

err_net_finalize:
	//printf("Finalizing net...\n");
	net_fini();

err_bgft_finalize:
	//printf("Finalizing BGFT...\n");
	bgft_fini();

err_appinstutil_finalize:
	//printf("Finalizing AppInstUtil...\n");
	app_inst_util_fini();

err_user_service_terminate:
	//printf("Terminating user service...\n");
	ret = sceUserServiceTerminate();
	if (ret) {
		EPRINTF("sceUserServiceTerminate failed: 0x%08X\n", ret);
	}

err:;

done:
	exit(0);
	return 0;
}


void catchReturnFromMain(int exit_code) {}
