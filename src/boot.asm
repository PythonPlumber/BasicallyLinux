section .multiboot
align 4
    dd 0x1BADB002             ; Magic number
    dd 0x00000003             ; Flags (align modules + mem info)
    dd -(0x1BADB002 + 0x00000003) ; Checksum

section .text
global _start
extern kernel_main

_start:
    ; Setup stack and call kernel_main
    mov esp, stack_top
    push ebx                  ; Pointer to Multiboot info
    call kernel_main
    cli
.hang: hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KB stack
stack_top:
