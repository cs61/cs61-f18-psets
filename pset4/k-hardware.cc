#include "kernel.hh"
#include "lib.hh"
#include "elf.h"
#include "k-vmiter.hh"
#include <atomic>

// k-hardware.cc
//
//    Functions for interacting with x86 hardware.


// hardware_init
//    Initialize hardware. Calls other functions bellow.

static void init_interrupts();
static void init_constructors();
static void init_cpu_state();
extern "C" { extern void exception_entry(); }
extern "C" { extern void syscall_entry(); }

void init_hardware() {
    // initialize console position
    cursorpos = 3 * CONSOLE_COLUMNS;

    // initialize CPU state, including virtual memory
    init_cpu_state();

    // initialize interrupt controller
    init_interrupts();

    // call C++ constructors for global objects
    // (NB none of these constructors may allocate memory)
    init_constructors();
}


// init_cpu_state
//    Set up segments, privileged CPU registers, and virtual memory,
//    including an initial page table `kernel_pagetable`.
//
//    The segment registers distinguish the kernel from applications:
//    the kernel runs with segments SEGSEL_KERN_CODE and SEGSEL_KERN_DATA,
//    and applications with SEGSEL_APP_CODE and SEGSEL_APP_DATA.
//    The kernel segment runs with full privilege (level 0), but application
//    segments run with less privilege (level 3).
//
//    The layouts of these types are defined by the hardware.

// segments
static uint64_t segments[7];

static void set_app_segment(uint64_t* segment, uint64_t type, int dpl) {
    *segment = type
        | X86SEG_S                    // code/data segment
        | ((uint64_t) dpl << 45)
        | X86SEG_P;                   // segment present
}

static void set_sys_segment(uint64_t* segment, uint64_t type, int dpl,
                            uintptr_t addr, size_t size) {
    segment[0] = ((addr & 0x0000000000FFFFFFUL) << 16)
        | ((addr & 0x00000000FF000000UL) << 32)
        | ((size - 1) & 0x0FFFFUL)
        | (((size - 1) & 0xF0000UL) << 48)
        | type
        | ((uint64_t) dpl << 45)
        | X86SEG_P;                   // segment present
    segment[1] = addr >> 32;
}

// processor state for taking an interrupt
static x86_64_taskstate task_descriptor;
extern x86_64_gatedescriptor interrupt_descriptors[256];

static void set_gate(x86_64_gatedescriptor* gate, int type, int dpl,
                     uintptr_t function) {
    assert(type >= 0 && type < 16);
    gate->gd_low = (function & 0x000000000000FFFFUL)
        | (SEGSEL_KERN_CODE << 16)
        | ((uint64_t) type << 40)
        | ((uint64_t) dpl << 45)
        | X86SEG_P
        | ((function & 0x00000000FFFF0000UL) << 32);
    gate->gd_high = function >> 32;
}

// kernel page tables
x86_64_pagetable kernel_pagetable[5];

