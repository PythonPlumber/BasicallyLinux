[bits 32]

section .multiboot
align 4
dd 0x1BADB002
dd 0x00000000
dd 0xE4524FFE

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main
    add esp, 8
    jmp $

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
