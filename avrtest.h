#ifndef AVRTEST_H
#define AVRTEST_H

#define AVRTEST_INVALID_OPCODE 0xffff

#if !defined (__ASSEMBLER__)

enum
  {
    LOG_ADDR_CMD,
    LOG_STR_CMD,  LOG_SET_FMT_ONCE_CMD,  LOG_SET_FMT_CMD, LOG_UNSET_FMT_CMD,
    LOG_PSTR_CMD, LOG_SET_PFMT_ONCE_CMD, LOG_SET_PFMT_CMD,

    LOG_U8_CMD, LOG_U16_CMD, LOG_U24_CMD, LOG_U32_CMD, LOG_U64_CMD,
    LOG_S8_CMD, LOG_S16_CMD, LOG_S24_CMD, LOG_S32_CMD, LOG_S64_CMD,
    LOG_X8_CMD, LOG_X16_CMD, LOG_X24_CMD, LOG_X32_CMD, LOG_X64_CMD,
    LOG_FLOAT_CMD, LOG_D64_CMD, LOG_F7T_CMD,

    LOG_TAG_FMT_CMD, LOG_TAG_PFMT_CMD,

    LOG_X_sentinel
  };

enum
  {
    TICKS_GET_CYCLES_CMD,
    TICKS_GET_INSNS_CMD,
    TICKS_GET_RAND_CMD,
    TICKS_GET_PRAND_CMD,
    TICKS_CYCLES_CALL_CMD,

    TICKS_RESET_CYCLES_CMD = 1 << 3,
    TICKS_RESET_INSNS_CMD  = 1 << 4,
    TICKS_RESET_PRAND_CMD  = 1 << 5,
    TICKS_RESET_ALL_CMD = TICKS_RESET_CYCLES_CMD
                          | TICKS_RESET_INSNS_CMD
                          | TICKS_RESET_PRAND_CMD
  };

enum
  {
    PERF_STOP_CMD, PERF_DUMP_CMD,
    PERF_START_CMD, PERF_START_CALL_CMD,
    PERF_STAT_U32_CMD, PERF_STAT_S32_CMD, PERF_STAT_FLOAT_CMD,

    PERF_STAT_CMD = PERF_STAT_U32_CMD
  };

enum
  {
    PERF_TAG_STR_CMD, PERF_TAG_S32_CMD, PERF_TAG_U32_CMD,
    PERF_TAG_S16_CMD, PERF_TAG_U16_CMD,
    PERF_TAG_FLOAT_CMD,
    PERF_LABEL_CMD, PERF_PLABEL_CMD,
    PERF_TAG_FMT_CMD, PERF_TAG_PFMT_CMD
  };

enum
  {
    AVRTEST_fopen, AVRTEST_fclose,
    AVRTEST_fgetc, AVRTEST_fputc,
    AVRTEST_feof,  AVRTEST_clearerr,
    AVRTEST_fread, AVRTEST_fwrite,
    AVRTEST_fseek, AVRTEST_fflush
  };

enum
  {
    AVRTEST_sin, AVRTEST_asin, AVRTEST_sinh, AVRTEST_asinh,
    AVRTEST_cos, AVRTEST_acos, AVRTEST_cosh, AVRTEST_acosh,
    AVRTEST_tan, AVRTEST_atan, AVRTEST_tanh, AVRTEST_atanh,
    AVRTEST_sqrt, AVRTEST_cbrt, AVRTEST_exp, AVRTEST_log,
    AVRTEST_trunc, AVRTEST_ceil, AVRTEST_floor, AVRTEST_round,
    AVRTEST_log2, AVRTEST_log10, AVRTEST_fabs,
    AVRTEST_EMUL_2args,
    AVRTEST_pow = AVRTEST_EMUL_2args,
    AVRTEST_atan2, AVRTEST_hypot, AVRTEST_fdim,
    AVRTEST_fmin, AVRTEST_fmax, AVRTEST_fmod,
    AVRTEST_mul, AVRTEST_div, AVRTEST_add, AVRTEST_sub,
    AVRTEST_ulp, AVRTEST_prand,
    AVRTEST_EMUL_misc,
    AVRTEST_ldexp = AVRTEST_EMUL_misc,
    AVRTEST_frexp, AVRTEST_modf, AVRTEST_powi,
    AVRTEST_u32to, AVRTEST_s32to,
    AVRTEST_cmp, AVRTEST_strto,
    AVRTEST_EMUL_sentinel
  };

enum
  {
    AVRTEST_MISC_flmap,
    AVRTEST_MISC_mulu32, AVRTEST_MISC_divu32, AVRTEST_MISC_modu32,
    AVRTEST_MISC_muls32, AVRTEST_MISC_divs32, AVRTEST_MISC_mods32,
    AVRTEST_MISC_mulu64, AVRTEST_MISC_divu64, AVRTEST_MISC_modu64,
    AVRTEST_MISC_muls64, AVRTEST_MISC_divs64, AVRTEST_MISC_mods64,
    AVRTEST_MISC_nofxtof,
    AVRTEST_MISC_rtof, AVRTEST_MISC_hrtof,
    AVRTEST_MISC_ktof, AVRTEST_MISC_hktof,
    AVRTEST_MISC_urtof, AVRTEST_MISC_uhrtof,
    AVRTEST_MISC_uktof, AVRTEST_MISC_uhktof,
    AVRTEST_MISC_ftor, AVRTEST_MISC_ftohr,
    AVRTEST_MISC_ftok, AVRTEST_MISC_ftohk,
    AVRTEST_MISC_ftour, AVRTEST_MISC_ftouhr,
    AVRTEST_MISC_ftouk, AVRTEST_MISC_ftouhk,
    AVRTEST_MISC_ftol,
    AVRTEST_MISC_ltof,
    AVRTEST_MISC_sentinel
  };

#ifdef IN_AVRTEST

/* These defines are for avrtest itself.  */

/* bits 5..3 */
#define PERF_CMD(n) (0xf & ((n) >> 4))

#define PERF_TAG_CMD(n) (0xf & ((n) >> 4))

/* bits 2..0 (LOG_CMD = 0) */
/* bits 2..0 (LOG_CMD = LOG_TAG_PERF) */
#define PERF_N(n)   ((n) & 0x7)

/* mask */
#define PERF_ALL    0xfe

#else /* IN_AVRTEST */

/* This defines can be used in the AVR application.  */

/* Logging Control */
#define LOG_TAG_CMD(N,T) ((((PERF_## T ##_CMD) & 0xf) << 4) | ((N) & 7))

/* Performance Measurement */
#define PERF_CMD_(n,c)       (((n) & 7) | (PERF_## c ##_CMD << 4))

