
CFLAGS = -Wall -O3 -gstabs

CC = gcc
AR = ar
RANLIB = ranlib
DEBUGLIB =
LFLAGS = -lm -gstabs

all: play_hvl

play_hvl: play_hvl.o hvl_replay.o
	$(CC) -o play_hvl play_hvl.o hvl_replay.o $(LFLAGS)

play_hvl.o: play_hvl.c hvl_replay.h
	$(CC) -c play_hvl.c -o play_hvl.o $(CFLAGS)

hvl_replay.o: hvl_replay.c hvl_replay.h
	$(CC) -c hvl_replay.c -o hvl_replay.o $(CFLAGS)
