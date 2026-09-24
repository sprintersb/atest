.PHONY: deltas delta clean help force

help:
	@echo ""
	@cat help.txt
	@echo ""

deltas: all.data

CC=avr-gcc
MCU=atmega128

# Prefer AVRtest that ships this very examples.  When you instead prefer
# AVRtest as found in PATH, you can use
## AVRTEST_HOME := $(shell dirname `which avrtests`)
# instead or specify AVRTEST_HOME on the command line.
AVRTEST_HOME ?= ../..

# What will be investigated.
FUNC=logf
LO="0.5"
HI="1-1"
OUT=float

# target delta
STEP=1
NUM=1

nums  := $(shell bash -c 'echo -n "$$(seq 0 $$(( '$(NUM)' - 1)) )"')
afunc := $(shell bash -c 'echo -n avrtest_$${0::-1}l' $(FUNC))

# Thanks to syscalls like avrest_printf, we don't need target functions
# like printf, hence there should be no need to link libprintf_flt.a.
## FLT = -Wl,-u,vfprintf -lprintf_flt # Only needed without avrtest_*printf.

dats := $(foreach num,$(nums),d-$(num).data)

.PHONY: force

exit_c = $(AVRTEST_HOME)/dejagnuboards/exit.c

exit.o: $(exit_c) force
	$(CC) -mmcu=$(MCU) -std=gnu99 -Os $< -c -o $@ -I$(AVRTEST_HOME)

CC_ARGS = -Os -mmcu=$(MCU) -std=gnu99 -o $@ -I$(AVRTEST_HOME) exit.o -save-temps -dp

delta.elf: delta.c exit.o force
	$(CC) $< $(CC_ARGS) $(FLT) -DFUNC=$(FUNC) -DAFUNC=$(afunc) $(ARGS)
	avr-objdump -d $@ > delta.lst

# avrtest-elf maps the ELF file to the right executable like avrtest-xmega.
avrtest_x = $(shell avrtest-elf $<) -q

# What doesn't work as expected is to gather stderr outputs in individual
# files and then let a script print them in an orderly manner.  What
# doesn't work as expected is tee-ing stderr for /immediate/ output
# of delta.c messages like expected run time.
d-%.data : delta.elf
	$(avrtest_x) ./$< $(AARGS) -args -num=$(NUM) -n=$* -lo="$(LO)" -hi="$(HI)" -step=$(STEP) -out=$(OUT) $(XARGS) > $@

# Use GNU make to spawn NUM processes.
all.data: force
	$(MAKE) -j$(NUM) $(dats)
	cat $(dats) > $@
	cat all.data

eval.x: eval.c
	gcc $< -O -o $@ -std=c99 -Wall -Werror -lm

delta: eval.x all.data
	cat all.data | ./eval.x -out=$(OUT)

clean:
	rm -f $(wildcard *.[isox] *.data *.elf *.elf.* *.lst config.h)
