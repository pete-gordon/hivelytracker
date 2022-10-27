
CFLAGS = -Wall -O3 -I.

CC = gcc
AR = ar
RANLIB = ranlib
DEBUGLIB =
LFLAGS = -lm

all: ht

ht: ht.o gui.o util.o replay.o about.o undo.o
	$(CC) -o ht ht.o gui.o util.o replay.o about.o undo.o $(LFLAGS)

ht.o: ht.c gui.h replay.h about.h
	$(CC) -c ht.c -o ht.o $(CFLAGS)

gui.o: gui.c gui.h util.h replay.h
	$(CC) -c gui.c -o gui.o $(CFLAGS)

util.o: util.c util.h
	$(CC) -c util.c -o util.o $(CFLAGS)

replay.o: replay.c replay.h undo.h
	$(CC) -c replay.c -o replay.o $(CFLAGS)

about.o: about.c about.h gui.h
	$(CC) -c about.c -o about.o $(CFLAGS)

undo.o: undo.c undo.h replay.h
	$(CC) -c undo.c -o undo.o $(CFLAGS)
