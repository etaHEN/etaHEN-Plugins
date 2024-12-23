// A lot of functions in here were pulled from HEN-V, credits to astrelsky for most of this



#include <ps5/kernel.h>
#include <elf.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mdbg.h"
#include "utils.h"
#include "process_utils.h"

#define SHARED_LIB_IMAGEBASE_OFFSET 0x30
#define SHARED_LIB_METADATA_OFFSET 0x148
#define LIB_HANDLE_OFFSET 0x28
#define METADATA_PLT_HELPER_OFFSET 0x28
#define NID_LENGTH 11
#define PROC_UCRED_OFFSET 0x40

typedef struct {
	uintptr_t symtab;
	size_t symtab_size;
	uintptr_t strtab;
	size_t strtab_size;
	uintptr_t plttab;
	size_t plttab_size;
} plt_helper_t;

typedef struct {
	uintptr_t sections;
	size_t num_sections;
} sections_iterator_t;

typedef struct {
	uintptr_t type;
	uintptr_t start;
	size_t length;
} section_t;


void get_kernel_string(uintptr_t addr, char* string) {
	char buf[1];
	size_t offset = 1;
	kernel_copyout(addr, &buf, sizeof(buf));
	while (buf[0] != 0x00) {
		kernel_copyout(addr + offset, &buf, sizeof(buf));
		offset++;
	}
	kernel_copyout(addr, string, offset);
}

uintptr_t shared_lib_get_imagebase(uintptr_t lib) {
	uintptr_t imagebase = 0;
	kernel_copyout(lib + SHARED_LIB_IMAGEBASE_OFFSET, &imagebase, sizeof(imagebase));
	return imagebase;
}

uintptr_t shared_object_get_eboot(uintptr_t obj) {
	uintptr_t eboot = 0;
	kernel_copyout(obj, &eboot, sizeof(eboot));
	return eboot;
}

uintptr_t proc_get_shared_object(uintptr_t proc) {
	uintptr_t obj = 0;
	kernel_copyout(proc + PROC_SHARED_OBJECT_OFFSET, &obj, sizeof(obj));
	return obj;
}

uintptr_t proc_get_eboot(uintptr_t proc) {
	uintptr_t obj = proc_get_shared_object(proc);
	if (obj == 0) {
		return 0;
	}
	return shared_object_get_eboot(obj);
}

uintptr_t proc_get_fd(uintptr_t proc) {
	uintptr_t fd = 0;
	kernel_copyout(proc + PROC_FD_OFFSET, &fd, sizeof(fd));
	return fd;
}

uintptr_t proc_get_next(uintptr_t proc) {
	uintptr_t next = 0;
	kernel_copyout(proc, &next, sizeof(next));
	return next;
}

int proc_get_pid(uintptr_t proc) {
	int pid = 0;
	kernel_copyout(proc + KERNEL_OFFSET_PROC_P_PID, &pid, sizeof(pid));
	return pid;
}

uintptr_t get_current_proc(void) {
	static uintptr_t g_current_proc = 0;
	if (g_current_proc != 0) {
		return g_current_proc;
	}
	g_current_proc = get_proc(getpid());
	return g_current_proc;
}

void proc_get_name(uintptr_t proc, char name[PROC_SELFINFO_NAME_SIZE]) {
	kernel_copyout(proc + PROC_SELFINFO_NAME_OFFSET, name, PROC_SELFINFO_NAME_SIZE);
}

void proc_get_path(uintptr_t proc, char* buf) {
	get_kernel_string(proc + PROC_PATH_OFFSET, buf);
}

void proc_get_content_id(uintptr_t proc, char* buf) {
	get_kernel_string(proc + PROC_CONTENT_ID_OFFSET, buf);
}

void proc_get_title_id(uintptr_t proc, char* buf) {
	get_kernel_string(proc + PROC_TITLE_ID_OFFSET, buf);
}

