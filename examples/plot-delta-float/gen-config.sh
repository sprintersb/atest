#!/usr/bin/env sh

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
