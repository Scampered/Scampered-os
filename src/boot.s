; boot.s — GRUB Multiboot loader
; NASM syntax, 32-bit

SECTION .multiboot
align 4
    dd 0x1BADB002          ; magic
    dd 0x00                ; flags
    dd -(0x1BADB002)       ; checksum

SECTION .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top     ; set up stack
    mov eax, ebx        ; ebx = multiboot_info from GRUB
    call kernel_main
    hlt
    jmp $

SECTION .bss
align 16
stack_bottom:
    resb 16384             ; 16 KB stack
stack_top:
