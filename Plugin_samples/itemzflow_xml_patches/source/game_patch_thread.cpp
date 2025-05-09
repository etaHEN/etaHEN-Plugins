#include <pthread.h>
#include <util.hpp>

#include "dbg/dbg.hpp"
#include "elf/elf.hpp"
#include "hijacker/hijacker.hpp"


#include "game_patch_memory.hpp"
#include "game_patch_thread.hpp"


#include "notify.hpp"
#include "print.hpp"

#include "pad.hpp"
#include "titleid_map.hpp"

#include "game_patch_xml.hpp"

#include <fcntl.h>

void write_log(const char *text) {
  int text_len = printf("%s", text);
  int fd = open("/data/etaHEN/cheat_plugin.log", O_WRONLY | O_CREAT | O_APPEND,
                0777);
  if (fd < 0) {
    return;
  }
  write(fd, text, text_len);
  close(fd);
}

void cheat_log(const char *fmt, ...) {
  char msg[0x1000]{};
  va_list args;
  va_start(args, fmt);
  int msg_len = vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Append newline at the end
  if (msg[msg_len - 1] != '\n') {
    strcat(msg, "\n");
  }

  write_log(msg);
}

extern "C" {
#define ENTRYPOINT_OFFSET 0x70

#define PROCESS_LAUNCHED 1

#define LOOB_BUILDER_SIZE 21
#define LOOP_BUILDER_TARGET_OFFSET 3
#define USLEEP_NID "QcteRwbsnV0"

#include "../extern/pfd_sfo_tools/sfopatcher/src/sfo.h"
#include "../extern/tiny-json/tiny-json.h"

int32_t sceKernelPrepareToSuspendProcess(pid_t pid);
int32_t sceKernelSuspendProcess(pid_t pid);
int32_t sceKernelPrepareToResumeProcess(pid_t pid);
int32_t sceKernelResumeProcess(pid_t pid);
int32_t sceUserServiceInitialize(int32_t *priority);
int32_t sceUserServiceGetForegroundUser(int32_t *new_id);
int32_t sceSysmoduleLoadModuleInternal(uint32_t moduleId);
int32_t sceSysmoduleUnloadModuleInternal(uint32_t moduleId);
int32_t sceVideoOutOpen();
int32_t sceVideoOutConfigureOutput();
int32_t sceVideoOutIsOutputSupported();
int sceKernelGetProcessName(int pid, char* name);
}

extern uint32_t FlipRate_ConfigureOutput_Ptr;
extern uint32_t FlipRate_isVideoModeSupported_Ptr;
int32_t g_isPatch120Hz;

int32_t g_game_patch_thread_running = false;

#define _MAX_PATH 260
#define APP_VER_SIZE 16
#define MASTER_VER_SIZE 16
#define CONTENT_ID_SIZE 64
#include <sys/stat.h>
bool if_exists(const char *path) {
  struct stat buffer;
  return stat(path, &buffer) == 0;
}
extern "C" {
int sceSystemServiceGetAppTitleId(int g_title_id, char *title_id);
int _sceApplicationGetAppId(int pid, int *appid);
}
static int32_t get_app_info(const char *title_id, char *out_app_ver,
                            char *out_master_ver, char *out_app_content_id,
                            const AppType app_mode)

