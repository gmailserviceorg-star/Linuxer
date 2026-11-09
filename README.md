# Linuxer

A minimal bootable CLI operating system for VirtualBox. GRUB boots a
Multiboot kernel, which loads an initramfs containing Bash and the
non-graphical Linux commands. A custom package manager (pop install)
adds more. A network driver module brings up the NIC.

No window managers. No display server. CLI by default, with a fullscreen
graphics mode toggled by Esc. Graphical tools are not installed unless you
pop install them yourself.

## Status

- GRUB boot (Multiboot v1): working
- Kernel entry in C: working
- VGA text output: working
- VBE framebuffer graphics mode: working (g to enter, Esc to exit)
- PS/2 keyboard polling: working (Esc, g)
- Interrupts + IDT: in progress
- Initramfs (cpio) unpack: in progress
- Bash + non-graphical coreutils in initramfs: in progress
- pop install package manager: in progress
- Network driver (kernel module): in progress
- Graphics text renderer (bitmap font): in progress

## Design

- Bootloader: GRUB, Multiboot v1.
- Kernel: 32-bit x86, freestanding C, no libc.
- Text mode: VGA text buffer at 0xB8000.
- Graphics mode: VBE linear framebuffer requested via the Multiboot
  header (1024x768, 32bpp). GRUB sets the mode before jumping to the kernel.
- Mode switching: press g in the CLI to enter fullscreen graphics mode.
  Press Esc to return to the CLI. Text is re-rendered on return.
- Keyboard: IRQ1 (PS/2) once the IDT is up; polling as a fallback.
- Initramfs: a cpio archive passed as a Multiboot module. The kernel
  unpacks it into a ramfs, then execs /bin/bash.
- Userland: real Bash, non-graphical GNU coreutils, util-linux.
  No X11/Wayland, no GUI tools.
- Package manager: pop install <pkg>. Packages are .tar.gz with a
  manifest (name, version, files, deps). v1 uses a local repo on the ISO;
  network fetching arrives once the NIC driver works.
- Network driver: kernel module for the VirtualBox NIC (E1000 first,
  virtio-net second). Loaded after boot.
- No graphical tools by default. No xclock, xeyes, or googly eyes.
  If someone wants them, they pop install them explicitly.

## Repository layout

    .
    ├── boot.asm           Multiboot header + entry point (NASM, ELF32)
    ├── kernel.c           Kernel: VGA text, framebuffer, keyboard, initramfs
    ├── idt.c / idt.h      IDT + IRQ handlers (keyboard, timer)
    ├── initramfs.c        cpio unpacker, ramfs, exec /bin/bash
    ├── gfx_text.c         Bitmap font renderer for graphics mode
    ├── pop/               pop install package manager
    ├── mynetdev.c         Network driver kernel module (E1000)
    ├── grub.cfg           GRUB menu entry + initramfs module
    ├── build.yml          GitHub Actions: builds linuxer.iso
    └── README.md

## Building

Locally, requires: nasm, gcc-multilib, ld, grub-pc-bin, grub-common,
xorriso, cpio.

    # 1. Kernel
    nasm -f elf32 boot.asm -o boot.o
    gcc -m32 -ffreestanding -fno-pie -nostdlib -c kernel.c -o kernel.o
    gcc -m32 -ffreestanding -fno-pie -nostdlib -c idt.c -o idt.o
    gcc -m32 -ffreestanding -fno-pie -nostdlib -c initramfs.c -o initramfs.o
    gcc -m32 -ffreestanding -fno-pie -nostdlib -c gfx_text.c -o gfx_text.o
    ld -m elf_i386 -Ttext 0x100000 -o kernel.bin \
       boot.o kernel.o idt.o initramfs.o gfx_text.o --oformat elf32-i386

    # 2. Initramfs (Bash + non-graphical coreutils + pop)
    mkdir -p initramfs/{bin,dev,proc,sys,tmp,etc,usr/bin}
    cp /bin/bash initramfs/bin/
    for c in ls cp mv rm cat echo mkdir rmdir ln chmod chown stat touch \
             head tail wc sort uniq grep sed awk find tar gzip \
             ps kill mount umount ip ping; do
        cp "$(command -v $c)" initramfs/bin/ 2>/dev/null || true
    done
    cp pop/pop initramfs/bin/pop
    (cd initramfs && find . | cpio -o -H newc) > initramfs.cpio

    # 3. ISO
    mkdir -p iso/boot/grub
    cp kernel.bin iso/boot/kernel.bin
    cp initramfs.cpio iso/boot/initramfs.cpio
    cp grub.cfg iso/boot/grub/grub.cfg
    grub-mkrescue -o linuxer.iso iso

GitHub Actions: push to main. build.yml installs the toolchain, builds
the kernel, assembles the initramfs, produces linuxer.iso, and uploads it
as an artifact named linuxer-iso.

## Running in VirtualBox

1. New VM: Type Other, Version Other/Unknown (64-bit).
2. Memory: 256 MB.
3. Do not add a hard disk.
4. Settings, System, Motherboard: Enable EFI must be unchecked.
5. Attach linuxer.iso as a CD/DVD in Settings, Storage.
6. Boot.

Expected output:

    Linuxer booting...
    Framebuffer detected: 1024x768x32
    Unpacking initramfs...
    Launching /bin/bash...

    bash-5.2#

Type normal Bash commands and non-graphical Linux commands. Press g to
enter graphics mode, Esc to return. Use pop install <pkg> to add packages
from the local repo on the ISO.

## Roadmap

1. Interrupts + IDT: IRQ1 keyboard, PIT timer, exception handlers.
2. Initramfs: cpio unpack into ramfs, mount devtmpfs/proc/sys, exec Bash.
3. Coreutils: non-graphical GNU coreutils and util-linux in the image.
4. pop install: local repo on the ISO first; manifest + deps.
5. Network driver: E1000 kernel module, then virtio-net. pop fetches from
   a remote repo once the NIC is up.
6. Graphics text renderer: bitmap font so graphical pop packages can print
   in framebuffer mode.
7. Esc toggle polish: save/restore CLI state cleanly across mode switches.

## License

TBD.