/* Convenience */

/* Logging with default format strings */
#define LOG_ADDR(X)  avrtest_syscall_7_a ((X), LOG_ADDR_CMD)
#define LOG_PSTR(X)  avrtest_syscall_7_a ((X), LOG_PSTR_CMD)
#define LOG_STR(X)   avrtest_syscall_7_a ((X), LOG_STR_CMD)
#define LOG_FLOAT(X) avrtest_syscall_7_f ((X), LOG_FLOAT_CMD)
#define LOG_F7T(X)   avrtest_syscall_7_a ((X), LOG_F7T_CMD)
#if __SIZEOF_DOUBLE__ == __SIZEOF_FLOAT__
#   define LOG_DOUBLE(X) LOG_FLOAT(X)
#else
#   define LOG_DOUBLE(X) avrtest_syscall_8_d64 ((X), LOG_D64_CMD)
#endif
#if __SIZEOF_LONG_DOUBLE__ == __SIZEOF_FLOAT__
#   define LOG_LDOUBLE(X) LOG_FLOAT(X)
#else
#   define LOG_LDOUBLE(X) avrtest_syscall_8_l64 ((X), LOG_D64_CMD)
#endif

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

#define LOG_U64(X) avrtest_syscall_8_u64 ((X), LOG_U64_CMD)
#define LOG_S64(X) avrtest_syscall_8_s64 ((X), LOG_S64_CMD)
#define LOG_X64(X) avrtest_syscall_8_u64 ((X), LOG_X64_CMD)
#define LOG_D64(X) avrtest_syscall_8_u64 ((X), LOG_D64_CMD)

/* Format string for next action only */
#define LOG_SET_FMT_ONCE(F)  avrtest_syscall_7_a ((F), LOG_SET_FMT_ONCE_CMD)
#define LOG_SET_PFMT_ONCE(F) avrtest_syscall_7_a ((F), LOG_SET_PFMT_ONCE_CMD)

/* Setting custom format string for all subsequent logs */
#define LOG_SET_FMT(F)   avrtest_syscall_7_a ((F), LOG_SET_FMT_CMD)
#define LOG_SET_PFMT(F)  avrtest_syscall_7_a ((F), LOG_SET_PFMT_CMD)

/* Reset to default format strings */
#define LOG_UNSET_FMT(F) avrtest_syscall_7 (LOG_UNSET_FMT_CMD)

/* Logging with custom format string (from RAM) */
#define LOG_FMT_ADDR(F,X)  LOG_DUMP_A_ (FMT, (F), (X), LOG_ADDR(_x))
#define LOG_FMT_PSTR(F,X)  LOG_DUMP_S_ (FMT, (F), (X), LOG_PSTR(_x))
#define LOG_FMT_STR(F,X)   LOG_DUMP_S_ (FMT, (F), (X), LOG_STR(_x))
#define LOG_FMT_FLOAT(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_FLOAT(_x))
#define LOG_FMT_DOUBLE(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_DOUBLE(_x))
#define LOG_FMT_LDOUBLE(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_LDOUBLE(_x))
#define LOG_FMT_F7T(F,X)   LOG_DUMP_A_ (FMT, (F), (X), LOG_F7T(_x))

#define LOG_FMT_U8(F,X)  LOG_DUMP_F_ (FMT, (F), (X), LOG_U8(_x))
#define LOG_FMT_U16(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_U16(_x))
#define LOG_FMT_U24(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_U24(_x))
#define LOG_FMT_U32(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_U32(_x))
#define LOG_FMT_U64(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_U64(_x))

#define LOG_FMT_S8(F,X)  LOG_DUMP_F_ (FMT, (F), (X), LOG_S8(_x))
#define LOG_FMT_S16(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_S16(_x))
#define LOG_FMT_S24(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_S24(_x))
#define LOG_FMT_S32(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_S32(_x))
#define LOG_FMT_S64(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_S64(_x))

#define LOG_FMT_X8(F,X)  LOG_DUMP_F_ (FMT, (F), (X), LOG_X8(_x))
#define LOG_FMT_X16(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_X16(_x))
#define LOG_FMT_X24(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_X24(_x))
#define LOG_FMT_X32(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_X32(_x))
#define LOG_FMT_X64(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_X64(_x))
#define LOG_FMT_D64(F,X) LOG_DUMP_F_ (FMT, (F), (X), LOG_D64(_x))

/* Logging with custom format string (from Flash) */
#define LOG_PFMT_ADDR(F,X)  LOG_DUMP_A_ (PFMT, (F), (X), LOG_ADDR(_x))
#define LOG_PFMT_PSTR(F,X)  LOG_DUMP_S_ (PFMT, (F), (X), LOG_PSTR(_x))
#define LOG_PFMT_STR(F,X)   LOG_DUMP_S_ (PFMT, (F), (X), LOG_STR(_x))
#define LOG_PFMT_FLOAT(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_FLOAT(_x))
#define LOG_PFMT_DOUBLE(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_DOUBLE(_x))
#define LOG_PFMT_LDOUBLE(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_LDOUBLE(_x))
#define LOG_PFMT_F7T(F,X)   LOG_DUMP_A_ (PFMT, (F), (X), LOG_F7T(_x))

#define LOG_PFMT_U8(F,X)  LOG_DUMP_F_ (PFMT, (F), (X), LOG_U8(_x))
#define LOG_PFMT_U16(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_U16(_x))
#define LOG_PFMT_U24(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_U24(_x))
#define LOG_PFMT_U32(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_U32(_x))
#define LOG_PFMT_U64(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_U64(_x))

#define LOG_PFMT_S8(F,X)  LOG_DUMP_F_ (PFMT, (F), (X), LOG_S8(_x))
#define LOG_PFMT_S16(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_S16(_x))
#define LOG_PFMT_S24(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_S24(_x))
#define LOG_PFMT_S32(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_S32(_x))
#define LOG_PFMT_S64(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_S64(_x))

#define LOG_PFMT_X8(F,X)  LOG_DUMP_F_ (PFMT, (F), (X), LOG_X8(_x))
#define LOG_PFMT_X16(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_X16(_x))
#define LOG_PFMT_X24(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_X24(_x))
#define LOG_PFMT_X32(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_X32(_x))
#define LOG_PFMT_X64(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_X64(_x))
#define LOG_PFMT_D64(F,X) LOG_DUMP_F_ (PFMT, (F), (X), LOG_D64(_x))

