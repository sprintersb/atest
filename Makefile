# Compile for host at build (cross if desired)
CC	= gcc
STRIP	= strip
WARN	= -W -Wall -Wno-unused-parameter -pedantic
CFLAGS_FOR_HOST= -O3 -fomit-frame-pointer -std=c99 $(WARN) $(CFLAGS)

# compile for i386-mingw32 at *-linux-*
WINCC	= i386-mingw32-gcc
WINSTRIP= i386-mingw32-strip

ifneq (,$(findstring Window,$(OS)))
# For the host
EXEEXT		= .exe
# For the build
BUILD_EXEEXT	= $(EXEEXT)
endif

# compile for build at build (native)
CC_FOR_BUILD	= gcc$(BUILD_EXEEXT)
CFLAGS_FOR_BUILD= -W -Wall -std=c99 -O2 -g

# compile for AVR at build
CC_FOR_AVR	= avr-gcc$(BUILD_EXEEXT)
CFLAGS_FOR_AVR	= -Os 

all	: avrtest$(EXEEXT) exit \
	  avrtest-xmega$(EXEEXT)
exit	: exit-atmega128.o exit-atmega103.o exit-atmega2560.o \
	  exit-atxmega128a3.o exit-atxmega128a1.o

DEPS = avrtest.h flag-tables.c sreg.h avr-insn.def Makefile

avrtest$(EXEEXT) : %$(EXEEXT) : avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) $< -o $*$(EXEEXT)
	$(CC) $(CFLAGS_FOR_HOST) $< -o $*_log$(EXEEXT) -DLOG_DUMP
	$(STRIP) $*$(EXEEXT)
	$(STRIP) $*_log$(EXEEXT)

avrtest-xmega$(EXEEXT) : %$(EXEEXT) : avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) $< -o $*$(EXEEXT) -DISA_XMEGA
	$(CC) $(CFLAGS_FOR_HOST) $< -o $*_log$(EXEEXT) -DISA_XMEGA -DLOG_DUMP
	$(STRIP) $*$(EXEEXT)
	$(STRIP) $*_log$(EXEEXT)

# Build some auto-generated files

.PHONY: flag-tables

flag-tables: gen-flag-tables$(BUILD_EXEEXT) Makefile
	./$< > $@.c

gen-flag-tables$(BUILD_EXEEXT): gen-flag-tables.c sreg.h Makefile
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $< -o $@

# Cross-compile Windows executables on Linux

ifneq ($(EXEEXT),.exe)
exe: avrtest.exe avrtest-xmega.exe
%.exe: %.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) $< -o $*.exe
	$(WINCC) $(CFLAGS_FOR_HOST) $< -o $*_log.exe -DLOG_DUMP
	$(WINSTRIP) $*.exe
	$(WINSTRIP) $*_log.exe

%-xmega.exe: %.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) $< -o $*-xmega.exe -DISA_XMEGA
	$(WINCC) $(CFLAGS_FOR_HOST) $< -o $*-xmega_log.exe -DISA_XMEGA -DLOG_DUMP
	$(WINSTRIP) $*-xmega.exe
	$(WINSTRIP) $*-xmega_log.exe
endif

# Cross-compile AVR exit*.o objects

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -mmcu=$* -I. $< -c -o $@

.PHONY: all clean clean-exit exe exit

clean: clean-exit
	rm -f $(wildcard *.o *.i *.s)
	rm -f avrtest avrtest_log avrtest-xmega avrtest-xmega_log
	rm -f $(wildcard *.exe)
	rm -f gen-flag-tables

clean-exit:
	rm -f $(wildcard exit-*.o)
