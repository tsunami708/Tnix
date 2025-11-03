OUTPUT_ARCH( "riscv" )
ENTRY( main )

PHDRS
{
  text PT_LOAD FLAGS(5);    
  rodata PT_LOAD FLAGS(4);  
  data PT_LOAD FLAGS(6);    
}

SECTIONS
{
  . = 0x1000;
  .text : {
    text_start = .;
    *(.text .text.*)
    text_end = .;
    ASSERT((text_end - text_start) <= 4096, "Error: .text section exceeds 4096 bytes");
  } :text

  . = ALIGN(0x1000);
  
  .rodata : {
    rodata_start = .;
    . = ALIGN(16);
    *(.srodata .srodata.*) 
    . = ALIGN(16);
    *(.rodata .rodata.*)
    rodata_end = .;
    ASSERT((rodata_end - rodata_start) <= 4096, "Error: .rodata section exceeds 4096 bytes");
  } :rodata
  
  .eh_frame : {
       *(.eh_frame)
       *(.eh_frame.*)
   } :rodata

  . = ALIGN(0x1000);
  .data : {
    data_start = .;
    . = ALIGN(16);
    *(.sdata .sdata.*) 
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(16);
    *(.sbss .sbss.*) 
    . = ALIGN(16);
    *(.bss .bss.*)
    bss_end = .;
    ASSERT((bss_end - data_start) <= 4096, "Error: .data + .bss sections exceed 4096 bytes");
  } :data
  PROVIDE(end = .);
}