#define LOG_DUMP_F_(W, F, X, C)                             \
  do {                                                      \
      __typeof__(X) _x = (X);                               \
      avrtest_syscall_7_a ((F), LOG_SET_## W ##_ONCE_CMD);  \
      C;                                                    \
    } while (0)

#define LOG_DUMP_S_(W, F, X, C)                             \
  do {                                                      \
      const char *_x = (X);                                 \
      avrtest_syscall_7_a ((F), LOG_SET_## W ##_ONCE_CMD);  \
      C;                                                    \
    } while (0)

#define LOG_DUMP_A_(W, F, X, C)                             \
  do {                                                      \
      const volatile void *_x = (X);                        \
      avrtest_syscall_7_a ((F), LOG_SET_## W ##_ONCE_CMD);  \
      C;                                                    \
    } while (0)

/* Tagging perf-meter rounds */

/* Tagging using default format string */
#define PERF_TAG_STR(N,X)   avrtest_syscall_6_s ((X), LOG_TAG_CMD (N, TAG_STR))
#define PERF_TAG_S16(N,X)   avrtest_syscall_6_i ((X), LOG_TAG_CMD (N, TAG_S16))
#define PERF_TAG_S32(N,X)   avrtest_syscall_6_l ((X), LOG_TAG_CMD (N, TAG_S32))
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
#define PERF_TAG_FMT_S16(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_i,  TAG_S16)
#define PERF_TAG_FMT_S32(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_l,  TAG_S32)
#define PERF_TAG_FMT_U16(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_u,  TAG_U16)
#define PERF_TAG_FMT_U32(N,F,X)   PERF_TAG_F_((F), (X), (N), _6_ul, TAG_U32)
#define PERF_TAG_FMT_FLOAT(N,F,X) PERF_TAG_F_((F), (X), (N), _6_f,  TAG_FLOAT)

/* Tagging with custom format string (from Flash) */
#define PERF_TAG_PFMT_STR(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_s,  TAG_STR)
#define PERF_TAG_PFMT_S16(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_i   TAG_S16)
#define PERF_TAG_PFMT_S32(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_l   TAG_S32)
#define PERF_TAG_PFMT_U16(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_u,  TAG_U16)
#define PERF_TAG_PFMT_U32(N,F,X)   PERF_TAG_PF_((F), (X), (N), _6_ul, TAG_U32)
#define PERF_TAG_PFMT_FLOAT(N,F,X) PERF_TAG_PF_((F), (X), (N), _6_f,  TAG_FLOAT)

#define PERF_TAG_F_(F,X,N,S,C) \
  do { PERF_TAG_FMT(F); avrtest_syscall##S (X, LOG_TAG_CMD (N,C)); } while (0)

#define PERF_TAG_PF_(F,X,N,S,C) \
  do { PERF_TAG_PFMT(F); avrtest_syscall##S (X, LOG_TAG_CMD (N,C)); } while (0)

/* Instruction logging control */

#define LOG_POP       avrtest_syscall_11 ()
#define LOG_PUSH_ON   avrtest_syscall_10 ()
#define LOG_PUSH_OFF  avrtest_syscall_9 ()
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

#define AT_INLINE __inline__ __attribute__((__always_inline__))

#define CPSE_rr_(r)                                      \
  (((__UINT32_TYPE__) AVRTEST_INVALID_OPCODE << 16)      \
   | 0x1000 | ((__UINT32_TYPE__) r << 4) | (r & 0xf)     \
   | (((__UINT32_TYPE__) r & 0x10) << 5))

__extension__ enum
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
  static AT_INLINE                                          \
  void avrtest_syscall ## S (void)                          \
  {                                                         \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N));     \
  }

#define AVRTEST_DEF_SYSCALL1(S, N, T1, R1)                  \
  static AT_INLINE                                          \
  void avrtest_syscall ## S (T1 _v1_)                       \
  {                                                         \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N),      \
                        "r" (r##R1));                       \
  }

#define AVRTEST_DEF_SYSCALL2(S, N, T1, R1, T2, R2)          \
  static AT_INLINE                                          \
  void avrtest_syscall ## S (T1 _v1_, T2 _v2_)              \
  {                                                         \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    register T2 r##R2 __asm (#R2) = _v2_;                   \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N),      \
                        "r" (r##R1), "r" (r##R2));          \
  }

#define AVRTEST_DEF_SYSCALL2_ext(S, N, T1, R1, T2, R2)      \
  __extension__ static AT_INLINE                            \
  void avrtest_syscall ## S (T1 _v1_, T2 _v2_)              \
  {                                                         \
    __extension__ register T1 r##R1 __asm (#R1) = _v1_;     \
    register T2 r##R2 __asm (#R2) = _v2_;                   \
    __asm __volatile__ (".long %1 ;; SYSCALL %0"            \
                        :: "n" (N), "n" (SYSCo_ ## N),      \
                        "r" (r##R1), "r" (r##R2));          \
  }

#define AVRTEST_DEF_SYSCALL1_0(S, N, T0, R0)                \
  static AT_INLINE                                          \
  T0 avrtest_syscall ## S (void)                            \
  {                                                         \
    register T0 r##R0 __asm (#R0);                          \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "=r" (r##R0)                      \
                        : "n" (N), "n" (SYSCo_ ## N));      \
    return r##R0;                                           \
  }

#define AVRTEST_DEF_SYSCALL1_1(S, N, T0, R0, T1, R1)        \
  static AT_INLINE                                          \
  T0 avrtest_syscall ## S (T1 _v1_)                         \
  {                                                         \
    register T0 r##R0 __asm (#R0);                          \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "=r" (r##R0)                      \
                        : "n" (N), "n" (SYSCo_ ## N),       \
                          "r" (r##R1));                     \
    return r##R0;                                           \
  }

#define AVRTEST_DEF_SYSCALL2_1(S, N, T0, R0, T1, R1)        \
  static AT_INLINE                                          \
  T0 avrtest_syscall ## S (T0 _v0_, T1 _v1_)                \
  {                                                         \
    register T0 r##R0 __asm (#R0) = _v0_;                   \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "+r" (r##R0)                      \
                        : "n" (N), "n" (SYSCo_ ## N),       \
                          "r" (r##R1));                     \
    return r##R0;                                           \
  }

#define AVRTEST_DEF_SYSCALL3_1(S, N, T0, R0, T1, R1, T2, R2)\
  static AT_INLINE                                          \
  T0 avrtest_syscall ## S (T0 _v0_, T1 _v1_, T2 _v2_)       \
  {                                                         \
    register T0 r##R0 __asm (#R0) = _v0_;                   \
    register T1 r##R1 __asm (#R1) = _v1_;                   \
    register T2 r##R2 __asm (#R2) = _v2_;                   \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "+r" (r##R0)                      \
                        : "n" (N), "n" (SYSCo_ ## N),       \
                          "r" (r##R1), "r" (r##R2));        \
    return r##R0;                                           \
  }

#define AVRTEST_DEF_SYSCALL1_1m(S, N, T0, R0, T2, R2)       \
  static AT_INLINE                                          \
  T0 avrtest_syscall ## S (unsigned char _v1_, T2 _v2_)     \
  {                                                         \
    register T0 res##R0 __asm (#R0);                        \
    register unsigned char r26 __asm("26") = _v1_;          \
    register T2 r##R2 __asm (#R2) = _v2_;                   \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "=r" (res##R0)                    \
                        : "n" (N), "n" (SYSCo_ ## N),       \
                          "r" (r26), "r" (r##R2));          \
    return res##R0;                                         \
  }

#define AVRTEST_DEF_SYSCALL2_1m(S, N, T0, R0, T2, R2, T3, R3)           \
  static AT_INLINE                                                      \
  T0 avrtest_syscall ## S (unsigned char _v1_, T2 _v2_, T3 _v3_)        \
  {                                                                     \
    register T0 res##R0 __asm (#R0);                                    \
    register unsigned char r26 __asm("26") = _v1_;                      \
    register T2 r##R2 __asm (#R2) = _v2_;                               \
    register T3 r##R3 __asm (#R3) = _v3_;                               \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"                        \
                        : "=r" (res##R0)                                \
                        : "n" (N), "n" (SYSCo_ ## N),                   \
                          "r" (r26), "r" (r##R2), "r" (r##R3));         \
    return res##R0;                                                     \
  }
/* Same, but with memory clobber for strtof's char**.  */
#define AVRTEST_DEF_SYSCALL2_1M(S, N, T0, R0, T2, R2, T3, R3)           \
  static AT_INLINE                                                      \
  T0 avrtest_syscall ## S (unsigned char _v1_, T2 _v2_, T3 _v3_)        \
  {                                                                     \
    register T0 res##R0 __asm (#R0);                                    \
    register unsigned char r26 __asm("26") = _v1_;                      \
    register T2 r##R2 __asm (#R2) = _v2_;                               \
    register T3 r##R3 __asm (#R3) = _v3_;                               \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"                        \
                        : "=r" (res##R0)                                \
                        : "n" (N), "n" (SYSCo_ ## N),                   \
                          "r" (r26), "r" (r##R2), "r" (r##R3)           \
                        : "memory");                                    \
    return res##R0;                                                     \
  }


#define AVRTEST_DEF_SYSCALL2_R20(S, N, T2)                  \
  static AT_INLINE                                          \
  unsigned long avrtest_syscall ## S (unsigned char _v1_, T2 _v2_) \
  {                                                         \
    register unsigned long _r22 __asm ("22");               \
    register unsigned char _r24 __asm ("24") = _v1_;        \
    register T2 _r20 __asm ("20") = _v2_;                   \
    __asm __volatile__ (".long %2 ;; SYSCALL %1"            \
                        : "=r" (_r22)                       \
                        : "n" (N), "n" (SYSCo_ ## N),       \
                          "r" (_r24), "r" (_r20)            \
                        : "memory");                        \
    return _r22;                                            \
  }

AVRTEST_DEF_SYSCALL2_R20 (_26_p, 26, const void*)
AVRTEST_DEF_SYSCALL2_R20 (_26_1, 26, __UINT8_TYPE__)
AVRTEST_DEF_SYSCALL2_R20 (_26_2, 26, __UINT16_TYPE__)
AVRTEST_DEF_SYSCALL2_R20 (_26_4, 26, __UINT32_TYPE__)
AVRTEST_DEF_SYSCALL1 (_27, 27, void*, 24)
AVRTEST_DEF_SYSCALL1_0 (_28, 28, int, 24)
AVRTEST_DEF_SYSCALL1 (_24, 24, char, 24)
AVRTEST_DEF_SYSCALL1 (_29, 29, char, 24)
AVRTEST_DEF_SYSCALL1 (_30, 30, __INT16_TYPE__, 24) /* exit */
AVRTEST_DEF_SYSCALL0 (_31, 31) /* abort */
AVRTEST_DEF_SYSCALL0 (_25, 25) /* abort_2nd_hit */

AVRTEST_DEF_SYSCALL0 (_0, 0) /* LOG_OFF  */
AVRTEST_DEF_SYSCALL0 (_1, 1) /* LOG_ON   */
AVRTEST_DEF_SYSCALL0 (_2, 2) /* LOG_PERF */
AVRTEST_DEF_SYSCALL0 (_9,   9) /* LOG_PUSH 0 */
AVRTEST_DEF_SYSCALL0 (_10, 10) /* LOG_PUSH 1 */
AVRTEST_DEF_SYSCALL0 (_11, 11) /* LOG_POP */

/* LOG_SET (N) */
AVRTEST_DEF_SYSCALL1 (_3, 3, unsigned, 24)

/* Cycle count, instruction cound, (pseudo) random number */
AVRTEST_DEF_SYSCALL1   (_4_r, 4, unsigned char, 24)
AVRTEST_DEF_SYSCALL1_1 (_4_g, 4, __UINT32_TYPE__, 22, unsigned char, 24)

/* Perf-meter control */
AVRTEST_DEF_SYSCALL1 (_5, 5, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_u, 5, __UINT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_s, 5,  __INT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_5_f, 5, float,           20, unsigned char, 24)

/* PERF_TAG */
AVRTEST_DEF_SYSCALL2 (_6_s, 6, const volatile char*, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_i,  6,  __INT16_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_u,  6, __UINT16_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_l,  6,  __INT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_ul, 6, __UINT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_6_f,  6, float, 20, unsigned char, 24)

/* Logging values */
AVRTEST_DEF_SYSCALL1 (_7, 7, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_a, 7, const volatile void*, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_f, 7, float,             20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u8,  7,  __UINT8_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s8,  7,   __INT8_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u16, 7, __UINT16_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s16, 7,  __INT16_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_u32, 7, __UINT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s32, 7,  __INT32_TYPE__, 20, unsigned char, 24)

#ifdef __UINT24_MAX__
AVRTEST_DEF_SYSCALL2_ext (_7_u24, 7, __uint24, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2_ext (_7_s24, 7, __int24,  20, unsigned char, 24)
#else
AVRTEST_DEF_SYSCALL2 (_7_u24, 7, __UINT32_TYPE__, 20, unsigned char, 24)
AVRTEST_DEF_SYSCALL2 (_7_s24, 7,  __INT32_TYPE__, 20, unsigned char, 24)
#endif

/* Logging 64-bit values */
#if __SIZEOF_LONG_LONG__ == 8
AVRTEST_DEF_SYSCALL2 (_8_u64, 8, __UINT64_TYPE__, 18, unsigned char, 26)
AVRTEST_DEF_SYSCALL2 (_8_s64, 8,  __INT64_TYPE__, 18, unsigned char, 26)
#else
#define avrtest_syscall_8_u64(a, b)                                     \
    do { (void) (a); (void) (b);                                        \
        __attribute__((__error__("-mint8: avrtest uint64_t syscall not" \
                                 " available without 64-bit integers")))\
        extern void avrtest_error (void);                               \
        avrtest_error();                                                \
    } while(0)
#define avrtest_syscall_8_s64(a, b)                                     \
    do { (void) (a); (void) (b);                                        \
        __attribute__((__error__("-mint8: avrtest int64_t syscall not"  \
                                 " available without 64-bit integers")))\
        extern void avrtest_error (void);                               \
        avrtest_error();                                                \
    } while(0)
#endif /* Have uint64_t */
#if __SIZEOF_DOUBLE__ == 8
AVRTEST_DEF_SYSCALL2 (_8_d64, 8,          double, 18, unsigned char, 26)
#endif
#if __SIZEOF_LONG_DOUBLE__ == 8
AVRTEST_DEF_SYSCALL2 (_8_l64, 8,     long double, 18, unsigned char, 26)
#endif

/* Misc stuff all in 21 */
AVRTEST_DEF_SYSCALL2 (_21a, 21, unsigned char, 26, unsigned char, 24)
static AT_INLINE void avrtest_misc_flmap (unsigned char _flmap)
{
    avrtest_syscall_21a (AVRTEST_MISC_flmap, _flmap);
}

AVRTEST_DEF_SYSCALL2_1m (_21_u32,21, __UINT32_TYPE__,22, __UINT32_TYPE__,22, __UINT32_TYPE__,18)
AVRTEST_DEF_SYSCALL2_1m (_21_s32,21, __INT32_TYPE__,22, __INT32_TYPE__,22, __INT32_TYPE__,18)
#define AVRTEST_DEFF(OP)                                                \
static AT_INLINE __UINT32_TYPE__                                        \
avrtest_##OP##u32 (__UINT32_TYPE__ _x, __UINT32_TYPE__ _y)              \
{                                                                       \
    return avrtest_syscall_21_u32 (AVRTEST_MISC_##OP##u32, _x, _y);     \
}                                                                       \
static AT_INLINE __INT32_TYPE__                                         \
avrtest_##OP##s32 (__INT32_TYPE__ _x, __INT32_TYPE__ _y)                \
{                                                                       \
    return avrtest_syscall_21_s32 (AVRTEST_MISC_##OP##s32, _x, _y);     \
}
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(mod)
#undef AVRTEST_DEFF

#ifndef __AVR_TINY__
AVRTEST_DEF_SYSCALL2_1m (_21_u64,21, __UINT64_TYPE__,18, __UINT64_TYPE__,18, __UINT64_TYPE__,10)
AVRTEST_DEF_SYSCALL2_1m (_21_s64,21, __INT64_TYPE__,18, __INT64_TYPE__,18, __INT64_TYPE__,10)
#define AVRTEST_DEFF(OP)                                                \
static AT_INLINE __UINT64_TYPE__                                        \
avrtest_##OP##u64 (__UINT64_TYPE__ _x, __UINT64_TYPE__ _y)              \
{                                                                       \
    return avrtest_syscall_21_u64 (AVRTEST_MISC_##OP##u64, _x, _y);     \
}                                                                       \
static AT_INLINE __INT64_TYPE__                                         \
avrtest_##OP##s64 (__INT64_TYPE__ _x, __INT64_TYPE__ _y)                \
{                                                                       \
    return avrtest_syscall_21_s64 (AVRTEST_MISC_##OP##s64, _x, _y);     \
}
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(mod)
#undef AVRTEST_DEFF
#endif /* AVR TINY */

#ifdef _AVRGCC_STDFIX_H
AVRTEST_DEF_SYSCALL1_1m (_21_ktof,21,  float,22, _Accum,22)
AVRTEST_DEF_SYSCALL1_1m (_21_uktof,21, float,22, unsigned _Accum,22)
AVRTEST_DEF_SYSCALL1_1m (_21_rtof,21,  float,22, _Fract,24)
AVRTEST_DEF_SYSCALL1_1m (_21_urtof,21, float,22, unsigned _Fract,24)
AVRTEST_DEF_SYSCALL1_1m (_21_hktof,21,  float,22, short _Accum,24)
AVRTEST_DEF_SYSCALL1_1m (_21_uhktof,21, float,22, unsigned short _Accum,24)
AVRTEST_DEF_SYSCALL1_1m (_21_hrtof,21,  float,22, short _Fract,24)
AVRTEST_DEF_SYSCALL1_1m (_21_uhrtof,21, float,22, unsigned short _Fract,24)

AVRTEST_DEF_SYSCALL1_1m (_21_ftok,21,   _Accum,22,          float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftouk,21,  unsigned _Accum,22, float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftor,21,   _Fract,24,          float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftour,21,  unsigned _Fract,24, float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftohk,21,  short _Accum,24,    float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftouhk,21, unsigned short _Accum,24, float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftohr,21,  short _Fract,24,          float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ftouhr,21, unsigned short _Fract,24, float,22)
#define AVRTEST_DEFF(ID, T, R)                                          \
static AT_INLINE float avrtest_##ID##tof (T _x)                         \
{                                                                       \
    return avrtest_syscall_21_##ID##tof (AVRTEST_MISC_##ID##tof, _x);   \
}                                                                       \
static AT_INLINE T avrtest_fto##ID (float _x)                           \
{                                                                       \
    return avrtest_syscall_21_fto##ID (AVRTEST_MISC_fto##ID, _x);       \
}
#else
AVRTEST_DEF_SYSCALL1 (_21_nofxtof,21, unsigned char,26)
#define AVRTEST_DEFF(ID, T, R)                                          \
static AT_INLINE float avrtest_##ID##tof (int _x)                       \
{                                                                       \
    (void) _x;                                                          \
    avrtest_syscall_21_nofxtof (AVRTEST_MISC_nofxtof);                  \
    return 0.0f;                                                        \
}                                                                       \
static AT_INLINE int avrtest_fto##ID (float _x)                         \
{                                                                       \
    (void) _x;                                                          \
    avrtest_syscall_21_nofxtof (AVRTEST_MISC_nofxtof);                  \
    return 0;                                                           \
}
#endif /* Require <stdfix.h> */
AVRTEST_DEFF(k, _Accum, 22) AVRTEST_DEFF(uk, unsigned _Accum, 22)
AVRTEST_DEFF(r, _Fract, 24) AVRTEST_DEFF(ur, unsigned _Fract, 24)
AVRTEST_DEFF(hk, short _Accum, 24) AVRTEST_DEFF(uhk, unsigned short _Accum, 24)
AVRTEST_DEFF(hr, short _Fract, 24) AVRTEST_DEFF(uhr, unsigned short _Fract, 24)
#undef AVRTEST_DEFF


AVRTEST_DEF_SYSCALL1_1m (_22_u32to,22, float,22, __UINT32_TYPE__,22)
static AT_INLINE float avrtest_utof (__UINT32_TYPE__ _u)
{
  return avrtest_syscall_22_u32to (AVRTEST_u32to, _u);
}

AVRTEST_DEF_SYSCALL1_1m (_22_s32to,22, float,22, __INT32_TYPE__,22)
static AT_INLINE float avrtest_stof (__INT32_TYPE__ _s)
{
  return avrtest_syscall_22_s32to (AVRTEST_s32to, _s);
}

AVRTEST_DEF_SYSCALL2_1m (_22_cmp,22, __INT8_TYPE__,24, float,22, float,18)
static AT_INLINE __INT8_TYPE__ avrtest_cmpf (float _x, float _y)
{
  return avrtest_syscall_22_cmp (AVRTEST_cmp, _x, _y);
}

/* Emulating IEEE single functions */
AVRTEST_DEF_SYSCALL2_1 (_22, 22, float, 22, unsigned char, 26)
AVRTEST_DEF_SYSCALL3_1 (_22_2, 22, float, 22, float, 18, unsigned char, 26)
AVRTEST_DEF_SYSCALL2_1m(_22_2fi,22,  float,22, float,22, int,20)
AVRTEST_DEF_SYSCALL2_1M(_22_2fpi,22, float,22, float,22, int*,20)
AVRTEST_DEF_SYSCALL2_1M(_22_2fpf,22, float,22, float,22, float*,20)
AVRTEST_DEF_SYSCALL2_1M(_22_strto,22, float,22, const char*,24, char**,22)

#define AVRTEST_DEFF(ID)                          \
  static AT_INLINE float                          \
  avrtest_##ID##f (float _x)                      \
  {                                               \
    return avrtest_syscall_22 (_x, AVRTEST_##ID); \
  }
AVRTEST_DEFF(sin) AVRTEST_DEFF(asin) AVRTEST_DEFF(sinh) AVRTEST_DEFF(asinh)
AVRTEST_DEFF(cos) AVRTEST_DEFF(acos) AVRTEST_DEFF(cosh) AVRTEST_DEFF(acosh)
AVRTEST_DEFF(tan) AVRTEST_DEFF(atan) AVRTEST_DEFF(tanh) AVRTEST_DEFF(atanh)
AVRTEST_DEFF(exp) AVRTEST_DEFF(log)  AVRTEST_DEFF(sqrt) AVRTEST_DEFF(cbrt)
AVRTEST_DEFF(trunc) AVRTEST_DEFF(ceil)  AVRTEST_DEFF(floor) AVRTEST_DEFF(round)
AVRTEST_DEFF(log2) AVRTEST_DEFF(log10) AVRTEST_DEFF(fabs)
#undef AVRTEST_DEFF

#define AVRTEST_DEFF(ID)                                \
  static AT_INLINE float                                \
  avrtest_##ID##f (float _x, float _y)                  \
  {                                                     \
    return avrtest_syscall_22_2 (_x, _y, AVRTEST_##ID); \
  }
AVRTEST_DEFF(pow)  AVRTEST_DEFF(atan2) AVRTEST_DEFF(hypot) AVRTEST_DEFF(fdim)
AVRTEST_DEFF(fmin) AVRTEST_DEFF(fmax)  AVRTEST_DEFF(fmod)
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(add) AVRTEST_DEFF(sub)
AVRTEST_DEFF(ulp) AVRTEST_DEFF(prand)
#undef AVRTEST_DEFF

static AT_INLINE float
avrtest_strtof (const char *_x, char **_y)
{
  return avrtest_syscall_22_strto (AVRTEST_strto, _x, _y);
}
static AT_INLINE float
avrtest_ldexpf (float _x, int _y)
{
  return avrtest_syscall_22_2fi (AVRTEST_ldexp, _x, _y);
}
static AT_INLINE float
avrtest_powif (float _x, int _y)
{
  return avrtest_syscall_22_2fi (AVRTEST_powi, _x, _y);
}
static AT_INLINE float
avrtest_frexpf (float _x, int *_y)
{
  return avrtest_syscall_22_2fpi (AVRTEST_frexp, _x, _y);
}
static AT_INLINE float
avrtest_modff (float _x, float *_y)
{
  return avrtest_syscall_22_2fpf (AVRTEST_modf, _x, _y);
}

/* Emulating IEEE double functions */
#ifndef __AVR_TINY__
#if __SIZEOF_LONG_LONG__ == 8
AVRTEST_DEF_SYSCALL2_1 (_23_u64, 23, __UINT64_TYPE__, 18, unsigned char, 26)
AVRTEST_DEF_SYSCALL3_1 (_23_u64_2, 23, __UINT64_TYPE__, 18,
                        __UINT64_TYPE__, 10, unsigned char, 26)
AVRTEST_DEF_SYSCALL2_1m (_23_u64_2di,23, __UINT64_TYPE__,18, __UINT64_TYPE__,18, int,16)
AVRTEST_DEF_SYSCALL2_1M (_23_u64_2dpi,23, __UINT64_TYPE__,18, __UINT64_TYPE__,18, int*,16)
AVRTEST_DEF_SYSCALL2_1M (_23_u64_2dpd,23, __UINT64_TYPE__,18, __UINT64_TYPE__,18, __UINT64_TYPE__*,16)

#define AVRTEST_DEFF(ID)                                \
  static AT_INLINE __UINT64_TYPE__                      \
  avrtest_##ID##_d64 (__UINT64_TYPE__ _x)               \
  {                                                     \
    return avrtest_syscall_23_u64 (_x, AVRTEST_##ID);   \
  }
AVRTEST_DEFF(sin) AVRTEST_DEFF(asin) AVRTEST_DEFF(sinh) AVRTEST_DEFF(asinh)
AVRTEST_DEFF(cos) AVRTEST_DEFF(acos) AVRTEST_DEFF(cosh) AVRTEST_DEFF(acosh)
AVRTEST_DEFF(tan) AVRTEST_DEFF(atan) AVRTEST_DEFF(tanh) AVRTEST_DEFF(atanh)
AVRTEST_DEFF(exp) AVRTEST_DEFF(log)  AVRTEST_DEFF(sqrt) AVRTEST_DEFF(cbrt)
AVRTEST_DEFF(trunc) AVRTEST_DEFF(ceil)  AVRTEST_DEFF(floor) AVRTEST_DEFF(round)
AVRTEST_DEFF(log2) AVRTEST_DEFF(log10) AVRTEST_DEFF(fabs)
#undef AVRTEST_DEFF

#define AVRTEST_DEFF(ID)                                                \
  static AT_INLINE __UINT64_TYPE__                                      \
  avrtest_##ID##_d64 (__UINT64_TYPE__ _x, __UINT64_TYPE__ _y)           \
  {                                                                     \
    return avrtest_syscall_23_u64_2 (_x, _y, AVRTEST_##ID);             \
  }
AVRTEST_DEFF(pow)  AVRTEST_DEFF(atan2) AVRTEST_DEFF(hypot) AVRTEST_DEFF(fdim)
AVRTEST_DEFF(fmin) AVRTEST_DEFF(fmax)  AVRTEST_DEFF(fmod)
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(add) AVRTEST_DEFF(sub)
AVRTEST_DEFF(ulp) AVRTEST_DEFF(prand)
#undef AVRTEST_DEFF

static AT_INLINE __UINT64_TYPE__
avrtest_ldexp_d64 (__UINT64_TYPE__ _x, int _y)
{
  return avrtest_syscall_23_u64_2di (AVRTEST_ldexp, _x, _y);
}
static AT_INLINE __UINT64_TYPE__
avrtest_powi_d64 (__UINT64_TYPE__ _x, int _y)
{
  return avrtest_syscall_23_u64_2di (AVRTEST_powi, _x, _y);
}
static AT_INLINE __UINT64_TYPE__
avrtest_frexp_d64 (__UINT64_TYPE__ _x, int *_y)
{
  return avrtest_syscall_23_u64_2dpi (AVRTEST_frexp, _x, _y);
}
static AT_INLINE __UINT64_TYPE__
avrtest_modf_d64 (__UINT64_TYPE__ _x, __UINT64_TYPE__ *_y)
{
  return avrtest_syscall_23_u64_2dpd (AVRTEST_modf, _x, _y);
}

#endif /* Have uint64_t */

#if __SIZEOF_LONG_DOUBLE__ == 8

AVRTEST_DEF_SYSCALL2_1m (_23_cmp,23, __INT8_TYPE__,24, long double,18, long double,10)
static AT_INLINE __INT8_TYPE__ avrtest_cmpl (long double _x, long double _y)
{
  return avrtest_syscall_23_cmp (AVRTEST_cmp, _x, _y);
}

AVRTEST_DEF_SYSCALL2_1 (_23, 23, long double, 18, unsigned char, 26)
AVRTEST_DEF_SYSCALL3_1 (_23_2, 23, long double, 18, long double, 10,
                        unsigned char, 26)
AVRTEST_DEF_SYSCALL2_1m(_23_2di, 23, long double,18, long double,18, int,16)
AVRTEST_DEF_SYSCALL2_1M(_23_2lpi,23, long double,18, long double,18, int*,16)
AVRTEST_DEF_SYSCALL2_1M(_23_2lpl,23, long double,18, long double,18, long double*,16)
AVRTEST_DEF_SYSCALL2_1M(_23_strto,23, long double,18, const char*,24, char**,22)

#define AVRTEST_DEFF(ID)                                \
  static AT_INLINE long double                          \
  avrtest_##ID##l (long double _x)                      \
  {                                                     \
    return avrtest_syscall_23 (_x, AVRTEST_##ID);       \
  }
AVRTEST_DEFF(sin) AVRTEST_DEFF(asin) AVRTEST_DEFF(sinh) AVRTEST_DEFF(asinh)
AVRTEST_DEFF(cos) AVRTEST_DEFF(acos) AVRTEST_DEFF(cosh) AVRTEST_DEFF(acosh)
AVRTEST_DEFF(tan) AVRTEST_DEFF(atan) AVRTEST_DEFF(tanh) AVRTEST_DEFF(atanh)
AVRTEST_DEFF(exp) AVRTEST_DEFF(log)  AVRTEST_DEFF(sqrt) AVRTEST_DEFF(cbrt)
AVRTEST_DEFF(trunc) AVRTEST_DEFF(ceil)  AVRTEST_DEFF(floor) AVRTEST_DEFF(round)
AVRTEST_DEFF(log2) AVRTEST_DEFF(log10) AVRTEST_DEFF(fabs)
#undef AVRTEST_DEFF

#define AVRTEST_DEFF(ID)                                \
  static AT_INLINE long double                          \
  avrtest_##ID##l (long double _x, long double _y)      \
  {                                                     \
    return avrtest_syscall_23_2 (_x, _y, AVRTEST_##ID); \
  }
AVRTEST_DEFF(pow)  AVRTEST_DEFF(atan2) AVRTEST_DEFF(hypot) AVRTEST_DEFF(fdim)
AVRTEST_DEFF(fmin) AVRTEST_DEFF(fmax)  AVRTEST_DEFF(fmod)
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(add) AVRTEST_DEFF(sub)
AVRTEST_DEFF(ulp) AVRTEST_DEFF(prand)
#undef AVRTEST_DEFF

static AT_INLINE long double
avrtest_ldexpl (long double _x, int _y)
{
  return avrtest_syscall_23_2di (AVRTEST_ldexp, _x, _y);
}
static AT_INLINE long double
avrtest_powil (long double _x, int _y)
{
  return avrtest_syscall_23_2di (AVRTEST_powi, _x, _y);
}
static AT_INLINE long double
avrtest_frexpl (long double _x, int *_y)
{
  return avrtest_syscall_23_2lpi (AVRTEST_frexp, _x, _y);
}
static AT_INLINE long double
avrtest_modfl (long double _x, long double *_y)
{
  return avrtest_syscall_23_2lpl (AVRTEST_modf, _x, _y);
}
static AT_INLINE long double
avrtest_strtold (char *_x, char** _y)
{
  return avrtest_syscall_23_strto (AVRTEST_strto, _x, _y);
}

AVRTEST_DEF_SYSCALL1_1m (_21_ftol,21, long double,18, float,22)
AVRTEST_DEF_SYSCALL1_1m (_21_ltof,21, float,22, long double,18)
static AT_INLINE long double avrtest_ftol (float _x)
{
  return avrtest_syscall_21_ftol (AVRTEST_MISC_ftol, _x);
}
static AT_INLINE float avrtest_ltof (long double _x)
{
  return avrtest_syscall_21_ltof (AVRTEST_MISC_ltof, _x);
}

#else /* long double = 4 */
#define avrtest_sinl   avrtest_sinf
#define avrtest_asinl  avrtest_asinf
#define avrtest_sinhl  avrtest_sinhf
#define avrtest_asinhl avrtest_asinhf
#define avrtest_cosl   avrtest_cosf
#define avrtest_acosl  avrtest_acosf
#define avrtest_coshl  avrtest_coshf
#define avrtest_acoshl avrtest_acoshf
#define avrtest_tanl   avrtest_tanf
#define avrtest_atanl  avrtest_atanf
#define avrtest_tanhl  avrtest_tanhf
#define avrtest_atanhl avrtest_atanhf
#define avrtest_expl   avrtest_expf
#define avrtest_logl   avrtest_logf
#define avrtest_sqrtl  avrtest_sqrtf
#define avrtest_cbrtl  avrtest_cbrtf
#define avrtest_truncl avrtest_truncf
#define avrtest_ceill  avrtest_ceilf
#define avrtest_floorl avrtest_floorf
#define avrtest_roundl avrtest_roundf
#define avrtest_log2l  avrtest_log2f
#define avrtest_log10l avrtest_log10f
#define avrtest_fabsl  avrtest_fabsf

#define avrtest_powl   avrtest_powf
#define avrtest_atan2l avrtest_atan2f
#define avrtest_hypotl avrtest_hypotf
#define avrtest_fdiml  avrtest_fdimf
#define avrtest_fminl  avrtest_fminf
#define avrtest_fmaxl  avrtest_fmaxf
#define avrtest_fmodl  avrtest_fmodf
#define avrtest_cmpl   avrtest_cmpf
#define avrtest_mull   avrtest_mulf
#define avrtest_divl   avrtest_divf
#define avrtest_addl   avrtest_addf
#define avrtest_subl   avrtest_subf
#define avrtest_ulpl   avrtest_ulpf
#define avrtest_frexpl avrtest_frexpf
#define avrtest_ldexpl avrtest_ldexpf
#define avrtest_powil  avrtest_powif
#define avrtest_prandl avrtest_prandf
#define avrtest_strtold avrtest_strtof
static AT_INLINE long double avrtest_modfl (long double x, long double *y) { return avrtest_modff (x, (float*) y); }
static AT_INLINE long double avrtest_ftol (float x) { return (long double) x; }
static AT_INLINE float avrtest_ltof (long double x) { return (float) x; }

#endif /* long double = 8 */
#endif /* !__AVR_TINY__ */

#undef AVRTEST_DEF_SYSCALL0
#undef AVRTEST_DEF_SYSCALL1
#undef AVRTEST_DEF_SYSCALL2
#undef AVRTEST_DEF_SYSCALL1_0
#undef AVRTEST_DEF_SYSCALL1_1
#undef AVRTEST_DEF_SYSCALL2_1
#undef AVRTEST_DEF_SYSCALL3_1

static AT_INLINE void
avrtest_abort (void)
{
  avrtest_syscall_31 ();
}

static AT_INLINE void
avrtest_exit (int _status)
{
  avrtest_syscall_30 ((__INT16_TYPE__) _status);
}

static AT_INLINE void
avrtest_putchar (int _c)
{
  avrtest_syscall_29 ((char) _c);
}

static AT_INLINE void
avrtest_putchar_stderr (int _c)
{
  avrtest_syscall_24 ((char) _c);
}

static AT_INLINE int
avrtest_getchar (void)
{
  return avrtest_syscall_28 ();
}

static AT_INLINE void
avrtest_abort_2nd_hit (void)
{
  avrtest_syscall_25 ();
}


static AT_INLINE __UINT32_TYPE__
avrtest_fileio_p (unsigned char _what, const void *_pargs)
{
  return avrtest_syscall_26_p (_what, _pargs);
}

static AT_INLINE __UINT32_TYPE__
avrtest_fileio_1 (unsigned char _what, __UINT8_TYPE__ _args)
{
  return avrtest_syscall_26_1 (_what, _args);
}

static AT_INLINE __UINT32_TYPE__
avrtest_fileio_2 (unsigned char _what, __UINT16_TYPE__ _args)
{
  return avrtest_syscall_26_2 (_what, _args);
}

static AT_INLINE __UINT32_TYPE__
avrtest_fileio_4 (unsigned char _what, __UINT32_TYPE__ _args)
{
  return avrtest_syscall_26_4 (_what, _args);
}

static AT_INLINE __UINT32_TYPE__
avrtest_cycles (void)
{
  return avrtest_syscall_4_g (TICKS_GET_CYCLES_CMD);
}

static AT_INLINE __UINT32_TYPE__
avrtest_insns (void)
{
  return avrtest_syscall_4_g (TICKS_GET_INSNS_CMD);
}

static AT_INLINE __UINT32_TYPE__
avrtest_rand (void)
{
  return avrtest_syscall_4_g (TICKS_GET_RAND_CMD);
}

static AT_INLINE __UINT32_TYPE__
avrtest_prand (void)
{
  return avrtest_syscall_4_g (TICKS_GET_PRAND_CMD);
}

static AT_INLINE void
avrtest_cycles_call (void)
{
  avrtest_syscall_4_r (TICKS_CYCLES_CALL_CMD);
}

static AT_INLINE void
avrtest_reset_cycles (void)
{
  avrtest_syscall_4_r (TICKS_RESET_CYCLES_CMD);
}

static AT_INLINE void
avrtest_reset_insns (void)
{
  avrtest_syscall_4_r (TICKS_RESET_INSNS_CMD);
}

static AT_INLINE void
avrtest_reset_prand (void)
{
  avrtest_syscall_4_r (TICKS_RESET_PRAND_CMD);
}

static AT_INLINE void
avrtest_reset_all (void)
{
  avrtest_syscall_4_r (TICKS_RESET_ALL_CMD);
}

#undef AT_INLINE

#endif /* IN_AVRTEST */

#else /* ASSEMBLER */

.macro avrtest_syscall _sysno
    cpse r\_sysno, r\_sysno
    .word AVRTEST_INVALID_OPCODE
.endm

#define LOG_OFF        avrtest_syscall 0
#define LOG_ON         avrtest_syscall 1
#define LOG_PUSH_OFF   avrtest_syscall 9
#define LOG_PUSH_ON    avrtest_syscall 10
#define LOG_POP        avrtest_syscall 11

#define AVRTEST_ABORT  avrtest_syscall 31
#define AVRTEST_EXIT   avrtest_syscall 30
#define AVRTEST_ABORT_2ND_HIT avrtest_syscall 25
#define AVRTEST_PUTCHAR       avrtest_syscall 29

#endif /* ASSEMBLER */
#endif /* AVRTEST_H */