void init_cpu_state() {
    // initialize segment descriptors for kernel code and data
    segments[0] = 0;
    set_app_segment(&segments[SEGSEL_KERN_CODE >> 3],
                    X86SEG_X | X86SEG_L, 0);
    set_app_segment(&segments[SEGSEL_KERN_DATA >> 3],
                    X86SEG_W, 0);
    set_app_segment(&segments[SEGSEL_APP_CODE >> 3],
                    X86SEG_X | X86SEG_L, 3);
    set_app_segment(&segments[SEGSEL_APP_DATA >> 3],
                    X86SEG_W, 3);
    set_sys_segment(&segments[SEGSEL_TASKSTATE >> 3],
                    X86SEG_TSS, 0,
                    (uintptr_t) &task_descriptor, sizeof(task_descriptor));

    // Task descriptor lets the kernel receive interrupts
    memset(&task_descriptor, 0, sizeof(task_descriptor));
    task_descriptor.ts_rsp[0] = KERNEL_STACK_TOP;

    // Macros in `k-exception.S` initialized `interrupt_descriptors[]`
    // with function pointers in the `gd_low` members. We must change
    // them to the weird format x86-64 expects.
    for (int i = 0; i < 256; ++i) {
        if (!(interrupt_descriptors[i].gd_low & X86SEG_P)) {
            uintptr_t addr = interrupt_descriptors[i].gd_low;
            int dpl = i == INT_SYSCALL ? 3 : 0;
            set_gate(&interrupt_descriptors[i], X86GATE_INTERRUPT, dpl, addr);
        }
    }

    x86_64_pseudodescriptor gdt, idt;
    gdt.limit = sizeof(segments) - 1;
    gdt.base = (uint64_t) segments;
    idt.limit = sizeof(interrupt_descriptors) - 1;
    idt.base = (uint64_t) interrupt_descriptors;

    // load segment descriptor tables
    asm volatile("lgdt %0; ltr %1; lidt %2"
                 :
                 : "m" (gdt.limit),
                   "r" ((uint16_t) SEGSEL_TASKSTATE),
                   "m" (idt.limit)
                 : "memory", "cc");


    // Set up control registers: check alignment
    uint32_t cr0 = rcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_WP | CR0_AM | CR0_MP | CR0_NE;
    lcr0(cr0);


    // set up syscall/sysret
    wrmsr(MSR_IA32_STAR, (uintptr_t(SEGSEL_KERN_CODE) << 32)
          | (uintptr_t(SEGSEL_APP_CODE) << 48));
    wrmsr(MSR_IA32_LSTAR, reinterpret_cast<uint64_t>(syscall_entry));
    wrmsr(MSR_IA32_FMASK, EFLAGS_TF | EFLAGS_DF | EFLAGS_IF
          | EFLAGS_IOPL_MASK | EFLAGS_AC | EFLAGS_NT);


    // set kernel page table
    memset(kernel_pagetable, 0, sizeof(kernel_pagetable));
    kernel_pagetable[0].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[1] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[1].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[2] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[2].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[3] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[2].entry[1] =
        (x86_64_pageentry_t) &kernel_pagetable[4] | PTE_P | PTE_W | PTE_U;

    // initalize it with user-accessible mappings for all physical memory
    for (vmiter it(kernel_pagetable);
         it.va() < MEMSIZE_PHYSICAL;
         it += PAGESIZE) {
        int r = it.map(it.va(), PTE_P | PTE_W | PTE_U);
        assert(r == 0);
    }

    lcr3((uintptr_t) kernel_pagetable);
}


// init_interrupts
//    Set up the interrupt controller (Intel part number 8259A).
//
//    Each interrupt controller supports up to 8 different kinds of interrupt.
//    The first x86s supported only one controller; this was too few, so modern
//    x86 machines can have more than one controller, a master and some slaves.
//    Much hoop-jumping is required to get the controllers to communicate!
//
//    Note: "IRQ" stands for "Interrupt ReQuest line", and stands for an
//    interrupt number.

#define MAX_IRQS        16      // Number of IRQs

// I/O Addresses of the two 8259A programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)

#define IRQ_SLAVE       2       // IRQ at which slave connects to master

// Timer-related constants
#define IO_TIMER1       0x040           /* 8253 Timer #1 */
#define TIMER_MODE      (IO_TIMER1 + 3) /* timer mode port */
#define   TIMER_SEL0    0x00            /* select counter 0 */
#define   TIMER_RATEGEN 0x04            /* mode 2, rate generator */
#define   TIMER_16BIT   0x30            /* r/w counter 16 bits, LSB first */

// Timer frequency: (TIMER_FREQ/freq) generates a frequency of 'freq' Hz.
#define TIMER_FREQ      1193182
#define TIMER_DIV(x)    ((TIMER_FREQ+(x)/2)/(x))

static uint16_t interrupts_enabled;

static void interrupt_mask() {
    uint16_t masked = ~interrupts_enabled;
    outb(IO_PIC1+1, masked & 0xFF);
    outb(IO_PIC2+1, (masked >> 8) & 0xFF);
}

