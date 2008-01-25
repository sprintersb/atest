
avrtest: avrtest.c Makefile
	gcc -W -Wall -O3 -fomit-frame-pointer avrtest.c -o avrtest
	gcc -DLOG_DUMP -W -Wall -O3 -fomit-frame-pointer avrtest.c -o avrtest_log

.PHONY: clean
clean:
	rm -f *.o avrtest
