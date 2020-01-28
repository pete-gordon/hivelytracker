PLATFORM = $(shell uname)

####### DEFAULT SETTINGS HERE #######

CFLAGS = -Wall -O3
LFLAGS = -lm

CC = gcc
AR = ar
RANLIB = ranlib
DEBUGLIB =
TARGET = hvl2wav

####### PLATFORM SPECIFIC STUFF HERE #######

ifeq ($(PLATFORM),os4)
CFLAGS += -ggdb -D_BIG_ENDIAN_
LFLAGS += -ggdb
endif

ifeq ($(PLATFORM),win)
LFLAGS += -lmingw32
TARGET = hvl2wav.exe
endif

ifeq ($(PLATFORM),Linux)
CFLAGS += -ggdb -D_BIG_ENDIAN_
LFLAGS += -ggdb
endif

####### SHOULDN'T HAVE TO CHANGE THIS STUFF #######

all: $(TARGET)

$(TARGET): hvl2wav.o replay.o
	$(CC) -o $(TARGET) hvl2wav.o replay.o $(LFLAGS)

hvl2wav.o: hvl2wav.c replay.h types.h
	$(CC) -c hvl2wav.c -o hvl2wav.o $(CFLAGS)

replay.o: replay.c replay.h types.h
	$(CC) -c replay.c -o replay.o $(CFLAGS)

clean:
	rm *.o $(TARGET)
