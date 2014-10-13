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
#define TICKS_PORT_ADDR  (0x24 + IOBASE) /* 4 Inputs (only 1st byte is magic) */
#define ABORT_PORT_ADDR  (0x29 + IOBASE) /* 1 Output */
#define LOG_PORT_ADDR    (0x2A + IOBASE) /* 1 Output */
#define EXIT_PORT_ADDR   (0x2F + IOBASE) /* 1 Output */
#define STDIN_PORT_ADDR  (0x32 + IOBASE) /* 1 Input  */
#define STDOUT_PORT_ADDR (0x32 + IOBASE) /* 1 Output */

enum
  {
    LOG_ADDR_CMD,
    LOG_STR_CMD,  LOG_SET_FMT_ONCE_CMD,  LOG_SET_FMT_CMD, LOG_UNSET_FMT_CMD,
    LOG_PSTR_CMD, LOG_SET_PFMT_ONCE_CMD, LOG_SET_PFMT_CMD,

    LOG_U8_CMD, LOG_U16_CMD, LOG_U24_CMD, LOG_U32_CMD,
    LOG_S8_CMD, LOG_S16_CMD, LOG_S24_CMD, LOG_S32_CMD,
    LOG_X8_CMD, LOG_X16_CMD, LOG_X24_CMD, LOG_X32_CMD,
    LOG_FLOAT_CMD,

    LOG_TAG_FMT_CMD, LOG_TAG_PFMT_CMD,

    TICKS_RESET_CMD, /* 1st TICKS command */
    TICKS_IS_CYCLES_CMD, TICKS_IS_INSNS_CMD, TICKS_IS_PRAND_CMD,
    TICKS_IS_RAND_CMD,
    TICKS_RESET_ALL_CMD,  /* last TICKS command */

    LOG_X_sentinel
  };

#ifdef IN_AVRTEST

/* This defines are for avrtest itself.  */

/* Inputs */
#define STDIN_PORT  STDIN_PORT_ADDR
#define TICKS_PORT  TICKS_PORT_ADDR

/* Outputs */
#define STDOUT_PORT STDOUT_PORT_ADDR
#define LOG_PORT    LOG_PORT_ADDR
#define EXIT_PORT   EXIT_PORT_ADDR
#define ABORT_PORT  ABORT_PORT_ADDR

#define FIRST_MAGIC_OUT_PORT ABORT_PORT
#define LAST_MAGIC_OUT_PORT  STDOUT_PORT

// bits 7..6
#define LOG_SET      3
#define LOG_DUMP     2
#define LOG_TAG_PERF 1
#define LOG_CMD(X) (3 & ((X) >> 6))

// bits 5..0 (LOG_CMD = LOG_SET, special values)
#define LOG_NUM(X) ((X) & 0x3f)
#define LOG_OFF_CMD      LOG_NUM (0)
#define LOG_ON_CMD       LOG_NUM (-1)
#define LOG_PERF_CMD     LOG_NUM (-2)
#define LOG_GET_ARGS_CMD LOG_NUM (-3)

// bits 5..3 (LOG_CMD = 0)
#define PERF_STOP       0
#define PERF_START      1
#define PERF_DUMP       2
#define PERF_STAT_U32   5
#define PERF_STAT_S32   6
#define PERF_STAT_FLOAT 7
#define PERF_CMD(n) (7 & ((n) >> 3))

#define PERF_STAT       PERF_STAT_U32

// bits 5..3 (LOG_CMD = LOG_TAG_PERF)
#define PERF_TAG_STR    0
#define PERF_TAG_U16    1
#define PERF_TAG_U32    2
#define PERF_TAG_FLOAT  3

#define PERF_LABEL      4
#define PERF_PLABEL     5

#define PERF_TAG_CMD(n) (7 & ((n) >> 3))

// bits 2..0 (LOG_CMD = 0)
// bits 2..0 (LOG_CMD = LOG_TAG_PERF)
#define PERF_N(n)   ((n) & 0x7)

// mask
#define PERF_ALL    0xfe

#else /* IN_AVRTEST */

/* This defines can be used in the AVR application.  */

/* Magic Ports */
#define STDIN_PORT  (*((volatile unsigned char*) STDIN_PORT_ADDR))
#define STDOUT_PORT (*((volatile unsigned char*) STDOUT_PORT_ADDR))
#define EXIT_PORT   (*((volatile unsigned char*) EXIT_PORT_ADDR))
#define ABORT_PORT  (*((volatile unsigned char*) ABORT_PORT_ADDR))
#define LOG_PORT    (*((volatile unsigned char*) LOG_PORT_ADDR))
#define TICKS_PORT  (*((volatile unsigned long*) TICKS_PORT_ADDR))

