.PHONY: all all-host all-avr exe exit all-avrtest
.PHONY: clean clean-host clean-exit clean-fileio clean-avr

WARN	= -W -Wall -Wno-unused-parameter -pedantic \
	  -Wstrict-prototypes -Wmissing-prototypes

# $(builddir) contains config.h built by configure
CFLAGS_FOR_HOST= -O3 -fomit-frame-pointer -std=c99 -dp -I$(builddir) $(WARN) $(CFLAGS)

# compile for AVR at build

CFLAGS_FOR_AVR	= -Os 

# Ditch built-in rules.
SUFFIXES =

A_log	= avrtest_log
A	= avrtest avrtest_log

A_xmega	= avrtest-xmega avrtest-xmega_log
A_log	+= avrtest-xmega_log
A	+= $(A_xmega)

A_tiny	= avrtest-tiny avrtest-tiny_log
A_log	+= avrtest-tiny_log
A	+= $(A_tiny)

A_s       = $(A:=.s)
A_log_s   = $(A_log:=.s)
A_tiny_s  = $(A_tiny:=.s)
A_xmega_s = $(A_xmega:=.s)
A_exe     = $(A:=$(EXEEXT))
A_log_exe = $(A_log:=$(EXEEXT))

EXE	= $(A_exe)

all: all-host all-avr

all-avrtest: $(A:=$(EXEEXT))

all-host : $(EXE)

all-build : flag-tables

###########################################################
# configure sets EXIT_MCUS according to Supported AVR mcus.
all-avr	: exit fileio

EXIT_O = $(patsubst %,exit-%.o, $(EXIT_MCUS))

exit	: $(EXIT_O)

# !!! When using a fileio module, link with:
# !!! -Wl,-wrap,feof -Wl,-wrap,clearerr -Wl,-wrap,fclose -Wl,-wrap,fread -Wl,-wrap,fwrite

FILEIO_MCUS = $(EXIT_MCUS)

FILEIO_O = $(patsubst %,fileio-%.o, $(FILEIO_MCUS))

fileio	: $(FILEIO_O)
###########################################################

DEPS_OPTIONS   = options.def options.h testavr.h Makefile
DEPS_HOST      = $(DEPS_OPTIONS) host.h avrtest.h
DEPS_PERF      = $(DEPS_HOST) perf.h logging.h
DEPS_GRAPH     = $(DEPS_OPTIONS) graph.h
DEPS_LOGGING   = $(DEPS_PERF) sreg.h graph.h
DEPS_LOAD_FLASH = $(DEPS_OPTIONS)
DEPS           = $(DEPS_HOST) sreg.h flag-tables.h

XDEF = -DHAVE_CONFIG_H

# Indent with TAB for automake
$(A_log_s)	: XDEF += -DAVRTEST_LOG
$(A_xmega_s)	: XDEF += -DISA_XMEGA
$(A_tiny_s)	: XDEF += -DISA_TINY

$(A_exe): XOBJ += options.o load-flash.o flag-tables.o host.o
$(A_exe): options.o load-flash.o flag-tables.o host.o

$(A_log_exe): XOBJ += logging.o graph.o perf.o
$(A_log_exe): XLIB += -lm
$(A_log_exe): logging.o graph.o perf.o

options.o: options.c $(DEPS_OPTIONS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

host.o: host.c $(DEPS_HOST)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging.o: logging.c $(DEPS_LOGGING)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

graph.o: graph.c $(DEPS_GRAPH)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

perf.o: perf.c $(DEPS_PERF)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

load-flash.o: load-flash.c $(DEPS_LOAD_FLASH)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

flag-tables.o: flag-tables.c Makefile
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

$(A_s): avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

$(EXE) : avrtest%$(EXEEXT) : $(A_s)
	$(CC) $(CFLAGS_FOR_HOST) $< -o $@ $(XOBJ) $(XLIB)

# Build some auto-generated files

.PHONY: flag-tables

flag-tables: gen-flag-tables$(BUILD_EXEEXT) Makefile
	./$< 0 > $@.c
	./$< 1 > $@.h

gen-flag-tables$(BUILD_EXEEXT): gen-flag-tables.c sreg.h Makefile
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $< -o $@


# Cross-compile AVR exit-*.o and fileio-*.o objects.
# $(srcdir) fcontains avrtest.h and fileio.h.

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -std=gnu99 -I$(srcdir) -c $< -o $@ -mmcu=$*

fileio-%.o: dejagnuboards/fileio.c fileio.h avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -std=gnu99 -I$(srcdir) -c $< -o $@ -mmcu=$*

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