uintptr_t shared_object_get_lib(uintptr_t obj, int handle) {
	uintptr_t lib = 0;
	kernel_copyout(obj, &lib, sizeof(lib));
	while (lib != 0) {
		int current_handle = -1;
		kernel_copyout(lib + LIB_HANDLE_OFFSET, &current_handle, sizeof(current_handle));
		if (current_handle == -1) {
			// read failed
			return -1;
		}
		if (current_handle == handle) {
			return lib;
		}
		kernel_copyout(lib, &lib, sizeof(lib));
	}
	return 0;
}

uintptr_t proc_get_lib(uintptr_t proc, int handle) {
	uintptr_t obj = proc_get_shared_object(proc);
	return shared_object_get_lib(obj, handle);
}

size_t get_symbol_address(const plt_helper_t *restrict helper, uintptr_t imagebase, const char *nid) {
	Elf64_Sym *symtab = malloc(helper->symtab_size);
	if (symtab == NULL) {
		return 0;
	}

	kernel_copyout(helper->symtab, symtab, helper->symtab_size);

	char *strtab = malloc(helper->strtab_size);
	if (strtab == NULL) {
		free(symtab);
		return 0;
	}

	kernel_copyout(helper->strtab, strtab, helper->strtab_size);

	const size_t num_symbols = helper->symtab_size / sizeof(Elf64_Sym);
	size_t addr = 0;
	for (size_t i = 1; i < num_symbols; i++) {
		if (memcmp(nid, strtab + symtab[i].st_name, NID_LENGTH) == 0) {
			addr = imagebase + symtab[i].st_value;
			break;
		}
	}

	free(symtab);
	free(strtab);
	return addr;
}

uintptr_t shared_lib_get_metadata(uintptr_t lib) {
	uintptr_t meta = 0;
	kernel_copyout(lib + SHARED_LIB_METADATA_OFFSET, &meta, sizeof(meta));
	return meta;
}

uintptr_t shared_lib_get_address(uintptr_t lib, const char *sym_nid) {
	plt_helper_t helper;
	uintptr_t meta = shared_lib_get_metadata(lib);
	if (meta == 0) {
		return 0;
	}
	uintptr_t imagebase = shared_lib_get_imagebase(lib);
	if (imagebase == 0) {
		return 0;
	}
	kernel_copyout(meta  + METADATA_PLT_HELPER_OFFSET, &helper, sizeof(plt_helper_t));
	return get_symbol_address(&helper, imagebase, sym_nid);
}

uintptr_t get_proc(int target_pid) {
	uintptr_t proc = 0;
	kernel_copyout(KERNEL_ADDRESS_ALLPROC, &proc, sizeof(proc));
	while (proc != 0) {
		int pid = proc_get_pid(proc);
		if (pid == target_pid) {
			return proc;
		}
		proc = proc_get_next(proc);
	}
	return 0;
}

uintptr_t proc_get_vmspace(uintptr_t proc) {
	uintptr_t obj = 0;
	kernel_copyout(proc + PROC_VMSPACE_OFFSET, &obj, sizeof(obj));
	return obj;
}

uintptr_t vmspace_get_root_entry(uintptr_t vmspace) {
	uintptr_t root = 0;
	kernel_copyout(vmspace + VMSPACE_ROOT_ENTRY, &root, sizeof(root));
	return root;
}

uint32_t vmspace_get_num_entries(uintptr_t vmspace) {
	uint32_t numEntries = 0;
	kernel_copyout(vmspace + VMSPACE_NUM_ENTRIES, &numEntries, sizeof(numEntries));
	return numEntries;
}

uintptr_t vmspace_entry_get_next_entry(uintptr_t entry) {
	uintptr_t nextEntry = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_NEXT_ENTRY, &nextEntry, sizeof(nextEntry));
	return nextEntry;
}

uintptr_t vmspace_entry_get_start(uintptr_t entry) {
	uintptr_t start = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_START, &start, sizeof(start));
	return start;
}

uintptr_t vmspace_entry_get_end(uintptr_t entry) {
	uintptr_t end = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_END, &end, sizeof(end));
	return end;
}

uintptr_t vmspace_entry_get_offset(uintptr_t entry) {
	uintptr_t offset = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_OFFSET, &offset, sizeof(offset));
	return offset;
}

