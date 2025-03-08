#include "../extern/mxml/mxml.h"
#include "game_patch_xml_cfg.hpp"
#include <stdint.h>

#include "game_patch_memory.hpp"
#include "game_patch_xml.hpp"
#include "notify.hpp"
void cheat_log(const char* fmt, ...);
// Include the `game_patch_fliprate_list.xml` as a symbol
__asm__(
	".intel_syntax noprefix\n"
	".section .data\n"
	"DefaultXml_FliprateList:\n"
	".incbin \"" "data/game_patch_fliprate_list.xml" "\"\n");

extern "C" const char DefaultXml_FliprateList[];

extern "C"
{
	int32_t sceKernelOpen(const char*, int32_t, int32_t);
	off_t sceKernelLseek(int32_t, off_t, int);
	int32_t sceKernelClose(int32_t);

	size_t sceKernelWrite(int32_t, const void*, size_t);
	size_t sceKernelRead(int32_t, void*, size_t);

	void* malloc(size_t);
	void free(void*);
}
#define ORBIS_KERNEL_ERROR_ENOENT 0x80020002

int32_t Read_File(const char* input_file, char** file_data, size_t* filesize, uint32_t extra)
{
	int32_t res = 0;
	int32_t fd = 0;

	cheat_log("Reading input_file \"%s\"\n", input_file);

	fd = sceKernelOpen(input_file, 0, 0777);
	if (fd < 0) {
		cheat_log("sceKernelOpen() 0x%08x\n", fd);
		res = fd;
		goto term;
	}

	*filesize = sceKernelLseek(fd, 0, SEEK_END);
	if (*filesize == 0) {
		cheat_log("ERROR: input_file is empty %i\n", res);
		res = -1;
		goto term;
	}

	res = sceKernelLseek(fd, 0, SEEK_SET);
	if (res < 0) {
		cheat_log("sceKernelLseek() 0x%08x\n", res);
		goto term;
	}

	*file_data = (char*)malloc(*filesize + extra);
	if (*file_data == NULL) {
		cheat_log("ERROR: malloc()\n");
		goto term;
	}

	res = sceKernelRead(fd, *file_data, *filesize);
	if (res < 0) {
		cheat_log("sceKernelRead() 0x%08x\n", res);
		goto term;
	}

	res = sceKernelClose(fd);

	if (res < 0) {
		cheat_log("ERROR: sceKernelClose() 0x%08x\n", res);
		goto term;
	}

	cheat_log("input_file %s has been read - Res: %d - filesize: %jd\n", input_file, res,
		*filesize);

	return res;

term:

	if (fd != 0) {
		sceKernelClose(fd);
	}

	return res;
}

int32_t Write_File(const char* input_file, unsigned char* file_data, uint64_t filesize)
{
	int32_t fd = 0;
	size_t size_written = 0;
	fd = sceKernelOpen(input_file, 0x200 | 0x002, 0777);
	if (fd < 0) {
		cheat_log("Failed to make file \"%s\"\n", input_file);
		return 0;
	}
	cheat_log("Writing input_file \"%s\" %li\n", input_file, filesize);
	size_written = sceKernelWrite(fd, file_data, filesize);
	cheat_log("Written input_file \"%s\" %li\n", input_file, size_written);
	sceKernelClose(fd);
	return 1;
}

int makeDefaultXml_List()
{
	FILE* f = fopen(XML_PATH_LIST, "rb");
	if (!f)
	{
		FILE* new_f = fopen(XML_PATH_LIST, "w");
		if (!new_f)
		{
			printf("Failed to create file: " XML_PATH_LIST "\n");
			return -1;
		}
		// Print default data to TTY
		printf("%s\n", DefaultXml_FliprateList);
		fputs(DefaultXml_FliprateList, new_f);
		fflush(new_f);
		fclose(new_f);
		printf_notification("Created default config file:\n" XML_PATH_LIST);
	}
	return 0;
}

