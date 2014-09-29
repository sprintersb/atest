# Compile for host at build (cross if desired)
CC	= gcc
STRIP	= strip
WARN	= -W -Wall -Wno-unused-parameter -pedantic \
	  # -Wstrict-prototypes -Wmissing-prototypes

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

.SUFFIXES:

A	= $(patsubst *%, avrtest%, * *_log *-xmega *-xmega_log)
A_log	= $(patsubst *%, avrtest%, *_log *-xmega_log)
A_xmega	= $(patsubst *%, avrtest%, *-xmega *-xmega_log)

EXE	= $(A:=$(EXEEXT))

all : all-host all-avr

all-avrtest: $(A:=$(EXEEXT))

all-host : $(EXE)

all-avr	: exit

all-build : flag-tables

EXIT_MCUS = atmega8 atmega168 atmega128 atmega103 atmega2560 atxmega128a3 atxmega128a1

EXIT_O = $(patsubst %,exit-%.o, $(EXIT_MCUS))

exit	: $(EXIT_O)

DEP_OPTIONS = 	options.def options.h Makefile
DEPS_LOGGING =	$(DEP_OPTIONS) testavr.h avr-insn.def sreg.h avrtest.h
DEPS =		$(DEPS_LOGGING) flag-tables.c

$(A_log:=.s)	: XDEF += -DLOG_DUMP
$(A_xmega:=.s)	: XDEF += -DISA_XMEGA

$(A:=$(EXEEXT))     : XOBJ += options.o
$(A:=$(EXEEXT))     : options.o

$(A_log:=$(EXEEXT)) : XOBJ += logging.o
$(A_log:=$(EXEEXT)) : XLIB += -lm
$(A_log:=$(EXEEXT)) : logging.o

options.o: options.c $(DEP_OPTIONS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging.o: logging.c $(DEPS_LOGGING)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DLOG_DUMP

$(A:=.s) : avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

$(EXE) : avrtest%$(EXEEXT) : avrtest%.s
	$(CC) $< -o $@ $(XOBJ) $(XLIB)
	$(STRIP) $@

# Build some auto-generated files

.PHONY: flag-tables

flag-tables: gen-flag-tables$(BUILD_EXEEXT) Makefile
	./$< > $@.c

gen-flag-tables$(BUILD_EXEEXT): gen-flag-tables.c sreg.h Makefile
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $< -o $@

# Cross-compile Windows executables on Linux

ifneq ($(EXEEXT),.exe)
exe: avrtest.exe avrtest-xmega.exe

W=-mingw32

$(A_log:=$(W).s)   : XDEF += -DLOG_DUMP
$(A_xmega:=$(W).s) : XDEF += -DISA_XMEGA

$(A:=.exe)     : XOBJ_W += options$(W).o
$(A:=.exe)     : options$(W).o

$(A_log:=.exe) : XOBJ_W += logging$(W).o
$(A_log:=.exe) : XLIB += -lm
$(A_log:=.exe) : logging$(W).o


options$(W).o: options.c $(DEP_OPTIONS)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging$(W).o: logging.c $(DEPS_LOGGING)
	$(WINCC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DLOG_DUMP

$(A:=$(W).s) : avrtest.c $(DEPS)
	$(WINCC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

EXE_W = $(A:=.exe)
$(EXE_W) : avrtest%.exe : avrtest%$(W).s
	$(WINCC) $< -o $@ $(XOBJ_W) $(XLIB)
	$(WINSTRIP) $@

endif

all-mingw32: $(EXE_W)

# Cross-compile AVR exit*.o objects

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -mmcu=$* -I. $< -c -o $@ -save-temps -dp

.PHONY: all all-host all-avr clean clean-exit exe exit all-mingw32 all-avrtest

clean: clean-exit
	rm -f $(wildcard *.o *.i *.s)
	rm -f $(EXE)
	rm -f $(A:=.s) $(A:=.i) $(A:=.o)
	rm -f $(wildcard *.exe)
	rm -f gen-flag-tables

clean-exit:
	rm -f $(wildcard exit-*.o)
