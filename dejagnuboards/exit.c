/* Copyright (c) 2004, Bjoern Haase
   All rights reserved.

   Anatoly Sokolov added code to make more tests PASS.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */


#include <stdlib.h>
#include <stdio.h>

#ifdef __AVR_XMEGA__
#include <avr/io.h>
#endif

#include "avrtest.h"

/* .weak in avr-libc/crt1/gcrt1.S */

extern void __vector_default (void);
void __vector_default (void)
{
  abort ();
}


static int
avrtest_fputc (char c, FILE *stream)
{
  (void) stream;

  /* sycall 29 */
  avrtest_putchar (c);

  return 0;
}

static int
avrtest_fputc_stderr (char c, FILE *stream)
{
  (void) stream;

  /* sycall 24 */
  avrtest_putchar_stderr (c);

  return 0;
}


static void __attribute__ ((constructor, used))
avrtest_init_stream (void)
{
  static FILE avrtest_stdout
    = FDEV_SETUP_STREAM (avrtest_fputc, NULL, _FDEV_SETUP_WRITE);
  static FILE avrtest_stderr
    = FDEV_SETUP_STREAM (avrtest_fputc_stderr, NULL, _FDEV_SETUP_WRITE);

  stdout = &avrtest_stdout;
  stderr = &avrtest_stderr;
}

#if defined NVMCTRL_CTRLB && defined NVMCTRL_FLMAP_gm && defined NVMCTRL_FLMAP_gp
/* Devices like AVR128* and AVR64* see a 32 KiB portion of their flash
   memory in the RAM address space.  Which 32 KiB segment is visible can
   be chosen by NVMCTRL_CRTLB.FLMAP.  */
#define HAVE_FLMAP
#endif


#ifdef HAVE_FLMAP
static void __attribute__ ((naked, section(".init0"), used))
avrtest_init_flmap (void)
{
  /* Reset value of FLMAP is all bits set to 1. */
  NVMCTRL_CTRLB = NVMCTRL_FLMAP_gm;
}

static void __attribute__ ((naked, section(".init4"), used))
avrtest_init_rodata (void)
{
  /* Copy 32 KiB block as of FLMAP from flash (LMA) to rodata (VMA).  */
  uint8_t flmap = NVMCTRL_CTRLB & NVMCTRL_FLMAP_gm;
  avrtest_misc_flmap (flmap >> NVMCTRL_FLMAP_gp);
}
#endif /* have FLMAP */


static void __attribute__ ((naked, section(".init8"), used))
avrtest_init_argc_argv (void)
{
  /* Use 0xf000 as start address for command line arguments as passed by
     -args ... from avrtest.  There's plenty of RAM in the simulator.
     The linker will never see that big address and hence won't complain.

     If you prefer a more common address, e.g. just after static storage
     (after .data and .bss) you can change the address to __heap_start:

      extern void *__heap_start[];
      void *pargs = __heap_start;
  */

#if defined __AVR_TINY__ || defined HAVE_FLMAP
  /* Use an address that won't appear as if flash was mirror'ed into
     the RAM space.  */
  void *pargs = (void*) 0x3000;
#elif __AVR_ARCH__ == 103
  /* avrxmega3 see flash starting at 0x8000 or 0x4000, hence
     use 0x1600 which is in a reserved address range.  */
  void *pargs = (void*) 0x1600;
#else
  void *pargs = (void*) 0xf000;
#endif

  /* Tell avrtest to set R24 to argc, R22 to argv, R20 to environ
     and to start command args (argv[0]) at `pargs'.  */

  avrtest_syscall_27 (pargs);

  /* syscall 25 */
  /* Initialization and call of main is supposed to be executed only
     once.  Trigger abort() if we execute the next syscall a 2nd time.  */
  avrtest_abort_2nd_hit ();
}


/* This defines never returning functions exit() and abort() */

/* Postpone raising EXIT until .fini1 below so that higher .fini dtors,
   destructors and functions registered by atexit() won't be bypassed.  */

static int avrtest_exit_code;

/* libgcc defines exit as weak.  */

void exit (int code)
{
#ifdef __AVR_TINY__
  int *addr = &avrtest_exit_code;
  /* Use indirect addressing on the reduced Tiny core as the address is
     likely to be out of scope of STS.  */
  __asm volatile ("st %a0+,%A1"  "\n\t"
                  "st %a0+,%B1"  "\n\t"
                  "%~jmp _exit"
                  : "+e" (addr)
                  : "r" (code));
#else
  __asm volatile ("sts %0+0,%A1"  "\n\t"
                  "sts %0+1,%B1"  "\n\t"
                  "%~jmp _exit"
                  : /* no outputs */
                  : "i" (&avrtest_exit_code), "r" (code));
#endif

  for (;;);
}


static void __attribute__ ((naked, used, section(".fini1")))
avrtest_fini1 (void)
{
  /* sycall 30 */
  avrtest_exit (avrtest_exit_code);
}

void abort (void)
{
  /* sycall 31 */
  avrtest_abort ();
  for (;;);
}
