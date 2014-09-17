#ifndef AVRTEST_H
#define AVRTEST_H

#ifndef IN_AVRTEST
#ifdef __AVR_SFR_OFFSET__
#define IOBASE __AVR_SFR_OFFSET__
#else
#define IOBASE 0x20
#endif
#endif /* !IN_AVRTEST */

/* In- and Outputs */
#define TICKS_PORT_ADDR  (0x24 + IOBASE) /* 4 Inputs */
#define ABORT_PORT_ADDR  (0x29 + IOBASE) /* 1 Output */
#define EXIT_PORT_ADDR   (0x2F + IOBASE) /* 1 Output */
#define STDIN_PORT_ADDR  (0x32 + IOBASE) /* 1 Input  */
#define STDOUT_PORT_ADDR (0x32 + IOBASE) /* 1 Output */

#ifdef IN_AVRTEST

/* This defines are for avrtest itself.  */

/* Inputs */
#define STDIN_PORT  STDIN_PORT_ADDR
#define TICKS_PORT  TICKS_PORT_ADDR

#define FIRST_MAGIC_IN_PORT  TICKS_PORT
#define LAST_MAGIC_IN_PORT   STDIN_PORT

/* Outputs */
#define STDOUT_PORT STDOUT_PORT_ADDR
#define EXIT_PORT   EXIT_PORT_ADDR
#define ABORT_PORT  ABORT_PORT_ADDR

#define FIRST_MAGIC_OUT_PORT ABORT_PORT
#define LAST_MAGIC_OUT_PORT  STDOUT_PORT

#else /* IN_AVRTEST */

/* This defines can be used in the AVR application.  */

#define STDIN_PORT  (*((volatile unsigned char*) STDIN_PORT_ADDR))
#define STDOUT_PORT (*((volatile unsigned char*) STDOUT_PORT_ADDR))
#define EXIT_PORT   (*((volatile unsigned char*) EXIT_PORT_ADDR))
#define ABORT_PORT  (*((volatile unsigned char*) ABORT_PORT_ADDR))
#define TICKS_PORT  (*((volatile unsigned long*) TICKS_PORT_ADDR))

#endif /* IN_AVRTEST */

#endif /* AVRTEST_H */



