/**
 * this linker script tells ld how to link and which symbols to add to the
 * kernel.x binary
 */

ENTRY(entry)
LS_Virt = 0x80001000;
LS_Phys = 0x1000;
/* Specify the memory areas */

SECTIONS
{
  .text LS_Virt : AT(LS_Phys) 
  {
    PROVIDE(kernel_start_address = ABSOLUTE(.));

    LS_Code = .;

    text_start_address = .;
    *(.text)
    *(.text.*)
    text_end_address = .;
    ro_data_start_address = .;
    *(.rodata*)
    ro_data_end_address = .;
  }
  
  .data ALIGN(4096) : AT(LS_Phys + (LS_Data - LS_Code))
  {
    LS_Data = .;
    data_start_address = .;
    *(.data*)
    data_end_address = .;
  }
  
  .bss ALIGN(4096) : AT(LS_Phys + (LS_Bss - LS_Code))
  {
    LS_Bss = .;
    bss_start_address = .;
    *(.bss)
    . = ALIGN(4096);
    *(COMMON)
    bss_end_address = .;

  }

  .stab ALIGN(4096) : AT(LS_Phys + (LS_Stab - LS_Code))
  {
    LS_Stab = .;
    stab_start_address_nr = .;
    *(.stab)
    stab_end_address_nr = .;
  }
  
  .stabstr ALIGN(4096) : AT(LS_Phys + (LS_Stabstr - LS_Code))
  {
    LS_Stabstr = .;
    stabstr_start_address_nr = .;
    *(.stabstr)
    . = ALIGN(4096);
    stabstr_end_address_nr = .;

    PROVIDE(kernel_end_address = .);
  }
  
  .ARM.attributes 0 : { *(.ARM.attributes) }
  .comment 0 : { *(.comment) }
}
