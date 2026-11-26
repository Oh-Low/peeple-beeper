/* Feather UF2 bootloader jumps to the image at 0x26000.
   The 100 KB store sits above the application and below the bootloader at 0xF4000. */
ENTRY(reset_handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00026000, LENGTH = 0x000B4000
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 0x00040000
}

__nvm_start = 0x000DA000;
__nvm_end = 0x000F4000;
_estack = ORIGIN(RAM) + LENGTH(RAM);
PROVIDE(end = _ebss);
PROVIDE(_end = _ebss);

SECTIONS
{
  .text :
  {
    KEEP(*(.vectors))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  _sidata = LOADADDR(.data);

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  ASSERT(_ebss <= _estack - 8192, "bss collides with the stack")
}
