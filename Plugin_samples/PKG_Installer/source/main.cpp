#include <dlfcn.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "sceAppInstUtil.h"

typedef struct notify_request
{
    char useless1[45];
    char message[3075];
} notify_request_t;

extern "C"
{
    int sceKernelSendNotificationRequest(int, notify_request_t *, size_t, int);
}

void notify(const char *fmt, ...)
{
    notify_request_t req;
    va_list args;

    bzero(&req, sizeof req);
    va_start(args, fmt);
    vsnprintf(req.message, sizeof req.message, fmt, args);
    va_end(args);

    sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

int main()
{
	notify("In App");
	
	int ret = sceAppInstUtilInitialize();
	if(ret){
		notify("sceAppInstUtilInitialize failed: 0x%08X\n", ret);
	   return -1;
	}
	
	PlayGoInfo arg3;
	SceAppInstallPkgInfo pkg_info;
	(void)memset(&arg3, 0, sizeof(arg3));
	
	for (size_t i = 0; i < NUM_LANGUAGES; i++) {
		strncpy(arg3.languages[i], "", sizeof(arg3.languages[i]) - 1);
	}
	
	for (size_t i = 0; i < NUM_IDS; i++) {
		 strncpy(arg3.playgo_scenario_ids[i], "",
					sizeof(playgo_scenario_id_t) - 1);
		 strncpy(*arg3.content_ids, "", sizeof(content_id_t) - 1);
	}
	
	MetaInfo in = {
		.uri = "/data/ps4_smb_client.pkg",
		.ex_uri = "",
		.playgo_scenario_id = "",
		.content_id = "",
		.content_name = "PKG TITLE",
		.icon_url = ""
	};
	
	int num = sceAppInstUtilInstallByPackage(&in, &pkg_info, &arg3);
	if (num == 0) {
		notify("Download and Install console Task initiated");
	} else {
		notify("DPI: Install failed with error code %d\n", num);
	}
	/* float prog = 0;
	SceAppInstallStatusInstalled status;
	
	while (strcmp(status.status, "playable") != 0) {
		sceAppInstUtilGetInstallStatus(pkg_info.content_id, &status);
		
		if (status.total_size != 0) {
			prog = ((float)status.downloaded_size / status.total_size) * 100.0f;
		}
	
		notify("DPI: Status: %s | error: %d | progress %.2f%% (%llu/%llu)\n", 
				   status.status, status.error_info.error_code, 
				   prog, status.downloaded_size, status.total_size);
	} */
	return 0;
}
