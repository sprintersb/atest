#!/usr/bin/env bash

# $1 = func
# $2 = OUT (rel, abs, ulp)

macro="FLOAT_SIZE"

case "$1" in
    *f)
	echo "#define $macro __SIZEOF_FLOAT__"
	echo "#define FUNC $1"
	echo "#define AFUNC avrtest_${1::-1}l"
	;;
    *l)
	echo "#define $macro __SIZEOF_LONG_DOUBLE__"
	echo "#define FUNC $1"
	echo "#define AFUNC avrtest_${1::-1}l"
	;;
    fp64_*)
	cat <<EOF
#define $macro __SIZEOF_LONG_DOUBLE__
#if $macro != 8
#error "$1 from fp64lib requires 64-bit long double"
#endif
#define FUNC $1
#define AFUNC avrtest_${1:5}l
long double $1 (long double);
EOF
	;;
    *)
	echo "$(basename $0): error: unknown FUNC: $1" >&2
	exit 1
	;;
esac

# Be case-insensitive below.
shopt -s nocasematch

case "$2" in
    rel*)
	echo "#define OUT_REL"
	;;
    abs*)
	echo "#define OUT_ABS"
	;;
    ulp)
	echo "#define OUT_ULP"
	;;
    fun)
	echo "#define OUT_FUN"
	;;
    *)
	echo "$(basename $0): error: unknown OUT: $2 (rel, abs, ulp, fun)" >&2
	exit 1
	;;
esac
