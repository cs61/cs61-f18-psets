###############################################################################
# Exception handlers
#
#   Assembly code defining kernel exception handlers
#   (for interrupts, traps, and faults).

// import constants from kernel.hh and x86-64.h
#include "obj/k-asm.h"

.text

// kernel_entry
//    The bootloader jumps here after loading the kernel.
//    The code initializes `%rsp` to the top of the kernel stack,
//    then jumps to `kernel_start`.
.globl kernel_entry
kernel_entry:
        // initialize stack pointer and base pointer
        movq $KERNEL_STACK_TOP, %rsp
        movq %rsp, %rbp
        // clear `%rflags`
        pushq $0
        popfq
        // check for multiboot command line; if found pass it along
        cmpl $0x2BADB002, %eax
        jne 1f
        testl $4, (%rbx)
        je 1f
        movl 16(%rbx), %edi
        jmp 2f
1:      movq $0, %rdi
2:      jmp _Z6kernelPKc



// Exception handlers and interrupt descriptor table
//    This code creates an exception handler for all 256 possible
//    exceptions, and initializes a table in the
//    `.interrupt_descriptors` section containing those handlers.
//    The `init_hardware` kernel function installs this table.

// The `exception_handler` macro creates one exception handler
.altmacro
.macro exception_handler num
exception_entry_\num:
        // push zero error code, unless exception did so already
.if \num != 8 && (\num < 10 || \num > 14) && \num != 17
        pushq $0
.endif
        // push exception number
        pushq $\num
        jmp exception_entry

        // add that handler to the `.interrupt_descriptors` section
.pushsection .interrupt_descriptors, "aw", @progbits
        .quad exception_entry_\num
        .quad 0
.popsection
.endm

// now create all 256 exception handlers and table
.set exception_number, 0
.rept 256
exception_handler %exception_number
.set exception_number, exception_number + 1
.endr


// Exception entry point
//    The 256 exception handlers defined above all jump here.

        .globl exception_entry
exception_entry:
        pushq %gs
        pushq %fs
        pushq %r15
        pushq %r14
        pushq %r13
        pushq %r12
        pushq %r11
        pushq %r10
        pushq %r9
        pushq %r8
        pushq %rdi
        pushq %rsi
        pushq %rbp
        pushq %rbx
        pushq %rdx
        pushq %rcx
        pushq %rax
        movq %rsp, %rdi

        // load kernel page table
        movq $kernel_pagetable, %rax
        movq %rax, %cr3

        call _Z9exceptionP8regstate
        // `exception` should never return.


        .globl _Z16exception_returnP16x86_64_pagetableP8regstate
_Z16exception_returnP16x86_64_pagetableP8regstate:
        // load process page table (first argument)
        movq %rdi, %cr3

        // restore registers (second argument)
        movq %rsi, %rsp
        popq %rax
        popq %rcx
        popq %rdx
        popq %rbx
        popq %rbp
        popq %rsi
        popq %rdi
        popq %r8
        popq %r9
        popq %r10
        popq %r11
        popq %r12
        popq %r13
        popq %r14
        popq %r15
        popq %fs
        popq %gs
        addq $16, %rsp

        // return to process
        iretq


// syscall_entry
//    Kernel entry point for the `syscall` instruction

        .globl syscall_entry
syscall_entry:
        movq %rsp, KERNEL_STACK_TOP - 16 // save entry %rsp to kernel stack
        movq $KERNEL_STACK_TOP, %rsp     // change to kernel stack

        // structure used by `iret`:
        pushq $(SEGSEL_APP_DATA + 3)   // %ss
        subq $8, %rsp                  // skip saved %rsp
        pushq %r11                     // %rflags
        pushq $(SEGSEL_APP_CODE + 3)   // %cs
        pushq %rcx                     // %rip

        // other registers:
        subq $8, %rsp                  // error code unused
        pushq $-1                      // reg_intno
        pushq %gs
        pushq %fs
        pushq %r15 // callee saved
        pushq %r14 // callee saved
        pushq %r13 // callee saved
        pushq %r12 // callee saved
        subq $8, %rsp                  // %r11 clobbered by `syscall`
        pushq %r10
        pushq %r9
        pushq %r8
        pushq %rdi
        pushq %rsi
        pushq %rbp // callee saved
        pushq %rbx // callee saved
        pushq %rdx
        subq $8, %rsp                  // %rcx clobbered by `syscall`
        pushq %rax

        // load kernel page table
        movq $kernel_pagetable, %rax
        movq %rax, %cr3

        // call syscall()
        movq %rsp, %rdi
        call _Z7syscallP8regstate

        // load process page table
        movq current, %rcx
        movq (%rcx), %rcx
        movq %rcx, %cr3

        // skip over other registers
        addq $(8 * 19), %rsp

        // return to process
        iretq
