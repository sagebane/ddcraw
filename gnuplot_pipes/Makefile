

CC = gcc
CFLAGS = -O3


all: gnuplot_pipes.o anim example test
clean:
	rm -f gnuplot_pipes.o anim example test *~

gnuplot_pipes.o: gnuplot_pipes.c gnuplot_pipes.h
	$(CC) $(CFLAGS) -c -o gnuplot_pipes.o gnuplot_pipes.c -I.

anim: anim.c gnuplot_pipes.o
	$(CC) $(CFLAGS) -o anim anim.c gnuplot_pipes.o
test: test.c gnuplot_pipes.o
	$(CC) $(CFLAGS) -o test test.c gnuplot_pipes.o -lm
example: example.c gnuplot_pipes.o
	$(CC) $(CFLAGS) -o example example.c gnuplot_pipes.o
