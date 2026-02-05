[bits 32]

section .multiboot
align 4
    dd 0x1BADB002             ; Magic number
    dd 0x00000003             ; Flags (align modules + mem info)
    dd -(0x1BADB002 + 0x00000003) ; Checksum

section .text
global _start
extern kernel_main

_start:
    ; Setup stack
     mov esp, stack_top

     ; Direct VGA test: '@' at top-left
     mov word [0xB8000], 0x0E40 ; '@' yellow on black
 
     ; Serial debug '!' to show we started
    mov dx, 0x3F8 + 5 ; LSR
.wait_serial:
    in al, dx
    test al, 0x20
    jz .wait_serial
    mov dx, 0x3F8
    mov al, '!'
    out dx, al
    
    ; Serial debug 'A' to show we are about to call kernel_main
    mov dx, 0x3F8 + 5 ; LSR
.wait_serial_A:
    in al, dx
    test al, 0x20
    jz .wait_serial_A
    mov dx, 0x3F8
    mov al, 'A'
    out dx, al

    push ebx                  ; Pointer to Multiboot info
    push eax                  ; Multiboot magic number
    call kernel_main
    cli
.hang: hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KB stack
stack_top:
