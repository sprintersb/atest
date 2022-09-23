/* !!! When using a fileio module, link with:
   !!! -Wl,-wrap,feof -Wl,-wrap,clearerr -Wl,-wrap,fclose -Wl,-wrap,fread -Wl,-wrap,fwrite */

#include "fileio.h" // Must be prior to #include <stdio.h>.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "avrtest.h"

typedef struct
{
    fileio_handle_t handle;
    FILE theFILE;
} fileio_file_t;

typedef struct
{
    fileio_handle_t handle;
    long pos;
    uint8_t whence;
} fseek_args_t;

typedef struct
{
    void *ptr;
    size_t size;
    size_t nmemb;
    fileio_handle_t handle;
} fread_args_t;

typedef struct
{
    const void *ptr;
    size_t size;
    size_t nmemb;
    fileio_handle_t handle;
} fwrite_args_t;


/* We have 8 file objects that can be used by the application,
   3 of which are occupied by host_stdin, host_stdout and host_stderr.
   Thus, an application can have no more than 5 files open at the
   same time.  */

static fileio_file_t files[FILEIO_N_FILES];

static const uint8_t n_files = sizeof (files) / sizeof (*files);


/* Map FILE* to the associated fileio_file_t*, or occupy a slot in files[]
   if NULL is passed.  */

static fileio_file_t* find_file (const FILE *theFILE)
{
    fileio_file_t *file = files;
    if (theFILE == NULL)
    {
        // We are searching an empty slot for a FILE that's to be opened.
        for (uint8_t i = 0; i < n_files - 3; i++, file++)
            if (file->handle == 0)
                return file;
    }
    else
    {
        // We are searching for an opened FILE to be acted upon.
        for (uint8_t i = 0; i < n_files; i++, file++)
            if (& file->theFILE == theFILE)
                return file;
    }
    return NULL;
}


/* Some local helper functions that operate on fileio_file_t*.  */

static int file_fclose (fileio_file_t *file)
{
    int ret = (int) avrtest_fileio_1 (AVRTEST_fclose, file->handle);
    file->handle = 0;
    return ret;
}

/* The put function that fopen will pass to FDEV_SETUP_STREAM.  This function
   is not used like fputc, for example avr-libc::fwrite() will immediately
   return if it encounters a non-zero return value.  */

static int file_put (char c, FILE *stream)
{
    fileio_file_t *file = (fileio_file_t*) fdev_get_udata (stream);
    if (stream != & file->theFILE)
        __builtin_abort();

    return EOF == (int) host_fputc (c, file->handle);
}


static int file_get (FILE *stream)
{
    fileio_file_t *file = (fileio_file_t*) fdev_get_udata (stream);
    if (stream != & file->theFILE)
        __builtin_abort();

    return host_fgetc (file->handle);
}


static fileio_file_t* file_fopen (const char *fname, const char *s_flags)
{
    fileio_file_t *file = find_file (NULL);

    if (!file)
        return NULL;

    uint8_t flags = 0;
    for (const char *s = s_flags; *s; s++)
    {
        if (*s == 'w' || *s == 'a' || *s == '+')
            flags |= _FDEV_SETUP_WRITE;

        if (*s == 'r')
            flags |= _FDEV_SETUP_READ;
    }

    union
    {
        uint32_t u;
        struct { const char *fname, *flags; };
    } args = { .fname = fname, .flags = s_flags };

    file->handle = (fileio_handle_t) avrtest_fileio_4 (AVRTEST_fopen, args.u);
    if (! file->handle)
        return NULL;

    file->theFILE = (FILE) FDEV_SETUP_STREAM (file_put, file_get, flags);
    fdev_set_udata (& file->theFILE, file);

    return file;
}


/* Implement host_xxx() analoga of stdio.h functions that take a handle
   instead of FILE*.  */


int host_fflush (fileio_handle_t handle)
{
    return (int) avrtest_fileio_1 (AVRTEST_fflush, handle);
}


int host_feof (fileio_handle_t handle)
{
    return (int) avrtest_fileio_1 (AVRTEST_feof, handle);
}


void host_clearerr (fileio_handle_t handle)
{
    avrtest_fileio_1 (AVRTEST_clearerr, handle);
}


int host_fputc (char c, fileio_handle_t handle)
{
    uint16_t args = (uint8_t) c;
    args |= (uint16_t) handle << 8;

    return (int) avrtest_fileio_2 (AVRTEST_fputc, args);
}


int host_fgetc (fileio_handle_t handle)
{
    return (int) avrtest_fileio_1 (AVRTEST_fgetc, handle);
}


size_t host_fwrite (const void *ptr, size_t size, size_t nmemb,
                    fileio_handle_t handle)
{
    fwrite_args_t args = { ptr, size, nmemb, handle };
    return (size_t) avrtest_fileio_p (AVRTEST_fwrite, &args);
}


