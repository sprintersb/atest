#ifndef AVRTEST_H

#define STDIN_PORT_ADDR  0x52
#define STDOUT_PORT_ADDR 0x52
#define EXIT_PORT_ADDR   0x4F
#define ABORT_PORT_ADDR  0x49

#ifdef IN_AVRTEST

/* This defines are for avrtest itself.  */

#define STDIN_PORT  STDIN_PORT_ADDR
#define STDOUT_PORT STDOUT_PORT_ADDR 
#define EXIT_PORT   EXIT_PORT_ADDR 
#define ABORT_PORT  ABORT_PORT_ADDR 

#else

/* This defines can be used in the AVR application.  */

#define STDIN_PORT  (*((volatile unsigned char*) STDIN_PORT_ADDR))
#define STDOUT_PORT (*((volatile unsigned char*) STDOUT_PORT_ADDR))
#define EXIT_PORT   (*((volatile unsigned char*) EXIT_PORT_ADDR))
#define ABORT_PORT  (*((volatile unsigned char*) ABORT_PORT_ADDR))

#endif /* IN_AVRTEST */

#define AVRTEST_H
#endif /* AVRTEST_H */



