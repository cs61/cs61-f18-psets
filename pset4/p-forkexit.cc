#include "process.hh"
#include "lib.hh"
#ifndef ALLOC_SLOWDOWN
#define ALLOC_SLOWDOWN 20
#endif

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;

void process_main() {
    while (1) {
        int x = rand() % (6 * ALLOC_SLOWDOWN);
        if (x == 0) {
            if (sys_fork() > 0) {
                sys_exit();
            }
        } else if (x < 4) {
            if (sys_fork() == 0) {
                break;
            }
        } else {
            sys_yield();
        }
    }

    pid_t p = sys_getpid();
    srand(p);

    heap_top = (uint8_t*) round_up((uintptr_t) end, PAGESIZE);
    stack_bottom = (uint8_t*) round_down((uintptr_t) read_rsp() - 1, PAGESIZE);

    // Allocate heap pages until (1) hit the stack (out of address space)
    // or (2) allocation fails (out of physical memory).
    while (1) {
        int x = rand() % (8 * ALLOC_SLOWDOWN);
        if (x < 8 * p) {
            if (heap_top == stack_bottom || sys_page_alloc(heap_top) < 0) {
                break;
            }
            *heap_top = p;      /* check we have write access to new page */
            heap_top += PAGESIZE;
            if (console[CPOS(24, 0)]) {
                /* clear "Out of physical memory" msg */
                console_printf(CPOS(24, 0), 0, "\n");
            }
        } else if (x == 8 * p) {
            if (sys_fork() == 0) {
                p = sys_getpid();
            }
        } else if (x == 8 * p + 1) {
            sys_exit();
        } else {
            sys_yield();
        }
    }

    // After running out of memory
    while (1) {
        if (rand() % (2 * ALLOC_SLOWDOWN) == 0) {
            sys_exit();
        } else {
            sys_yield();
        }
    }
}
