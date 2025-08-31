#! /bin/bash

# Copyright (c) 2007, Dmitry Xmelkov
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
# * Neither the name of the copyright holders nor the names of
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# $Id$

# Use
#
#     MCUS="..." ./run-avrtest.sh ...
#
# in order to override the predefined list of MCUs.
# Notice that this requires an  exit-<mcu>.o module  for each of the MCUs.
# When it is not present, you can generate it with, say
#
#    (cd $AVRTEST_HOME; make exit-<mcu>.o)
#
# Use
#
#     EXTRA_CFLAGS="..." ./run-avrtest.sh ...
#
# in order to add additional CFLAGS.
# In order to replace the CFLAGS below entirely, use
#
#     CFLAGS="..." ./run-avrtest.sh ...
#
# In order to use a compiler other than avr-gcc, use
#
#     CC=my-compiler ./run-avrtest.sh ...
#
# In order to pass additional arguments to the avrtest executable, use
#
#     AARGS='...' ./run-avrtest.sh ...


set -e

myname="$0"

: ${CC:=avr-gcc}
: ${avrtest:=avrtest}

AVRTEST_HOME=..

: ${MCU_LIST="atmega128 attiny3216 atmega2560 atxmega128a3 at90s8515 attiny40" }

FLAG_STOP=			# Stop at any error

Errx ()
{
    echo "$myname: $*"
    exit 1
}

Usage ()
{
    cat <<EOF
Usage: $1 [-shv] [FILE]...
Options:
  -s          Stop at any error, temporary files will save
  -h          Print this help
  -v          Verbose mode; echo shell commands being executed
If FILE is not specified, the full test list is used.
EOF
}

OPTS="shv"

# First option pass for -h only so that we get the defaults right when
# options like -a are specified.
while getopts $OPTS opt ; do
    case $opt in
	h)	Usage `basename $myname` ; exit 0 ;;
    esac
done

# Second option pass: When -h was not specified, do the work.
OPTIND=1
while getopts $OPTS opt ; do
    case $opt in
	s)	FLAG_STOP=1 ;;
	h)	Usage `basename $myname` ; exit 0 ;;
	v)	set -x ;;
	*)	Errx "Invalid option(s). Try '-h' for more info."
    esac
done
shift $((OPTIND - 1))

test_list=${*:-"arith/*.c compile/*.c sreg/*.c"}

CPPFLAGS="-Wundef -I.."
# -Wno-array-bounds: Ditch wrong warnings due to avr-gcc PR105523.
# This works with more GCC versions than --param=min-pagesize=0.
CFLAGS=${CFLAGS-"-W -Wall -pipe -Os ${EXTRA_CFLAGS}"}

Err_echo ()
{
    echo "*** $*"
    if [ $FLAG_STOP ] ; then
	Errx "Stop"
    fi
}

# Compose extra avr-gcc options for extended memory layout.
# $1 = 0: Stack is below static storage
# $1 = 1: Stack is at the end of static storage
# $2 = RAM start
# $3 = RAM end
o_mem ()
{
    local ramVMA=0x800000
    local ramSTART=$(printf "0x%x" $(($2 + ${ramVMA})))
    local ramEND=$(printf "0x%x" $(($3 + ${ramVMA})))
    local ramLEN=$(printf "0x%x" $((1 + $3 - $2)))
    local stack
    if [ $1 -eq 0 ] ; then
	# Stack is located below static storage.
	stack=$(printf "0x%x" $((${ramSTART} - 1 - ${ramVMA})))
    else
	# Stack is located at the end of static storage.
	stack=$(printf "0x%x" $((${ramEND} - ${ramVMA})))
    fi

    local ramstart="-Tdata=${ramSTART} \
		    -Wl,--defsym,__DATA_REGION_ORIGIN__=${ramSTART}"
    local ramlen="-Wl,--defsym,__DATA_REGION_LENGTH__=${ramLEN}"
    local stack="-Wl,--defsym,__stack=${stack}"
    local heap="-Wl,--defsym,__heap_end=${ramEND}"

    echo "${ramstart} ${ramlen} ${stack} ${heap}"
}

