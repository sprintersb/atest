
What is AVRtest?
================

AVRtest is a free software simulator for the
[AVR](https://en.wikipedia.org/wiki/AVR_microcontrollers) family
of 8-bit microcontrollers distributed under the GNU General Public License.

The main intention of AVRtest is to supply a fast, light-weight
and easy-to-use simulator to run the
[GCC](https://gcc.gnu.org) testsuite for avr-gcc and parts of the
[AVR-LibC](https://github.com/avrdudes/avr-libc) testsuite.

AVRtest is an AVR core smulator.  It does not simulate
internal peripherals like timers, I/O ports or interrupts.

Pre-built binaries for Windows are available at
https://sourceforge.net/projects/winavr/files/AVRtest

## Table of Contents

* [Introduction](#introduction)
* [Special Features](#special-features)
* [Running the avr-gcc Testsuite](#running-the-avr-gcc-testsuite-using-the-avrtest-simulator)
* [Building the exit.o Modules](#building-the-exito-modules)
* [Speed of Simulation](#speed-of-simulation)

### Selected Options

* [Getting Help](#-h-getting-help)
* [Specifying the Flash Size](#-s-size--specifying-simulated-flash-size)
* [Quiet Operation](#-q-quiet-operation)
* [Specifying the Maximum Instruction Count](#-m-maxcount-maximum-instruction-count-to-simulate)
* [Passing Arguments to the Program](#-args--passing-arguments-to-the-program)

### Features

* [Logging Control](#-no-log-and-logging-control)
* [Logging to the Host Computer](#logging-values-to-the-host-computer)
* [Support of FLMAP](#support-of-flmap)
* [File I/O](#file-io-with-the-file-system-of-the-host-computer)
  - [Using the fileio.c Module](#file-io--using-the-fileioc-module)
  - [Caveats, Restrictions and Limitations](#file-io--caveats-restrictions-and-limitations)
  - [Streams for the Host](#file-io--special-streams-for-the-hosts-stdin-stdout-stderr)
* [Performance Measurement](#performance-measurement)
* [Timing Data and Random Values](#timing-data-and-random-values)
* [32-Bit and 64-Bit Integer Emulation](#32-bit-and-64-bit-integer-emulation)
* [IEEE single Emulation](#ieee-single-emulation)
* [IEEE double Emulation](#ieee-double-emulation)
* [Assembler Support in avrtest.h](#assembler-support-in-avrtesth)
* [Compiler Support](#compiler-support)

Introduction
============

**avrtest** is an instruction set simulator for AVR core families<br>
`avr51`: ATmega128*, AT90USB128*, ATtiny2313, ... with a 2-byte PC<br>
`avr6`: ATmega256* with a 3-byte PC

**avrtest-xmega** is an instruction set simulator for AVR XMEGA core families<br>
`avrxmega6`: ATxmega128*, ... with a 3-byte PC<br>
`avrxmega3`: ATtiny212, ATtiny816, ... that see flash in RAM address space<br>
`avrxmega4`: ATxmega16*, ATxmega32*, ATxmega64*, ... with a 2-byte PC<br>
`avrxmega7`: ATxmega128A1*, ... with a 3-byte PC that use RAMPx in RAM accesses

**avrtest-tiny** is an instruction set simulator for the Reduced AVR TINY
core family<br>
`avrtiny`: ATtiny40, ... with only 16 GPRs

Also supported are other cores like `avr25`, `avrxmega2` etc.
They are basically aliases for one of the cores from above.

The executable that supports AVR XMEGA cores is named `avrtest-xmega`.
In addition to the `avrtest` simulator, it supports the XMEGA instructions
XCH, LAS, LAC and LAT.
For `avrxmega3`, it also supports the command line
option `-pm 0x4000` in order to set the location of the flash image in
the RAM address space for devices like ATmega4808.  The default for
this option and for avrxmega3 is `0x8000`.

The executable that supports Reduced AVR TINY cores is named `avrtest-tiny`.
It only supports general purpose registers (GPRs) R16...R31,
and many instructions like ADIW are not supported.  The LDS and STS
instructions are 1-word instructions that can access SRAM in the
range 0x40...0xbf.

In the remainder, avrtest is explained.  avrtest-xmega and avrtext-tiny
work similar.


Special Features
================

The simulated program can communicate with the simulator: It can write to
the host's standard output, and it can read from the host's standard input.
For example, an easy hello world program for AVR could look like this:

```cpp
#include <stdio.h>

int main (void)
{
    printf ("Hallo World!\n");
    return 0;
}
```

Compile this program as usual but link against `exit-atmega128.o`.
How to build that object file is explained in the next chapter.
Store the C source as `hello.c` and compile it with, e.g.

    avr-gcc hello.c -O -mmcu=atmega128 -o hello.elf /someplace/avrtest/exit-atmega128.o

The `exit-*.o` object implements stdout as a stream writing through to the
host computer's stdout, similar for stderr.  Similarly, stdin reads from
the host stdin.  Running the program

    avrtest hello.elf

will print something like

```
Hallo World!

 exit status: EXIT
      reason: exit 0 function called
     program: hello.elf
exit address: 0001a4
total cycles: 837
total instr.: 503
```

You can also use the respective primitive functions directly:

```cpp
#include "avrtest.h"

int main (void)
{
    char c;

    avrtest_putchar ('Q');

    c = avrtest_getchar();
    avrtest_putchar (c + 1);

    return 0;
}
```

The `avrtest_putchar` and `avrtest_getchar` functions are defined in `avrtest.h`
and act like `putchar` resp. `getchar` in an ordinary, hosted program:
`putchar` writes a character to the host's standard output,
and `getchar` reads a character from the host's standard input.

Compile the C source with

    avr-gcc inout.c -O -mmcu=atmega128 -o inout.elf -I /someplace/avrtest

When running the program with

    avrtest inout.elf

the simulator will print a `Q` on the console and wait for input.
Type `1<Enter>` and the output will be

```
Q1
2
 exit status: EXIT
      reason: infinite loop detected (normal exit)
     program: inout.elf
exit address: 0000be
total cycles: 27
total instr.: 18
```

There are more functions available for program &harr; simulator
interaction, see the respective sections below.


Running the avr-gcc Testsuite using the AVRtest Simulator
=================================================

http://lists.gnu.org/archive/html/avr-gcc-list/2009-09/msg00016.html

* Get avr-gcc, Binutils and AVR-LibC built from sources and working.
  For build instructions, see for example the
  [AVR-LibC documentation](https://avrdudes.github.io/avr-libc/avr-libc-user-manual/install_tools.html).

* Install [DejaGNU](https://www.gnu.org/software/dejagnu/),
  [expect](https://core.tcl-lang.org/expect/index) and TCL

* Unpack the AVRtest sources top `/someplace`.
  You find a link to the package at
  - "Code &rarr; Download ZIP" on https://github.com/sprintersb/atest
  - "Download Snapshot" from WinAVR
    https://sourceforge.net/p/winavr/code/HEAD/tree/trunk/avrtest/

* Run

      make

  inside `/someplace/avrtest` which will build executables from the AVRtest
  C-sources and AVR object files `exit-*.o` from `dejagnuboards/exit.c`.

* Adjust your `$HOME/.dejagnurc` file (or create one if it not already
  exists) as indicated in `/someplace/avrtest/.dejagnurc`.
  Change the `avrtest_dir` path to the directory where the avrtest
  executable is located:

      set avrtest_dir "/someplace/avrtest"

* You can add `/someplace/avrtest` to your PATH variable so that it's
  more convenient to start avrtest.  In order to run the GCC test
  suite this is not necessary, though.

* The `.dejagnurc` file sets the `avrlibc_include_dir` variable that points
  to a directory where the compiler can find `stdio.h` and similar headers
  from AVR-LibC.  Adjust this path to point to the `incude/avr` subdir
  of a valid AVR-LibC installation.  You can also adjust that path in
  the `*-sim.exp` file.

* `*-sim.exp` just sets some custom variables and then loads
  `avrtest.exp` from the same directory to run avrtest.

* Run the tests from the `./gcc` subdirectory of your avr-gcc build directory
  like:

      make -k check-gcc RUNTESTFLAGS="--target_board=atmega128-sim"

* Look in the `./gcc/testsuite` subdirectory of avr-gcc build directory for
  logged results `gcc.sum`, `gcc.log`, or add the `-all` switch to see it
  working one test at a time:

      make -k check RUNTESTFLAGS="--target_board=atmega128-sim -all"

* You can run parts of the testsuite like this:

      make -k check RUNTESTFLAGS="--target_board=atmega128-sim avr.exp"
      make -k check RUNTESTFLAGS="--target_board=atmega128-sim avr-torture.exp=pr41885*.c"

Voilà!


Building the `exit.o` Modules
=============================

The `exit-*.o` Modules are build during `make` using avr-gcc.
When you want to use a different avr-gcc not found in PATH, use:

    make clean-exit all-avr CC_FOR_AVR=/some-compiler-path/avr-gcc

When you need an exit module for a device not already present in the
Makefile like ATmega16, you can just run

    make exit-atmega16.o

Notice that an exit module is not strictly required in a program that's
simulated with AVRtest.  But it defines streams like stdout so that
functions like `printf` behave like in a hosted environment without
any further ado.  Moreover, the exit module implements `exit()` in such
a way that it reports the program's exit code to the host, whereas
without the module, the simulator will just print
*"infinite loop detected (normal exit)"* as it hits the `rjmp .-2` in
the stock `exit` implementation


`-h`: Getting Help
==================

Running

    avrtest -h

will print a help screen with available options:

```
  usage: avrtest [-d] [-e ENTRY] [-m MAXCOUNT] [-mmcu=ARCH] [-s SIZE]
                 [-no-log] [-no-stdin] [-no-stdout] [-no-stderr]
                 [-log=FILE] [-stdin=FILE] [-stdout=FILE] [-stderr=FILE]
                 [-q] [-flush] [-runtime] [-v]
                 [-graph[=FILE]] [-sbox=FOLDER]
                 program [-args [...]]
         avrtest --help
Options:
  -h            Show this help and exit.
  -args ...     Pass all following parameters as argc and argv to main.
  -d            Initialize SRAM from .data (for ELF program)
  -e ENTRY      Byte address of program entry.  Default for ENTRY is
                the entry point from the ELF program and 0 for non-ELF.
  -pm OFFSET    Set OFFSET where the program memory is seen in the
                LD's instruction address space (avrxmega3 only).
  -m MAXCOUNT   Execute at most MAXCOUNT instructions. Supported
                suffixes are k for 1000 and M for a million.
  -s SIZE       The size of the simulated flash.  For a program built
                for ATmega8, SIZE would be 8K or 8192 or 0x2000.
  -q            Quiet operation.  Only print messages explicitly
                requested.  Pass exit status from the program.
  -v            Verbose mode.  Print the loaded ELF program headers
                and the used streams.
  -runtime      Print avrtest execution time.
  -no-log       Disable instruction logging in avrtest_log.  Logging
                can still be turned on with LOG_ON etc., see README.
  -log=FILE     Commands like LOG_U8 will print to FILE on the host.
                FILE must be stdout (default), stderr, *.log or *.txt.
  -no-stdin     Disable avrtest_getchar (syscall 28) from avrtest.h.
  -no-stdout    Disable avrtest_putchar (syscall 29) from avrtest.h.
  -no-stderr    Disable avrtest_putchar_stderr (syscall 24).
  -stdin=FILE   stdin reads from the host's *.txt or *.data FILE.
  -stdout=FILE  Similar, but for stdout.
  -stderr=FILE  Similar, but for stderr.
  -flush        Flush stdout resp. stderr after each character.
  -sbox SANDBOX Provide the path to SANDBOX, which is a folder that the
                target program can access via file I/O (syscall 26).
  -graph[=FILE] Write a .dot FILE representing the dynamic call graph.
                For the dot tool see  http://graphviz.org
  -graph-help   Show more options to control graph generation and exit.
  -mmcu=ARCH    Select instruction set for ARCH
```


`-s SIZE`: Specifying simulated Flash Size
==========================================

The simulator must know the flash size of the simulated controller
so it can correctly simulare instructions like RJMP.  For example,
an  `RJMP .-4`  located at address 0x0 will jump to 0x1ffe on ATmega8
but to 0x7fe on ATtiny2313.

For programs that use the start-up code from AVR-LibC in `crt<mcu>.o`,
AVRtest will read the flash size from a special note section named
`.note.gnu.avr.deviceinfo` contained in that startup-code.  AVRtest will
print its contents with `-v`.

In the following situations, `-s SIZE` can be used to specify the flash size:

* The program does not use startup-code from AVR-LibC.  In that case,
  you can specify, say `-s 8192` or `-s 0x2000`  or `-s 8k` for ATmega8.

* You want to use a flash size other than shipped with `crt<mcu>.o`.

* Use `-s -1` if you don't want to specify a flash size and also don't
  want to use the flash size from `.note.gnu.avr.deviceinfo`.  AVRtest will
  use a core-specific default value.


`-q`: Quiet Operation
=============================================

With `-q` turned on, AVRtest will print no messages except the ones
explicitly requested by:

  - `-runtime`
  - `LOG_U8 (X)`, `PERF_DUMP_ALL` and similar logging commands, see below.

The following exit stati will be returned with `-q`:

  -  0 – Everything went fine.
  -  1 – Target program called `abort()`
  - `x` – Target program called `exit(x)`
  - 10 – Program timeout as set by `-m MAXCOUNT`
  - 11 – Something is wrong with the program file:  Does not fit into
        memory, not an AVR executable, ELF headers broken, ...
  - 12 – The program goes haywire:  Stack overflow, illegal instruction or PC.
  - 13 – Problem with symbol information like an odd function address.
  - 14 – Problem with using file I/O with the host's file system: Bad file
        handle or file name, illegal argument.  This does *not* encompass
        when fopen cannot open the file; this will just return a 0-handle.
  - 20 – Out of memory.
  - 21 – Wrong avrtest usage: Unknown options, etc.
  - 22 – Program file could not be found / read.
  - 23 – IEEE single emulation failed (e.g. on a big-endian host).
  - 24 – IEEE double emulation failed.
  - 42 – Fatal error in avrtest.

Without `-q`, the exit status will be 1 (`EXIT_FAILURE`) for the cases &ge; 20
from above.  For all the cases < 20 from above, 0 (`EXIT_SUCCESS`) will
be returned.


Speed of Simulation
===================

The main incentive behind AVRtest is speed of execution.  For AVRtest
compiled with a reasonably optimizing compiler (like with `gcc -O3`),
the speed of a simulated program is around

    45 MHz/GHz  for  avrtest              on x86_64
    12 MHz/GHz  for  avrtest_log -no-log  on x86_64

measured on an Intel Core2 Duo (x86_64).  On x86, the performance is
ca 70% of the above, i.e. around

    30 MHz/GHz  for  avrtest              on x86
     8 MHz/GHz  for  avrtest_log -no-log  on x86

The GHz-values refer to the execution speed of the host computer,
and the MHz-values refer to an AVR microcontroller.  For example,
with a x86_64 host running at 2 GHz, AVRtest will perform as fast
as an AVR running at around 90 MHz.

AVRtest is a single-core application.  The speed values are under
the assumption that the simulator performs no logging or printing.
The timing will also depend slighly on which instructions are being
simulated.


`-m MAXCOUNT`: Maximum Instruction Count to simulate
====================================================

Per default, there is no limitation of the number of simulated instructions.
This is a problem with malfunctioning programs.  When such cases may occur,
the maximal number of instructions that will be simulated can be set
with `-m MAXCOUNT`.

Supported suffixes are `k` for 1000 and `M` for a million, for example `-m 1M`
and `-m 1e6` will simulate no more than 1000000 instructions and terminate
with a TIMEOUT exit status when the program requires more instructions.
`-m 0` means no limitation (default).


`-args ...`: Passing Arguments to the Program
=============================================

All arguments after `-args` will be passed to the target program as if the
program was running from the command line.  This is accomplished by startup
code from `exit.c` located in section .init8 right before main is called.

If you have own startup code in .init8 make sure it will be located before
the code from `exit.c`, e.g. by appropriate order of the objects passed to
the linker or by means of a custom linker script.

The first argument (`argv[0]`) is the program name with directories
stripped off to save space.  The last argument `argv[argc]` is set to NULL.

The simulator will transfer the arguments to RAM and set R24 to `argc`,
R22 to `argv` and R20 to `env` so that you can write

```cpp
int main (int argc, char *argv[])
{
    ...
}
```

resp.

```cpp
int main (int argc, char *argv[], char *env[])
{
    ...
}
```

and use `argc` and `argv` as in any hosted application:  If the program under
simulation is `main.elf` and you run

    avrtest main.elf -args hallo "4 2"

then:

    argc       is 3
    argv[0]    is (the address of) string "main.elf" (i.e. argv[0][0] = 'm')
    argv[1]    is (the address of) string "hallo"
    argv[2]    is (the address of) string "4 2"
    argv[3]    is the NULL pointer

`env` is set to the NULL pointer if the program is executed by avrtest
and to a non-NULL pointer when executed by avrtest_log.

`exit.c:avrtest_init_argc_argv()` requests the command args to be put
starting at a RAM address hard-coded in `exit.c::avrtest_init_argc_argv()`.
If you prefer a different location then adjust `exit.c` according to your
needs, following the source comments.

`-no-args`
----------

will act as if no `-args` switch was supplied:  It skips all arguments
thereafter and sets `argc=0`, `argv=NULL` and `env` as described above.


`-no-log` and Logging Control
=============================

> :warning: Instruction logging is only supported by the avrtest_log family.

avrtest_log will log address and action of each executed instruction to
standard output of the host.  In cases where that is too much a flood of
information, you can start avrtest_log with `-no-log` and turn on instruction
logging by executing special commands defined in `avrtest.h`:

```cpp
// Turn on instruction logging.
LOG_ON;

// Turn it off again.
LOG_OFF;

// Push the current state of the logging (on or off) to an
// internal simulator stack, and then turn logging on / off.
LOG_PUSH_ON;
LOG_PUSH_OFF;

// Pop the logging state from the top of that stack.
LOG_POP;

// Turn on logging for the next N instructions and then switch
// it off again.
LOG_SET (N);

// Turn on logging during performance measurement described below,
// i.e. if any timer is in a START / STOP round.
LOG_PERF;
```

Please notice that this method of switching logging on / off is (low)
intrusive to the program.  The commands listed above are translated by
the compiler to machine instructions that might affect code generation
for the surrounding code (register allocation, jump offsets, ...).
The commands have low overhead; it's not more than an AVRtest syscall.

`LOG_ON`, `LOG_OFF`, `LOG_PUSH_ON`, `LOG_PUSH_OFF` and `LOG_POP`
are also supplied
as macros that can be used from assembly code.  They have no effect
on the internal state of the program (except for the program counter:
the respective syscalls consume 4 bytes of code).


Logging Values to the Host Computer
====================================

> :bulb:
The log destination can be set with `-log=FILE`, where FILE
is `stdout` (default), `stderr` or a `*.log` or `*.txt` file name.

In order to get values out of the running program, the following
low-overhead, low-intrusive commands might be useful:

    LOG_U8 (X);         print X as unsigned 8-bit decimal value
    LOG_S8 (X);         print X as signed 8-bit decimal value
    LOG_X8 (X);         print X as unsigned 8-bit hexadecimal value

Besides these you can use similar commands for wider values with
16, 24 and 32 bits, e.g.

    LOG_U16 (X);
    LOG_X32 (X);

etc.  Moreover, there are the following commands:

    LOG_STR (X);        log string X located in RAM
    LOG_PSTR (X);       log string X located in Flash
    LOG_ADDR (X);       log address X
    LOG_FLOAT (X);      log float X
    LOG_DOUBLE (X);     log double X
    LOG_LDOUBLE (X);    log long double X
    LOG_D64 (X);        log an uint64_t, the bit representation of which
                        is interpreted as an IEEE double number
                        (11-bit exponent, 52-bit encoded mantissa)
    LOG_F7T (X);        log LibF7's f7_t*

All of these logging commands have variants that take a custom
`printf` format string to print the supplied value:

    LOG_FMT_XXX (F, X);    log X using format string F located in RAM
    LOG_PFMT_XXX (F, X);   log X using format string F located in Flash

> :warning:
Please use format strings with care!  You can easily crash AVRtest
and make it raise a Segmentation Fault or other undefined behaviour
by specifying a format string that does not match the value!
Notice that the %-formats below refer to the HOST machine, i.e. the
machine that executes AVRtest!

AVRtest uses `double` to represent floating point values and `unsigned int`
for all other values up to 4 bytes.  The default format strings for the vanilla
LOG_XXX are:

    " %u "              for the "U" variants
    " %d "              for the "S" variants
    " 0x%02x "          for the 8-bit "X" variants
    ...                 ...
    " 0x%08x "          for the 32-bit "X" variants
    "%s"                for strings
    " 0x%04x "          for addresses
    " %.6f "            for float, double, long double and 64-bit unsigned
                        interpreted as a 64-bit IEEE floating point number
    "%s"                for f7_t*

For 8-byte values, the internal representation is `unsigned long long`, and
the default format strings are:

    " %llu "              for the "U" variant
    " %lld "              for the "S" variant
    " 0x%016llx "         for the "X" variant


Support of FLMAP
=================

Devices from the AVR64* and AVR128* families see a 32 KiB segment of their
program memory in the RAM address space.  Which segment is visible and
accessible is determined by bit field `NVMCTRL_CTRLB.FLMAP`.
AVRtest copies the segment as specified by FLMAP from program memory into
the RAM address space in .init4 by means of code from `exit-avr*.o`.

This means setting of `NVMCTRL_CTRLB.FLMAP` has only an effect when it happens
prior to that in .init1 ... .init3.

If you really want to test an application that switches FLMAP back and forth,
you have to trigger a re-write of the 32 KiB segment by means of a call to
    avrtest_misc_flmap (uint8_t flmap);
each time, where flmap ranges from 0 to 3.


File I/O with the File System of the Host Computer
===================================================

> :bulb:
In order to printf to a file on the host there is
`-stdout=FILE` where FILE is a *.txt or *.data file.
Similarly, there are `-stderr=FILE` and `-stdin=FILE.`
No file I/O complications are needed.

AVRtest supports basic file I/O capabilities that enable the target program
to communicate with the host's file system.  In order to enable this, option

    -sbox SANDBOX

has to be set on the command line.  SANDBOX is the name of a folder of the
host file system.  SANDBOX will be prepended to the file name passed to
`fopen()` without any further ado.  AVRtest implements several functionalities
found in `stdio.h` and provides them by means of a syscall that can be accessed
by functions from `avrtest.h`:

    unsigned long avrtest_fileio_p (char what, const void *pargs);
    unsigned long avrtest_fileio_1 (char what, unsigned char args);
    unsigned long avrtest_fileio_2 (char what, unsigned      args);
    unsigned long avrtest_fileio_4 (char what, unsigned long args);

WHAT specifies the host operation to perform.  For each supported function,
there is a macro AVRTEST_F that can be used as WHAT where F specifies the
function.  For example, AVRTEST_fopen specifies to call the host's fopen().
In order to specify FILEs, handles are passed around instead or FILE*
pointers:  If the prototype of the `stdio.h` function specifies a FILE*
argument, then the AVRtest interface uses a handle instead.  A handle is
a small 8-bit value.

ARGS resp. PARGS specifies the arguments to pass to the host.  If all of them
-- after replacing FILE* by its respective handle -- fit into 4 bytes, then
these arguments are passed compressed as ARGS.  If they do not fit into 4 bytes,
then the PARGS variant has to be used where PARGS points to a memory location
where to find the arguments.

To give an example, fopen whould be called as

    uint8_t handle = (uint8_t) avrtest_fileio_4 (AVRTEST_fopen, args);

where the low-word of ARGS is a const char* that points to the file name
(which will be appended to SANDBOX as-is), and the high-word is a char
pointer to the mode to use to open the file.  The return value ships the
handle associated to the host's FILE* in the low-byte of the return value,
the other bytes of the return value are unused.  HANDLE would be used in
subsequent calls like AVRTEST_fclose, that passes the file handle in the
low-byte of ARGS.

File I/O:  Using the fileio.c Module
-------------------------------------

For convenience, AVRtest comes with a target module that supplies wrappers
for the available file I/O functions.  The interface of these functions is
as specified by the C standard, and `fileio.c` then tries to map the FILE*
pointer to an associated handle that's needed for the respective AVRtest
syscall.  The following functions from `stdio.h` are implemented:

    fopen, fclose, feof, fflush, clearerr, fread, fwrite.

> :warning:
However, using `fileio.c` comes with some limitations, and it needs
special options when linking.  See the next section for details.

If a handle is not found, a default action might be performed.  Suppose for
example a call to fread.  If `fileio.c` finds a handle associated to the passed
FILE*, it will call the host's fread with the host's FILE* derived from the
handle; otherwise it calls the AVR-LibC implementation of fread which uses
FILE* and operates byte-by-byte.

Most functions like fputc, fgetc, fputs, fprintf, etc. that are not included
in the above list will work out-of-the-box and without special treatment or
wrappers from the fileio module.

You can use fileio.c/h as a module in your application, or you can link
against one of the pre-compiled object files built by make.  For a program
that simulates ATmega128 for example, the link command would read

    avr-gcc -mmcu=atmega128 fileio-atmega128.o -Wl,-wrap,fclose -Wl,-wrap,feof ...

and then simulate program.elf with avrtest or avrtest_log.

In order to circumvent AVR-LibC `stdio.h` pecularities and also for
better performance, the fileio module provides functions that use
handles directly instead of the need for an intermediate FILE*.
These functions with their obvious semantics are:

    fileio_handle_t host_fopen (const char *filename, const char *mode);
    int host_fclose (fileio_handle_t);
    int host_fflush (fileio_handle_t)
    int host_feof (fileio_handle_t);
    int host_fseek (fileio_handle_t, long pos, uint8_t whence);
    int host_fgetc (fileio_handle_t);
    int host_fputc (char, fileio_handle_t);
    void host_clearerr (fileio_handle_t);
    size_t host_fwrite (const void*, size_t, size_t, fileio_handle_t);
    size_t host_fread (void*, size_t, size_t, fileio_handle_t);


File I/O:  Caveats, Restrictions and Limitations
-------------------------------------------------

> :warning:
Due to peculiarities of AVR-LibC's `stdio.h` implementation, the
following rules have to be obeyed when using file I/O via `fileio.c`:

* The linker must be called with `-wrap` for the following symbols:

      feof, fwrite, fread, fclose, cleaerr

  i.e. avr-gcc has to be called with

      avr-gcc ... -Wl,-wrap,feof -Wl,-wrap,fwrite -Wl,-wrap,fread ...

  when linking.  For the documentation of `-wrap`, see
  http://sourceware.org/binutils/docs-2.32/ld/Options.html#index-_002d_002dwrap_003dsymbol

* When using AVRtest to run the GCC test suite for AVR, the host interactions
  that `exit-*.o` supplies via stdout and stderr should be sufficient.
  The complexity of fileio is not needed for the GCC test suite.


File I/O:  Special Streams for the Host's stdin, stdout, stderr
----------------------------------------------------------------

The fileio module provides

    FILE *host_stdin;
    FILE *host_stdout;
    FILE *host_stderr;

that are associated to the host's stdin, stdout and stderr, respectively.
The respective handles to use with functions like host_fflush() are:

    HANDLE_stdin
    HANDLE_stdout
    HANDLE_stderr


Performance Measurement
========================

> :warning: This feature is only supported by avrtest_log and avrtest-xmega_log.

The simulator supports 7 independently operating performance-meters 1...7:

    PERF_START (N);

starts perf-meter N which will start capturing values of the running program
like program counter, instruction count, stack pointer, call depth, etc.

    PERF_STOP (N);

will halt the perf-meter.  Upon encountering the next PERF_START(N)
the meter will be re-enabled and proceed.  The collected values can
be dumped at any time to the standard output of the host computer by

    PERF_DUMP (N);

or

    PERF_DUMP_ALL;

This will show a summary of the extremal values for the perf-meter(s) and
reset it completely so that you can use it for a different task afterwards.
Displayed values are:

```
 - Stack (abs):  Values of the stack pointer (SP).
 - Stack (rel):  Stack usage relative to the starting point.
 - Calls (abs):  The absolute call depth (CALLs increment, RETs decrement).
 - Calls (rel):  Call depth relative to the starting point.
```

Tracking of the call depth won't work as expected for setjmp / longjmp
and similar functions that set SP to an unknown call depth.

In order to give a specific perf-meter N a more descriptive name there are

    PERF_LABEL (N, L);
    PERF_PLABEL (N, L);

where label L is a C-string residing in RAM or Flash, respectively.
avrtest_log will cache the label so you can override it after PERF_LABEL.

If you want to find out what value lead to the round with mimimal or maximal
instruction cycles, the next START/STOP round can be tagged with a value:

    PERF_TAG_STR (N, STR);      Tag perf-meter N with string STR
    PERF_TAG_U16 (N, U);        ... with a 16-bit unsigned integer U
    PERF_TAG_S16 (N, U);        ... with a 16-bit signed integer U
    PERF_TAG_U32 (N, U);        ... with a 32-bit unsigned integer U
    PERF_TAG_S32 (N, U);        ... with a 32-bit signed integer U
    PERF_TAG_FLOAT (N, F);      ... with float F

Just like with LOG_XXX(), S16 and S32 are represented internally as "int",
U16 and U32 as "unsigned" and float as "double".  The default format strings
to print the tags are "%s" for string, " %u " for the unsigned versions,
" %d " for the signed ones and " %.6f " for float.

Custom format strings can be supplied by

    PERF_TAG_FMT_U16 (N, FMT, U);
    PERF_TAG_PFMT_S32 (N, FMT, U);

etc. with a format string FMT located in RAM resp. Flash.

Example:

```cpp
#include <math.h>
#include "avrtest.h"

float x, y;

int main (void)
{
    PERF_LABEL (1, "resource consumption of sin()");

    for (x = 0.0; x <= 3.14159; x += 0.01)
    {
        PERF_TAG_FMT_FLOAT (1, " sin (%.5f) ", x);
        PERF_START (1);
        y = sin (x);
        PERF_STOP (1);

        PERF_TAG_FMT_FLOAT (2, " x=%.5f ", x);
        PERF_STAT_FLOAT (2, fabs (x * (1 - x*x / 6) - y));
    }

    PERF_DUMP_ALL;
    return 0;
}
```

Compiling as usual, linking against `exit-*.o` and running

    avrtest_log ... -no-log -q

produces an output like

```
--- Dump # 1:
 Timer T1 "resource consumption of sin()" (315 rounds):  027c--029e
              Instructions        Ticks
    Total:       411402          541887
    Mean:          1306            1720
    Stand.Dev:     75.5            85.6
    Min:            686            1029
    Max:           1515            1952
    Calls (abs) in [   1,   5] was:   1 now:   1
    Calls (rel) in [   0,   4] was:   0 now:   0
    Stack (abs) in [04f8,04e5] was:04f8 now:04f8
    Stack (rel) in [   0,  19] was:   0 now:   0

           Min round Max round    Min tag           /   Max tag
    Calls       -all-same-                          /
    Stack       -all-same-                          /
    Instr.         1       315     sin (0.00000)    /   sin (3.14000)
    Ticks          1       315     sin (0.00000)    /   sin (3.14000)

 Stat  T2 "" (315 Values)
    Mean:       3.589886e-001     round    tag
    Stand.Dev:  5.247231e-001
    Min:        0.000000e+000         1     x=0.00000
    Max:        2.021442e+000       315     x=3.14000
```

The computation of `sin(x)` took 1029...1952 cycles (with an average
of 1720 cycles) for the 315 values it had been computed.
sin() needs 19 bytes stack space.  The maximal difference between
sin (x) and the 3rd-order approximation  x &minus; x<sup>3</sup> / 6  is 2.02144
and occurred for x = 3.14000.

    PERF_STAT_FLOAT (N, F);

in the example makes perf-meter N gather statistics of float value F:
mean (expected value), standard deviation, minimum, maximum, and the
START/STOP round for which a minimal / maximal value of F has been seen.
If rounds have been tagged, the tags for the rounds that yielded a minimal
and maximal value of F are also displayed.  Besides PERF_STAT_FLOAT there
is also PERF_STAT_U32 and PERF_STAT_S32 to get statistics for (un)signed
32-bit integer values.

In the sample code from above, one is interested in the resource consumption
of the sin function.  In order to supply that function with a value x and to
store the result y, additional instructions are needed:

    PERF_START (1);
    y = sin (x);
    PERF_STOP (1);

If these additional instructions are of no interest, use

    PERF_START_CALL (N);

which only adds costs of instructions that are at a call depth of at
least 1 (relative to the starting point).  This includes costs of
CALL and RET that start / finish the function.

> :bulb: Make sure that the function of interest is not inlined or optimized
away, e.g. by making inputs and outputs volatile and attributing the function
with `__attribute__((__noipa__))`.  The additional overhead caused by
the volatile accesses does not matter as it is ignored by PERF_START_CALL.


Timing Data and Random Values
==============================

Following functions return an unsigned 32-bit value:

    avrtest_cycles();       Program cycles of simulated instructions
    avrtest_insns();        Number of simulated instructions
    avrtest_prand();        A 32-bit pseudo random number
    avrtest_rand();         A 32-bit random number

The values are "owned" by the program and are distinct from the
counters used by performance meters or that are displayed when avrtest
terminates.  Except rand, the values can be reset to their value at
program start:

    avrtest_reset_cycles();       Set cycles of simulated instructions to 0
    avrtest_reset_insns();        Set number of simulated instructions to 0
    avrtest_reset_prand();        Reset the pseudo random number generator
    avrtest_reset_all();          Reset all of them

In order to determine the run time of a function more precisely, there is:

    avrtest_cycles_call();        The next avrtest_cycles() will capture the
                                  number of cycles from the first [R]CALL
                                  after avrtest_cycles_call() up to the
                                  respective RET.
Example:

    avrtest_cycles_call();
    func();
    // Get cycles consumed by func(), including [R]CALL and RET.
    avrtest_cycles_call();

The simulator does not account cycles to syscalls.
The values returned by `avrtest_prand()` do not depend on the host machine.
They only depend on the number of calls since the last reset.


32-Bit and 64-Bit Integer Emulation
===================================

AVRtest supports syscalls like `avrtest_<op>s32` and `avrtest_<op>u32`
with obvious semantics for `<op>` in: `mul`, `div`, `mod`.

AVRtest supports syscalls like `avrtest_<op>s64` and `avrtest_<op>u64`
with obvious semantics for `<op>` in: `mul`, `div`, `mod`.


IEEE single Emulation
======================

AVRtest supports syscalls like

    float avrtest_mulf (float, float);
    float avrtest_sinf (float);

in order to help with IEEE single implementation.

Supported functions with one float argument are:
    sin, asin, sinh, asinh, cos, acos, cosh, acosh, tan, atan, tanh, atanh,
    exp, log, log2, log10, sqrt, cbrt, trunc, ceil, floor, round, fabs.

Supported functions with two float arguments are:
    mul, div, add, sub, ulp, prand,
    pow, atan2, hypot, fdim, fmin, fmax, fmod.

Some more functions are:
    frexp, ldexp, modf, powi, cmp, uto, sto, strto.

> :warning:
Don't forget to append `f` to the function name for the `float` versions.

* `avrtest_utof` and `avrtest_stof` convert uint32_t resp. int32_t to float.
* `avrtest_cmpf` compares two floating-point values.  It returns –1, 0, +1
when the values are less, equal or greater, respectively.
It returns –128 for unordered comparisons.
* `avrtest_strtof` converts an ASCII string to float, just like `strtof`.
* `avrtest_prandf` returns an evenly distributed pseudo-random number
in the specified range.  The pseudo-random source is the same like
for `avrtest_prand`.
* `avrtest_ftol` and `avrtest_ltof` convert between float and long double.
* `avrtest_powif` implements `__builtin_powif`.

AVRtest will terminate with an error when the host IEEE single cannot
be used for emulation.

Here is an example for the usage of `avrtest_sinf`:

```cpp
#include "avrtest.h"

float compute_sinf (float x)
{
    float y = avrtest_sinf (x);
    LOG_FMT_FLOAT ("sinf(%f) = ", x);
    LOG_FMT_FLOAT ("%f\n", y);
    return y;
}
```

Fixed-point to/from float conversions `avrtest_<fx>tof` and
`avrtest_fto<fx>` are supported for `<fx>` in:
* `k`, `uk`, `hk`, `uhk`, `r`, `ur`, `hr`, `uhr`.

For example `uhk` are `unsigned short accum` conversions.
They require that `<stdfix.h>` is included prior to `avrtest.h`.


IEEE double Emulation
======================

AVRtest supports syscalls like

    long double avrtest_mull (long double, long double);
    long double avrtest_sinl (long double);

Plus, there are syscalls like

    uint64_t avrtest_mul_d64 (uint64_t, uint64_t);
    uint64_t double avrtest_sin_d64 (uint64_t);

with the same functionality, but they interpret `uint64_t` as IEEE double.
The _d64 versions are available irrespective of the layout of `long double`.

Supported functions are the same like for IEEE single but with `l`
instead of `f` at the end of the syscall name;
with the one exception of `avrtest_strtold` that an ASCII string to
long double, just like `strtold`.

AVRtest will terminate with an error when the host IEEE double cannot
be used for emulation.  The functions are not available for Reduced Tiny.

Here is an example for the usage of `avrtest_sqrtl`:

```cpp
#include "avrtest.h"

long double compute_sqrtl (long double)
{
    long double y = avrtest_sqrtl (x);
    // The host uses "double" for floating-point values, thus
    // use "%f" (or similar) to print and NOT "%Lf".
    LOG_FMT_LDOUBLE ("square-root(%f) = ", x);
    LOG_FMT_LDOUBLE ("%f\n", y);
    return y;
}
```


Assembler Support in `avrtest.h`
===============================

`avrtest.h` adds assembler support for a few AVRtest features:

    avrtest_syscall <sysno>
    LOG_OFF             ;; Same as "avrtest_syscall 0"
    LOG_ON              ;; Same as "avrtest_syscall 1"
    LOG_PUSH_OFF        ;; Same as "avrtest_syscall 9"
    LOG_PUSH_ON         ;; Same as "avrtest_syscall 10"
    LOG_POP             ;; Same as "avrtest_syscall 11"
    AVRTEST_ABORT       ;; Same as "avrtest_syscall 31"
    AVRTEST_EXIT        ;; Same as "avrtest_syscall 30", exit value = R25:R24.
    AVRTEST_PUTCHAR     ;; Same as "avrtest_syscall 29", char = R24
    AVRTEST_ABORT_2ND_HIT ;; Same as "avrtest_syscall 25"

`avrtest_syscall <sysno>` is an assembler macro which expands to

    CPSE  R<sysno>, R<sysno>
    .word 0xffff

i.e. it does not change any register and does not change the program
status (SREG).

Example:

    #include "avrtest.h"

    .text
    .global func

    func:
        LOG_ON
        ret

And then call this function from assembler or from C code after
declaring it with `extern void func (void);`.


Compiler Support
================

* avr-gcc can be used with `avrtest.h`.

* To date (2024), clang has bugs so that programs that include `avrtest.h`
  cannot be compiled and terminate with an error message (or internal error)
  for the used inline assembly.

* `avrtest.h` supports `avr-gcc -mint8` with an 8-bit `int` type.
  > :warning:
  Notice that `-mint8` is not a multilib option and therefore functions
  from libgcc and AVR-LibC will not work when they take an `int`, `long`
  or `long long` parameter.  In particular, `[__builtin_]exit()` will
  take an 8-bit value, and AVRtest will likely get a trashed 16-bit return
  value.  The same applies to `return`ing from main.  What works though
  is to use `avrtest_exit()` to terminate the program.
  Also notice that with `-mint8` no 64-bit integer types like `long long`
  or `uint64_t` are available.