{
  int32_t read_ret = 0;
  String sfo_or_json_path;
  switch (app_mode) {
  case PS4_APP: // CUSA Apps
  {
    sfo_or_json_path =
        String("/system_data/priv/appmeta/") + title_id + "/param.sfo";
    cheat_log("sfo: %s", sfo_or_json_path.c_str());
    sfo_context_t *sfo = sfo_alloc();
    int32_t sfo_ret = sfo_read(sfo, sfo_or_json_path.c_str());
    if (sfo_ret == -1) {
      cheat_log("Failed to get app information from param.sfo!\nsfo_path: %s\n",
              sfo_or_json_path.c_str());
    } else {
      const char *app_app_ver =
          (const char *)sfo_get_param_value(sfo, "APP_VER");
      const char *app_master_ver =
          (const char *)sfo_get_param_value(sfo, "VERSION");
      const char *app_content_id =
          (const char *)sfo_get_param_value(sfo, "CONTENT_ID");
      if (app_app_ver && app_master_ver && app_content_id) {
        cheat_log("\n"
                "sfo_read: 0x%08x\n"
                "APP_VER: %s\n"
                "VERSION: %s\n"
                "CONTENT_ID: %s\n",
                read_ret, app_app_ver, app_master_ver, app_content_id);
        strncpy(out_app_ver, app_app_ver, APP_VER_SIZE);
        strncpy(out_master_ver, app_master_ver, MASTER_VER_SIZE);
        strncpy(out_app_content_id, app_content_id, CONTENT_ID_SIZE);
        read_ret = 0;
      } else {
        read_ret = -1;
        cheat_log("failed to get sfo details???");
      }
    }
    if (sfo) {
      sfo_free(sfo);
    }
    break;
  }
  case PS5_APP: // PPSA Apps
  {
    sfo_or_json_path =
        String("/system_data/priv/appmeta/") + title_id + "/param.json";
    cheat_log("sfo_or_json_path: %s", sfo_or_json_path.c_str());
    if (!if_exists(sfo_or_json_path.c_str())) {
      sfo_or_json_path =
          String("/system_ex/app/") + title_id + "/sce_sys/param.json";
      if (!if_exists(sfo_or_json_path.c_str())) {
        cheat_log(
            "Failed to get app information from param.json!\nsfo_path: %s",
            sfo_or_json_path.c_str());
        read_ret = -1;
        break;
      }
    }
    FILE *file = fopen(sfo_or_json_path.c_str(), "r");
    if (!file) {
      cheat_log("Failed to get app information from param.sfo!\nsfo_path: %s",
                sfo_or_json_path.c_str());
      read_ret = -1;
      break;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_data = (char *)malloc(file_size + 1);
    if (!json_data) {
      fclose(file);
      read_ret = -1;
      break;
    }

    fread(json_data, 1, file_size, file);
    fclose(file);
    constexpr uint32_t MAX_TOKENS = 256;
    json_t pool[MAX_TOKENS]{};
    json_t const *my_json = json_create(json_data, pool, MAX_TOKENS);
    if (!my_json) {
      cheat_log("Error json create %s", sfo_or_json_path.c_str());
      read_ret = -1;
      break;
    }

    const char *contentId = json_getPropertyValue(my_json, "contentId");
    const char *contentVersion =
        json_getPropertyValue(my_json, "contentVersion");
    const char *masterVersion = json_getPropertyValue(my_json, "masterVersion");

    if (contentId && contentVersion && masterVersion) {
      cheat_log("\n"
                "json_getPropertyValue:\n"
                "contentId: %s (0x%p)\n"
                "contentVersion: %s (0x%p)\n"
                "masterVersion: %s (0x%p)\n",
                contentId, &contentId, contentVersion, &contentVersion,
                masterVersion, &masterVersion);
      strncpy(out_app_ver, contentVersion, APP_VER_SIZE);
      strncpy(out_master_ver, masterVersion, MASTER_VER_SIZE);
      strncpy(out_app_content_id, contentId, CONTENT_ID_SIZE);
      read_ret = 0;
    } else {
      cheat_log("Failed to retrieve %s values.\nContents of file:",
                sfo_or_json_path.c_str());
      puts(json_data);
      read_ret = -1;
    }
    if (json_data) {
      free(json_data);
    }
    break;
  }
  }
  return read_ret;
}

#undef _MAX_PATH

const bool isAlive(const pid_t pid) {
  if (pid <= 0) {
    return false;
  }
  for (const pid_t v : dbg::getAllPids()) {
    if (pid == v) {
      // cheat_log("(pid == v): %d\n", pid);
      return true;
    }
  }
  return false;
}

int32_t g_foundApp = false;

static void SuspendApp(pid_t pid) {
  sceKernelPrepareToSuspendProcess(pid);
  sceKernelSuspendProcess(pid);
}

static void ResumeApp(pid_t pid) {
  // we need to sleep the thread after suspension
  // because this will cause a kernel panic when user quits the process after
  // sometime the kernel will not be very happy with us.
  usleep(1000);
  sceKernelPrepareToResumeProcess(pid);
  sceKernelResumeProcess(pid);
}
extern "C" int sceSystemServiceGetAppIdOfRunningBigApp();
extern "C" int sceSystemServiceGetAppTitleId(int g_title_id, char *title_id);

int32_t patch_SetFlipRate(const Hijacker &hijacker, const pid_t pid) {
  static constexpr Nid sceVideoOutSetFlipRate_Nid{"CBiu4mCE1DA"};
  UniquePtr<SharedLib> lib = hijacker.getLib("libSceVideoOut.sprx"_sv);
  while (lib == nullptr) {
    lib = hijacker.getLib("libSceVideoOut.sprx"_sv);
    // usleep(100);
  }
  SuspendApp(pid);
  if (lib) {
    uint8_t is_mov_r14d_esi[3]{};
    const auto sceVideoOutSetFlipRate_ =
        hijacker.getFunctionAddress(lib.get(), sceVideoOutSetFlipRate_Nid);
    dbg::read(pid, sceVideoOutSetFlipRate_ + 0xa, &is_mov_r14d_esi,
              sizeof(is_mov_r14d_esi));
    if (is_mov_r14d_esi[0] == 0x41 && is_mov_r14d_esi[1] == 0x89 &&
        is_mov_r14d_esi[2] == 0xf6) {
      uint8_t xor_r14d_r14d[3] = {0x45, 0x31, 0xf6};
      dbg::write(pid, sceVideoOutSetFlipRate_ + 0xa, xor_r14d_r14d,
                 sizeof(xor_r14d_r14d));
      printf_notification("sceVideoOutSetFlipRate Patched");
    } else {
      // in case user loaded modified prx, lets make it do nothing
      printf_notification("Cannot find sceVideoOutSetFlipRate "
                          "location\nPatching it to return 0.");
      uint8_t xor_eax_eax_ret[3] = {0x31, 0xc0, 0xc3};
      dbg::write(pid, sceVideoOutSetFlipRate_, xor_eax_eax_ret,
                 sizeof(xor_eax_eax_ret));
    }
  }
  ResumeApp(pid);
  return 0;
}


bool Get_Running_App_TID(String &title_id, int &BigAppid) {
  char tid[255];
  BigAppid = sceSystemServiceGetAppIdOfRunningBigApp();
  if (BigAppid < 0) {
    return false;
  }
  (void)memset(tid, 0, sizeof tid);

  if (sceSystemServiceGetAppTitleId(BigAppid, &tid[0]) != 0) {
    return false;
  }

  title_id = String(tid);

  return true;
}

#include "game_patch_xml.hpp"

void *GamePatch_Thread(void *unused) {
  (void)unused;
  printf_notification("XML Patch thread running.\nBuilt: " __DATE__
                      " @ " __TIME__);
  makeDefaultXml_List();

  g_game_patch_thread_running = true;
  cheat_log("Game Patch thread running.\nBuilt: " __DATE__ " @ " __TIME__);
  while (g_game_patch_thread_running) {
    String tid;
    int appid = 0;
    if (!Get_Running_App_TID(tid, appid)) {
      if (g_foundApp)
        cheat_log("app is no longer running");

      usleep(1000);
      g_foundApp = false;
      continue;
    }

    if (g_foundApp) {
      usleep(1000);
      continue;
    }

    pid_t app_pid = 0;
    char proc_name[255] = { 0};
#if 0
for (auto p: dbg::getProcesses()) {

  app_pid = p.pid();
  const auto app = getProc(app_pid);
  if (strstr(app -> titleId().c_str(), tid.c_str()) != 0) {
    proc_name = p.name();
    break;
  }
}
#else
    for (size_t j = 0; j <= 9999; j++) {
      int bappid = 0;
      if (_sceApplicationGetAppId(j, &bappid) < 0)
        continue;

      if (appid == bappid) {
        app_pid = j; // APP PID NOT TO BE CONFUSED WITH APPID
        if(sceKernelGetProcessName(app_pid, &proc_name[0]) < 0) {
          cheat_log("sceKernelGetProcessName failed for %s (%d)", tid.c_str(), app_pid);
          continue;
        }
        cheat_log("Found %s (%d)", proc_name, app_pid);
        
        break;
      }
    }
#endif
    // cheat_log("found %s (%d) %s", tid.c_str(), app_pid, proc_name.c_str());
    const UniquePtr<Hijacker> executable = Hijacker::getHijacker(app_pid);
    uintptr_t text_base = 0;
    uint64_t text_size = 0;
    if (executable) {
      text_base = executable->getEboot()->getTextSection()->start();
      text_size = executable->getEboot()->getTextSection()->sectionLength();
    } else {
     // cheat_log("Failed to get hijacker for %s (%d)", tid.c_str(), app_pid);
      continue;
    }
    if (text_base == 0 || text_size == 0) {
      cheat_log("text_base == 0 || text_size == 0");
      continue;
    }
    // const auto app = getProc(app_pid);

    // cheat_log("Checking %s (%d)", tid.c_str(), app_pid);
    const char *g_title_id = tid.c_str();
    if (text_base && !g_foundApp &&
        (startsWith(g_title_id, "CUSA") || startsWith(g_title_id, "PCAS") ||
         startsWith(g_title_id, "PCJS"))) {
      char app_ver[APP_VER_SIZE]{};
      char master_ver[MASTER_VER_SIZE]{};
      char content_id[CONTENT_ID_SIZE]{};
      int32_t ret =
          get_app_info(g_title_id, app_ver, master_ver, content_id, PS4_APP);
      if (ret != 0) {
        // something went wrong
        cheat_log("get_app_info(%s) failed! %d", g_title_id, ret);
        continue;
      } else if (ret == 0) {
        g_foundApp = true;
      }
      cheat_log("suspending app %s", g_title_id);
      SuspendApp(app_pid);
      cheat_log("app %s suspended", g_title_id);
      // check xml database
      GamePatchInfo GameInfo{};
      GameInfo.image_pid = app_pid;
      GameInfo.image_base = text_base;
      GameInfo.image_size = text_size;
      strcpy(GameInfo.titleID, g_title_id);
      strcpy(GameInfo.titleVersion, app_ver);
      strcpy(GameInfo.ImageSelf, &proc_name[0]);
      GameInfo.app_mode = PS4_APP;
      Xml_ParseGamePatch(&GameInfo);
      ResumeApp(app_pid);
      cheat_log("app %s resumed", g_title_id);
      int32_t fliprate_game_found = false;
      if (Xml_parseTitleID_FliprateList(g_title_id)) {
        printf_notification("Title ID found in universal fliprate list:\n%s",
                            g_title_id);
        cheat_log("Flipped %s", g_title_id);
        ResumeApp(app_pid);
        patch_SetFlipRate(*executable, app_pid);
        fliprate_game_found = true;
      }
      if (!fliprate_game_found) {
        ResumeApp(app_pid);
      }
    } else if (text_base && !g_foundApp && (startsWith(g_title_id, "PPSA"))) {
      char app_ver[APP_VER_SIZE]{};       // `contentVersion`
      char master_ver[MASTER_VER_SIZE]{}; // `masterVersion`
      char content_id[CONTENT_ID_SIZE]{}; // `contentId`
      int32_t ret =
          get_app_info(g_title_id, app_ver, master_ver, content_id, PS5_APP);
      if (ret != 0) {
        // something went wrong
        printf_notification("get_app_info(%s) failed! %d", g_title_id, ret);
        continue;
      } else if (ret == 0) {
        g_foundApp = true;
      }
      cheat_log("suspending app %s", g_title_id);
      SuspendApp(app_pid);
      cheat_log("app %s suspended", g_title_id);
      // check xml database
      GamePatchInfo GameInfo{};
      GameInfo.image_pid = app_pid;
      GameInfo.image_base = text_base;
      GameInfo.image_size = text_size;
      strcpy(GameInfo.titleID, g_title_id);
      strcpy(GameInfo.titleVersion, app_ver);
      strcpy(GameInfo.ImageSelf, &proc_name[0]);
      GameInfo.app_mode = PS5_APP;
      Xml_ParseGamePatch(&GameInfo);
      ResumeApp(app_pid);
      cheat_log("app %s resumed", g_title_id);
      int32_t fliprate_game_found = false;
      if (Xml_parseTitleID_FliprateList(g_title_id)) {
        printf_notification("Title ID found in universal fliprate list:\n%s",
                            g_title_id);
        cheat_log("Flipped %s", g_title_id);
        ResumeApp(app_pid);
        patch_SetFlipRate(*executable, app_pid);
        fliprate_game_found = true;
      }
      if (!fliprate_game_found) {
        ResumeApp(app_pid);
      }
    }
  }

  printf_notification("Game Patch thread has requested to stop");
  pthread_exit(nullptr);
  return nullptr;
}

#undef APP_VER_SIZE
#undef MASTER_VER_SIZE
#undef CONTENT_ID_SIZE

/*
static void TestCallback(void *args)
{
        (void)args;
}

static constexpr __attribute__((used)) TitleIdMap
TITLEID_HANDLERS{{{TestCallback, "BREW00000"_tid}, {TestCallback,
{"BREW00001"}}}};
*/
