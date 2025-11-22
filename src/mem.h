#ifndef MEM_H
#define MEM_H

#include <cstdint>
#include <cstring>
#include <processthreadsapi.h>
#include <minwindef.h>
#include <memoryapi.h>
#include "impl.h"
namespace mem {

	inline void* hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, false, GetCurrentProcessId());

	inline const unsigned char jmp[] = { 0xEB };

    inline void Patch(void* dst, const void* src, std::uint32_t size) {
		if (!dst || !src || size <= 0) return;
		DWORD oldProtect;
		VirtualProtectEx(hProcess, dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
		std::memcpy(dst, src, size);
		if (!FlushInstructionCache(GetCurrentProcess(), dst, size)) {
            atfix::log("Flush failed");
        }
        VirtualProtectEx(hProcess, dst, size, oldProtect, &oldProtect);
	}

}

#endif