/* !!! When using a fileio module, link with:
   !!! -Wl,-wrap,feof -Wl,-wrap,clearerr -Wl,-wrap,fclose -Wl,-wrap,fread -Wl,-wrap,fwrite */

#ifndef FILEIO_H
#define FILEIO_H

/* avr-libc implements fflush as an inline function.  Work around that now.
   For the work-around to work, the following #include <stdio.h> must actually
   include stdio.h, i.e. the include guard must not be satisfied yet.
   This is the reason for why we throw the following #error.  */
#ifdef _STDIO_H_
#error This file must be included prior to #include <stdio.h>
#endif

#define fflush avrlibc_fflush
#include <stdio.h>
#undef fflush
extern int fflush (FILE*);

/* Undo some malicous #defines from avr-libc's stdio.h.  */
#undef feof
#undef clearerr

extern int feof (FILE*);
extern void clearerr (FILE*);

/* We provide 8 file objects that can be used by the application,
   3 of which are occupied by host_stdin, host_stdout and host_stderr.
   Thus, an application can have no more than 5 files open at the
   same time.  */
#define FILEIO_N_FILES 8

#include <stdint.h>

typedef uint8_t fileio_handle_t;

/* Files that can be used to access the host's stdxx streams without
   ambiguity.  A priori stdin, stdout and stderr have nothing to do
   with the host's stdxx streams.  ./dejagnuboards/exit.c sets up stdout
   and stderr streams for the target and wires them to use putchar on
   the host.  However, this is the *only* thing that's performed by
   exit.c, and therefore fflush(stdout) and similar calls will have no
   effect on the host.  You can use the following streams, and, if you
   like, set stdout = host_stdout.  */

extern FILE *host_stdin;
extern FILE *host_stdout;
extern FILE *host_stderr;

/* Handles for the host's stdxx streams that can be used with the host_
   functions below.  */

#define HANDLE_stdin    ((fileio_handle_t) -1)
#define HANDLE_stdout   ((fileio_handle_t) -2)
#define HANDLE_stderr   ((fileio_handle_t) -3)

/* For convenience, functions with the obvious semantics taking handles.  */

extern fileio_handle_t host_fopen (const char *filename, const char *mode);
extern int host_fclose (fileio_handle_t);
extern int host_fflush (fileio_handle_t);
extern int host_feof (fileio_handle_t);
extern int host_fgetc (fileio_handle_t);
extern int host_fputc (char, fileio_handle_t);
extern void host_clearerr (fileio_handle_t);
extern int host_fseek (fileio_handle_t, long pos, uint8_t whence);
extern size_t host_fwrite (const void*, size_t, size_t, fileio_handle_t);
extern size_t host_fread (void*, size_t, size_t, fileio_handle_t);

#endif /* FILEIO_H */
