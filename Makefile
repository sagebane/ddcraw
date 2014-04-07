CFLAGS=-Wall -g

all: test dcraw

test: main.o
	cc -o $@ $^ -lm
%.o: %.c
	cc -c $(CFLAGS) $<
cscope:
dcraw: dcraw.c
	gcc -o dcraw dcraw.c -g -lm -ljasper -ljpeg -llcms2
clean:
	rm -rf *.o *~ test dcraw *.ppm

.PHONY: all clean 
