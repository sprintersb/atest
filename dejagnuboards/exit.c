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
  STDOUT_PORT = c;

  return 0;
}


static void __attribute__ ((constructor, used))
avrtest_init_stream (void)
{
  static FILE avrtest_output_stream
    = FDEV_SETUP_STREAM (avrtest_fputc, NULL, _FDEV_SETUP_WRITE);

  stderr = stdout = &avrtest_output_stream;
}


#define BY_AVRTEST_LOG (*((unsigned char*) 0xffff))

static void __attribute__ ((naked, section(".init8"), used))
avrtest_init_argc_argv (void)
{
  if (BY_AVRTEST_LOG)
    {
      /* Use 0xf000 as start address for command line arguments as passed by
         -args ... from avrtest_log.  There's plenty of RAM in the simulator.
         The linker will never see that big address and hence won't complain.

         If you prefer a more common address, e.g. just after static storage
         (after .data and .bss) you can change the address to __heap_start:

          extern void *__heap_start[];
          uintptr_t pargs = (uintptr_t) __heap_start;
      */

      uintptr_t pargs = (uintptr_t) 0xf000;

      /* Tell avrtest_log to set R24 to argc, R22 to argv and to start
         command args (argv[0]) at `pargs'.  */

      LOG_PORT = LOG_GET_ARGS_CMD;
      LOG_PORT = pargs;
      LOG_PORT = pargs >> 8;
    }
  else
    {
      /* Set argc (R24) to 0.
         Set argv (R22) to the address of a memory location that contains
         NULL so that argv[0] = NULL.  */

      static void *avrtest_pnull[1];
      register int r24 __asm ("24") = 0;
      register void **r22 __asm ("22") = avrtest_pnull;
        __asm volatile ("" :: "r" (r24), "r" (r22));
    }
}


/* This defines never returning functions exit() and abort() */ 

/* Postpone writing EXIT_PORT until .fini1 below so that higher .fini dtors,
   destructors and functions registered by atexit() won't be bypassed.  */

static int avrtest_exit_code;

/* libgcc defines exit as weak.  */

void exit (int code)
{
  __asm volatile ("sts %0,%1"  "\n\t"
                  "%~jmp _exit"
                  : /* no outputs */
                  : "i" (&avrtest_exit_code), "r" (code));
  for (;;);
}

static void __attribute__ ((naked, used, section(".fini1")))
avrtest_fini1 (void)
{
  EXIT_PORT = avrtest_exit_code;
}

void abort (void)
{
  ABORT_PORT = 0;
  for (;;);
}