int Xml_parseTitleID_FliprateList(const char* titleId)
{
	makeDefaultXml_List();
	char* buffer = NULL;
	uint64_t length = 0;
	Read_File(XML_PATH_LIST, &buffer, &length, 0);
	cheat_log("File: " XML_PATH_LIST " exist");
	cheat_log("loading string");
	mxml_node_t* tree = mxmlLoadString(NULL, buffer, MXML_OPAQUE_CALLBACK);
	if (tree == NULL)
	{
		cheat_log("Couldn't load XML file.\n");
		return 0;
	}

	cheat_log("Finding TitleID\n");

	mxml_node_t* titleIDNode = mxmlFindElement(tree, tree, "TitleID", NULL, NULL, MXML_DESCEND);
	cheat_log("TitleID: 0x%p\n", titleIDNode);

	int found_id = 0;
	if (titleIDNode != NULL)
	{
		mxml_node_t* idNode = mxmlFindElement(titleIDNode, tree, "ID", NULL, NULL, MXML_DESCEND);

		while (idNode != NULL)
		{
			//cheat_log("ID: 0x%p\n", idNode);
			const char* idValue = mxmlGetOpaque(idNode);
			if (idValue != NULL)
			{
				cheat_log("TitleID: %s\n", idValue);
				if (strncmp(titleId, idValue, __builtin_strlen("CUSAxxxxx")) == 0)
				{
					found_id = 1;
					cheat_log("%s match !! found_id=0x%08x\n", titleId, found_id);
					break;
				}
			}
			idNode = mxmlFindElement(idNode, titleIDNode, "ID", NULL, NULL, MXML_NO_DESCEND);
		}
	}

	if (buffer)
	{
		free(buffer);
	}
	if (tree)
	{
		mxmlDelete(tree);
	}
	return found_id;
}

int simple_get_bool(const char* val)
{
	if (val == NULL || val[0] == 0)
	{
		return 0;
	}
	if (
		val[0] == '1' ||
		startsWith(val, "on") ||
		startsWith(val, "true") ||
		startsWith(val, "On") ||
		startsWith(val, "True"))
	{
		return 1;
	}
	else if (
		val[0] == '0' ||
		startsWith(val, "off") ||
		startsWith(val, "false") ||
		startsWith(val, "Off") ||
		startsWith(val, "False"))
	{
		return 0;
	}
	return 0;
}

const char* GetXMLAttr(mxml_node_t* node, const char* name)
{
	const char* AttrData = mxmlElementGetAttr(node, name);
	if (AttrData == NULL)
	{
		printf("XML Attribute: \"%s\" is empty\n", name);
		AttrData = "\0";
	}
	return AttrData;
}

#define MAX_PATH 260
#define NO_ASLR_ADDR_PS4 0x00400000
// http://www.cse.yorku.ca/~oz/hash.html
constexpr uint64_t djb2_hash(const char* str)
{
	uint64_t hash = 5381;
	uint32_t c = 0;
	while ((c = *str++))
	{
		hash = hash * 33 ^ c;
	}
	return hash;
}

uint64_t patch_hash_calc(const char* title, const char* name, const char* app_ver,
	const char* title_id, const char* elf)
{
	uint64_t output_hash = 0;
	char hash_str[1024]{};
	snprintf(hash_str, sizeof(hash_str), "%s%s%s%s%s", title, name, app_ver,
		title_id, elf);
	output_hash = djb2_hash(hash_str);
	cheat_log("input: \"%s\"", hash_str);
	cheat_log("output: 0x%016lx", output_hash);
	return output_hash;
}

#define MAX_PATH 260

//char g_game_elf[] = "eboot.bin";
//char g_game_ver[] = "01.00";
//uint64_t g_module_base = 0;
#define NO_ASLR_ADDR_PS4 0x00400000

char* unescape(const char* s)
{
	size_t len = strlen(s);
	char* unescaped_str = (char*)malloc(len + 1);
	if (!unescaped_str)
	{
		return nullptr;
	}
	uint32_t i{}, j{};
	for (i = 0, j = 0; s[i] != '\0'; i++, j++)
	{
		if (s[i] == '\\')
		{
			i++;
			switch (s[i])
			{
			case 'n':
				unescaped_str[j] = '\n';
				break;
			case '0':
				unescaped_str[j] = '\0';
				break;
			case 't':
				unescaped_str[j] = '\t';
				break;
			case 'r':
				unescaped_str[j] = '\r';
				break;
			case '\\':
				unescaped_str[j] = '\\';
				break;
			case 'x':
			{
				char hex_string[3] = { 0 };
				uint32_t val = 0;
				hex_string[0] = s[++i];
				hex_string[1] = s[++i];
				hex_string[2] = '\0';
				if (sscanf(hex_string, "%x", &val) != 1)
				{
					cheat_log("Invalid hex escape sequence: %s\n", hex_string);
					val = '?';
				}
				unescaped_str[j] = (char)val;
			}
			break;
			default:
				unescaped_str[j] = s[i];
				break;
			}
		}
		else
		{
			unescaped_str[j] = s[i];
		}
	}
	unescaped_str[j] = '\0';
	return unescaped_str;
}