#define TICKS_1PORT  (*((volatile unsigned char*)  TICKS_PORT_ADDR))
#define TICKS_2PORT  (*((volatile unsigned short*) TICKS_PORT_ADDR))
#define TICKS_pPORT  (*((const void* volatile*)    TICKS_PORT_ADDR))
#define TICKS_4PORT  (*((volatile unsigned long*)  TICKS_PORT_ADDR))
#define TICKS_fPORT  (*((volatile float*)          TICKS_PORT_ADDR))
#if defined (__UINT24_MAX__)
#define TICKS_3PORT  (*((volatile __uint24*) TICKS_PORT_ADDR))
#else
#define TICKS_3PORT  (*((volatile unsigned long*)  TICKS_PORT_ADDR))
#endif

/* Logging Control */
#define LOG_TAG_CMD(N,T) ((1 << 6) | (((T) & 7) << 3) | ((N) & 7))
#define LOG_DUMP_CMD(N)  ((2 << 6) | ((N) & 0x3f))
#define LOG_SET_CMD(N)   ((3 << 6) | ((N) & 0x3f))
#define LOG_OFF_CMD      LOG_SET_CMD (0)
#define LOG_ON_CMD       LOG_SET_CMD (-1)
#define LOG_PERF_CMD     LOG_SET_CMD (-2)
#define LOG_GET_ARGS_CMD LOG_SET_CMD (-3)

/* Performance Measurement */
#define PERF_CMD_(n,c)       (((n) & 7) | (c << 3))

/* Convenience */

/* Logging with default format strings */
#define LOG_ADDR(X)  LOG_DUMP_N_ (TICKS_pPORT, (X), LOG_ADDR_CMD)
#define LOG_PSTR(X)  LOG_DUMP_N_ (TICKS_pPORT, (X), LOG_PSTR_CMD)
#define LOG_STR(X)   LOG_DUMP_N_ (TICKS_pPORT, (X), LOG_STR_CMD)
#define LOG_FLOAT(X) LOG_DUMP_N_ (TICKS_fPORT, (X), LOG_FLOAT_CMD)

#define LOG_U8(X)  LOG_DUMP_N_ (TICKS_1PORT, (X), LOG_U8_CMD)
#define LOG_U16(X) LOG_DUMP_N_ (TICKS_2PORT, (X), LOG_U16_CMD)
#define LOG_U24(X) LOG_DUMP_N_ (TICKS_3PORT, (X), LOG_U24_CMD)
#define LOG_U32(X) LOG_DUMP_N_ (TICKS_4PORT, (X), LOG_U32_CMD)

#define LOG_S8(X)  LOG_DUMP_N_ (TICKS_1PORT, (X), LOG_S8_CMD)
#define LOG_S16(X) LOG_DUMP_N_ (TICKS_2PORT, (X), LOG_S16_CMD)
#define LOG_S24(X) LOG_DUMP_N_ (TICKS_3PORT, (X), LOG_S24_CMD)
#define LOG_S32(X) LOG_DUMP_N_ (TICKS_4PORT, (X), LOG_S32_CMD)

#define LOG_X8(X)  LOG_DUMP_N_ (TICKS_1PORT, (X), LOG_X8_CMD)
#define LOG_X16(X) LOG_DUMP_N_ (TICKS_2PORT, (X), LOG_X16_CMD)
#define LOG_X24(X) LOG_DUMP_N_ (TICKS_3PORT, (X), LOG_X24_CMD)
#define LOG_X32(X) LOG_DUMP_N_ (TICKS_4PORT, (X), LOG_X32_CMD)

/* Logging with custom format string (from RAM) */
#define LOG_FMT_ADDR(F,X)  LOG_DUMP_F_ ((F), LOG_ADDR(X))
#define LOG_FMT_PSTR(F,X)  LOG_DUMP_F_ ((F), LOG_PSTR(X))
#define LOG_FMT_STR(F,X)   LOG_DUMP_F_ ((F), LOG_STR(X))
#define LOG_FMT_FLOAT(F,X) LOG_DUMP_F_ ((F), LOG_FLOAT(X))

#define LOG_FMT_U8(F,X)  LOG_DUMP_F_ ((F), LOG_U8(X))
#define LOG_FMT_U16(F,X) LOG_DUMP_F_ ((F), LOG_U16(X))
#define LOG_FMT_U24(F,X) LOG_DUMP_F_ ((F), LOG_U24(X))
#define LOG_FMT_U32(F,X) LOG_DUMP_F_ ((F), LOG_U32(X))

