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

$(TARGET): hvl2wav.o hvl_replay.o hvl_tables.o
	$(CC) -o $(TARGET) $^ $(LFLAGS)

hvl2wav.o: hvl2wav.c hvl_replay.h ../hvl_replay.h types.h
	$(CC) -c hvl2wav.c -o hvl2wav.o $(CFLAGS)

hvl_replay.o: hvl_replay.c hvl_replay.h ../hvl_replay.h hvl_tables.h types.h
	$(CC) -c hvl_replay.c -o hvl_replay.o $(CFLAGS)

hvl_tables.o: hvl_tables.c hvl_tables.h types.h
	$(CC) -c hvl_tables.c -o hvl_tables.o $(CFLAGS)

clean:
	rm -f *.o $(TARGET)
