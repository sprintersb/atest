#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([avrtest AVR core simulator],[1.0],[You have the free sources.  Add extensions as you like it.],[avrtest],[http://sourceforge.net/p/winavr/code/HEAD/tree/trunk/avrtest])

AC_CONFIG_SRCDIR([avrtest.h])
AC_CONFIG_HEADERS([config.h])
dnl AC_CONFIG_MACRO_DIR(.)

# Find the build and host systems.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PROG_INSTALL

AC_ARG_PROGRAM

AM_INIT_AUTOMAKE

# Checks for programs.
#AC_REQUIRE(AC_PROG_CC)
AC_PROG_CC
ACX_PROG_STRIP
ACX_PROG_AVRGCC


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
AC_TYPE_UINT64_T
AC_C_INLINE
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday strtoul])
AC_CHECK_FUNCS([memset sqrt strrchr rand srand])

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

AC_SUBST(CC_FOR_BUILD)

#####################

AC_MSG_CHECKING([if target is avr])
case "$target" in
	avr* )
		AC_MSG_RESULT([yes])
		;;
	* )
		AC_MSG_RESULT(no)
		AC_MSG_WARN()
		AC_MSG_WARN(avrtest only supports target avr.)
		AC_MSG_WARN(Try configuring with:)
		AC_MSG_WARN("$0 ... --target=avr")
		AC_MSG_WARN()
		AC_MSG_ERROR(aborting configure)
		;;
esac

#####################

AC_MSG_NOTICE([])
AC_MSG_NOTICE([checking whether the AVR C compiler works])
AC_MSG_NOTICE([])

# For example, to use a freshly built, not yet installed avr-gcc
# where $TB is the built directory and $TI has AVR-LibC installed:
# configure ... CC_FOR_AVR="$TB/xgcc -B$TB $TI/avr/include"
CC_FOR_AVR=${CC_FOR_AVR-avr-gcc}

#AC_ARG_VAR ([[CC_FOR_AVR]], [[compile programs for Atmel AVR microcontrollers]])
AC_CHECK_PROG([CC_FOR_AVR], [avr-gcc], [avr-gcc], [no], [avr-gcc])
if test "x$CC_FOR_AVR" = "xno"; then
   AC_MSG_WARN([[avr-gcc not found, skip building AVR objects exit-*.o]])
fi
AC_SUBST([CC_FOR_AVR])

# Make sure that we found the right avr cross-compiler.

case "${CC_FOR_AVR}" in
   *gcc*)
       case "X`${CC_FOR_AVR} -dumpmachine`X" in
           XavrX) ;;
           *) AC_MSG_ERROR(Wrong AVR C-compiler found; check the PATH!) ;;
       esac
       ;;
   *) AC_MSG_ERROR(Wrong AVR C-compiler found; check the PATH!) ;;
esac

atest_save([ac_ext], [[c]])
atest_save([ac_exeext], [[.elf]])
atest_save([ac_obj], [[o]])

atest_cflags_avr="-O1 -mmcu=atmega8"
atest_compile_avr="$CC_FOR_AVR $atest_cflags_avr -c conftest.$ac_ext >&5"
atest_link_avr="$CC_FOR_AVR $atest_cflags_avr conftest.$ac_ext -o conftest$ac_exeext >&5"

atest_save([ac_compile], [[$atest_compile_avr]])
atest_save([ac_link], [[$atest_link_avr]])

exit_mcus="atmega8 atmega168 atmega128 atmega103 atmega2560 \
	   atxmega128a3 atxmega128a1 attiny40 attiny3216"

for mcu in $exit_mcus; do
  CHECK_AVR_DEVICE([$mcu],EXIT_MCUS)
done

AC_MSG_NOTICE([Suported AVR mcus:$EXIT_MCUS])

AC_SUBST(EXIT_MCUS)

dnl AC_MSG_CHECKING(whether $CC_FOR_AVR can compile C programs for AVR)

dnl atest_cflags_avr="-O1 -mmcu=atmega8"
dnl atest_compile_avr="$CC_FOR_AVR $atest_cflags_avr -c conftest.$ac_ext >&5"
dnl atest_link_avr="$CC_FOR_AVR $atest_cflags_avr conftest.$ac_ext -o conftest$ac_exeext >&5"

dnl    AC_COMPILE_IFELSE(
dnl 	[AC_LANG_PROGRAM([[#include <avr/io.h>
dnl 			   static void __attribute__((__constructor__,__used__))
dnl 			   init (void) {} ]])],
dnl 	[atest_cv_avrgcc_ok=yes], [atest_cv_avrgcc_ok=no])

dnl AC_MSG_RESULT($atest_cv_avrgcc_ok)

dnl AM_CONDITIONAL([BUILD_EXIT_O], [test x$atest_cv_avrgcc_ok = xyes])

dnl if test x$atest_cv_avrgcc_ok = xno; then
dnl    AC_MSG_WARN([[no working AVR compiler found, skip building exit-*.o]])
dnl fi

dnl atest_cflags_avr="-atxmega128a3"
dnl atest_compile_avr="$CC_FOR_AVR $atest_cflags_avr -c conftest.$ac_ext >&5"
dnl atest_link_avr="$CC_FOR_AVR $atest_cflags_avr conftest.$ac_ext -o conftest$ac_exeext >&5"

dnl if test x$atest_cv_avrgcc_ok = xyes; then
dnl    AC_MSG_CHECKING(for xmega support)
dnl    AC_COMPILE_IFELSE(
dnl 	[AC_LANG_PROGRAM([[]])],
dnl 	    [atest_cv_avrgcc_xmega_ok=yes], [atest_cv_avrgcc_xmega_ok=no])
dnl    AC_MSG_RESULT($atest_cv_avrgcc_xmega_ok)
dnl fi # xmega

dnl AM_CONDITIONAL([BUILD_XMEGA], [test x$atest_cv_avrgcc_xmega_ok = xyes])

atest_restore([ac_link])
atest_restore([ac_compile])
atest_restore([ac_objext])
atest_restore([ac_exeext])
atest_restore([ac_ext])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
