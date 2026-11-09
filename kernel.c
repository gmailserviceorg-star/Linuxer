// kernel.c - VGA text + VBE framebuffer graphics mode, Esc to exit graphics
// gcc -m32 -ffreestanding -fno-pie -nostdlib -c kernel.c -o kernel.o

#include <stdint.h>

/* ---------- Multiboot v1 info struct (only fields we need) ---------- */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    /* framebuffer fields (valid if flags bit 12 set) */
    uint32_t framebuffer_addr_low;
    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed));

#define MB_FLAG_FRAMEBUFFER (1 << 12)

/* ---------- VGA text mode ---------- */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)

static volatile uint16_t* vga = VGA_MEMORY;
static int text_row = 0;
static int text_col = 0;

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void text_putchar(char c) {
    if (c == '\n') {
        text_col = 0;
        text_row++;
    } else {
        vga[text_row * VGA_WIDTH + text_col] = vga_entry(c, 0x07);
        text_col++;
        if (text_col >= VGA_WIDTH) {
            text_col = 0;
            text_row++;
        }
    }
    if (text_row >= VGA_HEIGHT) {
        /* scroll: move everything up one line */
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
            vga[i] = vga[i + VGA_WIDTH];
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
            vga[i] = vga_entry(' ', 0x07);
        text_row = VGA_HEIGHT - 1;
    }
}

static void text_print(const char* s) {
    while (*s) text_putchar(*s++);
}

static void text_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = vga_entry(' ', 0x07);
    text_row = 0;
    text_col = 0;
}

/* ---------- Framebuffer graphics ---------- */
static uint8_t* fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t  fb_bpp = 0;
static int graphics_mode = 0;

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    uint8_t* p = fb_addr + y * fb_pitch + x * (fb_bpp / 8);
    if (fb_bpp == 32) {
        *(uint32_t*)p = color;
    } else if (fb_bpp == 24) {
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    } else if (fb_bpp == 16) {
        *(uint16_t*)p = (uint16_t)color;
    }
}

static void fb_fill(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, color);
}

static void gfx_enter(void) {
    if (!fb_addr) return;
    graphics_mode = 1;
    fb_fill(0x000000);                 /* black */
    /* draw a white box so you can see it works */
    for (uint32_t y = 100; y < 200; y++)
        for (uint32_t x = 100; x < 300; x++)
            fb_putpixel(x, y, 0xFFFFFF);
    /* red bar at top */
    for (uint32_t y = 0; y < 40; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, 0xFF0000);
}

static void gfx_exit(void) {
    graphics_mode = 0;
    /* VGA text memory is still intact, just re-render it */
    text_clear();
    text_print("Back to CLI. Type commands here.\n");
}

/* ---------- PS/2 keyboard (polling, minimal) ---------- */
#define KBD_DATA 0x60
#define KBD_STATUS 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "dN"(port));
    return r;
}

static int kbd_read_scancode(void) {
    if (inb(KBD_STATUS) & 0x01)
        return inb(KBD_DATA);
    return -1;
}

#define SCAN_ESC 0x01

/* ---------- Entry point ---------- */
void kernel_main(uint32_t mb_info_addr) {
    text_clear();
    text_print("CLI OS booting...\n");
    text_print("Kernel reached C code.\n");

    struct multiboot_info* mbi = (struct multiboot_info*)mb_info_addr;

    if (mbi->flags & MB_FLAG_FRAMEBUFFER) {
        fb_addr   = (uint8_t*)(uintptr_t)mbi->framebuffer_addr_low;
        fb_pitch  = mbi->framebuffer_pitch;
        fb_width  = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_bpp    = mbi->framebuffer_bpp;

        text_print("Framebuffer detected.\n");
        text_print("  addr:   0x");
        /* print addr as hex */
        {
            uint32_t a = (uint32_t)(uintptr_t)fb_addr;
            char buf[9]; buf[8] = 0;
            for (int i = 7; i >= 0; i--) {
                uint8_t n = a & 0xF;
                buf[i] = n < 10 ? '0' + n : 'A' + (n - 10);
                a >>= 4;
            }
            text_print(buf);
            text_print("\n");
        }
        text_print("  size:   ");
        {
            char buf[12]; int i = 0;
            uint32_t w = fb_width; 
            if (w == 0) { buf[i++] = '0'; }
            char tmp[12]; int t = 0;
            while (w) { tmp[t++] = '0' + (w % 10); w /= 10; }
            while (t) buf[i++] = tmp[--t];
            buf[i] = 0;
            text_print(buf);
        }
        text_print("x");
        {
            char buf[12]; int i = 0;
            uint32_t h = fb_height;
            if (h == 0) { buf[i++] = '0'; }
            char tmp[12]; int t = 0;
            while (h) { tmp[t++] = '0' + (h % 10); h /= 10; }
            while (t) buf[i++] = tmp[--t];
            buf[i] = 0;
            text_print(buf);
        }
        text_print("\n");
        text_print("Press 'g' for graphics mode, Esc to exit graphics.\n");
    } else {
        text_print("No framebuffer from GRUB.\n");
    }

    for (;;) {
        int sc = kbd_read_scancode();
        if (sc < 0) continue;

        /* ignore key-release (bit 7 set) */
        if (sc & 0x80) continue;

        if (!graphics_mode) {
            if (sc == 0x22) {          /* 'g' key */
                text_print("Entering graphics mode (Esc to exit)...\n");
                gfx_enter();
            }
        } else {
            if (sc == SCAN_ESC) {
                gfx_exit();
            }
        }
    }
}