uint16_t vmspace_entry_get_prot(uintptr_t entry) {
	uint16_t prot = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_PROT, &prot, sizeof(prot));
	return prot;
}

uint16_t vmspace_entry_get_max_prot(uintptr_t entry) {
	uint16_t prot = 0;
	kernel_copyout(entry + VMSPACE_ENTRY_MAX_PROT, &prot, sizeof(prot));
	return prot;
}

void vmspace_entry_set_prot(uintptr_t entry, int prot) {
	uint16_t buf = prot;
	kernel_copyin(&buf, entry + VMSPACE_ENTRY_PROT, sizeof(uint16_t));
	kernel_copyin(&buf, entry + VMSPACE_ENTRY_MAX_PROT, sizeof(uint16_t));
}

void vmspace_entry_get_name(uintptr_t entry, char name[PROC_SELFINFO_NAME_SIZE]) {
	kernel_copyout(entry + VMSPACE_ENTRY_NAME, name, PROC_SELFINFO_NAME_SIZE);
}

int sys_proc_list(struct proc_list_entry *procs, uint64_t *num) {
	int count = 0;
    if(!procs) {
        // count
		uintptr_t proc = 0;
		kernel_copyout(KERNEL_ADDRESS_ALLPROC, &proc, sizeof(proc));
		do {
			count++;
			proc = proc_get_next(proc);
		} while (proc != 0);
        
        *num = count;
    } else {
        // fill structure
        count = *num;
		uint64_t proc = 0;
		kernel_copyout(KERNEL_ADDRESS_ALLPROC, &proc, sizeof(proc));
        for (int i = 0; i < count; i++) {
			char name[32];
			proc_get_name(proc, name);
			memcpy(procs[i].p_comm, name, sizeof(name));
            procs[i].pid = proc_get_pid(proc);
			proc = proc_get_next(proc);
			if (proc == 0) {
				break;
			}
        }
    }
	return 0;
}

uintptr_t sys_proc_alloc(tracer_t* tracer, int pid, uintptr_t length) {
	uint64_t proc = get_proc(pid);
	uint64_t libc = proc_get_lib(proc, 2);
	uint64_t mallocAddress = shared_lib_get_address(libc, "gQX+4GDQjpM");
	uint64_t allocatedAddress = tracer_call(tracer, mallocAddress, length, 0, 0, 0, 0, 0);
	return allocatedAddress;
}

uintptr_t sys_proc_free(tracer_t* tracer, int pid, uintptr_t address, uintptr_t length) {
	// Idk the best way to recreate this... ignore the length?
	uint64_t proc = get_proc(pid);
	uint64_t libc = proc_get_lib(proc, 2);
	uint64_t freeAddress = shared_lib_get_address(libc, "tIhsqj0qsFE");
	tracer_call(tracer, freeAddress, address, 0, 0, 0, 0, 0);
	return 0;
}

int sys_proc_protect(int pid, uint64_t address, int prot) {
	// Credit to SiSTRo for the idea on protection.
	uintptr_t proc = get_proc(pid);
	uintptr_t vmSpace = proc_get_vmspace(proc);
	uintptr_t currentEntry = vmspace_get_root_entry(vmSpace);

	while (currentEntry != 0) {
		uint64_t start = vmspace_entry_get_start(currentEntry);
		uint64_t end = vmspace_entry_get_end(currentEntry);
		if ((address >= start) && (address < end)) {
			vmspace_entry_set_prot(currentEntry, prot);
			return 0;
		}
		currentEntry = vmspace_entry_get_next_entry(currentEntry);
	}
	
    return 1;
}

int sys_proc_protect_by_name(int pid, const char* name, int prot) {
	uintptr_t proc = get_proc(pid);
	uintptr_t vmSpace = proc_get_vmspace(proc);
	uintptr_t currentEntry = vmspace_get_root_entry(vmSpace);

	while (currentEntry != 0) {
		//uint64_t start = vmspace_entry_get_start(currentEntry);
		//uint64_t end = vmspace_entry_get_end(currentEntry);
		char entryName[32];
		vmspace_entry_get_name(currentEntry, entryName);
		if (strcmp(entryName, name) == 0) {
			vmspace_entry_set_prot(currentEntry, prot);
		}
		memset(entryName, 0, sizeof(entryName));
		currentEntry = vmspace_entry_get_next_entry(currentEntry);
	}
	
    return 1;
}

