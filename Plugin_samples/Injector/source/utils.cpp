#include "utils.hpp"
#include <cstring>
#include <nid.hpp>
#include <fcntl.h>
#include <string>
#include <sys/sysctl.h>
#include <ps5/klog.h>
extern "C"     int sceKernelGetProcessName(int pid, char *out);
void write_log(const char* text)
{
	int text_len = printf("%s", text);
	int fd = open("/data/etaHEN/injector_plugin.log", O_WRONLY | O_CREAT | O_APPEND, 0777);
	klog_puts(text);
	if (fd < 0)
	{
		return;
	}
	write(fd, text, text_len);
	close(fd);
}

void plugin_log(const char* fmt, ...)
{
	char msg[0x1000]{};
	va_list args;
	va_start(args, fmt);
	int msg_len = vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	// Append newline at the end
	if (msg[msg_len-1] == '\n')
	{
		write_log(msg);
	}
	else
	{
	     strcat(msg, "\n");
	     write_log(msg);
	}
}