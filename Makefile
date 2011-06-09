CFLAGS=-O3 -fomit-frame-pointer
WARN=-W -Wall -Wno-unused-parameter
CC=gcc
AVRGCC=avr-gcc

all: avrtest exit-atmega128.o exit-atmega2560.o

avrtest: avrtest.c avrtest.h Makefile
	$(CC) $(WARN) $(CFLAGS) $< -o $@
	$(CC) $(WARN) $(CFLAGS) $< -o $@_log -DLOG_DUMP

exit-%.o: dejagnuboards/exit.c avrtest.h Makefile
	$(AVRGCC) -c -Os -mmcu=$* -I. $< -o $@

.PHONY: clean

clean:
	rm -f *.o avrtest avrtest_log
