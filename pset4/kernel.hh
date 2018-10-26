#ifndef WEENSYOS_KERNEL_H
#define WEENSYOS_KERNEL_H
#include "x86-64.h"
#include "lib.hh"
#if WEENSYOS_PROCESS
#error "kernel.hh should not be used by process code."
#endif
struct elf_header;
struct elf_program;

// kernel.h
//
//    Functions, constants, and definitions for the kernel.


// Process state type
typedef enum procstate {
    P_FREE = 0,                         // free slot
    P_RUNNABLE,                         // runnable process
    P_BLOCKED,                          // blocked process
    P_BROKEN                            // faulted process
} procstate_t;

// Process descriptor type
struct proc {
    x86_64_pagetable* pagetable;        // process's page table (must be 1st)
    pid_t pid;                          // process ID
    regstate regs;                      // process's current registers
    procstate_t state;                  // process state (see above)
};

// Process table
#define NPROC 16                // maximum number of processes
extern proc ptable[NPROC];


// Kernel start address
#define KERNEL_START_ADDR       0x40000
// Top of the kernel stack
#define KERNEL_STACK_TOP        0x80000

// First application-accessible address
#define PROC_START_ADDR         0x100000

// Physical memory size
#define MEMSIZE_PHYSICAL        0x200000
// Number of physical pages
#define NPAGES                  (MEMSIZE_PHYSICAL / PAGESIZE)

// Virtual memory size
#define MEMSIZE_VIRTUAL         0x300000

struct pageinfo {
    uint8_t owner;
};
extern pageinfo pages[NPAGES];


// Segment selectors
#define SEGSEL_KERN_CODE        0x8             // kernel code segment
#define SEGSEL_KERN_DATA        0x10            // kernel data segment
#define SEGSEL_APP_CODE         0x18            // application code segment
#define SEGSEL_APP_DATA         0x20            // application data segment
#define SEGSEL_TASKSTATE        0x28            // task state segment


// Hardware interrupt numbers
#define INT_HARDWARE            32
#define INT_TIMER               (INT_HARDWARE + 0)


// init_hardware
//    Initialize x86 hardware, including memory, interrupts, and segments.
//    All accessible physical memory is initially mapped as readable
//    and writable to both kernel and application code.
void init_hardware();

// init_timer(rate)
//    Set the timer interrupt to fire `rate` times a second. Disables the
//    timer interrupt if `rate <= 0`.
void init_timer(int rate);


void* kalloc(size_t sz);
void kfree(void* ptr);


// kernel page table (used for virtual memory)
extern x86_64_pagetable kernel_pagetable[];

// reserved_physical_address(pa)
//    Returns non-zero iff `pa` is a reserved physical address.
bool reserved_physical_address(uintptr_t pa);

// allocatable_physical_address(pa)
//    Returns non-zero iff `pa` is an allocatable physical address.
bool allocatable_physical_address(uintptr_t pa);

// check_pagetable
//    Validate a page table by checking that important kernel procedures
//    are mapped at the expected addresses.
void check_pagetable(x86_64_pagetable* pagetable);

// set_pagetable
//    Change page table after checking it.
void set_pagetable(x86_64_pagetable* pagetable);

// check_page_table_mappings
//    Check operating system invariants about kernel mappings for a page
//    table. Panic if any of the invariants are false.
void check_page_table_mappings(x86_64_pagetable* pagetable);

// poweroff
//    Turn off the virtual machine.
void __noreturn poweroff();

// reboot
//    Reboot the virtual machine.
void __noreturn reboot();

// exception_return
//    Return from an exception to user mode: load the page table
//    and registers and start the process back up. Defined in k-exception.S.
void __noreturn exception_return(x86_64_pagetable* pagetable, regstate* reg);


// console_show_cursor(cpos)
//    Move the console cursor to position `cpos`, which should be between 0
//    and 80 * 25.
void console_show_cursor(int cpos);


// keyboard_readc
//    Read a character from the keyboard. Returns -1 if there is no character
//    to read, and 0 if no real key press was registered but you should call
//    keyboard_readc() again (e.g. the user pressed a SHIFT key). Otherwise
//    returns either an ASCII character code or one of the special characters
//    listed below.
int keyboard_readc();

#define KEY_UP          0300
#define KEY_RIGHT       0301
#define KEY_DOWN        0302
#define KEY_LEFT        0303
#define KEY_HOME        0304
#define KEY_END         0305
#define KEY_PAGEUP      0306
#define KEY_PAGEDOWN    0307
#define KEY_INSERT      0310
#define KEY_DELETE      0311

// check_keyboard
//    Check for the user typing a control key. 'a', 'f', and 'e' cause a soft
//    reboot where the kernel runs the allocator programs, "fork", or
//    "forkexit", respectively. Control-C or 'q' exit the virtual machine.
//    Returns key typed or -1 for no key.
int check_keyboard();


// init_process(p, flags)
//    Initialize special-purpose registers for process `p`. Constants for
//    `flags` are listed below.
void init_process(proc* p, int flags);

#define PROCINIT_ALLOW_PROGRAMMED_IO    0x01
#define PROCINIT_DISABLE_INTERRUPTS     0x02


// program_loader
//    Iterator type for executables.
struct program_loader {
    program_loader(int program_number);
    uintptr_t entry() const;          // virtual address of entry %rip

    // Per-segment functions:
    uintptr_t va() const;             // virtual address of current segment
    size_t size() const;              // size of current segment in bytes
                                      // (0 at end of executable)
    const char* data() const;         // pointer to data for current segment
    size_t data_size() const;         // size of `data()`
    bool writable() const;            // true iff current segment is writable

    void operator++();                // move to next segment
    void reset();                     // start over from first segment

  private:
    elf_header* elf_;
    elf_program* ph_;
    elf_program* endph_;

    void fix();
};


// log_printf, log_vprintf
//    Print debugging messages to the host's `log.txt` file. We run QEMU
//    so that messages written to the QEMU "parallel port" end up in `log.txt`.
void log_printf(const char* format, ...) __attribute__((noinline));
void log_vprintf(const char* format, va_list val) __attribute__((noinline));


// error_printf, error_vprintf
//    Print debugging messages to the console and to the host's
//    `log.txt` file via `log_printf`.
int error_printf(int cpos, int color, const char* format, ...)
    __attribute__((noinline));
int error_vprintf(int cpos, int color, const char* format, va_list val)
    __attribute__((noinline));

__no_asan
bool lookup_symbol(uintptr_t addr, const char** name, uintptr_t* start);

#endif
