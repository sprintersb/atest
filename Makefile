
avrtest: avrtest.c Makefile
	#gcc -W -Wall -Wno-unused-parameter -O3 -fomit-frame-pointer avrtest.c -o avrtest
	gcc -W -Wall -Wno-unused-parameter -O3 -fomit-frame-pointer avrtest.c -o avrtest
	#gcc -g -pg -W -Wall -Wno-unused-parameter -O3 avrtest.c -o avrtest
	gcc -DLOG_DUMP -W -Wall -Wno-unused-parameter -O3 -fomit-frame-pointer avrtest.c -o avrtest_log

.PHONY: clean
clean:
	rm -f *.o avrtest