void init_interrupts() {
    // mask all interrupts
    interrupts_enabled = 0;
    interrupt_mask();

    /* Set up master (8259A-1) */
    // ICW1:  0001g0hi
    //    g:  0 = edge triggering (1 = level triggering)
    //    h:  0 = cascaded PICs (1 = master only)
    //    i:  1 = ICW4 required (0 = no ICW4)
    outb(IO_PIC1, 0x11);

    // ICW2:  Trap offset. Interrupt 0 will cause trap INT_HARDWARE.
    outb(IO_PIC1+1, INT_HARDWARE);

    // ICW3:  On master PIC, bit mask of IR lines connected to slave PICs;
    //        on slave PIC, IR line at which slave connects to master (0-8)
    outb(IO_PIC1+1, 1<<IRQ_SLAVE);

    // ICW4:  000nbmap
    //    n:  1 = special fully nested mode
    //    b:  1 = buffered mode
    //    m:  0 = slave PIC, 1 = master PIC
    //    (ignored when b is 0, as the master/slave role
    //    can be hardwired).
    //    a:  1 = Automatic EOI mode
    //    p:  0 = MCS-80/85 mode, 1 = intel x86 mode
    outb(IO_PIC1+1, 0x3);

    /* Set up slave (8259A-2) */
    outb(IO_PIC2, 0x11);                        // ICW1
    outb(IO_PIC2+1, INT_HARDWARE + 8);  // ICW2
    outb(IO_PIC2+1, IRQ_SLAVE);         // ICW3
    // NB Automatic EOI mode doesn't tend to work on the slave.
    // Linux source code says it's "to be investigated".
    outb(IO_PIC2+1, 0x01);                      // ICW4

    // OCW3:  0ef01prs
    //   ef:  0x = NOP, 10 = clear specific mask, 11 = set specific mask
    //    p:  0 = no polling, 1 = polling mode
    //   rs:  0x = NOP, 10 = read IRR, 11 = read ISR
    outb(IO_PIC1, 0x68);             /* clear specific mask */
    outb(IO_PIC1, 0x0a);             /* read IRR by default */

    outb(IO_PIC2, 0x68);               /* OCW3 */
    outb(IO_PIC2, 0x0a);               /* OCW3 */

    // re-disable interrupts
    interrupt_mask();
}


// init_constructors
//    Initialize global objects (C++ compiler).

void init_constructors() {
    typedef void (*constructor_function)();
    extern constructor_function __init_array_start[];
    extern constructor_function __init_array_end[];
    for (auto fp = __init_array_start; fp != __init_array_end; ++fp) {
        (*fp)();
    }
}


// init_timer(rate)
//    Set the timer interrupt to fire `rate` times a second. Disables the
//    timer interrupt if `rate <= 0`.

void init_timer(int rate) {
    if (rate > 0) {
        outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
        outb(IO_TIMER1, TIMER_DIV(rate) % 256);
        outb(IO_TIMER1, TIMER_DIV(rate) / 256);
        interrupts_enabled |= 1 << (INT_TIMER - INT_HARDWARE);
    } else {
        interrupts_enabled &= ~(1 << (INT_TIMER - INT_HARDWARE));
    }
    interrupt_mask();
}


// check_pagetable
//    Validate a page table by checking that important kernel procedures
//    are mapped at the expected addresses.

void check_pagetable(x86_64_pagetable* pagetable) {
    assert(((uintptr_t) pagetable & PAGEOFFMASK) == 0); // must be page aligned
    assert(vmiter(pagetable, (uintptr_t) exception_entry).pa()
           == (uintptr_t) exception_entry);
    assert(vmiter(kernel_pagetable, (uintptr_t) pagetable).pa()
           == (uintptr_t) pagetable);
    assert(vmiter(pagetable, (uintptr_t) kernel_pagetable).pa()
           == (uintptr_t) kernel_pagetable);
}


// set_pagetable
//    Change page table after checking it.

void set_pagetable(x86_64_pagetable* pagetable) {
    check_pagetable(pagetable);
    lcr3((uintptr_t) pagetable);
}


// reserved_physical_address(pa)
//    Returns true iff `pa` is a reserved physical address.

#define IOPHYSMEM       0x000A0000
#define EXTPHYSMEM      0x00100000

bool reserved_physical_address(uintptr_t pa) {
    return pa < PAGESIZE || (pa >= IOPHYSMEM && pa < EXTPHYSMEM);
}


// allocatable_physical_address(pa)
//    Returns true iff `pa` is an allocatable physical address, i.e.,
//    not reserved or holding kernel data.

bool allocatable_physical_address(uintptr_t pa) {
    extern char kernel_end[];
    return !reserved_physical_address(pa)
        && (pa < KERNEL_START_ADDR
            || pa >= round_up((uintptr_t) kernel_end, PAGESIZE))
        && (pa < KERNEL_STACK_TOP - PAGESIZE
            || pa >= KERNEL_STACK_TOP)
        && pa < MEMSIZE_PHYSICAL;
}


