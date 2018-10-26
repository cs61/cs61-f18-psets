WEENSYOS
========

Type `make run` to run our OS using the QEMU emulator. We expect this
to work only on Linux hosts. If you have problems, check out
Troubleshooting below.

Running the OS
--------------

There are several ways to run the OS.

*   `make run`

    Build the OS and pop up a QEMU window to run it. Close the QEMU
    window to exit the OS.

*   `make run-console`

    Build the OS and run QEMU in the current terminal window. Press
    Control-C in the terminal to exit the OS.

In all of these run modes, QEMU also creates a file named `log.txt`.
The code we hand out doesn't actually log anything yet, but you may
find it useful to add your own calls to `log_printf` from the kernel.

Finally, run `make clean` to clean up your directory.

Source
------

Real operating systems are big. We have tried to boil down this OS to
a minimum, comment it to help you, and separate x86 specifics from
more fundamental issues. Here is an overview of the code.

### Important code

* `kernel.cc`: The kernel. Uses functions declared and described in
  `kernel.hh` and `lib.hh`.
* `k-vmiter.hh`: Virtual and physical memory iterators as described in
  Section 4.
* `p-allocator.cc`, `p-fork.cc`, and `p-forkexit.cc`: The applications.
  Uses functions declared and described in `process.hh` and `lib.hh`.

### Support code

You may read these if you're interested but you should be able to do
the pset using only the code and descriptions in `kernel.cc`, `lib.hh`,
`kernel.hh`, and `k-vmiter.hh`.

* `bootentry.S`, `boot.cc`: The bootloader.
* `k-hardware.cc`: Functions that set up and interact with x86
  hardware state using programmed I/O and memory-mapped I/O
  instructions.
* `k-exception.S`: Kernel assembly code for handling exceptions
  (interrupts, traps, and faults).
* `process.cc`: Support code for applications.
* `lib.cc`, `lib.hh`: Support code useful in both the kernel and
  applications.
* `x86-64.h`: x86-64 hardware definitions, including functions that
  correspond to important x86-64 instructions.
* `elf.h`: ELF support information. (ELF is a format used for
  executables.)

Build Files
-----------

The main output of the build process is a disk image, `weensyos.img`.
QEMU "boots" off this disk image, but it could also boot on real
hardware! The build process also produces other files that you can
look at and puts them in the `obj/` directory.

*   `obj/kernel.asm`

    This file is the output of `objdump -S` on the kernel. Use it to see
    the kernel's assembly code.

*   `obj/kernel.sym`

    This smaller file just lists all the kernel's symbols (i.e.,
    variable names).

*   `obj/p-allocator.asm`, `obj/p-allocator.sym`, ...

    Similar files are generated for process code.

Troubleshooting
---------------

The OS runs using the QEMU full-system emulator. Our makefiles may
install QEMU for you. On your own Linux machine, you will need to
install QEMU yourself, using a command such as (on Ubuntu) `sudo apt
install qemu-system-x86`.

If Control-C doesn't work on your QEMU, make sure you are using an
actual Control key. On some machines QEMU ignores key remappings (such
as swapped Control and Caps Lock).

If Control-C still doesn't work on your QEMU, forcibly close it by
running `make kill`.
