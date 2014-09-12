CFLAGS=-O3 -fomit-frame-pointer
WARN=-W -Wall -Wno-unused-parameter
CC=gcc
WINCC=i386-mingw32-gcc
AVRGCC=avr-gcc

ifneq (,$(findstring Window,$(OS)))
exe=.exe
endif

all       : avrtest$(exe) exit
exit      : exit-atmega128.o exit-atmega103.o exit-atmega2560.o

# xmega is not supported in 4.6, build xmega stuff by make all-xmega
all-xmega : avrtest-xmega$(exe) exit-xmega all
exit-xmega: exit-atxmega128a3.o exit-atxmega128a1.o  exit

DEPS = avrtest.h flag-tables.c sreg.h Makefile

avrtest$(exe) : %$(exe) : avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $*$(exe)
	$(CC) $(WARN) $(CFLAGS) $< -o $*_log$(exe) -DLOG_DUMP

avrtest-xmega$(exe) : %$(exe) : avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $*$(exe) -DISA_XMEGA
	$(CC) $(WARN) $(CFLAGS) $< -o $*_log$(exe) -DISA_XMEGA -DLOG_DUMP

# Build some auto-generated files

flag-tables.c: gen-flag-tables$(exe) Makefile
	./gen-flag-tables$(exe) > $@

gen-flag-tables$(exe): gen-flag-tables.c sreg.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@

# Cross-compile Windows executables on Linux

ifneq ($(exe),.exe)
exe: avrtest.exe avrtest-xmega.exe
%.exe: %.c $(DEPS)
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*.exe -save-temps -dp
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*_log.exe -DLOG_DUMP

%-xmega.exe: %.c $(DEPS)
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*-xmega.exe -DISA_XMEGA
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*-xmega_log.exe -DISA_XMEGA -DLOG_DUMP
endif

# Cross-compile AVR exit*.o objects

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(AVRGCC) -c -Os -mmcu=$* -I. $< -o $@


.PHONY: all clean clean-exit exe exit exit-xmega all-xmega

clean: clean-exit
	rm -f $(wildcard *.o *.i *.s)
	rm -f avrtest avrtest_log avrtest-xmega avrtest-xmega_log
	rm -f $(wildcard *.exe)
	rm -f gen-flag-tables

clean-exit:
	rm -f $(wildcard exit-*.o)