// pci_make_configaddr(bus, slot, func)
//    Construct a PCI configuration space address from parts.

static int pci_make_configaddr(int bus, int slot, int func) {
    return (bus << 16) | (slot << 11) | (func << 8);
}


// pci_config_readl(bus, slot, func, offset)
//    Read a 32-bit word in PCI configuration space.

#define PCI_HOST_BRIDGE_CONFIG_ADDR 0xCF8
#define PCI_HOST_BRIDGE_CONFIG_DATA 0xCFC

static uint32_t pci_config_readl(int configaddr, int offset) {
    outl(PCI_HOST_BRIDGE_CONFIG_ADDR, 0x80000000 | configaddr | offset);
    return inl(PCI_HOST_BRIDGE_CONFIG_DATA);
}


// pci_find_device
//    Search for a PCI device matching `vendor` and `device`. Return
//    the config base address or -1 if no device was found.

static int pci_find_device(int vendor, int device) {
    for (int bus = 0; bus != 256; ++bus) {
        for (int slot = 0; slot != 32; ++slot) {
            for (int func = 0; func != 8; ++func) {
                int configaddr = pci_make_configaddr(bus, slot, func);
                uint32_t vendor_device = pci_config_readl(configaddr, 0);
                if (vendor_device == (uint32_t) (vendor | (device << 16))) {
                    return configaddr;
                } else if (vendor_device == (uint32_t) -1 && func == 0) {
                    break;
                }
            }
        }
    }
    return -1;
}

// poweroff
//    Turn off the virtual machine. This requires finding a PCI device
//    that speaks ACPI; QEMU emulates a PIIX4 Power Management Controller.

#define PCI_VENDOR_ID_INTEL     0x8086
#define PCI_DEVICE_ID_PIIX4     0x7113

void poweroff() {
    int configaddr = pci_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_PIIX4);
    if (configaddr >= 0) {
        // Read I/O base register from controller's PCI configuration space.
        int pm_io_base = pci_config_readl(configaddr, 0x40) & 0xFFC0;
        // Write `suspend enable` to the power management control register.
        outw(pm_io_base + 4, 0x2000);
    }
    // No PIIX4; spin.
    console_printf(CPOS(24, 0), 0xC000, "Cannot power off!\n");
    while (1) {
    }
}


// reboot
//    Reboot the virtual machine.

void reboot() {
    outb(0x92, 3); // does not return
    while (1) {
    }
}


// init_process(p, flags)
//    Initialize special-purpose registers for process `p`.

void init_process(proc* p, int flags) {
    memset(&p->regs, 0, sizeof(p->regs));
    p->regs.reg_cs = SEGSEL_APP_CODE | 3;
    p->regs.reg_fs = SEGSEL_APP_DATA | 3;
    p->regs.reg_gs = SEGSEL_APP_DATA | 3;
    p->regs.reg_ss = SEGSEL_APP_DATA | 3;
    p->regs.reg_rflags = EFLAGS_IF;

    if (flags & PROCINIT_ALLOW_PROGRAMMED_IO) {
        p->regs.reg_rflags |= EFLAGS_IOPL_3;
    }
    if (flags & PROCINIT_DISABLE_INTERRUPTS) {
        p->regs.reg_rflags &= ~EFLAGS_IF;
    }
}


// console_show_cursor(cpos)
//    Move the console cursor to position `cpos`, which should be between 0
//    and 80 * 25.

void console_show_cursor(int cpos) {
    if (cpos < 0 || cpos > CONSOLE_ROWS * CONSOLE_COLUMNS) {
        cpos = 0;
    }
    outb(0x3D4, 14);
    outb(0x3D5, cpos / 256);
    outb(0x3D4, 15);
    outb(0x3D5, cpos % 256);
}



// keyboard_readc
//    Read a character from the keyboard. Returns -1 if there is no character
//    to read, and 0 if no real key press was registered but you should call
//    keyboard_readc() again (e.g. the user pressed a SHIFT key). Otherwise
//    returns either an ASCII character code or one of the special characters
//    listed in kernel.hh.

// Unfortunately mapping PC key codes to ASCII takes a lot of work.

#define MOD_SHIFT       (1 << 0)
#define MOD_CONTROL     (1 << 1)
#define MOD_CAPSLOCK    (1 << 3)

