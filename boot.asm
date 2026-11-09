MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_VIDEO_MODE    equ 1 << 2
MBOOT_MAGIC         equ 0x1BADB002
MBOOT_FLAGS         equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_VIDEO_MODE
MBOOT_CHECKSUM      equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    dd 0        ; mode_type
    dd 1024     ; width
    dd 768      ; height
    dd 32       ; depth

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; 1. Set up stack safely
    mov esp, stack_top

    ; 2. Clear the screen directly from Assembly
    ; This proves we reached our code without crashing
    mov edi, 0xB8000
    mov ecx, 80 * 25
    mov ax, 0x0F20      ; 0x0F = white on black, 0x20 = space
    rep stosw

    ; 3. Push Multiboot pointer and call C
    push ebx
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
