#ifndef AVRTEST_H
#define AVRTEST_H

#ifndef IN_AVRTEST
#ifdef __AVR_SFR_OFFSET__
#define IOBASE __AVR_SFR_OFFSET__
#else
#define IOBASE 0x20
#endif
#endif /* !IN_AVRTEST */

#define AVRTEST_INVALID_OPCODE 0xffff

/* In- and Outputs */
#define TICKS_PORT_ADDR  (0x24 + IOBASE) /* 4 Inputs (only 1st byte is magic) */

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

    LOG_X_sentinel
  };

enum
  {
    TICKS_RESET_CMD, TICKS_RESET_ALL_CMD,
    TICKS_IS_RAND_CMD, TICKS_IS_CYCLES_CMD,
    TICKS_IS_PRAND_CMD, TICKS_IS_INSNS_CMD
  };

enum
  {
    PERF_STOP_CMD, PERF_START_CMD, PERF_START_CALL_CMD,
    PERF_DUMP_CMD,
    PERF_STAT_U32_CMD, PERF_STAT_S32_CMD, PERF_STAT_FLOAT_CMD,

    PERF_STAT_CMD = PERF_STAT_U32_CMD
  };

enum
  {
    PERF_TAG_STR_CMD, PERF_TAG_U16_CMD, PERF_TAG_U32_CMD,
    PERF_TAG_FLOAT_CMD,
    PERF_LABEL_CMD, PERF_PLABEL_CMD,
    PERF_TAG_FMT_CMD, PERF_TAG_PFMT_CMD
  };

#ifdef IN_AVRTEST

/* This defines are for avrtest itself.  */

// Inputs
#define TICKS_PORT  TICKS_PORT_ADDR

// bits 5..3
#define PERF_CMD(n) (7 & ((n) >> 3))

#define PERF_TAG_CMD(n) (7 & ((n) >> 3))

// bits 2..0 (LOG_CMD = 0)
// bits 2..0 (LOG_CMD = LOG_TAG_PERF)
#define PERF_N(n)   ((n) & 0x7)

// mask
#define PERF_ALL    0xfe

#else /* IN_AVRTEST */

/* This defines can be used in the AVR application.  */

/* Magic Ports */
#define TICKS_PORT  (*((volatile unsigned long*) TICKS_PORT_ADDR))

/* Logging Control */
#define LOG_TAG_CMD(N,T) ((((PERF_## T ##_CMD) & 7) << 3) | ((N) & 7))

/* Performance Measurement */
#define PERF_CMD_(n,c)       (((n) & 7) | (PERF_## c ##_CMD << 3))

/* Convenience */

/* Logging with default format strings */
#define LOG_ADDR(X)  avrtest_syscall_7_a ((X), LOG_ADDR_CMD)
#define LOG_PSTR(X)  avrtest_syscall_7_a ((X), LOG_PSTR_CMD)
#define LOG_STR(X)   avrtest_syscall_7_a ((X), LOG_STR_CMD)
#define LOG_FLOAT(X) avrtest_syscall_7_f ((X), LOG_FLOAT_CMD)

#define LOG_U8(X)  avrtest_syscall_7_u8  ((X), LOG_U8_CMD)
#define LOG_U16(X) avrtest_syscall_7_u16 ((X), LOG_U16_CMD)
#define LOG_U24(X) avrtest_syscall_7_u24 ((X), LOG_U24_CMD)
#define LOG_U32(X) avrtest_syscall_7_u32 ((X), LOG_U32_CMD)

#define LOG_S8(X)  avrtest_syscall_7_s8  ((X), LOG_S8_CMD)
#define LOG_S16(X) avrtest_syscall_7_s16 ((X), LOG_S16_CMD)
#define LOG_S24(X) avrtest_syscall_7_s24 ((X), LOG_S24_CMD)
#define LOG_S32(X) avrtest_syscall_7_s32 ((X), LOG_S32_CMD)

