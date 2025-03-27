#include "utils.hpp"
#include <notify.hpp>
#include <signal.h>

void sig_handler(int signo)
{
	printf_notification("the error disabler plugin has crashed with signal %d\nif you need it you can relaunch via the etaHEN toolbox in debug settings", signo);
	printBacktraceForCrash();
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

	unlink("/data/etaHEN/disabler_plugin.log");

	printf_notification("PS5 FG Error Message Disabler 4.0");
	plugin_log("Error Disabler 4.0 PS5 Ed. starting...");

	if(patchShellCore())
           printf_notification("Patched out the Error Messages!");
	else
	   printf_notification("Failed to patch out the error messages");
	
	while(1){
	  sleep(0x420);
	}
	// TODO add elf loader with options for process name and type (daemon/game)
	// add whatever other crap people may want
	return 0;
}
