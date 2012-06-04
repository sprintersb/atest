CFLAGS=-O3 -fomit-frame-pointer
WARN=-W -Wall -Wno-unused-parameter
CC=gcc
AVRGCC=avr-gcc

all: avrtest exit-atmega128.o exit-atmega103.o exit-atmega2560.o
all: avrtest-xmega exit-atxmega128a3.o

DEPS = avrtest.h flag-tables.c sreg.h Makefile
avrtest: avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $@
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DLOG_DUMP

avrtest-xmega: avrtest.c $(DEPS)
	$(CC) $(WARN) $(CFLAGS) $< -o $@ -DISA_XMEGA
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DISA_XMEGA -DLOG_DUMP

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(AVRGCC) -c -Os -mmcu=$* -I. $< -o $@

flag-tables.c: gen-flag-tables Makefile
	./gen-flag-tables > $@

gen-flag-tables: gen-flag-tables.c sreg.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f $(wildcard *.o *.i *.s)
	rm -f avrtest avrtest_log avrtest-xmega avrtest-xmega_log
	rm -f gen-flag-tables
