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

#include "avrtest.h"


static int
avrtest_fputc (char c, FILE *stream)
{
  (void) stream;
  avrtest_syscall_29 (c);
  return 0;
}


static void __attribute__ ((constructor, used))
avrtest_init_stream (void)
{
  static FILE avrtest_output_stream
    = FDEV_SETUP_STREAM (avrtest_fputc, NULL, _FDEV_SETUP_WRITE);

  stderr = stdout = &avrtest_output_stream;
}


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

  void *pargs = (void*) 0xf000;

  /* Tell avrtest to set R24 to argc, R22 to argv, R20 to environ
     and to start command args (argv[0]) at `pargs'.  */
/*
  register void *r24 __asm ("24");
  __asm volatile (".long 0xffff13cc ; syscall 28 (set argc, argv)"
                  :: "r" (r24 = pargs));
*/
  avrtest_syscall_27 (pargs);
}


/* This defines never returning functions exit() and abort() */

#if __GNUC__ > 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 5)
#define Barrier() __builtin_unreachable()
#else
#define Barrier() for (;;)
#endif

/* Postpone raising EXIT until .fini1 below so that higher .fini dtors,
   destructors and functions registered by atexit() won't be bypassed.  */

static unsigned char avrtest_exit_code;

/* libgcc defines exit as weak.  */

void exit (int code)
{
  __asm volatile ("sts %0,%1"  "\n\t"
                  "%~jmp _exit"
                  : /* no outputs */
                  : "i" (&avrtest_exit_code), "r" (code));
  Barrier();
}


static void __attribute__ ((naked, used, section(".fini1")))
avrtest_fini1 (void)
{
  avrtest_syscall_30 (avrtest_exit_code);
}

void abort (void)
{
  avrtest_syscall_31 ();
  Barrier();
}
