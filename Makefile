CFLAGS=-Os -fomit-frame-pointer -std=c99
WARN=-W -Wall -Wno-unused-parameter -pedantic
CC=gcc
STRIP=strip
WINCC=i386-mingw32-gcc
WINSTRIP=i386-mingw32-strip
AVRGCC=avr-gcc

ifneq (,$(findstring Window,$(OS)))
exe=.exe
endif

all       : avrtest$(exe) exit \
	    avrtest-xmega$(exe)
exit      : exit-atmega128.o exit-atmega103.o exit-atmega2560.o \
	    exit-atxmega128a3.o exit-atxmega128a1.o

all-xmega  : ; @true # backward compatibility
exit-xmega : ; @true # backward compatibility

DEPS = avrtest.h flag-tables.c sreg.h avr-insn.def Makefile

avrtest$(exe) : %$(exe) : avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $*$(exe)
	$(CC) $(WARN) $(CFLAGS) $< -o $*_log$(exe) -DLOG_DUMP
	$(STRIP) $*$(exe)
	$(STRIP) $*_log$(exe)

avrtest-xmega$(exe) : %$(exe) : avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $*$(exe) -DISA_XMEGA
	$(CC) $(WARN) $(CFLAGS) $< -o $*_log$(exe) -DISA_XMEGA -DLOG_DUMP
	$(STRIP) $*$(exe)
	$(STRIP) $*_log$(exe)

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
	$(WINSTRIP) $*.exe
	$(WINSTRIP) $*_log.exe

%-xmega.exe: %.c $(DEPS)
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*-xmega.exe -DISA_XMEGA
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*-xmega_log.exe -DISA_XMEGA -DLOG_DUMP
	$(WINSTRIP) $*-xmega.exe
	$(WINSTRIP) $*-xmega_log.exe
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
