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
#define LOG_PORT_ADDR    (0x2A + IOBASE) /* 1 Output */
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
#define LOG_PORT    LOG_PORT_ADDR
#define EXIT_PORT   EXIT_PORT_ADDR
#define ABORT_PORT  ABORT_PORT_ADDR

#define FIRST_MAGIC_OUT_PORT ABORT_PORT
#define LAST_MAGIC_OUT_PORT  STDOUT_PORT

// bits 7..6
#define LOG_SET 3
#define LOG_CMD(X) (3 & ((X) >> 6))

// bits 5..0
#define LOG_NUM(X) ((X) & 0x3f)
#define LOG_STOP_NUM     LOG_NUM (0)
#define LOG_START_NUM    LOG_NUM (-1)
#define LOG_PERF_NUM     LOG_NUM (-2)
#define LOG_GET_ARGS_NUM LOG_NUM (-3)

// bits 5..4
#define PERF_STOP  0
#define PERF_START 1
#define PERF_MEAN  2
#define PERF_DUMP  3
#define PERF_CMD(n) (3 & ((n) >> 4))

// bits 3..0
#define PERF_N(n)   ((n) & 0xf)

// mask
#define PERF_ALL    0xfffe

#else /* IN_AVRTEST */

/* This defines can be used in the AVR application.  */

/* Magic Ports */
#define STDIN_PORT  (*((volatile unsigned char*) STDIN_PORT_ADDR))
#define STDOUT_PORT (*((volatile unsigned char*) STDOUT_PORT_ADDR))
#define EXIT_PORT   (*((volatile unsigned char*) EXIT_PORT_ADDR))
#define ABORT_PORT  (*((volatile unsigned char*) ABORT_PORT_ADDR))
#define LOG_PORT    (*((volatile unsigned char*) LOG_PORT_ADDR))
#define TICKS_PORT  (*((volatile unsigned long*) TICKS_PORT_ADDR))

/* Logging Control */
#define LOG_SET_CMD(N)   ((3 << 6) | ((N) & 0x3f))
#define LOG_STOP_CMD     LOG_SET_CMD (0)
#define LOG_START_CMD    LOG_SET_CMD (-1)
#define LOG_PERF_CMD     LOG_SET_CMD (-2)
#define LOG_GET_ARGS_CMD LOG_SET_CMD (-3)

/* Performance Measurement */
#define PERF_STOP_VAL(n)  (((n) & 0xf) | (0 << 4))
#define PERF_START_VAL(n) (((n) & 0xf) | (1 << 4))
#define PERF_MEAN_VAL(n)  (((n) & 0xf) | (2 << 4))
#define PERF_DUMP_VAL(n)  (((n) & 0xf) | (3 << 4))

/* Convenience */
#define LOG_SET(N)   do { LOG_PORT = LOG_SET_CMD(N); } while (0)
#define LOG_START    do { LOG_PORT = LOG_START_CMD;  } while (0)
#define LOG_STOP     do { LOG_PORT = LOG_STOP_CMD;   } while (0)
#define LOG_PERF     do { LOG_PORT = LOG_PERF_CMD;   } while (0)

#define PERF_STOP(n)  do { LOG_PORT = PERF_STOP_VAL(n);  } while (0)
#define PERF_START(n) do { LOG_PORT = PERF_START_VAL(n); } while (0)
#define PERF_MEAN(n)  do { LOG_PORT = PERF_MEAN_VAL(n);  } while (0)
#define PERF_DUMP(n)  do { LOG_PORT = PERF_DUMP_VAL(n);  } while (0)

#define PERF_STOP_ALL  do { LOG_PORT = PERF_STOP_VAL(0);  } while (0)
#define PERF_START_ALL do { LOG_PORT = PERF_START_VAL(0); } while (0)
#define PERF_MEAN_ALL  do { LOG_PORT = PERF_MEAN_VAL(0);  } while (0)
#define PERF_DUMP_ALL  do { LOG_PORT = PERF_DUMP_VAL(0);  } while (0)

#endif /* IN_AVRTEST */

#endif /* AVRTEST_H */