# $1 = MCU as understood by avr-gcc.
# o_gcc: Extra options for avr-gcc
# o_sim: Extra options for avrtext
set_extra_options ()
{
    # As AVRtest is just simulating cores, not exact hardware, we can
    # add more RAM at will.

    o_gcc= # Extra options for gcc
    o_sim= # Extra options for AVRtest.  Default mmcu=avr51

    case $1 in
	atmega128 | atmega103)
	    o_gcc="$(o_mem 0 0x2000 0xffff)"
	    ;;
	atmega2560)
	    o_sim="-mmcu=avr6"
	    o_gcc="$(o_mem 0 0x2000 0xffff)"
	    ;;
	attiny3216)
	    o_sim="-mmcu=avrxmega3"
	    o_gcc="$(o_mem 0 0x2000 0x7fff)"
	    ;;
	atxmega128a3)
	    o_sim="-mmcu=avrxmega6"
	    o_gcc="$(o_mem 0 0x2000 0xffff)"
	    ;;
	atxmega128a1)
	    o_sim="-mmcu=avrxmega7"
	    o_gcc="$(o_mem 0 0x2000 0xffff)"
	    ;;
	avr128da32)
	    o_sim="-mmcu=avrxmega4"
	    o_gcc="$(o_mem 0 0x1000 0x7fff)"
	    ;;
	at90s8515)
	    o_sim="-mmcu=avr2"
	    o_gcc="$(o_mem 0 0x2000 0xffff)"
	    ;;
	attiny40)
	    o_sim="-mmcu=avrtiny -s 8k"
	    o_gcc="$(o_mem 1 0x40 0x3fff) \
		   -Wl,--pmem-wrap-around=8k \
		   -Wl,--defsym=__TEXT_REGION_LENGTH__=8K"
	    ;;
	*)
	    echo "Define extra options for $1"
	    exit 1
	;;
    esac
    o_gcc="$o_gcc ${AVRTEST_HOME}/exit-$1.o"
}


# $1 = ELF file
# $2 = mcu as understood by avr-gcc
# Extra options for AVRtest are passed in o_sim.
Simulate_avrtest ()
{
    # The following exit stati will be returned with -q:
    # -  0  Everything went fine.
    # -  1  Target program called abort()
    # -  x  Target program called exit (x)
    # - 10  Program timeout as set by -m MAXCOUNT.
    # - 11  Something is wrong with the program file:  Does not fit into
    #       memory, not an AVR executable, ELF headers broken, ...
    # - 12  The program goes haywire:  Stack overflow, illegal instruction
    #       or PC.
    # - 13  Problem with symbol information like an odd function address.
    # - 14  Problem with using file I/O with the host's file system: Bad file
    #       handle or file name, illegal argument.  This does *not* encompass
    #       when fopen cannot open the file; this will just return a 0-handle.
    # - 20  Out of memory.
    # - 21  Wrong avrtest usage: Unknown options, etc.
    # - 22  Program file could not be found / read.
    # - 42  Fatal error in avrtest.

    # -no-stdin keeps AVRtest from hanging in rare situations of bogus
    # code that tries to read from stdin, but there is no input.

    # AVRtest has 3 flavours: avrtest, avrtest-xmega and avrtest-tiny.
    local suff=
    case "$o_sim" in
	*avrxmega* ) suff="-xmega" ;;
	*avrtiny*  ) suff="-tiny"  ;;
    esac

    msg=$(${AVRTEST_HOME}/${avrtest}${suff} \
			 -q -no-stdin $1 $o_sim -m 60000000000 $AARGS 2>&1)
    RETVAL=$?
    #echo "MSG = $msg"
    #echo " - $AVRTEST_HOME/$avrtest$suff -q $1 $o_sim -m 60000000000 $AARGS"
    [ $RETVAL -eq 0 ]
}


# Usage: Compile SRCFILE MCU ELFILE
Compile ()
{
    local flags=

    flags="-std=gnu99"

    $CC $CPPFLAGS $CFLAGS $flags -mmcu=$2 $1 $o_gcc -o $3
}


n_files=0	# number of operated files
n_emake=0	# number of compile/link errors
n_esimul=0	# number of simulation errors

for test_file in $test_list ; do
    case `basename $test_file` in

	*.c | *.cpp | *.cc | *.S | *.sx)
	    n_files=$(($n_files + 1))

	    rootname=$(basename $test_file | sed -e 's:\..*::g')

	    elf_file=$rootname.elf

	    for mcu in ${MCUS-${MCU_LIST}} ; do
		set_extra_options $mcu
		echo -n "Simulate avrtest: $test_file "
		echo -n "$mcu ... "
		if ! Compile $test_file $mcu $elf_file; 
		then
		    Err_echo "compile failed"
		    n_emake=$(($n_emake + 1))
		    break
		elif ! Simulate_avrtest $elf_file $mcu
		then
		    Err_echo "simulate avrtest failed: $RETVAL"
		    n_esimul=$(($n_esimul + 1))
		else
		    echo "OK"
		fi
	    done
	    rm -f $elf_file
	    ;;

	*)
	    Errx "Unknown file type: $test_file"
    esac
done

echo "-------"
echo "Done.  Number of operated files: $n_files"

if [ $(($n_emake + $n_ehost + $n_esimul)) -gt 0 ] ; then
    [ $n_emake -gt 0 ]   && echo "*** Compile/link errors: $n_emake"
    [ $n_esimul -gt 0 ]  && echo "*** Simulate errors:     $n_esimul"
    exit 1
else
    echo "Success."
fi

# eof