#define LOG_X8(X)  avrtest_syscall_7_u8  ((X), LOG_X8_CMD)
#define LOG_X16(X) avrtest_syscall_7_u16 ((X), LOG_X16_CMD)
#define LOG_X24(X) avrtest_syscall_7_u24 ((X), LOG_X24_CMD)
#define LOG_X32(X) avrtest_syscall_7_u32 ((X), LOG_X32_CMD)

/* Format string for next action only */
#define LOG_SET_FMT_ONCE(F)  avrtest_syscall_7_a ((F), LOG_SET_FMT_ONCE_CMD)
#define LOG_SET_PFMT_ONCE(F) avrtest_syscall_7_a ((F), LOG_SET_PFMT_ONCE_CMD)

/* Setting custom format string for all subsequent logs */
#define LOG_SET_FMT(F)   avrtest_syscall_7_a ((F), LOG_SET_FMT_CMD)
#define LOG_SET_PFMT(F)  avrtest_syscall_7_a ((F), LOG_SET_PFMT_CMD)

/* Reset to default format strings */
#define LOG_UNSET_FMT(F) avrtest_syscall_7 (LOG_UNSET_FMT_CMD)

/* Logging with custom format string (from RAM) */
#define LOG_FMT_ADDR(F,X)  LOG_DUMP_F_ (FMT, (F), LOG_ADDR(X))
#define LOG_FMT_PSTR(F,X)  LOG_DUMP_F_ (FMT, (F), LOG_PSTR(X))
#define LOG_FMT_STR(F,X)   LOG_DUMP_F_ (FMT, (F), LOG_STR(X))
#define LOG_FMT_FLOAT(F,X) LOG_DUMP_F_ (FMT, (F), LOG_FLOAT(X))

#define LOG_FMT_U8(F,X)  LOG_DUMP_F_ (FMT, (F), LOG_U8(X))
#define LOG_FMT_U16(F,X) LOG_DUMP_F_ (FMT, (F), LOG_U16(X))
#define LOG_FMT_U24(F,X) LOG_DUMP_F_ (FMT, (F), LOG_U24(X))
#define LOG_FMT_U32(F,X) LOG_DUMP_F_ (FMT, (F), LOG_U32(X))

#define LOG_FMT_S8(F,X)  LOG_DUMP_F_ (FMT, (F), LOG_S8(X))
#define LOG_FMT_S16(F,X) LOG_DUMP_F_ (FMT, (F), LOG_S16(X))
#define LOG_FMT_S24(F,X) LOG_DUMP_F_ (FMT, (F), LOG_S24(X))
#define LOG_FMT_S32(F,X) LOG_DUMP_F_ (FMT, (F), LOG_S32(X))

#define LOG_FMT_X8(F,X)  LOG_DUMP_F_ (FMT, (F), LOG_X8(X))
#define LOG_FMT_X16(F,X) LOG_DUMP_F_ (FMT, (F), LOG_X16(X))
#define LOG_FMT_X24(F,X) LOG_DUMP_F_ (FMT, (F), LOG_X24(X))
#define LOG_FMT_X32(F,X) LOG_DUMP_F_ (FMT, (F), LOG_X32(X))

/* Logging with custom format string (from Flash) */
#define LOG_PFMT_ADDR(F,X)  LOG_DUMP_F_ (PFMT, (F), LOG_ADDR(X))
#define LOG_PFMT_PSTR(F,X)  LOG_DUMP_F_ (PFMT, (F), LOG_PSTR(X))
#define LOG_PFMT_STR(F,X)   LOG_DUMP_F_ (PFMT, (F), LOG_STR(X))
#define LOG_PFMT_FLOAT(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_FLOAT(X))

#define LOG_PFMT_U8(F,X)  LOG_DUMP_F_ (PFMT, (F), LOG_U8(X))
#define LOG_PFMT_U16(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_U16(X))
#define LOG_PFMT_U24(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_U24(X))
#define LOG_PFMT_U32(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_U32(X))