#define KEY_SHIFT       0372
#define KEY_CONTROL     0373
#define KEY_ALT         0374
#define KEY_CAPSLOCK    0375
#define KEY_NUMLOCK     0376
#define KEY_SCROLLLOCK  0377

#define CKEY(cn)        0x80 + cn

static const uint8_t keymap[256] = {
    /*0x00*/ 0, 033, CKEY(0), CKEY(1), CKEY(2), CKEY(3), CKEY(4), CKEY(5),
        CKEY(6), CKEY(7), CKEY(8), CKEY(9), CKEY(10), CKEY(11), '\b', '\t',
    /*0x10*/ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', CKEY(12), CKEY(13), CKEY(14), KEY_CONTROL, 'a', 's',
    /*0x20*/ 'd', 'f', 'g', 'h', 'j', 'k', 'l', CKEY(15),
        CKEY(16), CKEY(17), KEY_SHIFT, CKEY(18), 'z', 'x', 'c', 'v',
    /*0x30*/ 'b', 'n', 'm', CKEY(19), CKEY(20), CKEY(21), KEY_SHIFT, '*',
        KEY_ALT, ' ', KEY_CAPSLOCK, 0, 0, 0, 0, 0,
    /*0x40*/ 0, 0, 0, 0, 0, KEY_NUMLOCK, KEY_SCROLLLOCK, '7',
        '8', '9', '-', '4', '5', '6', '+', '1',
    /*0x50*/ '2', '3', '0', '.', 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x60*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x70*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x80*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x90*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, CKEY(14), KEY_CONTROL, 0, 0,
    /*0xA0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0xB0*/ 0, 0, 0, 0, 0, '/', 0, 0,  KEY_ALT, 0, 0, 0, 0, 0, 0, 0,
    /*0xC0*/ 0, 0, 0, 0, 0, 0, 0, KEY_HOME,
        KEY_UP, KEY_PAGEUP, 0, KEY_LEFT, 0, KEY_RIGHT, 0, KEY_END,
    /*0xD0*/ KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    /*0xE0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0xF0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
};

static const struct keyboard_key {
    uint8_t map[4];
} complex_keymap[] = {
    /*CKEY(0)*/ {{'1', '!', 0, 0}},  /*CKEY(1)*/ {{'2', '@', 0, 0}},
    /*CKEY(2)*/ {{'3', '#', 0, 0}},  /*CKEY(3)*/ {{'4', '$', 0, 0}},
    /*CKEY(4)*/ {{'5', '%', 0, 0}},  /*CKEY(5)*/ {{'6', '^', 0, 036}},
    /*CKEY(6)*/ {{'7', '&', 0, 0}},  /*CKEY(7)*/ {{'8', '*', 0, 0}},
    /*CKEY(8)*/ {{'9', '(', 0, 0}},  /*CKEY(9)*/ {{'0', ')', 0, 0}},
    /*CKEY(10)*/ {{'-', '_', 0, 037}},  /*CKEY(11)*/ {{'=', '+', 0, 0}},
    /*CKEY(12)*/ {{'[', '{', 033, 0}},  /*CKEY(13)*/ {{']', '}', 035, 0}},
    /*CKEY(14)*/ {{'\n', '\n', '\r', '\r'}},
    /*CKEY(15)*/ {{';', ':', 0, 0}},
    /*CKEY(16)*/ {{'\'', '"', 0, 0}},  /*CKEY(17)*/ {{'`', '~', 0, 0}},
    /*CKEY(18)*/ {{'\\', '|', 034, 0}},  /*CKEY(19)*/ {{',', '<', 0, 0}},
    /*CKEY(20)*/ {{'.', '>', 0, 0}},  /*CKEY(21)*/ {{'/', '?', 0, 0}}
};

