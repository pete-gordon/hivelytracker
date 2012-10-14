
WRAPPERDIR := $(CURDIR)
BASEDIR = $(CURDIR)/..

CC = gcc

CFLAGS = -g -Dmain=SDL_main -D__SDL_WRAPPER__ -I$(BASEDIR) -I$(WRAPPERDIR)
LFLAGS += -g -lm -mwindows -lmingw32 -lSDLmain -lSDL -lSDL_image

TARGET = hivelytracker.exe

OBJECTS = sdl_wrapper.o ht.o ..\gui.o ..\replay.o ..\util.o ..\undo.o


$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d
