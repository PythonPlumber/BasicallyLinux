ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    .text : {
        *(.multiboot)
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    .rodata : {
        *(.rodata*)
    }

    .ai_model BLOCK(4K) : ALIGN(4K)
    .ai_model : {
        _ai_model_start = .;
        KEEP(*(.ai_model*))
        _ai_model_end = .;
    }

    .data BLOCK(4K) : ALIGN(4K)
    .data : {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    .bss : {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
