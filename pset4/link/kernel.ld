OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)

PHDRS {
    text PT_LOAD;
}

SECTIONS {
    . = 0x40000;

    /* Text segment: instructions and read-only globals */
    kernel_start = .;
    .text : {
        *(.text.unlikely .text.*_unlikely .text.unlikely.*)
        *(.text.exit .text.exit.*)
        *(.text.startup .text.startup.*)
        *(.text.hot .text.hot.*)
        *(.text .stub .text.* .gnu.linkonce.t.*)
    } :text

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
        *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
    } :text

    /* Constructors: these sections support global initialization
       functions, such as for global C++ objects with constructors. */
    .init_array : {
        PROVIDE(__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)
                SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE(__init_array_end = .);
    } :text
    .ctors : {
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
    } :text

    /* Data segment: read/write and zero-initialized globals */
    . = ALIGN(4096);       /* Align to a page boundary */
    PROVIDE(kernel_data = .);
    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(16);
        interrupt_descriptors = .;
        KEEP (*(.interrupt_descriptors))
    } :text
    PROVIDE(kernel_edata = .);

    .bss : {
        *(.bss .bss.* .gnu.linkonce.b.*)
    } :text
    PROVIDE(kernel_end = .);

    /DISCARD/ : { *(.eh_frame .note.GNU-stack) }
}