#define LOG_PFMT_S8(F,X)  LOG_DUMP_F_ (PFMT, (F), LOG_S8(X))
#define LOG_PFMT_S16(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_S16(X))
#define LOG_PFMT_S24(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_S24(X))
#define LOG_PFMT_S32(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_S32(X))

#define LOG_PFMT_X8(F,X)  LOG_DUMP_F_ (PFMT, (F), LOG_X8(X))
#define LOG_PFMT_X16(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_X16(X))
#define LOG_PFMT_X24(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_X24(X))
#define LOG_PFMT_X32(F,X) LOG_DUMP_F_ (PFMT, (F), LOG_X32(X))

#define LOG_DUMP_F_(W, F, C)                                \
  do {                                                      \
      avrtest_syscall_7_a ((F), LOG_SET_## W ##_ONCE_CMD);  \
      C;                                                    \
    } while (0)

/* Tagging perf-meter rounds */

/* Tagging using default format string */
#define PERF_TAG_STR(N,X)   avrtest_syscall_6_s ((X), LOG_TAG_CMD (N, TAG_STR))
#define PERF_TAG_U16(N,X)   avrtest_syscall_6_u ((X), LOG_TAG_CMD (N, TAG_U16))
#define PERF_TAG_U32(N,X)   avrtest_syscall_6_ul((X), LOG_TAG_CMD (N, TAG_U32))
#define PERF_TAG_FLOAT(N,X) avrtest_syscall_6_f ((X), LOG_TAG_CMD (N, TAG_FLOAT))

/* Labeling perf-meter */
#define PERF_LABEL(N,L)     avrtest_syscall_6_s ((L), LOG_TAG_CMD (N, LABEL))
#define PERF_PLABEL(N,L)    avrtest_syscall_6_s ((L), LOG_TAG_CMD (N, PLABEL))

/* Set custom format string for tagging */
#define PERF_TAG_FMT(F)     avrtest_syscall_6_s (F, LOG_TAG_CMD (0, TAG_FMT))
#define PERF_TAG_PFMT(F)    avrtest_syscall_6_s (F, LOG_TAG_CMD (0, TAP_PFMT))

/* Tagging with custom format string (from RAM) */
#define PERF_TAG_FMT_STR(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_s,  TAG_STR)
#define PERF_TAG_FMT_U16(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_u,  TAG_U16)
#define PERF_TAG_FMT_U32(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_ul, TAG_U32)
#define PERF_TAG_FMT_FLOAT(N,F,X) PERF_TAG_F_((F), (X), (N), _6_f,  TAG_STR)

/* Tagging with custom format string (from Flash) */
#define PERF_TAG_PFMT_STR(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_s,  TAG_STR)
#define PERF_TAG_PFMT_U16(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_u,  TAG_U16)
#define PERF_TAG_PFMT_U32(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_ul, TAG_U32)
#define PERF_TAG_PFMT_FLOAT(N,F,X) PERF_TAG_PF_((F), (X), (N), _6_f,  TAG_FLOAT)

#define PERF_TAG_F_(F,X,N,S,C) \
  do { PERF_TAG_FMT(F); avrtest_syscall##S (X, LOG_TAG_CMD (N,C)); } while (0)

#define PERF_TAG_PF_(F,X,N,S,C) \
  do { PERF_TAG_PFMT(F); avrtest_syscall##S (X, LOG_TAG_CMD (N,C)); } while (0)

/* Instruction logging control */

#define LOG_SET(N)    avrtest_syscall_3 (N)
#define LOG_PERF      avrtest_syscall_2 ()
#define LOG_ON        avrtest_syscall_1 ()
#define LOG_OFF       avrtest_syscall_0 ()

/* Controling perf-meters */
#define PERF_STOP(n)        avrtest_syscall_5 (PERF_CMD_((n), STOP))
#define PERF_START(n)       avrtest_syscall_5 (PERF_CMD_((n), START))
#define PERF_DUMP(n)        avrtest_syscall_5 (PERF_CMD_((n), DUMP))
#define PERF_START_CALL(n)  avrtest_syscall_5 (PERF_CMD_((n), START_CALL))

#define PERF_STOP_ALL         PERF_STOP (0)
#define PERF_START_ALL        PERF_START (0)
#define PERF_START_CALL_ALL   PERF_START_CALL (0)
#define PERF_DUMP_ALL         PERF_DUMP (0)

/* Perf-meter Min/Max on value from program */
#define PERF_STAT_U32(n,x)   avrtest_syscall_5_u ((x), PERF_CMD_((n),STAT_U32))
#define PERF_STAT_S32(n,x)   avrtest_syscall_5_s ((x), PERF_CMD_((n),STAT_S32))
#define PERF_STAT_FLOAT(n,x) avrtest_syscall_5_f ((x), PERF_CMD_((n),STAT_FLOAT))

/* Controlling what slot of TICKS_PORT will be accessable */
#define TICKS_IS_CYCLES  avrtest_syscall_4 (TICKS_IS_CYCLES_CMD)  /* default */
#define TICKS_IS_INSNS   avrtest_syscall_4 (TICKS_IS_INSNS_CMD)
#define TICKS_IS_PRAND   avrtest_syscall_4 (TICKS_IS_PRAND_CMD)
#define TICKS_IS_RAND    avrtest_syscall_4 (TICKS_IS_RAND_CMD)
#define TICKS_RESET      avrtest_syscall_4 (TICKS_RESET_CMD)
#define TICKS_RESET_ALL  avrtest_syscall_4 (TICKS_RESET_ALL_CMD)


#define CPSE_rr_(r)                         \
  (((0UL + AVRTEST_INVALID_OPCODE) << 16)   \
   | 0x1000 | (r << 4) | (r & 0xf) | ((r & 0x10) << 5))

enum
{
  SYSCo_0 = CPSE_rr_(0),  SYSCo_10 = CPSE_rr_(10),  SYSCo_20 = CPSE_rr_(20),
  SYSCo_1 = CPSE_rr_(1),  SYSCo_11 = CPSE_rr_(11),  SYSCo_21 = CPSE_rr_(21),
  SYSCo_2 = CPSE_rr_(2),  SYSCo_12 = CPSE_rr_(12),  SYSCo_22 = CPSE_rr_(22),
  SYSCo_3 = CPSE_rr_(3),  SYSCo_13 = CPSE_rr_(13),  SYSCo_23 = CPSE_rr_(23),
  SYSCo_4 = CPSE_rr_(4),  SYSCo_14 = CPSE_rr_(14),  SYSCo_24 = CPSE_rr_(24),
  SYSCo_5 = CPSE_rr_(5),  SYSCo_15 = CPSE_rr_(15),  SYSCo_25 = CPSE_rr_(25),
  SYSCo_6 = CPSE_rr_(6),  SYSCo_16 = CPSE_rr_(16),  SYSCo_26 = CPSE_rr_(26),
  SYSCo_7 = CPSE_rr_(7),  SYSCo_17 = CPSE_rr_(17),  SYSCo_27 = CPSE_rr_(27),
  SYSCo_8 = CPSE_rr_(8),  SYSCo_18 = CPSE_rr_(18),  SYSCo_28 = CPSE_rr_(28),
  SYSCo_9 = CPSE_rr_(9),  SYSCo_19 = CPSE_rr_(19),  SYSCo_29 = CPSE_rr_(29),

  SYSCo_30 = CPSE_rr_(30),
  SYSCo_31 = CPSE_rr_(31),
};

#define AVRTEST_DEF_SYSCALL0(S, N)                          \
  static __inline__ __attribute__((__always_inline__))      \
  void avrtest_syscall ## S (void)                          \
  {                                                         \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N));     \
  }

#define AVRTEST_DEF_SYSCALL1(S, N, T1, R1)                  \
  static __inline__ __attribute__((__always_inline__))      \
  void avrtest_syscall ## S (T1 _v1_)                       \
  {                                                         \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N),      \
                        "r" (r##R1));                       \
  }

#define AVRTEST_DEF_SYSCALL2(S, N, T1, R1, T2, R2)          \
  static __inline__ __attribute__((__always_inline__))      \
  void avrtest_syscall ## S (T1 _v1_, T2 _v2_)              \
  {                                                         \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    register T2 r##R2 __asm (#R2) = _v2_;                   \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N),      \
                        "r" (r##R1), "r" (r##R2));          \
  }


static __inline__ __attribute__((__always_inline__))
int avrtest_syscall_28 (void)
{
  register int r24 __asm ("24");
  __asm __volatile__ (".long %2 ;; SYSCALL %1"
                      : "=r" (r24)
                      : "n" (28), "n" (SYSCo_28));
  return r24;
}

AVRTEST_DEF_SYSCALL1 (_27, 27, void*, 24)
AVRTEST_DEF_SYSCALL1 (_29, 29, char, 24)
AVRTEST_DEF_SYSCALL1 (_30, 30, int, 24)
AVRTEST_DEF_SYSCALL0 (_31, 31)

AVRTEST_DEF_SYSCALL0 (_0, 0) // LOG_OFFI
AVRTEST_DEF_SYSCALL0 (_1, 1) // LOG_ON
AVRTEST_DEF_SYSCALL0 (_2, 2) // LOG_PERF

/* LOG_SET (N) */
AVRTEST_DEF_SYSCALL1 (_3, 3, unsigned, 24)

/* TICKS_PORT config & reset */
AVRTEST_DEF_SYSCALL1 (_4, 4, unsigned char, 24)

/* Perf-meter control */
AVRTEST_DEF_SYSCALL1 (_5, 5, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_u, 5, unsigned long, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_s, 5, signed long,   20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_f, 5, float,         20, unsigned char, 24)

/* PERF_TAG */
AVRTEST_DEF_SYSCALL2 (_6_s, 6, const volatile char*, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_u, 6, unsigned, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_ul, 6, unsigned long, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_f,  6, float, 20, unsigned char, 24)

/* Logging values */
AVRTEST_DEF_SYSCALL1 (_7, 7, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_a, 7, const volatile void*, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_f, 7, float,                20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u8,  7, unsigned char, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s8,  7,   signed char, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u16, 7, unsigned int,  20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s16, 7,   signed int,  20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u32, 7, unsigned long, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s32, 7,   signed long, 20, unsigned char, 24)

#ifdef __UINT24_MAX__
AVRTEST_DEF_SYSCALL2 (_7_u24, 7, __uint24, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s24, 7, __int24,  20, unsigned char, 24)
#else
AVRTEST_DEF_SYSCALL2 (_7_u24, 7, unsigned long, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s24, 7,   signed long, 20, unsigned char, 24)
#endif

/* Get ticks / insns / (pseudo) random value */

static __inline__ __attribute__((__always_inline__))
unsigned long avrtest_syscall_8 (void)
{
  register unsigned long r22 __asm ("22");
  __asm __volatile__ (".long %2 ;; SYSCALL %1"
                      : "=r" (r22)
                      : "n" (8), "n" (SYSCo_8));
  return r22;
}

#undef AVRTEST_DEF_SYSCALL0
#undef AVRTEST_DEF_SYSCALL1
#undef AVRTEST_DEF_SYSCALL2

static __inline__ __attribute__((__always_inline__))
void avrtest_abort (void)
{
  avrtest_syscall_31 ();
}

static __inline__ __attribute__((__always_inline__))
void avrtest_exit (int _status)
{
  avrtest_syscall_30 (_status);
}

static __inline__ __attribute__((__always_inline__))
void avrtest_putchar (int _c)
{
  avrtest_syscall_29 (_c);
}

static __inline__ __attribute__((__always_inline__))
int avrtest_getchar (void)
{
  return avrtest_syscall_28 ();
}

static __inline__ __attribute__((__always_inline__))
unsigned long avrtest_ticks (void)
{
  return avrtest_syscall_8 ();
}

#endif /* IN_AVRTEST */

#endif /* AVRTEST_H */
