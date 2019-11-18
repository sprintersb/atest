#!/bin/sh

print_exp()
{
    mmcu="$1"
    avrtest_mmcu="$2"
    extra_ldflags="$3"
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

set extra_ldflags "${extra_ldflags}"

load_generic_config "avrtest"
EOF
}

print_exp "atmega128"  "avr51" "-Wl,-Tbss=0x802000,--defsym=__heap_end=0x80ffff"
print_exp "atmega103"  "avr51" "-Wl,-Tbss=0x802000,--defsym=__heap_end=0x80ffff"
print_exp "atmega64"   "avr51" "-Wl,-Tbss=0x802000,--defsym=__heap_end=0x80ffff"
print_exp "atmega8"    "avr51" "-Wl,-Tbss=0x802000,--defsym=__heap_end=0x80ffff"
print_exp "atmega2560" "avr6"  "-Wl,-Tbss=0x802000,--defsym=__heap_end=0x80ffff,--defsym=__stack=0x1fff"
print_exp "atxmega128a3" "avrxmega6" "-Wl,--defsym=__heap_end=0x80ffff,--defsym=__stack=0x1fff"
print_exp "attiny40" "avrtiny" "-Wl,--defsym=__heap_end=0x801fff -Wl,--defsym=__DATA_REGION_LENGTH__=8K -Wl,--defsym=__TEXT_REGION_LENGTH__=16K -Tavrtiny-rodata.x "
print_exp "attiny3216" "avrxmega3" "-Tdata=0x802000 -Wl,--defsym=__stack=0x1fff,--defsym=__heap_end=0x807fff"
