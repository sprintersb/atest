CFLAGS=-O3 -fomit-frame-pointer
WARN=-W -Wall -Wno-unused-parameter
CC=gcc
AVRGCC=avr-gcc

all: avrtest exit-atmega128.o exit-atmega2560.o
all: avrtest-xmega exit-atxmega128a3.o

avrtest: avrtest.c avrtest.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DLOG_DUMP

avrtest-xmega: avrtest.c avrtest.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@ -DISA_XMEGA
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DISA_XMEGA -DLOG_DUMP

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(AVRGCC) -c -Os -mmcu=$* -I. $< -o $@

.PHONY: clean

clean:
	rm -f *.o avrtest avrtest_log avrtest-xmega avrtest-xmega_log
