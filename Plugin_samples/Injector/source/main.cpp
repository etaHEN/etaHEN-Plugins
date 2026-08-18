#include "utils.hpp"
#include <notify.hpp>
#include <signal.h>
#include <ps5/klog.h>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define ETAHEN_PATH "/data/etaHEN"
#define PLUGIN_CONFIG_PATH ETAHEN_PATH "/Injector.ini"
#define INJECTOR_DIR "/data/InjectorPlugin"

extern "C"
{
	int sceSystemServiceGetAppIdOfRunningBigApp();
	int sceSystemServiceGetAppTitleId(int app_id, char *title_id);
}

extern uint8_t elf_start[];
extern const unsigned int elf_size;

void sig_handler(int signo)
{
	printf_notification("the error disabler plugin has crashed with signal %d\nif you need it you can relaunch via the etaHEN toolbox in debug settings", signo);
	printBacktraceForCrash();
	exit(-1);
}

#define MAX_PROC_NAME 0x100

bool file_exists(const char* filename) {
    struct stat buff;
    return stat(filename, &buff) == 0 ? true : false;
}

bool Get_Running_App_TID(std::string &title_id, int &BigAppid)
{
	char tid[255];
	BigAppid = sceSystemServiceGetAppIdOfRunningBigApp();
	if (BigAppid < 0)
	{
		return false;
	}
	(void)memset(tid, 0, sizeof tid);

	if (sceSystemServiceGetAppTitleId(BigAppid, &tid[0]) != 0)
	{
		return false;
	}

	title_id = std::string(tid);
	return true;
}

int send_injector_data(const char *ip, int port,
					   const char *proc_name,
					   const uint8_t *data, size_t data_size)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		klog_perror("socket");
		return -1;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
	{
		klog_perror("inet_pton");
		close(sock);
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		klog_perror("connect");
		close(sock);
		return -1;
	}

	uint8_t header[MAX_PROC_NAME] = {0};
	size_t name_len = strlen(proc_name);

	if (name_len > MAX_PROC_NAME)
		name_len = MAX_PROC_NAME;

	memcpy(header, proc_name, name_len);

	// Send header
	if (send(sock, header, MAX_PROC_NAME, 0) != MAX_PROC_NAME)
	{
		klog_perror("send header");
		close(sock);
		return -1;
	}

	// Send payload
	ssize_t sent = send(sock, data, data_size, 0);
	if (sent < 0 || (size_t)sent != data_size)
	{
		klog_perror("send data");
		close(sock);
		return -1;
	}

	plugin_log("Sent %zu bytes to %s:%d\n", MAX_PROC_NAME + data_size, ip, port);

	close(sock);
	return 0;
}

char* trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void execute_payload_command(char * line) {
	line = trim(line);
    if (strlen(line) == 0 || line[0] == ';') return;

    if (line[0] == '!') {
        int seconds = atoi(line + 1);
        plugin_log("Config: sleeping for %d seconds\n", seconds);
        sleep(seconds);
        return;
    }

    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", INJECTOR_DIR, line);

    struct stat st;
    if (stat(fullpath, &st) < 0) {
        plugin_log("File not found: %s\n", fullpath);
        return;
    }

    FILE* f = fopen(fullpath, "rb");
    if (f) {
        uint8_t* buf = (uint8_t*)malloc(st.st_size);
        fread(buf, 1, st.st_size, f);
        fclose(f);

        plugin_log("Injecting: %s\n", line);
        if (send_injector_data("127.0.0.1", 9033, "eboot.bin", buf, st.st_size)) {
            printf_notification("Injected: %s", line);
        }
        free(buf);
    }
}

void process_config_section(const char* target_section) {
	if (!file_exists(PLUGIN_CONFIG_PATH)) {
        printf_notification("Not found gamepad.ini config file\n");
        return;
    }


    FILE* file = fopen(PLUGIN_CONFIG_PATH, "r");

    if (!file) {
        plugin_log("!file_open: %s (errno: %d)\n", strerror(errno), errno);
        return;
    }

    char line[512];
    char target_header[128];
    snprintf(target_header, sizeof(target_header), "[%s]", target_section);

    int in_target_section = 0;

    while (fgets(line, sizeof(line), file)) {
        char* trimmed = trim(line);
        
        if (trimmed[0] == '\0') continue;
        
        if (trimmed[0] == '[') {
            if (strcmp(trimmed, target_header) == 0) {
                in_target_section = 1;
            } else {
                in_target_section = 0;
            }
            continue;
        }

        if (in_target_section) {
            execute_payload_command(trimmed);
        }
    }

    fclose(file);
}
void send_all_payloads(const char * tid) {
    printf_notification("Starting Injection Sequence");

    plugin_log("Processing [default] section\n");
    process_config_section("default");

    plugin_log("Processing [%s] section\n", tid);
    process_config_section(tid);

    printf_notification("All payloads finished");
}

uintptr_t kernel_base = 0;
bool plugin_just_started = true;
int main()
{
	payload_args_t *args = payload_get_args();
	kernel_base = args->kdata_base_addr;

	struct sigaction new_SIG_action;
	new_SIG_action.sa_handler = sig_handler;
	sigemptyset(&new_SIG_action.sa_mask);
	new_SIG_action.sa_flags = 0;

	for (int i = 0; i < 12; i++)
		sigaction(i, &new_SIG_action, NULL);

	std::string tid;
	int bappid, last_bappid = -1;
	while (true)
	{
		if (Get_Running_App_TID(tid, bappid))
		{
			if ((bappid != last_bappid) && (tid.rfind("CUSA") != std::string::npos || tid.rfind("SCUS") != std::string::npos))
			{
				printf_notification("Game launch detected. Inject in 10s");
				sleep(plugin_just_started ? 1 : 10);
				int bappid_tmp;
				if (!Get_Running_App_TID(tid, bappid_tmp))
					continue;
				if (bappid == bappid_tmp)
				{
					send_all_payloads(tid.c_str());
					last_bappid = bappid;
				} else {
					plugin_log("Abort injection. Maybe your app closed");
				}
			}
		}
		plugin_just_started = false;
		sleep(5);
	}

	return 0;
}
