; boot.asm - Multiboot header with graphics request + entry point
; nasm -f elf32 boot.asm -o boot.o

MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_VIDEO_MODE    equ 1 << 2      ; ask GRUB for a graphics mode
MBOOT_MAGIC         equ 0x1BADB002
MBOOT_FLAGS         equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_VIDEO_MODE
MBOOT_CHECKSUM      equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    ; --- video mode fields (only used when bit 2 is set) ---
    dd 0                    ; mode_type: 0 = linear graphics mode
    dd 1024                 ; width
    dd 768                  ; height
    dd 32                   ; depth (bpp)

section .bss
align 16
stack_bottom:
    resb 16384              ; 16 KiB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top      ; set up stack
    push ebx                ; Multiboot info pointer -> kernel_main arg
    cli                     ; no interrupts yet
    call kernel_main
.hang:
    hlt
    jmp .hang
