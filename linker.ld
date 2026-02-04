ENTRY(_start)

SECTIONS
{
    .multiboot 0x100000 : AT(0x0) {
        KEEP(*(.multiboot))
    }

    . = 0x100000 + SIZEOF(.multiboot);
    kernel_start = .;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .ai_model : {
        _ai_model_start = .;
        KEEP(*(.ai_model*))
        _ai_model_end = .;
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