uintptr_t proc_get_ucred(uintptr_t proc) {
	uintptr_t ucred = 0;
	kernel_copyout(proc + PROC_UCRED_OFFSET, &ucred, sizeof(ucred));
	return ucred;
}

// struct elf_params {
// 	dlsym_t* dlsym;             // 0x00
// 	uint64_t processBaseAddress;// 0x08
// };

// int proc_create_thread(tracer_t* tracer, int pid, uint64_t address) {
// 	void *rpcldraddr = 0;
//     void *stackaddr = 0;
// 	void *paramAddr = 0;

//     uint64_t ldrsize = sizeof(rpcldr);
//     ldrsize += (PAGE_SIZE - (ldrsize % PAGE_SIZE));
    
//     uint64_t stacksize = 0x80000;

//     // allocate rpc ldr
// 	rpcldraddr = sys_proc_alloc(tracer, pid, ldrsize);
// 	sys_proc_protect(pid, rpcldraddr, 0x7);

//     // allocate stack
// 	stackaddr = sys_proc_alloc(tracer, pid, stacksize);
// 	sys_proc_protect(pid, stackaddr, 0x7);

// 	paramAddr = sys_proc_alloc(tracer, pid, sizeof(struct elf_params));
// 	sys_proc_protect(pid, paramAddr, 0x7);

//     // write loader
// 	userland_copyin(pid, (void*)rpcldr, rpcldraddr, sizeof(rpcldr));

// 	uint64_t proc = get_proc(pid);
// 	uint64_t libKernel = proc_get_lib(proc, 0x2001);

//     uint64_t _scePthreadAttrInit = shared_lib_get_address(libKernel, "nsYoNRywwNg");
// 	uint64_t _scePthreadAttrSetstacksize = shared_lib_get_address(libKernel, "UTXzJbWhhTE");
// 	uint64_t _scePthreadCreate = shared_lib_get_address(libKernel, "6UgtwV+0zb4");
// 	uint64_t _thr_initial = libKernel + 0x0000000000095E98;
// 	uint64_t _scePthreadAttrSetstackaddr = shared_lib_get_address(libKernel, "F+yfmduIBB8");

//     if (!_scePthreadAttrInit) {
// 		return 1;
//     }

//     // write variables
// 	userland_copyin(pid, &address, rpcldraddr + offsetof(struct rpcldr_header, stubentry), sizeof(address));
//     userland_copyin(pid, &_scePthreadAttrInit, rpcldraddr + offsetof(struct rpcldr_header, scePthreadAttrInit), sizeof(_scePthreadAttrInit));
// 	userland_copyin(pid, &_scePthreadAttrSetstacksize, rpcldraddr + offsetof(struct rpcldr_header, scePthreadAttrSetstacksize), sizeof(_scePthreadAttrSetstacksize));
// 	userland_copyin(pid, &_scePthreadCreate, rpcldraddr + offsetof(struct rpcldr_header, scePthreadCreate), sizeof(_scePthreadCreate));
// 	userland_copyin(pid, &_thr_initial, rpcldraddr + offsetof(struct rpcldr_header, thr_initial), sizeof(_thr_initial));

	

//     // execute loader
//     // note: do not enter in the pid information as it expects it to be stored in userland
//     uint64_t ldrentryaddr = (uint64_t)rpcldraddr + *(uint64_t *)(rpcldr + 4);
//     r = create_thread(thr, NULL, (void *)ldrentryaddr, NULL, stackaddr, stacksize, NULL, NULL, NULL, 0, NULL);
//     if (r) {
// 		return 1;
//     }

//     // wait until loader is done
//     uint8_t ldrdone = 0;
//     while (!ldrdone) {
// 		userland_copyout(pid, (rpcldraddr + offsetof(struct rpcldr_header, ldrdone)), &ldrdone, sizeof(ldrdone));
//     }

//     return 0;
// }