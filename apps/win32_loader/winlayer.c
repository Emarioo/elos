
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ELOS_SYSCALL_IMPL
#define ELOS_ERROR_STRING_IMPL
#include "elos/syscalls.h"

__declspec(dllexport)
void __stdcall  print(void* data, int size) {
    SYS_debug_log(data, size);
}
