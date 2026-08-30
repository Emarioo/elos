/*
    @TODO

    Consider memory allocation for line text. Current approach is frail.
        Realloc or allocate more text buffer memory.
        Since the memory is used in lines we can't realloc because it would
        invalidate the memory.
        We would need to allocate new memory, go through all lines and copy text
        to new buffer then free old buffer.
        Or we can have a linked list of memory chunks we can allocate from.
        A heap allocator implementation in user land that is optimized
        for small strings may be a good idea.
        Or some other good idea for small strings.

    Opening big files. How large files do we want to support. 512MB?
        1GB but special optimizations for memory with read-only limitation?

    Show column and line number where cursor is, for multiple cursors pick first cursor?
        Scroll in X direction. important for long lines
        Pick text_content_x based on how many lines are in the file. Large numbers (1002) touches the text area.

    Insert, delete, replace
    Undo, redo
    Multiple cursors
    Text selection
    Copy and paste
    Syntax highlight
    Handle large files (1G)
    Search, regex matching and replace
    Hex editor

    IN ELOS implement repeat kernel service.
    Don't use repeat events from the keyboard device.
    Make our own where we can choose repeat delay and frequency.
    We can also support multiple repeats. For example you hold down left and up arrow. Only up arrow is repeated.
    You release up arrow and nothing happens even though left arrow is still down. This we can fix with repeat kernel service.

Tests
    Open /pkg/slate/slate.elf and replace .rodata strings with something else and save.
    Restart the program and see if different text is printed.

*/

#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "win32_loader/coff_parser.h"



u64 ticks_per_second;

void _start() {
    SYS_ticks_per_second(&ticks_per_second);

    printf("Hello from 32-bit mode, %llu MHz\n", ticks_per_second / 0x100000LU);

    const char* path = "/pkg/win32_loader/wintest.exe";

    // @TODO Parse COFF/PE (wintest program) and allocate sections
    //   fix relocations, fix import table. Provide win32->elos functions to the import table.
    //   Then start executing.

    dump_coff(path);

    SYS_exit(5);
}

