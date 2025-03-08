#include "installer.h"
#include "pkg.h"
#include "util.h"
#include "module.h"


#define _NDBG 
#define BGFT_HEAP_SIZE (1 * 1024 * 1024)
#define WAIT_TIME (UINT64_C(5) * 1000 * 1000) /* 5 secs */


static bool s_app_inst_util_initialized = false;


bool app_inst_util_init(void) {
	int ret;

	if (s_app_inst_util_initialized) {
		goto done;
	}

	ret = sceAppInstUtilInitialize();
	if (ret) {
		EPRINTF("sceAppInstUtilInitialize failed: 0x%08X\n", ret);
		goto err;
	}

	s_app_inst_util_initialized = true;

done:
	return true;

err:
	s_app_inst_util_initialized = false;

	return false;
}

void app_inst_util_fini(void) {
	int ret;

	if (!s_app_inst_util_initialized) {
		return;
	}

	ret = sceAppInstUtilTerminate();
	if (ret) {
		EPRINTF("sceAppInstUtilTerminate failed: 0x%08X\n", ret);
	}

	s_app_inst_util_initialized = false;
}

bool app_inst_util_uninstall_game(const char* title_id, int* error) {
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!title_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	ret = sceAppInstUtilAppUnInstall(title_id);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppUnInstall failed: 0x%08X\n", ret);
		goto err;
	}

	return true;

err:
	return false;
}

bool app_inst_util_uninstall_ac(const char* content_id, int* error) {
	struct pkg_content_info content_info;
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!content_id) {
invalid_content_id:
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}
	if (!pkg_parse_content_id(content_id, &content_info)) {
		goto invalid_content_id;
	}

	ret = sceAppInstUtilAppUnInstallAddcont(content_info.title_id, content_info.label);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppUnInstallAddcont failed: 0x%08X\n", ret);
		goto err;
	}

done:
	return true;

err:
	return false;
}

bool app_inst_util_uninstall_patch(const char* title_id, int* error) {
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!title_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	ret = sceAppInstUtilAppUnInstallPat(title_id);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppUnInstallPat failed: 0x%08X\n", ret);
		goto err;
	}

	return true;

err:
	return false;
}

bool app_inst_util_uninstall_theme(const char* content_id, int* error) {
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!content_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	ret = sceAppInstUtilAppUnInstallTheme(content_id);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppUnInstallTheme failed: 0x%08X\n", ret);
		goto err;
	}

	return true;

err:
	return false;
}

bool app_inst_util_is_exists(const char* title_id, bool* exists, int* error) {
	int flag;
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!title_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	ret = sceAppInstUtilAppExists(title_id, &flag);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppExists failed: 0x%08X\n", ret);
		goto err;
	}

	if (exists) {
		*exists = flag;
	}

	return true;

err:
	return false;
}

bool app_inst_util_get_size(const char* title_id, unsigned long* size, int* error) {
	int ret;

	if (!s_app_inst_util_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!title_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	ret = sceAppInstUtilAppGetSize(title_id, size);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceAppInstUtilAppGetSize failed: 0x%08X\n", ret);
		goto err;
	}

	return true;

err:
	return false;
}


bool bgft_download_register_package_task(const char* content_id, const char* content_url, const char* content_name, const char* icon_path, const char* package_type, const char* package_sub_type, unsigned long package_size, bool is_patch, int* out_task_id, int* error)
{
	return false;
}

bool bgft_download_start_task(int task_id, int* error) {

	return false;
}

bool bgft_download_stop_task(int task_id, int* error) {
	
	return false;
}

bool bgft_download_pause_task(int task_id, int* error) {

	return false;
}

bool bgft_download_resume_task(int task_id, int* error) {
	
	return false;
}

bool bgft_download_unregister_task(int task_id, int* error) {

	return false;
}

bool bgft_download_reregister_task_patch(int old_task_id, int* new_task_id, int* error) {

	return false;
}

bool bgft_download_find_task_by_content_id(const char* content_id, int sub_type, int* task_id, int* error) {
	OrbisBgftTaskId tmp_id;
	int ret;

	if (!s_bgft_initialized) {
		ret = ORBIS_KERNEL_ERROR_ENXIO;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	if (!content_id) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}
	if (!((OrbisBgftTaskSubType)sub_type > ORBIS_BGFT_TASK_SUB_TYPE_UNKNOWN && (OrbisBgftTaskSubType)sub_type < ORBIS_BGFT_TASK_SUB_TYPE_MAX)) {
		ret = ORBIS_KERNEL_ERROR_EINVAL;
		if (error) {
			*error = ret;
		}
		goto err;
	}

	tmp_id = ORBIS_BGFT_INVALID_TASK_ID;
	ret = sceBgftServiceDownloadFindTaskByContentId(content_id, (OrbisBgftTaskSubType)sub_type, &tmp_id);
	if (ret) {
		if (error) {
			*error = ret;
		}
		EPRINTF("sceBgftServiceDownloadFindTaskByContentId failed: 0x%08X\n", ret);
		goto err;
	}

	if (task_id) {
		*task_id = (int)tmp_id;
	}

	return true;

err:
	return false;
}
