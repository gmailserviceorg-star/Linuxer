// kernel.c - everything in one file

#include <stdint.h>

/* ---------- Multiboot info ---------- */
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
    uint32_t framebuffer_addr_low;
    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed));

#define MB_FLAG_FRAMEBUFFER (1 << 12)

/* ---------- VGA text ---------- */
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

static void text_print_hex(uint32_t v) {
    char buf[9];
    buf[8] = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t n = v & 0xF;
        buf[i] = n < 10 ? '0' + n : 'A' + (n - 10);
        v >>= 4;
    }
    text_print(buf);
}

static void text_print_dec(uint32_t v) {
    char buf[12];
    int i = 11;
    buf[i] = 0;
    if (v == 0) { buf[--i] = '0'; }
    while (v) { buf[--i] = '0' + (v % 10); v /= 10; }
    text_print(&buf[i]);
}

/* ---------- Framebuffer ---------- */
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

/* 5x7 bitmap font, characters 0x20 to 0x7E */
static const uint8_t font[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* ~ */
};

static int gfx_col = 0;
static int gfx_row = 0;

static void gfx_clear(uint32_t color) {
    fb_fill(color);
    gfx_col = 0;
    gfx_row = 0;
}

static void gfx_char(char c, uint32_t fg, uint32_t bg, int scale) {
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t* glyph = font[(uint8_t)c - 0x20];
    int px = gfx_col * 6 * scale;
    int py = gfx_row * 8 * scale;
    for (int x = 0; x < 6 * scale; x++)
        for (int y = 0; y < 8 * scale; y++)
            fb_putpixel(px + x, py + y, bg);
    for (int x = 0; x < 5; x++) {
        uint8_t bits = glyph[x];
        for (int y = 0; y < 7; y++) {
            if (bits & (1 << y)) {
                for (int sx = 0; sx < scale; sx++)
                    for (int sy = 0; sy < scale; sy++)
                        fb_putpixel(px + x * scale + sx, py + y * scale + sy, fg);
            }
        }
    }
    gfx_col++;
    if (gfx_col * 6 * scale >= (int)fb_width) {
        gfx_col = 0;
        gfx_row++;
    }
}

static void gfx_print(const char* s, uint32_t fg, uint32_t bg, int scale) {
    while (*s) {
        if (*s == '\n') {
            gfx_col = 0;
            gfx_row++;
        } else {
            gfx_char(*s, fg, bg, scale);
        }
        s++;
    }
}

static void gfx_enter(void) {
    if (!fb_addr) return;
    graphics_mode = 1;
    gfx_clear(0x000000);
    gfx_print("Graphics mode. Esc to return to CLI.\n", 0xFFFFFF, 0x000000, 2);
    gfx_print("Hello from Linuxer.\n", 0x00FF00, 0x000000, 2);
}

static void gfx_exit(void) {
    graphics_mode = 0;
    text_clear();
    text_print("Back to CLI.\n");
}

/* ---------- PS/2 keyboard ---------- */
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

/* ---------- Entry ---------- */
void kernel_main(uint32_t mb_info_addr) {
    text_clear();
    text_print("Linuxer booting...\n");

    struct multiboot_info* mbi = (struct multiboot_info*)mb_info_addr;

    if (mbi->flags & MB_FLAG_FRAMEBUFFER) {
        fb_addr   = (uint8_t*)(uintptr_t)mbi->framebuffer_addr_low;
        fb_pitch  = mbi->framebuffer_pitch;
        fb_width  = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_bpp    = mbi->framebuffer_bpp;

        text_print("Framebuffer: ");
        text_print_dec(fb_width);
        text_print("x");
        text_print_dec(fb_height);
        text_print("x");
        text_print_dec(fb_bpp);
        text_print(" at 0x");
        text_print_hex((uint32_t)(uintptr_t)fb_addr);
        text_print("\n");
        text_print("Press g for graphics, Esc to exit.\n");
    } else {
        text_print("No framebuffer.\n");
    }

    for (;;) {
        int sc = kbd_read_scancode();
        if (sc < 0) continue;
        if (sc & 0x80) continue;
        if (!graphics_mode) {
            if (sc == 0x22) gfx_enter();
        } else {
            if (sc == SCAN_ESC) gfx_exit();
        }
    }
}
