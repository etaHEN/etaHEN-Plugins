#include "utils.hpp"
#include <notify.hpp>
#include <signal.h>

void sig_handler(int signo)
{
	printf_notification("the BO6 enabler plugin has crashed with signal %d\nif you need it you can relaunch via the etaHEN toolbox in debug settings", signo);
	printBacktraceForCrash();
    printf("ItemzLocalKillApp(sceSystemServiceGetAppId(BLOP60000)) returned %i\n", sceSystemServiceKillApp(sceSystemServiceGetAppId("BLOP60000"), -1, 0, 0));
}

extern "C"{
	int32_t sceKernelPrepareToSuspendProcess(pid_t pid);
	int32_t sceKernelSuspendProcess(pid_t pid);
	int32_t sceKernelPrepareToResumeProcess(pid_t pid);
	int32_t sceKernelResumeProcess(pid_t pid);
	int32_t sceUserServiceInitialize(int32_t* priority);
	int32_t sceUserServiceGetForegroundUser(int32_t* new_id);
	int32_t sceSysmoduleLoadModuleInternal(uint32_t moduleId);
	int32_t sceSysmoduleUnloadModuleInternal(uint32_t moduleId);
	int32_t sceVideoOutOpen();
	int32_t sceVideoOutConfigureOutput();
	int32_t sceVideoOutIsOutputSupported();

	int sceKernelLoadStartModule(const char *name, size_t argc, const void *argv, uint32_t flags, void *option, int *res);
}
static void SuspendApp(pid_t pid)
{
	sceKernelPrepareToSuspendProcess(pid);
	sceKernelSuspendProcess(pid);
}

static void ResumeApp(pid_t pid)
{
	// we need to sleep the thread after suspension
	// because this will cause a kernel panic when user quits the process after sometime
	// the kernel will not be very happy with us.
	usleep(1000);
	sceKernelPrepareToResumeProcess(pid);
	sceKernelResumeProcess(pid);
}
uintptr_t kernel_base = 0;
int main()
{
	puts("plugin entered");

	payload_args_t *args = payload_get_args();
	kernel_base = args->kdata_base_addr;

	struct sigaction new_SIG_action;
	new_SIG_action.sa_handler = sig_handler;
	sigemptyset(&new_SIG_action.sa_mask);
	new_SIG_action.sa_flags = 0;

	for (int i = 0; i < 12; i++)
		sigaction(i, &new_SIG_action, NULL);

	unlink("/data/etaHEN/plloader_plugin.log");

	printf_notification("Game Plugin Loader 0.0.1A PS5 Ed.");
	bo6_log("Game Plugin Loader 0.0.1A PS5 Ed. starting...");
    
	String title_id;
	int appid = 0;
	while(!Is_BLOPS_Running(appid))
	{
		printf_notification("Waiting for Black Ops 6 to start...");
		sleep(6);
	}

	int bappid = 0, pid = 0;
	for (size_t j = 0; j <= 9999; j++) {
        if(_sceApplicationGetAppId(j, &bappid) < 0)
            continue;

        if(appid == bappid){
            pid = j;
			bo6_log("BLOPS6 is running, appid 0x%X, pid %i", appid, pid);
			printf_notification("BLOPS6 is running, appid 0x%X, pid %i", appid, pid);
            break;
        }
    }

	UniquePtr<Hijacker> executable = Hijacker::getHijacker(pid);
	uintptr_t text_base = 0;
	uint64_t text_size = 0;
	if (executable)
	{
		text_base = executable->getEboot()->getTextSection()->start();
		text_size = executable->getEboot()->getTextSection()->sectionLength();
	}
	else
	{
		bo6_log("Failed to get hijacker for (%d)", pid);
		printf_notification("Failed to get hijacker for (%d), try re-running the plugin", pid);
		return -1;
	}
	if (text_base == 0 || text_size == 0)
	{
		bo6_log("text_base == 0 || text_size == 0");
		printf_notification("text_base == 0 || text_size == 0 (%d), try re-running the plugin", pid);
		return -1;
	}

	SuspendApp(pid);
	sleep(1);

	if(!patchBO6(executable, text_base)){
		bo6_log("Failed to patch the game");
		printf_notification("Failed to patch BO6, try re-running the plugin");
		return -1;
	}

	sleep(1);
	ResumeApp(pid);

    
	while(1){

	}


	// TODO add elf loader with options for process name and type (daemon/game)
	// add whatever other crap people may want
	return 0;
}
