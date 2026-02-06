[bits 16]
section .text

global smp_trampoline_start
global smp_trampoline_end
extern smp_ap_entry

smp_trampoline_start:
    cli
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Calculate physical address of GDT pointer (0x1000 + offset)
    mov eax, 0x1000 + (gdt_ptr_16 - smp_trampoline_start)
    lgdt [eax]

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Long jump to 32-bit code (0x1000 + offset)
    jmp 0x08:(0x1000 + (smp_trampoline_32 - smp_trampoline_start))

[bits 32]
smp_trampoline_32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack - the BSP will provide this address at 0x1000 + offset
    mov eax, 0x1000 + (smp_ap_stack - smp_trampoline_start)
    mov esp, [eax]
    
    ; Jump to C entry point
    call smp_ap_entry

.halt:
    hlt
    jmp .halt

align 16
gdt_16:
    dq 0x0000000000000000 ; Null descriptor
    dq 0x00CF9A000000FFFF ; Code descriptor
    dq 0x00CF92000000FFFF ; Data descriptor

gdt_ptr_16:
    dw 23
    dd 0x1000 + (gdt_16 - smp_trampoline_start)

global smp_ap_stack
smp_ap_stack:
    dd 0

smp_trampoline_end:
