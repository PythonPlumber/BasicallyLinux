[bits 32]
global gdt_flush

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush_cs
flush_cs:
    ret

; Simple macro for serial debug in assembly if needed, 
; but let's just use the segment load as proof of success.