int keyboard_readc() {
    static uint8_t modifiers;
    static uint8_t last_escape;

    if ((inb(KEYBOARD_STATUSREG) & KEYBOARD_STATUS_READY) == 0) {
        return -1;
    }

    uint8_t data = inb(KEYBOARD_DATAREG);
    uint8_t escape = last_escape;
    last_escape = 0;

    if (data == 0xE0) {         // mode shift
        last_escape = 0x80;
        return 0;
    } else if (data & 0x80) {   // key release: matters only for modifier keys
        int ch = keymap[(data & 0x7F) | escape];
        if (ch >= KEY_SHIFT && ch < KEY_CAPSLOCK) {
            modifiers &= ~(1 << (ch - KEY_SHIFT));
        }
        return 0;
    }

    int ch = (unsigned char) keymap[data | escape];

    if (ch >= 'a' && ch <= 'z') {
        if (modifiers & MOD_CONTROL) {
            ch -= 0x60;
        } else if (!(modifiers & MOD_SHIFT) != !(modifiers & MOD_CAPSLOCK)) {
            ch -= 0x20;
        }
    } else if (ch >= KEY_CAPSLOCK) {
        modifiers ^= 1 << (ch - KEY_SHIFT);
        ch = 0;
    } else if (ch >= KEY_SHIFT) {
        modifiers |= 1 << (ch - KEY_SHIFT);
        ch = 0;
    } else if (ch >= CKEY(0) && ch <= CKEY(21)) {
        ch = complex_keymap[ch - CKEY(0)].map[modifiers & 3];
    } else if (ch < 0x80 && (modifiers & MOD_CONTROL)) {
        ch = 0;
    }

    return ch;
}


// symtab: reference to kernel symbol table; useful for debugging.
// The `mkchickadeesymtab` function fills this structure in.
elf_symtabref symtab = {
    reinterpret_cast<elf_symbol*>(0x1000000), 0, nullptr, 0
};

// lookup_symbol(addr, name, start)
//    Use the debugging symbol table to look up `addr`. Return the
//    corresponding symbol name (usually a function name) in `*name`
//    and the first address in that function in `*start`.

