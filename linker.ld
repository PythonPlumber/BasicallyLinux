ENTRY(_start)

SECTIONS
{
    . = 1M;
    kernel_start = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        /* MUST be at the very top of the binary */
        KEEP(*(.multiboot))
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .ai_model BLOCK(4K) : ALIGN(4K)
    {
        _ai_model_start = .;
        KEEP(*(.ai_model*))
        _ai_model_end = .;
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
