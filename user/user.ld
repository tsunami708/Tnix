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
  .text : {
    *(.text .text.*)
  } :text

  . = ALIGN(0x1000);
  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) 
    . = ALIGN(16);
    *(.rodata .rodata.*)
  } :rodata

  .eh_frame : {
       *(.eh_frame)
       *(.eh_frame.*)
   } :rodata

  . = ALIGN(0x1000);
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) 
    . = ALIGN(16);
    *(.data .data.*)
  } :data

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) 
    . = ALIGN(16);
    *(.bss .bss.*)
  } :data

  PROVIDE(end = .);
}