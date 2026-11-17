#!/usr/bin/env bash

# $1 = func
# $2 = OUT (rel, abs, ulp, fun, cyc)

macro="FLOAT_SIZE"

echo "#define FUNC $1"
echo "#define S_FUNC \"$(sed 's:_:\\\\\\\\_:g' <<< $1)\""

case "$1" in
    *f)
	echo "#define AFUNC avrtest_${1::-1}_d64"
	echo "#define $macro __SIZEOF_FLOAT__"
	;;
    *l)
	echo "#define AFUNC avrtest_${1::-1}_d64"
	echo "#define $macro __SIZEOF_LONG_DOUBLE__"
	;;
    fp64_*)
	cat <<EOF
#define USE_FP64LIB
#define AFUNC avrtest_${1:5}_d64
LL $1 (LL);
#define $macro 8
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
