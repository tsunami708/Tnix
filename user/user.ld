OUTPUT_ARCH( "riscv" )

PHDRS
{
  text PT_LOAD FLAGS(5);    
  rodata PT_LOAD FLAGS(4);  
  data PT_LOAD FLAGS(6);    
}

SECTIONS
{
  . = 0x1000;
  __text_start = .;
  .text : {
    *(.text .text.*)
    __text_end = .;
    ASSERT(__text_end - __text_start <= 4096, "Error: .text section exceeds 4096 bytes");
  } :text

  . = ALIGN(0x1000);
  __rodata_start = .;
  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) 
    . = ALIGN(16);
    *(.rodata .rodata.*)
    __rodata_end = .;
    ASSERT(__rodata_end - __rodata_start <= 4096, "Error: .rodata section exceeds 4096 bytes");
  } :rodata

  .eh_frame : {
       *(.eh_frame)
       *(.eh_frame.*)
   } :rodata

  . = ALIGN(0x1000);
  __data_start = .;
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) 
    . = ALIGN(16);
    *(.data .data.*)
    __data_end = .;
  } :data

  __bss_start = .;
  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) 
    . = ALIGN(16);
    *(.bss .bss.*)
    __bss_end = .;
    ASSERT((__data_end - __data_start) + (__bss_end - __bss_start) <= 4096, "Error: .data + .bss sections exceed 4096 bytes");
  } :data

  PROVIDE(end = .);
}