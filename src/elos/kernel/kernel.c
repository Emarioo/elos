/*
    
*/

#include "elos/kernel/frame/frame.h"
#include "immintrin.h"
#include "elos/kernel/common/string.h"
#include "elos/kernel/log/print.h"
#include "elos/kernel/common/cpu.h"
#include "elos/kernel/driver/pata.h"
#include "elos/kernel/driver/pci.h"
#include "elos/kernel/debug/debug.h"
#include "elos/kernel/memory/phys_allocator.h"
#include "elos/kernel/memory/paging.h"
#include "fs/fat.h"


u32 make_color(u8 r, u8 g, u8 b) {
    return (int)r | ((int)g << 8) | ((int)b << 16);
}

static unsigned long xorshift_state = 2463534242;

unsigned int xorshift32(void) {
    unsigned x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return xorshift_state = x;
}

void srandx(unsigned long s) {
    xorshift_state = s ? s : 2463534242;
}

// TODO: GDT,IDT tables should be placed elsewhere
#pragma pack(push, 1)
typedef struct GDT_IDT_Register {
    u16 size;
    u64 addr;
} GDT_IDT_Register;
#pragma pack(pop)

static GDT_IDT_Register _gdt_register;
static GDT_IDT_Register _idt_register;

static u64 _gdt[3];
static u64 _idt[1024];

void init_gdt_idt() {
    
    _gdt[0] = 0;
    _gdt[1] = (( 0b0010LLU ) << 52) | (( 0b10011010LLU ) << 40);
    _gdt[2] = (( 0b0000LLU ) << 52) | (( 0b10010010LLU ) << 40);

    // @TODO: Setup ring-3 user task system segments
    
    _gdt_register.size = sizeof(_gdt);
    _gdt_register.addr = (u64)&_gdt;
    
    // @TODO: Interrupt descriptor table

    asm ( "lgdt %0\n" : : "m" (_gdt_register) );

    asm volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :::"rax"
    );
}

void kernel_entry() {
    init_paging();

    init_gdt_idt();

    int width,height;
    draw_frame_info(&width,&height);
    
    draw_rect(0, 0, width, height, DARK_BLUE);
    
    printf("Hello World!\n");
    printf("Yes sir\n");

    u8 sector[512];

    // init_pci();

    struct {
        void* addr;
        u16 limit;
    } interrupt_table = { 0, 0 };

    asm ( "sidt %0\n" : : "m" (interrupt_table) );

    init_pata();
    
    // FAT32_boot_record* rec = (FAT32_boot_record*) sector;
    for (int i = 0; i < 512; i++) {
        sector[i] = i;
    }

    int res = ata_read_sectors(sector, 1, 1);


    // ata_write_sector(sector, 0);

    // printf("%2x ", (int) 0);

    
    // u8* sectors = kernel_alloc(512 * 100, NULL);
    
    // int res = ata_read_sector(sectors, 0);


    for (int i=0;i<20;i++) {
        printf("%2x ", (int) sector[i]);
        if ((i+1) % 32 == 0) {
            printf("\n");
        }
    }

    // memset(sectors, 0, sizeof(sector));

    // int lba=0;
    // while (1) {

    //     printf("lba %d    \n", lba);
    //     int res = ata_read_sector(sectors + lba * 512, lba);
    //     if(res)
    //         break;
        
    //     lba++;

    //     // for (int n=0;n<512;n++) {
    //     //     printf("%x ", (int) n * 0x8592);
    //     //     if ((n+1) % 12 == 0) {
    //     //         printf("\n");
    //     //     }
    //     //         //     }
    //     //     // printf("%x\n", (int) 0x589151);
    //     //     // printf("%4x\n", (int) 0x4);
    //     //     // printf("%2x\n", (int) 0x239);
    //     // }

    //     // for (int i=0;i<512;i++) {
    //     //     printf("%2x ", (int) sector[i]);
    //     //     if ((i+1) % 32 == 0) {
    //     //         printf("\n");
    //     //     }
    //     // }
    //     sleep_ns(1000000000);
    // }

    while(1) ;

    // int width,height;
    // draw_frame_info(&width,&height);
    
    // // u32 green = make_color(10, 170, 50);
    // // u32 white = make_color(255, 255, 255);

    // int font_size = 16;
    
    
    // draw_glyphs_from_text_bcolor(40, 400, 16, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);

    // draw_glyphs_from_text_bcolor(40, 420, 18, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    // draw_glyphs_from_text_bcolor(40, 440, 20, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    // draw_glyphs_from_text_bcolor(40, 480, 24, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    // draw_glyphs_from_text_bcolor(40, 500, 29, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    // draw_glyphs_from_text_bcolor(40, 540, 30, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    // draw_glyphs_from_text_bcolor(40, 580, 36, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    
    // draw_rect(0, 0, 100, 100, xorshift32());

    // draw_text_bcolor(520, 550, font_size, PTR_CSTR("Hello Sailor!") , WHITE, BLACK);

    // for(int c = 0; c < 128; c++) {
    //     draw_char(10 + (c%16) * 12, 10 + (c/16) * 12, font_size, c, xorshift32());
    // }
    
    // while (1) {
    //     int x = ((u64)xorshift32() * (u64)width) / 0xFFFFFFFFu;
    //     int y = ((u64)xorshift32() * (u64)height) / 0xFFFFFFFFu;
    //     // int w = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;
    //     // int h = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;
    //     int r = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;
    //     int g = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;
    //     int b = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;

    //     u32 col = make_color(r,g,b);

    //     // draw_char(x, y, font_size, c, xorshift32());

        
    //     // Draw random text
    //     // draw_glyphs_from_text_bcolor(x, y, 16, PTR_CSTR("Hello Sailor!"), g_default_font, col, 0);


    //     // _mm_pause();
    //     // for(int n=0;n<100000;n++) {
    //     //     x *= x;
    //     // }
    // }
}



int bugs;
void kernel_bug() {
    printf("KERNEL BUG");
    serial_printf("KERNEL BUG");
    bugs++;
}
