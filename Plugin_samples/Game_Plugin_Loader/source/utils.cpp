#include "utils.hpp"
#include <cstring>
#include <nid.hpp>
#include <fcntl.h>
#include <string>
void write_log(const char* text)
{
	int text_len = printf("%s", text);
	int fd = open("/data/etaHEN/plloader_plugin.log", O_WRONLY | O_CREAT | O_APPEND, 0777);
	if (fd < 0)
	{
		return;
	}
	write(fd, text, text_len);
	close(fd);
}

void bo6_log(const char* fmt, ...)
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

extern "C" int sceSystemServiceGetAppIdOfRunningBigApp();
extern "C" int sceSystemServiceGetAppTitleId(int app_id, char* title_id);

bool Is_BLOPS_Running(int &BigAppid)
{
  std::string title_id;
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

	title_id = std::string (tid);
  if(title_id == "CUSA03041")
	{
	    bo6_log("CUSA03041 is running, appid 0x%X", BigAppid);
       return true;
	}

	return false;
}

bool patchBO6(UniquePtr<Hijacker> &hijacker, uint64_t alsr_b) {
  bo6_log("Patching BO6 Now");

  BO6Builder builder = BUILDER_TEMPLATE;
  BO6Stuff stuff{*hijacker};

  UniquePtr<SharedLib> lib = hijacker->getLib("libScePad.sprx");
  bo6_log("libScePad.sprx addr: 0x%llx", lib->imagebase());
  stuff.scePadReadState = hijacker->getFunctionAddress(lib.get(), nid::scePadReadState);

  bo6_log("scePadReadState addr: 0x%llx", stuff.scePadReadState);
  if (stuff.scePadReadState == 0) {
    bo6_log("failed to locate scePadReadState");
    return false;
  }

  stuff.ASLR_Base = alsr_b;
  strcpy(stuff.prx_path, "/data/etaHEN/plugins/Sheriff-132v106.prx");

  auto code = hijacker->getTextAllocator().allocate(BO6Builder::SHELLCODE_SIZE);
  bo6_log("shellcode addr: 0x%llx", code);
  auto stuffAddr = hijacker->getDataAllocator().allocate(sizeof(BO6Stuff));
  // static constexpr Nid printfNid{"hcuQgD53UxM"};
  // static constexpr Nid amd64_set_fsbaseNid{"3SVaehJvYFk"};
  auto meta = hijacker->getEboot()->getMetaData();
  const auto &plttab = meta->getPltTable();
  auto index = meta->getSymbolTable().getSymbolIndex(nid::scePadReadState);
  for (const auto &plt : plttab) {
    if (ELF64_R_SYM(plt.r_info) == index) {
      builder.setExtraStuffAddr(stuffAddr);
      hijacker->write(code, builder.shellcode);
      hijacker->write(stuffAddr, stuff);

      uintptr_t hook_adr = hijacker->getEboot()->imagebase() + plt.r_offset;

      // write the hook
      hijacker->write<uintptr_t>(hook_adr, code);
      bo6_log("hook addr: 0x%llx", hook_adr);

      return true;
    }
  }
  return false;
}