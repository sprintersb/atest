#!/usr/bin/env bash

# $1 = func
# $2 = OUT (rel, abs, ulp)

macro="FLOAT_SIZE"

case "$1" in
    *f)
	echo "#define $macro __SIZEOF_FLOAT__"
	;;
    *l)
	echo "#define $macro __SIZEOF_LONG_DOUBLE__"
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
    *)
	echo "$(basename $0): error: unknown OUT: $2 (rel, abs, ulp)" >&2
	exit 1
	;;
esac
