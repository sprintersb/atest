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
putchar_exit_c (char c, FILE *stream)
{
  (void) stream;
  STDOUT_PORT = c;

  return 0;
}

#define BY_AVRTEST_LOG (*((unsigned char*) 0xffff))

static void __attribute__ ((constructor, used))
init_exit_c (void)
{
  static FILE file_exit_c =
    {
      .put   = putchar_exit_c,
      .get   = NULL,
      .flags = _FDEV_SETUP_WRITE,
      .udata = 0,
    };

  stderr = stdout = &file_exit_c;
}


static void __attribute__ ((naked, section(".init8"), used))
init_args (void)
{
  if (BY_AVRTEST_LOG)
    {
/*
      extern void *__heap_start[];
      uintptr_t pargs = (uintptr_t) __heap_start;
*/
      uintptr_t pargs = (uintptr_t) 0xf000;
      LOG_PORT = LOG_GET_ARGS_CMD;
      LOG_PORT = pargs;
      LOG_PORT = pargs >> 8;
    }
  else
    {
/*
      static void *pnull[2];
*/
      void **pnull = (void**) 0xf000;
      register int r24 __asm ("24");
      register void **r22 __asm ("22");
      __asm volatile ("" :: "r" (r24 = 0), "r" (r22 = pnull));
    }
}


/* This defines never returning functions exit() and abort() */ 

void __attribute__ ((noreturn))
exit (int code) 
{
  EXIT_PORT = code;
  for (;;);
}

void __attribute__ ((noreturn))
abort (void)
{
  ABORT_PORT = 0;
  for (;;);
}
