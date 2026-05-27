/*
    Main terminal
*/

#include "elos/keyboard.h"
#include "elos/kernel/video/frame.h"
#include "elos/frame_buffer.h"
#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/cpu.h"
#include "elos/execution.h"
#include "elos/kernel_console.h"

#include "user/pipe.h"

int ring_size = 0x10000;
Handle ringBuffer;

#define printf(...) KCON_printf(__VA_ARGS__)

void proc1();
void proc2();

void producer_main() {

    printf("Starting producers\n");
    
    int res = create_ringBuffer(ring_size, &ringBuffer);
    if (res < 0) {
        printf("Could not create ring buffer\n");
        return;
    }

    int coreCount = CPU_get_core_count();
    for (int i=0;i<coreCount;i++) {
        EXEC_create_kernel_thread(proc1, i);
        EXEC_create_kernel_thread(proc2, i);
    }

    while (1) pause();
}


#define MS 1000000


typedef struct {
    char text[16];
} Message;

void proc1() {
    int core = CPU_get_core_index();
    printf("Proc 1, on core: %d\n", core);

    // while (1) pause();

    int index = 0;

    Message messages[] = {
        { "Hello" },
        { "World" },
        { "Writing" },
        { "Bytes" },
        { "To you." },
    };

    while (1) {
        int written = write(ringBuffer, &messages[index], sizeof(messages[index]));
        index = (index + 1) % ARRAY_LENGTH(messages);

        printf("C%d Sent %d\n", core, index);
        CPU_sleep(10 * MS);
    }
}

void proc2() {
    int core = CPU_get_core_index();
    printf("Proc 2, on core: %d\n", core);

    // while (1) pause();

    Message msg;

    while (1) {
        int read_bytes = read(ringBuffer, &msg, sizeof(msg));
        printf("C%d Read: %s\n", core, msg.text);

        CPU_sleep(300 * MS);
    }
}
