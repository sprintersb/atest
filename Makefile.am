WARN	= -W -Wall -Wno-unused-parameter -pedantic \
	  # -Wstrict-prototypes -Wmissing-prototypes

CFLAGS_FOR_HOST= -O3 -fomit-frame-pointer -std=c99 -dp $(WARN) $(CFLAGS)

# compile for AVR at build

CFLAGS_FOR_AVR	= -Os 

SUFFIXES=

A	= $(patsubst *%, avrtest%, * *_log)
A_log	= $(patsubst *%, avrtest%, *_log)
A_xmega	=

if BUILD_XMEGA
A	+= $(patsubst *%, avrtest-xmega%, * *_log)
A_log	+= $(patsubst *%, avrtest-xmega%, *_log)
A_xmega	+= $(patsubst *%, avrtest-xmega%, * *_log)
endif # BUILD_XMEGA

A_s       = $(A:=.s)
A_log_s   = $(A_log:=.s)
A_xmega_s = $(A_xmega:=.s)
A_exe     = $(A:=$(EXEEXT))
A_log_exe = $(A_log:=$(EXEEXT))

EXE	= $(A_exe)

all : all-host all-avr

all-avrtest: $(A:=$(EXEEXT))

all-host : $(EXE)

all-avr	: exit

all-build : flag-tables

EXIT_MCUS =

if BUILD_EXIT_O

EXIT_MCUS += atmega8 atmega168 atmega128 atmega103 atmega2560

if BUILD_XMEGA

EXIT_MCUS += atxmega128a3 atxmega128a1

endif # BUILD_XMEGA
endif # BUILD_EXIT_O

EXIT_O = $(patsubst %,exit-%.o, $(EXIT_MCUS))

exit	: $(EXIT_O)

DEP_OPTIONS = 	options.def options.h Makefile
DEPS_LOGGING =	$(DEP_OPTIONS) testavr.h avr-insn.def sreg.h avrtest.h
DEPS =		$(DEPS_LOGGING) flag-tables.c

# Indent with TAB for automake
$(A_log_s): XDEF += -DAVRTEST_LOG
$(A_xmega_s): XDEF += -DISA_XMEGA

$(A_exe): XOBJ += options.o
$(A_exe): options.o

$(A_log_exe): XOBJ += logging.o
$(A_log_exe): XLIB += -lm
$(A_log_exe): logging.o

options.o: options.c $(DEP_OPTIONS)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@

logging.o: logging.c logging.h $(DEPS_LOGGING)
	$(CC) $(CFLAGS_FOR_HOST) -c $< -o $@ -DAVRTEST_LOG

$(A_s): avrtest.c $(DEPS)
	$(CC) $(CFLAGS_FOR_HOST) -S $< -o $@ $(XDEF)

$(EXE) : avrtest%$(EXEEXT) : avrtest%.s
	$(CC) $(CFLAGS_FOR_HOST) $< -o $@ $(XOBJ) $(XLIB)
	$(STRIP) $@

# Build some auto-generated files

.PHONY: flag-tables

flag-tables: gen-flag-tables$(BUILD_EXEEXT) Makefile
	./$< > $@.c

gen-flag-tables$(BUILD_EXEEXT): gen-flag-tables.c sreg.h Makefile
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $< -o $@


# Cross-compile AVR exit*.o objects

$(EXIT_O): exit-%.o :dejagnuboards/exit.c avrtest.h Makefile
	$(CC_FOR_AVR) $(CFLAGS_FOR_AVR) -mmcu=$* -I$(srcdir) $< -c -o $@ -save-temps -dp

.PHONY: all all-host all-avr clean clean-exit exe exit all-avrtest

clean: clean-exit
	$(RM) -f $(A:=.s) $(EXE) logging.o options.o
#	rm -f $(wildcard *.o *.i *.s)
#	rm -f $(EXE)
#	rm -f $(A:=.s) $(A:=.i) $(A:=.o)
#	rm -f $(wildcard *.exe)
#	rm -f gen-flag-tables

clean-exit:
	$(RM) -f $(EXIT_O)
