CFLAGS=-O3 -fomit-frame-pointer
WARN=-W -Wall -Wno-unused-parameter
CC=gcc
WINCC=i386-mingw32-gcc
AVRGCC=avr-gcc

all: avrtest exit-atmega128.o exit-atmega103.o exit-atmega2560.o
all: avrtest-xmega exit-atxmega128a3.o
exe: avrtest.exe avrtest-xmega.exe

DEPS = avrtest.h flag-tables.c sreg.h Makefile

avrtest: avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $@
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DLOG_DUMP

avrtest-xmega: avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $@ -DISA_XMEGA
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DISA_XMEGA -DLOG_DUMP

%.exe: %.c $(DEPS)
	$(WINCC) $(WARN) $(CFLAGS) $< -o $@ -save-temps -dp
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*_log.exe -DLOG_DUMP

%-xmega.exe: %.c $(DEPS)
	$(WINCC) $(WARN) $(CFLAGS) $< -o $@ -DISA_XMEGA
	$(WINCC) $(WARN) $(CFLAGS) $< -o $*-xmega_log.exe -DISA_XMEGA -DLOG_DUMP

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(AVRGCC) -c -Os -mmcu=$* -I. $< -o $@

flag-tables.c: gen-flag-tables Makefile
	./gen-flag-tables > $@

gen-flag-tables: gen-flag-tables.c sreg.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@

.PHONY: clean clean-exe

clean:
	rm -f $(wildcard *.o *.i *.s)
	rm -f avrtest avrtest_log avrtest-xmega avrtest-xmega_log
	rm -f $(wildcard *.exe)
	rm -f gen-flag-tables