#define LOG_FMT_S8(F,X)  LOG_DUMP_F_ ((F), LOG_S8(X))
#define LOG_FMT_S16(F,X) LOG_DUMP_F_ ((F), LOG_S16(X))
#define LOG_FMT_S24(F,X) LOG_DUMP_F_ ((F), LOG_S24(X))
#define LOG_FMT_S32(F,X) LOG_DUMP_F_ ((F), LOG_S32(X))

#define LOG_FMT_X8(F,X)  LOG_DUMP_F_ ((F), LOG_X8(X))
#define LOG_FMT_X16(F,X) LOG_DUMP_F_ ((F), LOG_X16(X))
#define LOG_FMT_X24(F,X) LOG_DUMP_F_ ((F), LOG_X24(X))
#define LOG_FMT_X32(F,X) LOG_DUMP_F_ ((F), LOG_X32(X))

/* Logging with custom format string (from Flash) */
#define LOG_PFMT_ADDR(F,X)  LOG_DUMP_PF_ ((F), LOG_ADDR(X))
#define LOG_PFMT_PSTR(F,X)  LOG_DUMP_PF_ ((F), LOG_PSTR(X))
#define LOG_PFMT_STR(F,X)   LOG_DUMP_PF_ ((F), LOG_STR(X))
#define LOG_PFMT_FLOAT(F,X) LOG_DUMP_PF_ ((F), LOG_FLOAT(X))

#define LOG_PFMT_U8(F,X)  LOG_DUMP_PF_ ((F), LOG_U8(X))
#define LOG_PFMT_U16(F,X) LOG_DUMP_PF_ ((F), LOG_U16(X))
#define LOG_PFMT_U24(F,X) LOG_DUMP_PF_ ((F), LOG_U24(X))
#define LOG_PFMT_U32(F,X) LOG_DUMP_PF_ ((F), LOG_U32(X))

#define LOG_PFMT_S8(F,X)  LOG_DUMP_PF_ ((F), LOG_S8(X))
#define LOG_PFMT_S16(F,X) LOG_DUMP_PF_ ((F), LOG_S16(X))
#define LOG_PFMT_S24(F,X) LOG_DUMP_PF_ ((F), LOG_S24(X))
#define LOG_PFMT_S32(F,X) LOG_DUMP_PF_ ((F), LOG_S32(X))

#define LOG_PFMT_X8(F,X)  LOG_DUMP_PF_ ((F), LOG_X8(X))
#define LOG_PFMT_X16(F,X) LOG_DUMP_PF_ ((F), LOG_X16(X))
#define LOG_PFMT_X24(F,X) LOG_DUMP_PF_ ((F), LOG_X24(X))
#define LOG_PFMT_X32(F,X) LOG_DUMP_PF_ ((F), LOG_X32(X))

#define LOG_DUMP_PF_(F, C) do { LOG_SET_PFMT_ONCE (F); C; } while (0)
#define LOG_DUMP_F_(F, C)  do { LOG_SET_FMT_ONCE (F); C;  } while (0)
#define LOG_DUMP_N_(P,X,CMD) do { P=X; LOG_PORT=LOG_DUMP_CMD(CMD);} while (0)

/* Setting custom format string for all subsequent logs */
#define LOG_SET_FMT(F)    LOG_DUMP_N_(TICKS_pPORT,(F),LOG_SET_FMT_CMD)
#define LOG_SET_PFMT(F)   LOG_DUMP_N_(TICKS_pPORT,(F),LOG_SET_PFMT_CMD)

/* Reset to default format strings */
#define LOG_UNSET_FMT    do { LOG_PORT = LOG_UNSET_FMT_CMD; } while (0)

/* Format string for next action only */
#define LOG_SET_FMT_ONCE(F)  LOG_DUMP_N_(TICKS_pPORT,(F),LOG_SET_FMT_ONCE_CMD)
#define LOG_SET_PFMT_ONCE(F) LOG_DUMP_N_(TICKS_pPORT,(F),LOG_SET_PFMT_ONCE_CMD)

/* Tagging perf-meter rounds */
/* Cache format string for next tag */
#define LOG_TAG_FMT(F)  LOG_DUMP_N_(TICKS_pPORT, (F), LOG_TAG_FMT_CMD)
#define LOG_TAG_PFMT(F) LOG_DUMP_N_(TICKS_pPORT, (F), LOG_TAG_PFM_CMDT)

/* Tagging with default format strings */

