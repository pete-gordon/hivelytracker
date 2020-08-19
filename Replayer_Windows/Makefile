
CC = gcc

CFLAGS = -g
LFLAGS += -g -lm -lwinmm

TARGET = play_hvl.exe

OBJECTS = hvl_replay.o \
          hvl_tables.o \
          main.o


$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d
