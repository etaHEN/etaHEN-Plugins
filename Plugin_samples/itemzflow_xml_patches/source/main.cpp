#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/_pthreadtypes.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include "dbg.hpp"
#include "dbg/dbg.hpp"
#include <notify.hpp>
#include "elf/elf.hpp"
#include "fd.hpp"
#include "hijacker/hijacker.hpp"
#include "servers.hpp"
#include "util.hpp"
#include "notify.hpp"
#define STANDALONE 1 // sendable using nc (no host features, scripts will not work in this mode)
#define RESTMODE 1	 // able to enter sleep mode (no host features, scripts will not work in this mode)

#include <pthread.h>
#include "game_patch_thread.hpp"

extern void makenewapp();
extern "C" void free(void *);

extern "C" ssize_t _read(int, void *, size_t);
extern "C" ssize_t _write(int, void *, size_t);

void AbortServer::run(TcpSocket &sock)
{
	// any connection signals to shutdown the daemon
	puts("abort signal received");
	sock.close();
}
extern "C" int sceSystemServiceKillApp(int, int, int, int);
extern "C" int sceSystemServiceGetAppId(const char *);
#ifdef RESTMODE
#define BUILD_MSG "Rest Mode Build"
#else
#define BUILD_MSG "Non Rest Mode Build"
#endif
#include "backtrace.hpp"
extern "C" int sceSystemServiceLoadExec(const char *path, void* args);
void sig_handler(int signo)
{
	printf_notification("the Itemzflow XML Patches plugin has crashed with signal %d", signo);
	printBacktraceForCrash();
    exit(-1);
}

#include "game_patch_xml_cfg.hpp"
uintptr_t kernel_base = 0;
int main()
{
	puts("daemon entered");
	struct sigaction new_SIG_action;
	new_SIG_action.sa_handler = sig_handler;
	sigemptyset(&new_SIG_action.sa_mask);
	new_SIG_action.sa_flags = 0;

	
	sigaction(11, &new_SIG_action, NULL);
	sigaction(10, &new_SIG_action, NULL);
	sigaction(12, &new_SIG_action, NULL);
	sigaction(6, &new_SIG_action, NULL);
	sigaction(7, &new_SIG_action, NULL);
	

	mkdir(BASE_ETAHEN_PATCH_PATH, 0777);
	mkdir(BASE_ETAHEN_PATCH_SETTINGS_PATH, 0777);
	mkdir(BASE_ETAHEN_PATCH_DATA_PATH_PS4, 0777);
	mkdir(BASE_ETAHEN_PATCH_DATA_PATH_PS5, 0777);

	payload_args_t *args = payload_get_args();
	kernel_base = args->kdata_base_addr;

	unlink("/data/etaHEN/cheat_plugin.log");

	printf_notification("The Itemzflow XML Cheats plugin has started\n%s\nSpecial Thanks to illusion", BUILD_MSG);
	// remove this when it's possible to load elf into games at boot
	pthread_t game_patch_thread_id = nullptr;
	pthread_create(&game_patch_thread_id, nullptr, GamePatch_Thread, nullptr);

	g_game_patch_thread_running = true;
#ifdef RESTMODE
	while (g_game_patch_thread_running)
	{
		sleep(0x420);
	}
#else

	AbortServer abortServer{};

	abortServer.TcpServer::run();

	// finishes on connect
	abortServer.join();
	puts("abort thread finished");
#endif
	g_game_patch_thread_running = false;
	puts("g_game_patch_thread_running = false");
#ifdef RESTMODE
#else
	commandServer.stop();
	puts("command server done");
	puts("stopping elf handler");
	serverSock = nullptr; // closed the socket
	pthread_kill(elfHandler, SIGUSR1);
	pthread_join(elfHandler, nullptr);
	puts("elf handler done");
#endif
	pthread_join(game_patch_thread_id, nullptr);
	puts("game patch thread finished");
	//pthread_join(game_patch_input_thread_id, nullptr);
	//puts("game patch input thread finished");

	printf_notification("daemon exit");

	// TODO add elf loader with options for process name and type (daemon/game)
	// add whatever other crap people may want
	return 0;
}
