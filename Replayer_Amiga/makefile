
CFLAGS = -Wall -O3 -gstabs

CC = gcc
AR = ar
RANLIB = ranlib
DEBUGLIB =
LFLAGS = -lm -gstabs

all: play_hvl

play_hvl: play_hvl.o hvl_replay.o hvl_tables.o
	$(CC) -o play_hvl play_hvl.o hvl_replay.o hvl_tables.o $(LFLAGS)

play_hvl.o: play_hvl.c hvl_replay.h
	$(CC) -c play_hvl.c -o play_hvl.o $(CFLAGS)

hvl_replay.o: hvl_replay.c hvl_replay.h hvl_tables.h
	$(CC) -c hvl_replay.c -o hvl_replay.o $(CFLAGS)

hvl_tables.o: hvl_tables.c hvl_replay.h hvl_tables.h
	$(CC) -c hvl_tables.c -o hvl_tables.o $(CFLAGS)