size_t host_fread (void *ptr, size_t size, size_t nmemb,
                   fileio_handle_t handle)
{
    fread_args_t args = { ptr, size, nmemb, handle };
    return (size_t) avrtest_fileio_p (AVRTEST_fread, &args);
}


int host_fseek (fileio_handle_t handle, long pos, uint8_t whence)
{
    if ((int8_t) handle < 0)
        // No avail repositioning host's std streams.
        return -1;

    fseek_args_t args;
    args.handle = handle;
    args.pos    = pos;
    args.whence = whence;
    return (int) avrtest_fileio_p (AVRTEST_fseek, &args);
}


fileio_handle_t host_fopen (const char *fname, const char *s_flags)
{
    fileio_file_t *file = file_fopen (fname, s_flags);
    return file ? file->handle : 0;
}


int host_fclose (fileio_handle_t handle)
{
    if ((int8_t) handle > 0)
    {
        fileio_file_t *file = files;
        for (uint8_t i = 0; i < n_files; i++, file++)
            if (file->handle == handle)
                return file_fclose (file);
    }
    return 0;
}


/* Now implement the stuff for stdio.h.  Many of the functions are provided
   as __wrap_FUNC so that the link stage has to use -Wl,-wrap,FUNC.  The
   linker will use symbol __real_FUNC for the implementation of FUNC, and
   references to FUNC will reference __wrap_FUNC instead.  */

extern int __wrap_feof (FILE*);
extern int __wrap_fclose (FILE*);
extern void __wrap_clearerr (FILE*);
extern size_t __wrap_fread (void*, size_t size, size_t nmemb, FILE*);
extern size_t __wrap_fwrite (const void*, size_t size, size_t nmemb, FILE*);


int __wrap_fclose (FILE *pFILE)
{
    fileio_file_t *file;

    if (pFILE && ((file = find_file (pFILE))))
        return file_fclose (file);

    // Do not forward to fclose().  The documentation of avr-libc states
    // that fdev_close() should be called for streams that the user created
    // by means of fdev_setup_stream().  fclose() would only trigger linkage
    // of malloc.
    return 0;
}


void __wrap_clearerr (FILE *pFILE)
{
    fileio_file_t *file = find_file (pFILE);

    if (file)
        host_clearerr (file->handle);

    // The "real" clrarerr from avr-libc::stdio.h.
    pFILE->flags &= (uint8_t) ~(__SERR | __SEOF);
}


int __wrap_feof (FILE *pFILE)
{
    fileio_file_t *file = find_file (pFILE);

    if (file)
        return host_feof (file->handle);

    extern __typeof__(feof) __real_feof;
    return __real_feof (pFILE);
}


FILE* fopen (const char *fname, const char *s_flags)
{
    fileio_file_t *file = file_fopen (fname, s_flags);
    return file ? & file->theFILE : NULL;
}


int fseek (FILE *pFILE, long pos, int whence)
{
    fileio_file_t *file = find_file (pFILE);
    return file
        ? host_fseek (file->handle, pos, (uint8_t) whence)
        : 0;
}


int fflush (FILE *pFILE)
{
    fileio_handle_t handle = 0;

    if (pFILE == NULL)
        // NULL means all streams.
        handle = 0;
    else
    {
        fileio_file_t *file = find_file (pFILE);
        if (!file)
            return 0;
        handle = file->handle;
    }

    return host_fflush (handle);
}


size_t __wrap_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *pFILE)
{
    fileio_file_t *file = find_file (pFILE);
    if (!file)
    {
        extern typeof (fwrite) __real_fwrite;
        return __real_fwrite (ptr, size, nmemb, pFILE);
    }

    return host_fwrite (ptr, size, nmemb, file->handle);
}


size_t __wrap_fread (void *ptr, size_t size, size_t nmemb, FILE *pFILE)
{
    fileio_file_t *file = find_file (pFILE);
    if (!file)
    {
        extern typeof (fread) __real_fread;
        return __real_fread (ptr, size, nmemb, pFILE);
    }

    return host_fread (ptr, size, nmemb, file->handle);
}

/* Set up the host's special stdxx file objects.  */

FILE *host_stdin  = & files[n_files - 1].theFILE;
FILE *host_stdout = & files[n_files - 2].theFILE;
FILE *host_stderr = & files[n_files - 3].theFILE;

__attribute__((__constructor__,__used__))
static void
init_host_streams (void)
{
    *host_stdin  = (FILE) FDEV_SETUP_STREAM (NULL, file_get, _FDEV_SETUP_READ);
    *host_stdout = (FILE) FDEV_SETUP_STREAM (file_put, NULL, _FDEV_SETUP_WRITE);
    *host_stderr = *host_stdout;
    fdev_set_udata (host_stdin,  & files[n_files - 1]);
    fdev_set_udata (host_stdout, & files[n_files - 2]);
    fdev_set_udata (host_stderr, & files[n_files - 3]);
    files[n_files - 1].handle = HANDLE_stdin;
    files[n_files - 2].handle = HANDLE_stdout;
    files[n_files - 3].handle = HANDLE_stderr;
}
