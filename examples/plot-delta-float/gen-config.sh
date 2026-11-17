#!/usr/bin/env bash

# $1 = func
# $2 = OUT (rel, abs, ulp)

macro="FLOAT_SIZE"

echo "#define FUNC $1"
echo "#define S_FUNC \"$(sed 's:_:\\\\\\\\_:g' <<< $1)\""

case "$1" in
    *f)
	echo "#define AFUNC avrtest_${1::-1}l"
	echo "#define $macro __SIZEOF_FLOAT__"
	;;
    *l)
	echo "#define AFUNC avrtest_${1::-1}l"
	echo "#define $macro __SIZEOF_LONG_DOUBLE__"
	;;
    fp64_*)
	cat <<EOF
#define AFUNC avrtest_${1:5}l
long double $1 (long double);
#define $macro __SIZEOF_LONG_DOUBLE__
#if $macro != 8
#error "$1 from fp64lib requires 64-bit long double"
#endif
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
    cyc)
	echo "#define OUT_CYC"
	;;
    *)
	echo "$(basename $0): error: unknown OUT: $2 (rel, abs, ulp, fun, cyc)" >&2
	exit 1
	;;
esac
