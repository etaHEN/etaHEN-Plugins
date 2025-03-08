#pragma once

#include "common.h"

#define PLAYGOSCENARIOID_SIZE 3
#define CONTENTID_SIZE 0x30
#define LANGUAGE_SIZE 8

#define NUM_LANGUAGES 30
#define NUM_IDS 64

typedef char playgo_scenario_id_t[PLAYGOSCENARIOID_SIZE];
typedef char language_t[LANGUAGE_SIZE];
typedef char content_id_t[CONTENTID_SIZE];

typedef struct {
    int32_t error_code;
    int32_t version;
    char description[512];
    char type[9];
} SceAppInstallErrorInfo;

typedef struct {
    char status[16];
    char src_type[8];
    uint32_t remain_time;
    uint64_t downloaded_size;
    uint64_t initial_chunk_size;
    uint64_t total_size;
    uint32_t promote_progress;
    SceAppInstallErrorInfo error_info;
    int32_t local_copy_percent;
    bool is_copy_only;
} SceAppInstallStatusInstalled;


typedef struct {
    content_id_t content_id;
    int content_type;
    int content_platform;
} SceAppInstallPkgInfo;

typedef struct {
    const char* uri;
    const char* ex_uri;
    const char* playgo_scenario_id;
    const char* content_id;
    const char* content_name;
    const char* icon_url;
} MetaInfo;

typedef struct {
    language_t languages[NUM_LANGUAGES];
    playgo_scenario_id_t playgo_scenario_ids[NUM_IDS];
    content_id_t content_ids[NUM_IDS];
    unsigned char unknown[6480]; // standard sony practice of wasting memory?
} PlayGoInfo;

int sceAppInstUtilInstallByPackage(MetaInfo* arg1, SceAppInstallPkgInfo* pkg_info, PlayGoInfo* arg2);

int sceAppInstUtilGetInstallStatus(const char* content_id, SceAppInstallStatusInstalled* status);

bool app_inst_util_init(void);
void app_inst_util_fini(void);

bool app_inst_util_uninstall_game(const char* title_id, int* error);
bool app_inst_util_uninstall_ac(const char* content_id, int* error);
bool app_inst_util_uninstall_patch(const char* title_id, int* error);
bool app_inst_util_uninstall_theme(const char* content_id, int* error);

bool app_inst_util_is_exists(const char* title_id, bool* exists, int* error);
bool app_inst_util_get_size(const char* title_id, unsigned long* size, int* error);

bool bgft_init(void);
void bgft_fini(void);

bool bgft_download_register_package_task(const char* content_id, const char* content_url, const char* content_name, const char* icon_path, const char* package_type, const char* package_sub_type, unsigned long package_size, bool is_patch, int* task_id, int* error);
bool bgft_download_start_task(int task_id, int* error);
bool bgft_download_stop_task(int task_id, int* error);
bool bgft_download_pause_task(int task_id, int* error);
bool bgft_download_resume_task(int task_id, int* error);
bool bgft_download_unregister_task(int task_id, int* error);
bool bgft_download_reregister_task_patch(int old_task_id, int* new_task_id, int* error);
bool bgft_download_get_task_progress(int task_id, struct bgft_download_task_progress_info* progress_info, int* error);
bool bgft_download_find_task_by_content_id(const char* content_id, int sub_type, int* task_id, int* error);
