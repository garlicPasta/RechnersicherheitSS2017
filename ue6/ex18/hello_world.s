.section ".note.openbsd.ident", "a"
   .p2align 2
   .long   0x8
   .long   0x4
   .long   0x1
   .ascii "OpenBSD\0"
   .long   0x
   .p2align 2

.section .data
hello:
   .ascii "Hello, World\n\0"

.section .text

.globl _start

_start:
   pushl $13
   pushl $hello
   pushl $1 
   pushl %eax
   movl $4, %eax 
   int $0x80     
   addl $12, %esp
   xor %eax, %eax
   pushl %eax  
   pushl %eax  
   movl $1, %eax
   int $0x80      