__no_asan
bool lookup_symbol(uintptr_t addr, const char** name, uintptr_t* start) {
    if (!kernel_pagetable[2].entry[8]) {
        kernel_pagetable[2].entry[8] = 0x1000000 | PTE_PS | PTE_P | PTE_W;
    }

    size_t l = 0;
    size_t r = symtab.nsym;
    while (l < r) {
        size_t m = l + ((r - l) >> 1);
        auto& sym = symtab.sym[m];
        if (sym.st_value <= addr
            && (sym.st_size != 0
                ? addr < sym.st_value + sym.st_size
                : m + 1 == symtab.nsym || addr < (&sym)[1].st_value)) {
            if (name) {
                *name = symtab.strtab + symtab.sym[m].st_name;
            }
            if (start) {
                *start = symtab.sym[m].st_value;
            }
            return true;
        } else if (symtab.sym[m].st_value < addr) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    return false;
}


// log_printf, log_vprintf
//    Print debugging messages to the host's `log.txt` file. We run QEMU
//    so that messages written to the QEMU "parallel port" end up in `log.txt`.

#define IO_PARALLEL1_DATA       0x378
#define IO_PARALLEL1_STATUS     0x379
# define IO_PARALLEL_STATUS_BUSY        0x80
#define IO_PARALLEL1_CONTROL    0x37A
# define IO_PARALLEL_CONTROL_SELECT     0x08
# define IO_PARALLEL_CONTROL_INIT       0x04
# define IO_PARALLEL_CONTROL_STROBE     0x01

static void delay() {
    (void) inb(0x84);
    (void) inb(0x84);
    (void) inb(0x84);
    (void) inb(0x84);
}

static void parallel_port_putc(printer* p, unsigned char c, int color) {
    static int initialized;
    (void) p, (void) color;
    if (!initialized) {
        outb(IO_PARALLEL1_CONTROL, 0);
        initialized = 1;
    }

    for (int i = 0;
         i < 12800 && (inb(IO_PARALLEL1_STATUS) & IO_PARALLEL_STATUS_BUSY) == 0;
         ++i) {
        delay();
    }
    outb(IO_PARALLEL1_DATA, c);
    outb(IO_PARALLEL1_CONTROL, IO_PARALLEL_CONTROL_SELECT
         | IO_PARALLEL_CONTROL_INIT | IO_PARALLEL_CONTROL_STROBE);
    outb(IO_PARALLEL1_CONTROL, IO_PARALLEL_CONTROL_SELECT
         | IO_PARALLEL_CONTROL_INIT);
}

void log_vprintf(const char* format, va_list val) {
    printer p;
    p.putc = parallel_port_putc;
    printer_vprintf(&p, 0, format, val);
}

void log_printf(const char* format, ...) {
    va_list val;
    va_start(val, format);
    log_vprintf(format, val);
    va_end(val);
}


// check_keyboard
//    Check for the user typing a control key. 'a', 'f', and 'e' cause a soft
//    reboot where the kernel runs the allocator programs, "fork", or
//    "forkexit", respectively. Control-C or 'q' exit the virtual machine.
//    Returns key typed or -1 for no key.

int check_keyboard() {
    int c = keyboard_readc();
    if (c == 'a' || c == 'f' || c == 'e') {
        // Turn off the timer interrupt.
        init_timer(-1);
        // Install a temporary page table to carry us through the
        // process of reinitializing memory. This replicates work the
        // bootloader does.
        x86_64_pagetable* pt = (x86_64_pagetable*) 0x8000;
        memset(pt, 0, PAGESIZE * 2);
        pt[0].entry[0] = 0x9000 | PTE_P | PTE_W;
        pt[1].entry[0] = PTE_P | PTE_W | PTE_PS;
        lcr3((uintptr_t) pt);
        // The soft reboot process doesn't modify memory, so it's
        // safe to pass `multiboot_info` on the kernel stack, even
        // though it will get overwritten as the kernel runs.
        uint32_t multiboot_info[5];
        multiboot_info[0] = 4;
        const char* argument = "fork";
        if (c == 'a') {
            argument = "allocator";
        } else if (c == 'e') {
            argument = "forkexit";
        }
        uintptr_t argument_ptr = (uintptr_t) argument;
        assert(argument_ptr < 0x100000000L);
        multiboot_info[4] = (uint32_t) argument_ptr;
        asm volatile("movl $0x2BADB002, %%eax; jmp kernel_entry"
                     : : "b" (multiboot_info) : "memory");
    } else if (c == 0x03 || c == 'q') {
        poweroff();
    }
    return c;
}


// error_vprintf
//    Print debugging messages to the console and to the host's
//    `log.txt` file via `log_printf`.

int error_vprintf(int cpos, int color, const char* format, va_list val) {
    va_list val2;
    __builtin_va_copy(val2, val);
    log_vprintf(format, val2);
    va_end(val2);
    return console_vprintf(cpos, color, format, val);
}


// fail
//    Loop until user presses Control-C, then poweroff.

void __noreturn fail() {
    while (1) {
        check_keyboard();
    }
}


// panic, assert_fail
//    Use console_printf() to print a failure message and then wait for
//    control-C. Also write the failure message to the log.

bool panicking;

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    panicking = true;

    if (format) {
        // Print panic message to both the screen and the log
        int cpos = error_printf(CPOS(23, 0), COLOR_ERROR, "PANIC: ");
        cpos = error_vprintf(cpos, COLOR_ERROR, format, val);
        if (CCOL(cpos)) {
            error_printf(cpos, COLOR_ERROR, "\n");
        }
    } else {
        error_printf(CPOS(23, 0), COLOR_ERROR, "PANIC");
        log_printf("\n");
    }

    va_end(val);
    fail();
}

void assert_fail(const char* file, int line, const char* msg) {
    cursorpos = CPOS(23, 0);
    error_printf("%s:%d: kernel assertion '%s' failed\n", file, line, msg);

    uintptr_t rsp = read_rsp(), rbp = read_rbp();
    uintptr_t stack_top = round_up(rsp, PAGESIZE);
    int frame = 1;
    while (rbp >= rsp && rbp < stack_top) {
        uintptr_t* rbpx = reinterpret_cast<uintptr_t*>(rbp);
        uintptr_t next_rbp = rbpx[0];
        uintptr_t ret_rip = rbpx[1];
        if (!ret_rip) {
            break;
        }
        const char* name;
        if (lookup_symbol(ret_rip, &name, nullptr)) {
            error_printf("  #%d  %p  <%s>\n", frame, ret_rip, name);
        } else {
            error_printf("  #%d  %p\n", frame, ret_rip);
        }
        rbp = next_rbp;
        ++frame;
    }
    fail();
}


// program_loader
//    This type iterates over the loadable segments in a binary.

