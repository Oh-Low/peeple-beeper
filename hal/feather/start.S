.syntax unified
.cpu cortex-m4
.thumb

.section .vectors, "a", %progbits
.align 2
.global vectors
vectors:
  .word _estack
  .word reset_handler
  .word fault_handler
  .word fault_handler
  .word fault_handler
  .word fault_handler
  .word fault_handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word fault_handler
  .word 0
  .word 0
  .word fault_handler
  .word fault_handler
  .rept 48
  .word fault_handler
  .endr

.section .text
.thumb_func
.global reset_handler
.type reset_handler, %function
reset_handler:
  ldr r0, =_estack
  mov sp, r0
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  bcs 2f
  ldr r3, [r0], #4
  str r3, [r1], #4
  b 1b
2:
  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
3:
  cmp r1, r2
  bcs 4f
  str r3, [r1], #4
  b 3b
4:
  bl main
5:
  b 5b

.thumb_func
.global fault_handler
.type fault_handler, %function
fault_handler:
  bl fault_blink
  b fault_handler
