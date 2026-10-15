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

# Use
#
#     EXTRA_CFLAGS="..." ./run-logging.sh ...
#
# in order to add additional CFLAGS.
# In order to replace the CFLAGS below entirely, use
#
#     CFLAGS="..." ./run-logging.sh ...
#
# In order to use a compiler other than avr-gcc, use
#
#     CC=my-compiler ./run-logging.sh ...
#
# In order to pass additional arguments to the avrtest executable, use
#
#     AARGS='...' ./run-logging.sh ...


set -e

myname=$(basename $0)

: ${CC:=avr-gcc}

AVRTEST_HOME=..

FLAG_STOP=			# Stop at any error
FLAG_LOG=			# Show expected (at:filter'ed) logs
FLAG_KEEP=			# Keep .out files

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
  -k          Keep the log files
              0.log = avrtest_log output
              1.log = at:filter'ed output
              2.log = diff between expectation and 1.log
  -l          Show the (at:filter'ed) avrtest_log outputs
  -v          Verbose mode; echo shell commands being executed
If FILE is not specified, logging/*.[cS] is used.
EOF
}

OPTS="shklv"

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
	k)	FLAG_KEEP=1 ;;
	l)	FLAG_LOG=1 ;;
	v)	FLAG_VERB=1; set -x ;  ;;
	*)	Errx "Invalid option(s). Try '-h' for more info."
    esac
done
shift $((OPTIND - 1))

test_list=${*:-"logging/*.[cS]"}

CPPFLAGS="-I.."
# -Wno-array-bounds: Ditch wrong warnings due to avr-gcc PR105523.
# This works with more GCC versions than --param=min-pagesize=0.
CFLAGS=${CFLAGS-"-W -Wall -pipe ${EXTRA_CFLAGS}"}

Err_echo ()
{
    echo "*** $myname: $*"
    if [ $FLAG_STOP ] ; then
	Errx "Stop"
    fi
}


# $1 = ELF file
# $2 = mcu as understood by avr-gcc
# $3 = avrtest output
Simulate_avrtest ()
{
    msg=$($(${AVRTEST_HOME}/avrtest-elf $1 _log) \
	      -q -no-log -no-stdin $1 -m 6000 $AARGS 2>&1 > $3)
    RETVAL=$?
    #echo "MSG = $msg"
    [ $RETVAL -eq 0 ]
}


# Usage: Compile SRCFILE MCU ELFILE
Compile ()
{
    $CC $CPPFLAGS $CFLAGS $flags -mmcu=$2 $1 -o $3
}

Extract_Directives ()
{
    filter=
    if filter=$(grep '.* at:filter { .* }.*' $1); then
	filter=$(sed 's/.* at:filter { \(.*\) }.*/\1/' <<< "$filter")
    fi
    flags=
    if flags=$(grep '.* at:flags { .* }.*' $1); then
	flags=$(sed 's/.* at:flags { \(.*\) }.*/\1/' <<< "$flags")
    fi
}

n_files=0	# number of operated files
n_emake=0	# number of compile/link errors
n_esimul=0	# number of simulation errors
n_ediff=0	# number of diff errors / compare .log with .1.out

for test_file in $test_list ; do

    case "$test_file" in
	-* ) Err_echo "error: $test_file option specified after the files"
	     exit 1
	     ;;
    esac

    Extract_Directives "$test_file"

    case "$test_file" in

	*.c | *.cpp | *.S | *.sx)
	    n_files=$(($n_files + 1))

	    echo -n "Simulate avrtest: $test_file "

	    root=$(basename $test_file | sed -e 's:\..*::g')
	    testbase=$(sed -e 's:\..*::g' <<< "$test_file")
	    testdir=$(dirname "$test_file")

	    log_files=$(find "$testdir" -name "$root"'-[a-z0-9]*.log')
	    if [ -z "$log_files" ]; then
		echo
		echo "Warning: no ${testbase}-<mcu>.log files"
	    fi

	    for log_file in $log_files; do
		#echo "log_file=$log_file"
		mcu=$(basename $log_file .log | sed 's:.*-\([a-z0-9]*\):\1:')
		#echo "log_file=$log_file mcu=$mcu"

		echo -n "$mcu ... "

		out0_file=$root.0.out # raw avrtest_log output
		out1_file=$root.1.out # at:filter'ed
		out2_file=$root.2.out # delta .log <-> .1.out
		elf_file=$root.elf

		if ! Compile $test_file $mcu $elf_file; then
		    Err_echo "compile failed"
		    n_emake=$(($n_emake + 1))
		    break
		elif ! Simulate_avrtest $elf_file $mcu $out0_file; then
		    Err_echo "simulate avrtest failed: $RETVAL"
		    n_esimul=$(($n_esimul + 1))
		else
		    if [ -n "$filter" ]; then
			cmd="cat $out0_file | $filter > $out1_file"
			eval "$cmd" || true
		    else
			cp $out0_file $out1_file
		    fi
		    # Allow sloppy logs with trailing spaces.  This is more
		    # convenient git-wise than diff --ignore-trailing-space.
		    sed -i 's:[ \t]*$::' "$out1_file"
		    if [ -n "$FLAG_LOG" ]; then
			echo ""
			echo "--------------------------------------"
			cat $out1_file
			echo "--------------------------------------"
		    fi
		    if ! diff -u $log_file $out1_file > $out2_file; then
			[[ -n $FLAG_VERB ]] && cat $out2_file
			Err_echo "diff failed"
			n_ediff=$(($n_ediff + 1))
		    else
			echo "OK"
		    fi
		fi
		[[ -z $FLAG_KEEP ]] && rm -f -- ./$root.[0-2].out
	    done
	    rm -f $elf_file
	    ;;

	*)
	    Errx "Unknown file type: $test_file"
    esac
done

echo "-------"
echo "Done.  Number of operated files: $n_files"

if [ $(($n_emake + $n_esimul + $n_ediff)) -gt 0 ] ; then
    [ $n_emake  -gt 0 ]  && echo "*** Compile/link errors: $n_emake"
    [ $n_esimul -gt 0 ]  && echo "*** Simulate errors:     $n_esimul"
    [ $n_ediff  -gt 0 ]  && echo "*** Diff errors:         $n_ediff"
    exit 1
else
    echo "Success."
fi

# eof
