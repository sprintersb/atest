# Compile for host at build (cross if desired)
CC	= gcc
WARN	= -W -Wall -Wno-unused-parameter -pedantic \
	  -Wstrict-prototypes -Wmissing-prototypes

CFLAGS_FOR_HOST= -O3 -fomit-frame-pointer -std=c99 -dp $(WARN) $(CFLAGS)

# compile for i386-mingw32 at *-linux-*
WINCC	= i386-mingw32-gcc
WINCC	= i686-w64-mingw32-gcc

ifneq (,$(findstring Window,$(OS)))
# For the host
EXEEXT		= .exe
# For the build
BUILD_EXEEXT	= $(EXEEXT)
endif

# compile for build at build (native)
CC_FOR_BUILD	= gcc$(BUILD_EXEEXT)
CFLAGS_FOR_BUILD= -W -Wall -std=c99 -O2 -g

# Compile for AVR at build.
CC_FOR_AVR	= avr-gcc$(BUILD_EXEEXT)
CFLAGS_FOR_AVR	= -Os

# Ditch built-in rules.
.SUFFIXES:

A	= $(patsubst *%, avrtest%, * *_log *-xmega *-xmega_log *-tiny *-tiny_log)
A_log	= $(patsubst *%, avrtest%, *_log *-xmega_log *-tiny_log)
A_xmega	= $(patsubst *%, avrtest%, *-xmega *-xmega_log)
A_tiny	= $(patsubst *%, avrtest%, *-tiny *-tiny_log)

EXE	= $(A:=$(EXEEXT))

all : all-host all-avr

all-avrtest: $(A:=$(EXEEXT))

all-host : $(EXE)

all-avr	: exit fileio

all-build : flag-tables

EXIT_MCUS = atmega8 atmega168 atmega128 atmega103 atmega2560 \
	    atxmega128a3 atxmega128a1 attiny40 attiny3216

EXIT_O = $(patsubst %,exit-%.o, $(EXIT_MCUS))

exit	: $(EXIT_O)

# !!! When using a fileio module, link with:
# !!! -Wl,-wrap,feof -Wl,-wrap,clearerr -Wl,-wrap,fclose -Wl,-wrap,fread -Wl,-wrap,fwrite

FILEIO_MCUS = $(EXIT_MCUS)

FILEIO_O = $(patsubst %,fileio-%.o, $(FILEIO_MCUS))

fileio	: $(FILEIO_O)

DEPS += options.def options.h testavr.h avr-opcode.def host.h Makefile
DEPS += graph.h perf.h logging.h avrtest.h sreg.h flag-tables.h

$(A_log:=.s)	: XDEF += -DAVRTEST_LOG
$(A_xmega:=.s)	: XDEF += -DISA_XMEGA
$(A_tiny:=.s)	: XDEF += -DISA_TINY

$(A:=$(EXEEXT))     : XOBJ += options.o load-flash.o flag-tables.o host.o
$(A:=$(EXEEXT))     : options.o load-flash.o flag-tables.o host.o

$(A_log:=$(EXEEXT)) : XOBJ += logging.o graph.o perf.o
$(A_log:=$(EXEEXT)) : XLIB += -lm
$(A_log:=$(EXEEXT)) : logging.o graph.o perf.o

options.o: options.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

host.o: host.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging.o: logging.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

graph.o: graph.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

perf.o: perf.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

load-flash.o: load-flash.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

flag-tables.o: flag-tables.c Makefile
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

$(A:=.s) : avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

$(EXE) : avrtest%$(EXEEXT) : avrtest%.s
	$(CC) $< -o $@ $(XOBJ) $(CFLAGS_FOR_HOST) $(XLIB)

# Build some auto-generated files

.PHONY: flag-tables

flag-tables: gen-flag-tables$(BUILD_EXEEXT) Makefile
	./$< 0 > $@.c
	./$< 1 > $@.h

gen-flag-tables$(BUILD_EXEEXT): gen-flag-tables.c sreg.h Makefile
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $< -o $@

# Cross-compile Windows executables on Linux

ifneq ($(EXEEXT),.exe)
exe:	avrtest.exe avrtest-xmega.exe avrtest-tiny.exe \
	avrtest_log.exe avrtest-xmega_log.exe avrtest-tiny_log.exe

W=-mingw32

$(A_log:=$(W).s)   : XDEF += -DAVRTEST_LOG
$(A_xmega:=$(W).s) : XDEF += -DISA_XMEGA
$(A_tiny:=$(W).s)  : XDEF += -DISA_TINY

$(A:=.exe)     : XOBJ_W += options$(W).o load-flash$(W).o flag-tables$(W).o host$(W).o
$(A:=.exe)     : options$(W).o load-flash$(W).o flag-tables$(W).o host$(W).o

$(A_log:=.exe) : XOBJ_W += logging$(W).o graph$(W).o perf$(W).o
$(A_log:=.exe) : XLIB += -lm
$(A_log:=.exe) : logging$(W).o graph$(W).o perf$(W).o


options$(W).o: options.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@

host$(W).o: host.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging$(W).o: logging.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

graph$(W).o: graph.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

perf$(W).o: perf.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

load-flash$(W).o: load-flash.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@

flag-tables$(W).o: flag-tables.c Makefile
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@

$(A:=$(W).s) : avrtest.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

EXE_W = $(A:=.exe)
$(EXE_W) : avrtest%.exe : avrtest%$(W).s
	$(WINCC) $< -o $@ $(XOBJ_W) $(CFLAGS_FOR_HOST) $(XLIB)

endif

all-mingw32: $(EXE_W)

# Cross-compile AVR exit-*.o and fileio-*.o objects.

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -std=gnu99 -I. -c $< -o $@ -mmcu=$*

fileio-%.o: dejagnuboards/fileio.c fileio.h avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -std=gnu99 -I. -c $< -o $@ -mmcu=$*

.PHONY: all all-host all-avr exe exit all-mingw32 all-avrtest
.PHONY: clean clean-host clean-exit clean-fileio clean-avr

clean-host:
	rm -f $(filter-out $(wildcard exit-*.[iso] fileio-*.[iso]) , $(wildcard *.[iso]))
	rm -f $(wildcard *.exe gen-flag-tables)
	rm -f $(wildcard $(A:=.s) $(A:=.i) $(A:=.o))
	rm -f $(wildcard $(EXE))

clean-exit:
	rm -f $(wildcard exit-*.[iso])

clean-fileio:
	rm -f $(wildcard fileio-*.[iso])

clean-avr: clean-exit clean-fileio

clean: clean-host clean-avr
