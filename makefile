
CFLAGS = -Wall -O3 -gstabs -I.

CC = gcc
AR = ar
RANLIB = ranlib
DEBUGLIB =
LFLAGS = -use-dynld -lm -gstabs
PREFIX = /usr

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

install:
	mkdir -p $(PREFIX)/share/man/man1/
	docs/hivelytracker.1 $(PREFIX)/share/man/man1/
	gzip -9 $(PREFIX)/share/man/man1/hivelytracker.1
	install -o root -g root -m 755 sdl/hivelytracker $(PREFIX)/bin
	install -o root -g root -m 644 sdl/winicon.png $(PREFIX)/share/icons/hivelytracker.png
	install -o root -g root -m 644 sdl/hivelytracker.desktop $(PREFIX)/share/applications
	install -o root -g root -m 644 Instruments $(PREFIX)share/hivelytracker                                                                            
	install -o root -g root -m 644 Skins $(PREFIX)/share/hivelytracker
