#ifndef WEENSYOS_PROCESS_H
#define WEENSYOS_PROCESS_H
#include "lib.hh"
#include "x86-64.h"
#if WEENSYOS_KERNEL
#error "process.h should not be used by kernel code."
#endif

// process.h
//
//    Support code for WeensyOS processes.


// SYSTEM CALLS

// sys_getpid
//    Return current process ID.
inline pid_t sys_getpid() {
    register uintptr_t rax asm("rax") = SYSCALL_GETPID;
    asm volatile ("syscall"
                  : "+a" (rax)
                  :
                  : "cc", "rcx", "rdx", "rsi", "rdi",
                    "r8", "r9", "r10", "r11", "memory");
    return rax;
}

// sys_yield
//    Yield control of the CPU to the kernel. The kernel will pick another
//    process to run, if possible.
inline void sys_yield() {
    register uintptr_t rax asm("rax") = SYSCALL_YIELD;
    asm volatile ("syscall"
                  : "+a" (rax)
                  :
                  : "cc", "rcx", "rdx", "rsi", "rdi",
                    "r8", "r9", "r10", "r11", "memory");
}

// sys_page_alloc(addr)
//    Allocate a page of memory at address `addr`. `Addr` must be page-aligned
//    (i.e., a multiple of PAGESIZE == 4096). Returns 0 on success and -1
//    on failure.
inline int sys_page_alloc(void* addr) {
    register uintptr_t rax asm("rax") = SYSCALL_PAGE_ALLOC;
    asm volatile ("syscall"
                  : "+a" (rax), "+D" (addr)
                  :
                  : "cc", "rcx", "rdx", "rsi",
                    "r8", "r9", "r10", "r11", "memory");
    return rax;
}

// sys_fork()
//    Fork the current process. On success, return the child's process ID to
//    the parent, and return 0 to the child. On failure, return -1.
inline pid_t sys_fork() {
    register uintptr_t rax asm("rax") = SYSCALL_FORK;
    asm volatile ("syscall"
                  : "+a" (rax)
                  :
                  : "cc", "rcx", "rdx", "rsi", "rdi",
                    "r8", "r9", "r10", "r11", "memory");
    return rax;
}

// sys_exit()
//    Exit this process. Does not return.
inline void __noreturn sys_exit() {
    register uintptr_t rax asm("rax") = SYSCALL_EXIT;
    asm volatile ("syscall"
                  : "+a" (rax)
                  :
                  : "cc", "rcx", "rdx", "rsi", "rdi",
                    "r8", "r9", "r10", "r11", "memory");

    // should never get here
    while (1) {
    }
}

// sys_panic(msg)
//    Panic.
inline pid_t __noreturn sys_panic(const char* msg) {
    register uintptr_t rax asm("rax") = SYSCALL_PANIC;
    asm volatile ("syscall"
                  : "+a" (rax), "+D" (msg)
                  :
                  : "cc", "rcx", "rdx", "rsi",
                    "r8", "r9", "r10", "r11", "memory");

    // should never get here
    while (1) {
    }
}


// OTHER HELPER FUNCTIONS

// app_printf(format, ...)
//    Calls console_printf() (see lib.h). The cursor position is read from
//    `cursorpos`, a shared variable defined by the kernel, and written back
//    into that variable. The initial color is based on the current process ID.
void app_printf(int colorid, const char* format, ...);

#endif
