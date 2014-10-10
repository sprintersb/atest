#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([avrtest AVR core simulator], [1.0],
        [You have the free sources.  Add extensions as you like it.],
	[avrtest],
        [http://sourceforge.net/p/winavr/code/HEAD/tree/trunk/avrtest])

AC_CONFIG_SRCDIR([avrtest.h])
AC_CONFIG_HEADERS([config.h])

# Find the build and host systems.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PROG_INSTALL

AC_ARG_PROGRAM

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC_C99
#ACX_PROG_STRIP
#ACX_PROG_AVRGCC


#if test $host != $build; then
#   AC_CHECK_PROGS(BUILD_CC)
#, gcc, cc)
#fi




# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h limits.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_C_INLINE
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday strtoul])
AC_CHECK_FUNCS([memset sqrt strrchr])

# autoconf is wrong with sqrt gcc:
# AC_CHECK_FUNCS([sqrt])

# Dont' use AC_FUNC_MALLOC as this will be detected not correctly
# AC_FUNC_MALLOC

# Checks special compiler features
AC_LANG(C)

atest_DEFINE_IF_ATTRIBUTE([ALWAYS_INLINE], [always_inline],,inline)
atest_DEFINE_IF_ATTRIBUTE([HOT], [hot],,)
atest_DEFINE_IF_ATTRIBUTE([COLD], [cold],,)
atest_DEFINE_IF_ATTRIBUTE([FASTCALL], [fastcall],,)
atest_DEFINE_IF_ATTRIBUTE([NOINLINE], [noinline],,)
atest_DEFINE_IF_ATTRIBUTE([NORETURN], [noreturn],,)


AC_MSG_CHECKING(whether the host system is little endian)
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[
	int a[__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__?1:-1];]])],
    	[atest_cv_end_little=yes], [atest_cv_end_little=unknown])
if test x$atest_cv_end_little = xyes; then
   AC_DEFINE(ENDIAN_LITTLE, ,
   [Define to 1 if your host system is little endian])
fi
AC_MSG_RESULT($atest_cv_end_little)


AC_MSG_CHECKING(whether the host system's float is little endian)
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[
	int a[__FLOAT_WORD_ORDER__==__ORDER_LITTLE_ENDIAN__?1:-1];]])],
    	[atest_cv_end_little_f=yes], [atest_cv_end_little_f=unknown])
if test x$atest_cv_end_little_f = xyes; then
   AC_DEFINE(ENDIAN_FLOAT_LITTLE, ,
   [Define to 1 if your host system's float is little endian])
fi
AC_MSG_RESULT($atest_cv_end_little_f)

#AC_CONFIG_FILES(foo-config.h:foo-config.h.in,)

AC_MSG_NOTICE([])
AC_MSG_NOTICE([checking whether the native build compiler works])
AC_MSG_NOTICE([])

CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD- -O}

AC_PROG_CC_FOR_BUILD([[// code]], CC_FOR_BUILD_OK, dummy)

AC_SUBST(CC_FOR_BUILD)

#####################

AC_MSG_NOTICE([])
AC_MSG_NOTICE([checking whether the AVR compiler works])
AC_MSG_NOTICE([])

CC_FOR_AVR=${CC_FOR_AVR-avr-gcc}
#AC_ARG_VAR ([[CC_FOR_AVR]], [[compile programs for Atmel AVR microcontrollers]])
AC_CHECK_PROG(CC_FOR_AVR, [avr-gcc], [avr-gcc], [no], [avr-gcc])
if test x$CC_FOR_AVR = xno; then
   AC_MSG_WARN([[avr-gcc not found, skip building exit-*.o]])
fi
AC_SUBST(CC_FOR_AVR)

AC_MSG_CHECKING(whether $CC_FOR_AVR can compile C programs for AVR)

atest_save([ac_ext], [[c]])
atest_save([ac_exeext], [[.elf]])
atest_save([ac_obj], [[o]])

atest_cflags_avr="-O1 -mmcu=atmega8"
atest_compile_avr="$CC_FOR_AVR $atest_cflags_avr -c conftest.$ac_ext >&5"
atest_link_avr="$CC_FOR_AVR $atest_cflags_avr conftest.$ac_ext -o conftest$ac_exeext >&5"

atest_save([ac_compile], [[$atest_compile_avr]])
atest_save([ac_link], [[$atest_link_avr]])

   AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[#include <avr/io.h>
			   static void __attribute__((__constructor__,__used__))
			   init (void) {} ]])],
	[atest_cv_avrgcc_ok=yes], [atest_cv_avrgcc_ok=no])

AC_MSG_RESULT($atest_cv_avrgcc_ok)

AM_CONDITIONAL([BUILD_EXIT_O], [test x$atest_cv_avrgcc_ok = xyes])

if test x$atest_cv_avrgcc_ok = xno; then
   AC_MSG_WARN([[no working AVR compiler found, skip building exit-*.o]])
fi

atest_cflags_avr="-atxmega128a3"
atest_compile_avr="$CC_FOR_AVR $atest_cflags_avr -c conftest.$ac_ext >&5"
atest_link_avr="$CC_FOR_AVR $atest_cflags_avr conftest.$ac_ext -o conftest$ac_exeext >&5"

if test x$atest_cv_avrgcc_ok = xyes; then
   AC_MSG_CHECKING(for xmega support)
   AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[]])],
	    [atest_cv_avrgcc_xmega_ok=yes], [atest_cv_avrgcc_xmega_ok=no])
   AC_MSG_RESULT($atest_cv_avrgcc_xmega_ok)
fi # xmega

AM_CONDITIONAL([BUILD_XMEGA], [test x$atest_cv_avrgcc_xmega_ok = xyes])

atest_restore([ac_link])
atest_restore([ac_compile])
atest_restore([ac_objext])
atest_restore([ac_exeext])
atest_restore([ac_ext])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
