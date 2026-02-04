ENTRY(_start)

PHDRS
{
    multiboot PT_LOAD FLAGS(5); /* Read + Execute */
    text PT_LOAD FLAGS(5);      /* Read + Execute */
    data PT_LOAD FLAGS(6);      /* Read + Write */
}

SECTIONS
{
    /* The kernel starts at 1MB */
    . = 1M;
    kernel_start = .;

    .multiboot : AT(ADDR(.multiboot))
    {
        KEEP(*(.multiboot))
    } : multiboot

    .text : AT(ADDR(.text))
    {
        *(.text)
        *(.text.*)
    } : text

    .rodata : AT(ADDR(.rodata))
    {
        *(.rodata)
        *(.rodata.*)
    } : data

    .ai_model : AT(ADDR(.ai_model))
    {
        _ai_model_start = .;
        KEEP(*(.ai_model*))
        _ai_model_end = .;
    } : data

    .data : AT(ADDR(.data))
    {
        *(.data)
        *(.data.*)
    } : data

    .bss : AT(ADDR(.bss))
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } : data

    kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