#define PERF_TAG_STR(N,X)   PERF_TAG_(TICKS_pPORT, (X), (N), 0)
#define PERF_TAG_U16(N,X)   PERF_TAG_(TICKS_2PORT, (X), (N), 1)
#define PERF_TAG_U32(N,X)   PERF_TAG_(TICKS_4PORT, (X), (N), 2)
#define PERF_TAG_FLOAT(N,X) PERF_TAG_(TICKS_fPORT, (X), (N), 3)
/* Labeling perf-meter */
#define PERF_LABEL(N,L)     PERF_TAG_(TICKS_pPORT, (L), (N), 4)
#define PERF_PLABEL(N,L)    PERF_TAG_(TICKS_pPORT, (L), (N), 5)

/* Tagging with custom format string (from RAM) */
#define PERF_TAG_FMT_STR(N,F,X)   PERF_TAG_F_(TICKS_pPORT, (F), (X), (N), 0)
#define PERF_TAG_FMT_U16(N,F,X)   PERF_TAG_F_(TICKS_2PORT, (F), (X), (N), 1)
#define PERF_TAG_FMT_U32(N,F,X)   PERF_TAG_F_(TICKS_4PORT, (F), (X), (N), 2)
#define PERF_TAG_FMT_FLOAT(N,F,X) PERF_TAG_F_(TICKS_fPORT, (F), (X), (N), 3)

/* Tagging with custom format string (from Flash) */
#define PERF_TAG_PFMT_STR(N,F,X)   PERF_TAG_PF_(TICKS_pPORT, (F), (X), (N), 0)
#define PERF_TAG_PFMT_U16(N,F,X)   PERF_TAG_PF_(TICKS_2PORT, (F), (X), (N), 1)
#define PERF_TAG_PFMT_U32(N,F,X)   PERF_TAG_PF_(TICKS_4PORT, (F), (X), (N), 2)
#define PERF_TAG_PFMT_FLOAT(N,F,X) PERF_TAG_PF_(TICKS_fPORT, (F), (X), (N), 3)

#define PERF_TAG_(P,X,N,S)  \
  do { P = X; LOG_PORT = LOG_TAG_CMD(N,S); } while (0)

#define PERF_TAG_F_(P,F,X,N,S) \
  do { LOG_TAG_FMT(F); PERF_TAG_(P,X,N,S); } while (0)

#define PERF_TAG_PF_(P,F,X,N,S)  \
  do { LOG_TAG_PFMT(F); PERF_TAG_(P,X,N,S); } while (0)

/* Instruction logging control */
#define LOG_SET(N)    do { LOG_PORT = LOG_SET_CMD(N); } while (0)
#define LOG_ON        do { LOG_PORT = LOG_ON_CMD;     } while (0)
#define LOG_OFF       do { LOG_PORT = LOG_OFF_CMD;    } while (0)
#define LOG_PERF      do { LOG_PORT = LOG_PERF_CMD;   } while (0)

/* Controling perf-meters */
#define PERF_STOP(n)  do { LOG_PORT = PERF_CMD_(n,0); } while (0)
#define PERF_START(n) do { LOG_PORT = PERF_CMD_(n,1); } while (0)
#define PERF_DUMP(n)  do { LOG_PORT = PERF_CMD_(n,2); } while (0)

#define PERF_STOP_ALL  PERF_STOP (0)
#define PERF_START_ALL PERF_START (0)
#define PERF_DUMP_ALL  PERF_DUMP (0)

/* Perf-meter Min/Max on value from program */
#define PERF_STAT_U32(n,x)   PERF_STATX_ (TICKS_4PORT, (x), (n), 5)
#define PERF_STAT_S32(n,x)   PERF_STATX_ (TICKS_4PORT, (x), (n), 6)
#define PERF_STAT_FLOAT(n,x) PERF_STATX_ (TICKS_fPORT, (x), (n), 7)

#define PERF_STATX_(P,X,n,c) do { P=(X); LOG_PORT = PERF_CMD_(n,c); } while (0)

/* Controlling what slot of TICKS_PORT will be accessable */

#define TICKS_IS_CYCLES  LOG_SEND_CMD_ (TICKS_IS_CYCLES_CMD)  /* default */
#define TICKS_IS_INSNS   LOG_SEND_CMD_ (TICKS_IS_INSNS_CMD)
#define TICKS_IS_PRAND   LOG_SEND_CMD_ (TICKS_IS_PRAND_CMD)
#define TICKS_IS_RAND    LOG_SEND_CMD_ (TICKS_IS_RAND_CMD)
#define TICKS_RESET      LOG_SEND_CMD_ (TICKS_RESET_CMD)
#define TICKS_RESET_ALL  LOG_SEND_CMD_ (TICKS_RESET_ALL_CMD)

#define LOG_SEND_CMD_(N)    do { LOG_PORT = LOG_DUMP_CMD (N);} while (0)

#endif /* IN_AVRTEST */

#endif /* AVRTEST_H */
