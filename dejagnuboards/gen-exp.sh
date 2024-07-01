#!/bin/sh

print_exp()
{
    mmcu="$1"
    avrtest_mmcu="$2"
    avrtest_opts="$7"
    extra_ldflags="$6"

    ramVMA=0x800000
    ramSTART=$(printf "0x%x" $(($4 + ${ramVMA})))
    ramEND=$(printf "0x%x" $(($5 + ${ramVMA})))
    ramLEN=$(printf "0x%x" $((1 + $5 - $4)))
    if test "$3" = "SPlow"; then
	# Stack is located below static storage.
	stack=$(printf "0x%x" $((${ramSTART} - 1 - ${ramVMA})))
    elif test "$3" = "SPhigh"; then
	# Stack is located at end of static storage.
	stack=$(printf "0x%x" $((${ramEND} - ${ramVMA})))
    else
	echo "error: unknown stack position"
	exit 1
    fi

    ramstart="-Tdata=${ramSTART} -Wl,--defsym,__DATA_REGION_ORIGIN__=${ramSTART}"
    ramlen="-Wl,--defsym,__DATA_REGION_LENGTH__=${ramLEN}"
    stack="-Wl,--defsym,__stack=${stack}"
    heap="-Wl,--defsym,__heap_end=${ramEND}"
    ld_ram="${ramstart} ${ramlen} ${stack} ${heap}"

cat > $1-sim.exp <<EOF
# Directory where to find the avrtest executable and the exit-*.o modules.
# "avrtest_dir" can be specified in .dejagnurc or be overriden below.

# set avrtest_dir "/home/pmarques"

# Used as -mmcu when compiling a C source with avr-gcc and to find the
# right exit*.o module.  If you need a specific exit-\${mmcu}.o module,
# then run:
#
# > cd \${avrtest_dir}
# > make exit-\${mmcu}.o

set mmcu "${mmcu}"

# MCU for avrtest.
# One of: "avr51", "avr6", "avrxmega6", "avrxmega3", "avrtiny".

set avrtest_mmcu "${avrtest_mmcu}"

# Extra options to pass to avrtest.

set avrtest_opts "${avrtest_opts}"

# "avrlibc_include_dir" will be used as search path for include
# files from AVR-LibC where stdio.h etc. is located.  You can set
# it in .dejagnurc or override it below.  It needs not to be in the
# install path of the toolchain under test, but should be "reasonably close"
# to the upcoming installation.  This is only needed if the install path
# doesn't already contain AVR-LibC headers.

# set avrlibc_include_dir "/avrgcc_install/avr/include"

# Extra CFLAGS to add to all compilations.  Notice that you can also
# use --tool_opts in RUNTESTFLAGS to accomplish this:
# > make -k check-gcc RUNTESTFLAGS="--target_board=... --tool_opts='...' ..."

# set extra_cflags ""

# Extra LDFLAGS to add to all compilations.  This serves to arrange
# the memory layout and sizes so as to reduce the numbber of test
# that are FAILing due to small memory.  The simulator has plenty
# of memory, and there is no need to stick to AVR harware limitations.

set extra_ldflags "${extra_ldflags} ${ld_ram}"

load_generic_config "avrtest"
EOF
}

# $1: -mmcu=$1 for avr-gcc
# $2: -mmcu=$2 for avrtest
# $3: SPlow: Stack is located below static storage $4.
#     SPhigh: Stack is located at the end of SRAM at $5 - 1.
# $4: Start of static storage
# $5: End of static storage.
# $6: Extra linker flags.
# $7: Extra avrtest flags.

print_exp "atmega128"  "avr51" SPlow 0x2000 0xffff "" ""
print_exp "atmega103"  "avr51" SPlow 0x2000 0xffff "" ""
print_exp "atmega64"   "avr51" SPlow 0x2000 0xffff "" ""
print_exp "atmega8"    "avr51" SPlow 0x2000 0xffff "" ""
print_exp "at90s8515"  "avr2"  SPlow 0x2000 0xffff "" ""
print_exp "atmega2560" "avr6"  SPlow 0x2000 0xffff "" ""
print_exp "atxmega128a3" "avrxmega6" SPlow 0x2000 0xffff "" ""
print_exp "atxmega128a1" "avrxmega7" SPlow 0x2000 0xffff "" ""
print_exp "attiny40" "avrtiny" SPhigh 0x40 0x3fff "-Wl,--defsym=__TEXT_REGION_LENGTH__=8K -Tavrtiny-rodata.x -Wl,--pmem-wrap-around=8k" "-s 8k"
print_exp "attiny3216" "avrxmega3" SPlow 0x2000 0x7fff "" ""
print_exp "avr128da32" "avrxmega4" SPlow 0x2000 0x7fff "" ""