extern uint8_t _binary_obj_p_allocator_start[];
extern uint8_t _binary_obj_p_allocator_end[];
extern uint8_t _binary_obj_p_allocator2_start[];
extern uint8_t _binary_obj_p_allocator2_end[];
extern uint8_t _binary_obj_p_allocator3_start[];
extern uint8_t _binary_obj_p_allocator3_end[];
extern uint8_t _binary_obj_p_allocator4_start[];
extern uint8_t _binary_obj_p_allocator4_end[];
extern uint8_t _binary_obj_p_fork_start[];
extern uint8_t _binary_obj_p_fork_end[];
extern uint8_t _binary_obj_p_forkexit_start[];
extern uint8_t _binary_obj_p_forkexit_end[];

struct ramimage {
    void* begin;
    void* end;
} ramimages[] = {
    { _binary_obj_p_allocator_start, _binary_obj_p_allocator_end },
    { _binary_obj_p_allocator2_start, _binary_obj_p_allocator2_end },
    { _binary_obj_p_allocator3_start, _binary_obj_p_allocator3_end },
    { _binary_obj_p_allocator4_start, _binary_obj_p_allocator4_end },
    { _binary_obj_p_fork_start, _binary_obj_p_fork_end },
    { _binary_obj_p_forkexit_start, _binary_obj_p_forkexit_end }
};

program_loader::program_loader(int program_number) {
    // check that this is a valid program
    int nprograms = sizeof(ramimages) / sizeof(ramimages[0]);
    assert(program_number >= 0 && program_number < nprograms);
    elf_ = (elf_header*) ramimages[program_number].begin;
    assert(elf_->e_magic == ELF_MAGIC);
    // XXX should check that no ELF pointers go beyond the data!

    reset();
}
void program_loader::fix() {
    while (ph_ && ph_ != endph_ && ph_->p_type != ELF_PTYPE_LOAD) {
        ++ph_;
    }
}
uintptr_t program_loader::va() const {
    return ph_ != endph_ ? ph_->p_va : 0;
}
size_t program_loader::size() const {
    return ph_ != endph_ ? ph_->p_memsz : 0;
}
const char* program_loader::data() const {
    return ph_ != endph_ ? (const char*) elf_ + ph_->p_offset : nullptr;
}
size_t program_loader::data_size() const {
    return ph_ != endph_ ? ph_->p_filesz : 0;
}
bool program_loader::writable() const {
    return ph_ != endph_ && (ph_->p_flags & ELF_PFLAG_WRITE);
}
uintptr_t program_loader::entry() const {
    return elf_->e_entry;
}
void program_loader::operator++() {
    if (ph_ != endph_) {
        ++ph_;
        fix();
    }
}
void program_loader::reset() {
    ph_ = (elf_program*) ((uint8_t*) elf_ + elf_->e_phoff);
    endph_ = ph_ + elf_->e_phnum;
    fix();
}


// Function definitions required by the C++ compiler and calling convention

namespace std {
const nothrow_t nothrow;
}

extern "C" {
// The __cxa_guard functions control the initialization of static variables.

// __cxa_guard_acquire(guard)
//    Return 0 if the static variables guarded by `*guard` are already
//    initialized. Otherwise lock `*guard` and return 1. The compiler
//    will initialize the statics, then call `__cxa_guard_release`.
int __cxa_guard_acquire(long long* arg) {
    std::atomic<char>* guard = reinterpret_cast<std::atomic<char>*>(arg);
    if (guard->load(std::memory_order_relaxed) == 2) {
        return 0;
    }
    while (1) {
        char old_value = guard->exchange(1);
        if (old_value == 2) {
            guard->exchange(2);
            return 0;
        } else if (old_value == 1) {
            pause();
        } else {
            return 1;
        }
    }
}

// __cxa_guard_release(guard)
//    Mark `guard` to indicate that the static variables it guards are
//    initialized.
void __cxa_guard_release(long long* arg) {
    std::atomic<char>* guard = reinterpret_cast<std::atomic<char>*>(arg);
    guard->store(2);
}

// __cxa_pure_virtual()
//    Used as a placeholder for pure virtual functions.
void __cxa_pure_virtual() {
    panic("pure virtual function called in kernel!\n");
}

// __dso_handle, __cxa_atexit
//    Used to destroy global objects at "program exit". We don't bother.
void* __dso_handle;
void __cxa_atexit(...) {
}

}


// the `proc::pagetable` member must come first in the structure
static_assert(offsetof(proc, pagetable) == 0);