bool hex_prefix(const char* str)
{
	return (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'));
}

void patch_data1(int pid, const char* patch_type_str, uint64_t addr, const char* value, uint32_t source_size, uint64_t jump_target)
{
	uint64_t patch_type = djb2_hash(patch_type_str);
	static bool ShowNotifyOnce = false;
	switch (patch_type)
	{
	case djb2_hash("byte"):
	case djb2_hash("mask_byte"):
	{
		uint8_t real_value = 0;
		if (hex_prefix(value))
		{
			real_value = strtol(value, NULL, 16);
		}
		else
		{
			real_value = strtol(value, NULL, 10);
		}
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("bytes16"):
	case djb2_hash("mask_bytes16"):
	{
		uint16_t real_value = 0;
		if (hex_prefix(value))
		{
			real_value = strtol(value, NULL, 16);
		}
		else
		{
			real_value = strtol(value, NULL, 10);
		}
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("bytes32"):
	case djb2_hash("mask_bytes32"):
	{
		uint32_t real_value = 0;
		if (hex_prefix(value))
		{
			real_value = strtol(value, NULL, 16);
		}
		else
		{
			real_value = strtol(value, NULL, 10);
		}
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("bytes64"):
	case djb2_hash("mask_bytes64"):
	{
		int64_t real_value = 0;
		if (hex_prefix(value))
		{
			real_value = strtoll(value, NULL, 16);
		}
		else
		{
			real_value = strtoll(value, NULL, 10);
		}
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("bytes"):
	case djb2_hash("mask"):
	case djb2_hash("mask_bytes"):
	{
		write_bytes(pid, addr, value);
		break;
	}
	case djb2_hash("float32"):
	case djb2_hash("mask_float32"):
	{
		float real_value = 0;
		real_value = strtod(value, NULL);
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("float64"):
	case djb2_hash("mask_float64"):
	{
		double real_value = 0;
		real_value = strtod(value, NULL);
		write_bytes(pid, addr, &real_value, sizeof(real_value));
		break;
	}
	case djb2_hash("utf8"):
	case djb2_hash("mask_utf8"):
	{
		char* new_str = unescape(value);
		if (!new_str)
		{
			break;
		}
		uint64_t char_len = strlen(new_str);
		write_bytes(pid, addr, (void*)new_str, char_len + 1); // get null
		free(new_str);
		break;
	}
	case djb2_hash("utf16"):
	case djb2_hash("mask_utf16"):
	{
		char* new_str = unescape(value);
		if (!new_str)
		{
			break;
		}
		for (uint32_t i = 0; new_str[i] != '\x00'; i++)
		{
			uint8_t val_ = new_str[i];
			uint8_t value_[2] = { val_, 0x00 };
			write_bytes(pid, addr, value_, sizeof(value_));
			addr = addr + 2;
		}
		uint8_t value_[2] = { 0x00, 0x00 };
		write_bytes(pid, addr, value_, sizeof(value_));
		free(new_str);
		break;
	}
	case djb2_hash("mask_jump32"):
	{
		cheat_log("Warning: mask_jump32 not tested");
		constexpr uint32_t MAX_PATTERN_LENGTH = 256;
		if (source_size < 5)
		{
			cheat_log("Can't create code cave with size less than 32 bit jump!\n");
			break;
		}
		if (source_size >= MAX_PATTERN_LENGTH)
		{
			cheat_log("Can't create code cave with size more than %u bytes!\n", MAX_PATTERN_LENGTH);
			break;
		}
		uint8_t nop_bytes[MAX_PATTERN_LENGTH];
		(void)memset(nop_bytes, 0x90, sizeof(nop_bytes));
		write_bytes(pid, addr, nop_bytes, source_size);
		uint64_t bytearray_size = 0;
		uint8_t* bytearray = hexstrtochar2(value, &bytearray_size);
		if (!bytearray)
		{
			break;
		}
		uint64_t code_cave_end = jump_target + bytearray_size;
		uint8_t jump_32[5] = { 0xe9, 0x00, 0x00, 0x00, 0x00 };
		int32_t target_jmp = (int32_t)(jump_target - addr - sizeof(jump_32));
		int32_t target_return = (int32_t)(addr)-(code_cave_end);
		write_bytes(pid, jump_target, bytearray, bytearray_size);
		write_bytes(pid, addr, jump_32, sizeof(jump_32));
		write_bytes(pid, addr + 1, &target_jmp, sizeof(target_jmp));
		write_bytes(pid, jump_target + bytearray_size, jump_32, sizeof(jump_32));
		write_bytes(pid, code_cave_end + 1, &target_return, sizeof(target_return));
		free(bytearray);
		break;
	}
	case djb2_hash("patchCall"):
	case djb2_hash("mask_patchCall"):
	{
		if (!ShowNotifyOnce)
		{
			printf_notification("patchCall not supported yet");
			ShowNotifyOnce = true;
		}
		cheat_log("patchCall not supported yet");
		/*
		u8 call_bytes[5] = { 0 };
		memcpy(call_bytes, (void*)addr, sizeof(call_bytes));
		if (call_bytes[0] == 0xe8 || call_bytes[0] == 0xe9)
		{
			int32_t branch_target = *(int32_t*)(call_bytes + 1);
			if (branch_target)
			{
				uintptr_t branched_call = addr + branch_target + sizeof(call_bytes);
				cheat_log("0x%016lx: 0x%08x -> 0x%016lx\n", addr, branch_target, branched_call);
				s64 bytearray_size = 0;
				u8* bytearray = hexstrtochar2(value, &bytearray_size);
				if (!bytearray)
				{
					break;
				}
				sys_proc_rw(branched_call, bytearray, bytearray_size);
				free(bytearray);
			}
		}
		*/
		break;
	}
	default:
	{
		cheat_log("Patch type: '%s (#%.16lx) not found or unsupported\n", patch_type_str, patch_type);
		cheat_log("Patch data:\n");
		cheat_log("      Address: 0x%lx\n", addr);
		cheat_log("      Value: %s\n", value);
		cheat_log("      Jump Size: %u\n", source_size);
		cheat_log("      Jump Target: 0x%lx\n", jump_target);
		break;
	}
	}
}

int Xml_ParseGamePatch(GamePatchInfo* info)
{
	uint32_t patch_lines = 0;
	uint32_t patch_items = 0;
	char* patch_buffer = nullptr;
	uint64_t patch_size = 0;
	char input_file[MAX_PATH]{};
	switch (info->app_mode)
	{
	case PS4_APP:
	{
		snprintf(input_file, sizeof(input_file), BASE_ETAHEN_PATCH_DATA_PATH_PS4 "/%s.xml", info->titleID);
		break;
	}
	case PS5_APP:
	{
		snprintf(input_file, sizeof(input_file), BASE_ETAHEN_PATCH_DATA_PATH_PS5 "/%s.xml", info->titleID);
		break;
	}
	default:
	{
		cheat_log("Unknown app type %d", info->app_mode);
		return 0;
	}
        }
	int32_t res = Read_File(input_file, &patch_buffer, &patch_size, 0);

	if (res)
	{
		cheat_log("file %s not found\n", input_file);
		cheat_log("error: 0x%08x\n", res);
		return 0;
	}

	if (patch_buffer && patch_size)
	{
		mxml_node_t* node, * tree = NULL;
		tree = mxmlLoadString(NULL, patch_buffer, MXML_NO_CALLBACK);

		if (!tree)
		{
			printf_notification("could not parse XML: %s", input_file);
			cheat_log("XML: could not parse XML:\n%s\n", patch_buffer);
			free(patch_buffer);
			return 0;
		}

		for (node = mxmlFindElement(tree, tree, "Metadata", NULL, NULL, MXML_DESCEND); node != NULL;
			node = mxmlFindElement(node, tree, "Metadata", NULL, NULL, MXML_DESCEND))
		{
			char* settings_buffer = nullptr;
			uint64_t settings_size = 0;
			bool PRX_patch = false;

			bool WantRelativeAddr = false;
			uint64_t ImageBaseAddr = 0;

			const char* TitleData = GetXMLAttr(node, "Title");
			const char* NameData = GetXMLAttr(node, "Name");
			const char* AppVerData = GetXMLAttr(node, "AppVer");
			const char* AppElfData = GetXMLAttr(node, "AppElf");

			const char* ImgBaseData = GetXMLAttr(node, "ImageBase");


			cheat_log("Title: \"%s\"\n", TitleData);
			cheat_log("Name: \"%s\"\n", NameData);
			cheat_log("AppVer: \"%s\"\n", AppVerData);
			cheat_log("AppElf: \"%s\"\n", AppElfData);
			if (*ImgBaseData)
			{
				ImageBaseAddr = strtoull(ImgBaseData, NULL, 16);
				WantRelativeAddr = ImageBaseAddr == 0 ? true : false;
				cheat_log("ImageBase: \"%s\"\n", ImgBaseData);
				cheat_log("ImageBase Parsed: \"0x%016llx\"\n", ImageBaseAddr);
				cheat_log("WantRelativeAddr: \"%s\"\n", WantRelativeAddr ? "true" : "false");
			}


			uint64_t hashout = patch_hash_calc(TitleData, NameData, AppVerData, input_file, AppElfData);
			char settings_path[MAX_PATH] = { 0 };
			snprintf(settings_path, sizeof(settings_path), BASE_ETAHEN_PATCH_SETTINGS_PATH "/0x%016lx.txt", hashout); ///0x%016llx.txt >>>
			//sceKernelChmod(settings_path, 0777);
			int32_t res = Read_File(settings_path, &settings_buffer, &settings_size, 0);
			cheat_log("settings_path: %s, 0x%08x\n", settings_path, res);
			if (res == ORBIS_KERNEL_ERROR_ENOENT)
			{
				cheat_log("file %s not found, initializing false. ret: 0x%08x\n", settings_path, res);
				uint8_t false_data[] = { '0', '\n' };
				Write_File(settings_path, false_data, sizeof(false_data));
				continue;
			}
			if (!settings_buffer || !settings_size)
			{
				cheat_log("Settings 0x%016lx has no data!\n", hashout);
				cheat_log("File size %li bytes\n", settings_size);
				continue;
			}
			if (settings_buffer[0] == '1' && !strcmp(info->ImageSelf, AppElfData))
			{
				int32_t ret_cmp = strcmp(info->titleVersion, AppVerData);
				if (!ret_cmp)
				{
					cheat_log("App ver %s == %s\n", info->titleVersion, AppVerData);
				}
				else if (startsWith(AppVerData, "mask") || startsWith(AppVerData, "all"))
				{
					cheat_log("App ver masked: %s\n", AppVerData);
				}
				else if (ret_cmp)
				{
					printf_notification("Wrong App version\nGame version: %s\nExpected: %s", info->titleVersion, AppVerData);
					cheat_log("App ver %s != %s\n", info->titleVersion, AppVerData);
					cheat_log("Skipping patch entry\n");
					continue;
				}
				patch_items++;
				mxml_node_t* Patchlist_node = mxmlFindElement(node, node, "PatchList", NULL, NULL, MXML_DESCEND);
				for (mxml_node_t* Line_node = mxmlFindElement(node, node, "Line", NULL, NULL, MXML_DESCEND); Line_node != NULL;
					Line_node = mxmlFindElement(Line_node, Patchlist_node, "Line", NULL, NULL, MXML_DESCEND))
				{
					uint64_t addr_real = 0;
					bool use_mask = false;
					const char* gameType = GetXMLAttr(Line_node, "Type");
					const char* gameAddr = GetXMLAttr(Line_node, "Address");
					const char* gameValue = GetXMLAttr(Line_node, "Value");
					// starts with `mask`
					if (startsWith(gameType, "mask"))
					{
						use_mask = true;
					}
					if (use_mask)
					{
						/*
						uint64_t jump_addr = 0;
						uint32_t jump_size = 0;
						const char* gameOffset = nullptr;
						if (startsWith(gameType, "mask_jump32"))
						{
							const char* gameJumpTarget = GetXMLAttr(Line_node, "Target");
							const char* gameJumpSize = GetXMLAttr(Line_node, "Size");
							jump_addr = addr_real = (uint64_t)PatternScan(g_module_base, g_module_size, gameJumpTarget);
							jump_size = strtoul(gameJumpSize, NULL, 10);
							cheat_log("Target: 0x%lx jump size %u\n", jump_addr, jump_size);
						}
						gameOffset = GetXMLAttr(Line_node, "Offset");
						addr_real = (uint64_t)PatternScan(g_module_base, g_module_size, gameAddr);
						if (!addr_real)
						{
							cheat_log("Masked Address: %s not found\n", gameAddr);
							continue;
						}
						cheat_log("Masked Address: 0x%lx\n", addr_real);
						cheat_log("Offset: %s\n", gameOffset);
						uint32_t real_offset = 0;
						if (gameOffset[0] != '0')
						{
							if (gameOffset[0] == '-')
							{
								cheat_log("Offset mode: subtract\n");
								real_offset = strtoul(gameOffset + 1, NULL, 10);
								cheat_log("before offset: 0x%lx\n", addr_real);
								addr_real = addr_real - real_offset;
								cheat_log("after offset: 0x%lx\n", addr_real);
							}
							else if (gameOffset[0] == '+')
							{
								cheat_log("Offset mode: addition\n");
								real_offset = strtoul(gameOffset + 1, NULL, 10);
								cheat_log("before offset: 0x%lx\n", addr_real);
								addr_real = addr_real + real_offset;
								cheat_log("after offset: 0x%lx\n", addr_real);
							}
						}
						else
						{
							cheat_log("Mask does not reqiure offsetting.\n");
						}
						*/
					}
					cheat_log("Type: \"%s\"\n", gameType);
					if (gameAddr && !use_mask)
					{
						addr_real = strtoull(gameAddr, NULL, 16);
						cheat_log("Address: 0x%lx\n", addr_real);
					}
					cheat_log("Value: \"%s\"\n", gameValue);
					cheat_log("patch line: %u\n", patch_lines);
					if (gameType && addr_real && *gameValue != '\0') // type, address and value must be present
					{
						if (!PRX_patch && !use_mask)
						{
							uint64_t xml_addr = addr_real;
							uint64_t PatchBaseAddresss = (ImageBaseAddr == 0 ? NO_ASLR_ADDR_PS4 : ImageBaseAddr);
							// previous self, eboot patches were made with no aslr addresses
							if (WantRelativeAddr)
							{
								addr_real = info->image_base + addr_real;
							}
							else
							{
								addr_real = info->image_base + (addr_real - PatchBaseAddresss);
							}
							cheat_log("Patching process address: 0x%016llx", addr_real);
							cheat_log("Process ID: %i", info->image_pid);
							cheat_log("Process Image Base: 0x%016llx", info->image_base);
							cheat_log("XML Patch Address: 0x%016llx", xml_addr);
							cheat_log("XML Patch Image Base: 0x%016llx", PatchBaseAddresss);
							// previous self, eboot patches were made with no aslr addresses
							//addr_real = info->image_base + (addr_real - NO_ASLR_ADDR_PS4);
						}
						else if (PRX_patch && !use_mask)
						{
							addr_real = info->image_base + addr_real;
						}
						//patch_data1(gameType, addr_real, gameValue, jump_size, jump_addr);
						// `mask_jump32` won't work yet
						patch_data1(info->image_pid, gameType, addr_real, gameValue, 0, 0);
						patch_lines++;
					}
				}
			}
			else{
				if (settings_buffer[0] == '1' && strcmp(info->ImageSelf, AppElfData) != 0){
			   	   printf_notification("Required ELF not found for patching\nHas: %s\nExpected: %s", info->ImageSelf, AppElfData);
				   cheat_log("Required ELF not found for patching\nHas: %s\nExpected: %s", info->ImageSelf, AppElfData);
				}
			}
			if (settings_buffer)
			{
				free(settings_buffer);
			}
		}

		mxmlDelete(node);
		mxmlDelete(tree);
		free(patch_buffer);

		if (patch_items > 0 && patch_lines > 0)
		{
			printf_notification(""
				"%s:%s\n"
				"%u %s Applied\n"
				"%u %s Applied",
				info->titleID,
				info->titleVersion,
				patch_items, (patch_items == 1) ? "Patch" : "Patches",
				patch_lines, (patch_lines == 1) ? "Patch Line" : "Patch Lines");
		}
		else
		{
			cheat_log("patch_items: %d\n"
				"patch_lines: %d", patch_items, patch_lines);
			cheat_log("No patch applied for '%s\\%s'", info->titleID, info->titleVersion);
		}
	}
	else // if (!patch_buffer && !patch_size)
	{
		printf_notification("File %s\nis empty", input_file);
		return 0;
	}
	return 1;
}
