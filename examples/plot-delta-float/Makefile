.PHONY: clean help force

help:
	@echo ""
	@cat help.txt
	@echo ""

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

# targets plot.png / plot.svg
NX = 1000
DIM = 850,500
# Extract image dimensions to pass to plot.gplot.
PIXX = $(shell bash -c 'cut -d, -f1 <<< "$(DIM)"')
PIXY = $(shell bash -c 'cut -d, -f2 <<< "$(DIM)"')

afunc := $(shell bash -c 'echo -n avrtest_$${0::-1}l' $(FUNC))

# Thanks to syscalls like avrest_printf, we don't need target functions
# like printf, hence there should be no need to link libprintf_flt.a.
## FLT = -Wl,-u,vfprintf -lprintf_flt # Only needed without avrtest_*printf.

.PHONY: force

exit_c = $(AVRTEST_HOME)/dejagnuboards/exit.c

exit.o: $(exit_c) force
	$(CC) -mmcu=$(MCU) -std=gnu99 -Os $< -c -o $@ -I$(AVRTEST_HOME)

CC_ARGS = -Os -mmcu=$(MCU) -std=gnu99 -o $@ -I$(AVRTEST_HOME) exit.o -save-temps -dp

plot.elf: plot.c config.h exit.o force
	$(CC) $< $(CC_ARGS) $(FLT) -DFUNC=$(FUNC) -DAFUNC=$(afunc) $(ARGS)
	avr-objdump -d $@ > plot.lst

# config.h tries to guess the required float format (IEEE single or double).
config.h: gen-config.sh force
	./$< $(FUNC) > $@

# avrtest-elf maps the ELF file to the right executable like avrtest-xmega.
avrtest_x = $(shell avrtest-elf $<) -q

# Use intermediate plot1.data so make stops when avrtest goes EXIT_FAILURE.
plot.data : plot.elf
	$(avrtest_x) ./$< $(AARGS) -args -nx=$(NX) -lo="$(LO)" -hi="$(HI)" -out=$(OUT) $(XARGS) > plot1.data
	grep '^==' plot1.data | sed -e 's:^==::' > $@

GPLOT_ARGS = -e pixx=\"$(PIXX)\" -e pixy=\"$(PIXY)\"

plot.svg : plot.data plot.gplot
	gnuplot $(GPLOT_ARGS) plot.gplot

plot.png : plot.data plot.gplot
	gnuplot $(GPLOT_ARGS) -e png_svg=\"png\" plot.gplot

all.data: force
	$(MAKE) -j$(NUM) $(dats)
	cat $(dats) > $@
	cat all.data

clean:
	rm -f $(wildcard *.[isox] *.data *.elf *.elf.* *.lst config.h)
	rm -f $(wildcard *.png *.svg)
