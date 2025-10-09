OUTPUT_ARCH( "riscv" )
ENTRY( entry )

PHDRS
{
  text PT_LOAD FLAGS(5);    /* R + X = 4 + 1 = 5 */
  rodata PT_LOAD FLAGS(4);  /* R = 4 */
  data PT_LOAD FLAGS(6);    /* R + W = 4 + 2 = 6 */
}

SECTIONS
{
  /*
   * ensure that entry.S / entry is at 0x80000000,
   * where qemu's -kernel jumps.
  */
  . = 0x80000000;

  .text : {
    kernel/boot/entry.o(entry)
    *(.text)
    PROVIDE(etext = .);
    . = ALIGN(4096);
    PROVIDE(trampoline = .);
    trampoline_start = .;
    *(.text.trampoline.utrap_entry)
    *(.text.trampoline)
    ASSERT(. - trampoline_start <= 4096, "Error: trampoline exceeds 4KB");
  } :text

    /* 仅包含 systemd_main 的代码 */
  .init : {
    . = ALIGN(4096);
    init_start = .;
    PROVIDE(init = .);
    *(.init.text)         
    PROVIDE(einit = .);
    ASSERT(. - init_start <= 4096, "Error: init exceeds 4KB");
  } :text

  .rodata : {
    . = ALIGN(4096);
    PROVIDE(brodata = .);
    *(.srodata .srodata.*) 
    . = ALIGN(16);
    *(.rodata .rodata.*)
    PROVIDE(erodata = .);
  } :rodata

  .data : {
    . = ALIGN(4096);
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