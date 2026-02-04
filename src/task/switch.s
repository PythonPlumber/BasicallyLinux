[bits 32]
global switch_to
global context_switch
extern current_task_ptr

%define CTX_EAX 0
%define CTX_EBX 4
%define CTX_ECX 8
%define CTX_EDX 12
%define CTX_ESI 16
%define CTX_EDI 20
%define CTX_ESP 24
%define CTX_EBP 28
%define CTX_EIP 32
%define CTX_EFLAGS 36

switch_to:
    pushfd
    pushad
    mov eax, [current_task_ptr]
    test eax, eax
    jz .load_next
    mov edx, [esp+36]
    mov [eax+CTX_EIP], edx
    mov edx, [esp+32]
    mov [eax+CTX_EFLAGS], edx
    mov edx, [esp+28]
    mov [eax+CTX_EAX], edx
    mov edx, [esp+24]
    mov [eax+CTX_ECX], edx
    mov edx, [esp+20]
    mov [eax+CTX_EDX], edx
    mov edx, [esp+16]
    mov [eax+CTX_EBX], edx
    mov edx, [esp+12]
    mov [eax+CTX_ESP], edx
    mov edx, [esp+8]
    mov [eax+CTX_EBP], edx
    mov edx, [esp+4]
    mov [eax+CTX_ESI], edx
    mov edx, [esp]
    mov [eax+CTX_EDI], edx
.load_next:
    mov eax, [esp+40]
    mov [current_task_ptr], eax
    mov esp, [eax+CTX_ESP]
    push dword [eax+CTX_EFLAGS]
    popfd
    push dword [eax+CTX_EIP]
    mov ebp, [eax+CTX_EBP]
    mov ebx, [eax+CTX_EBX]
    mov ecx, [eax+CTX_ECX]
    mov edx, [eax+CTX_EDX]
    mov esi, [eax+CTX_ESI]
    mov edi, [eax+CTX_EDI]
    mov eax, [eax+CTX_EAX]
    ret

%define PROC_ESP 36

context_switch:
    mov eax, [esp+4]
    mov edx, [esp+8]
    mov ecx, [esp+12]
    test eax, eax
    jz .load_next
    mov [eax+PROC_ESP], ecx
.load_next:
    mov esp, [edx+PROC_ESP]
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret
