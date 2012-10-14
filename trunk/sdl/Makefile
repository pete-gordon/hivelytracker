
WRAPPERDIR := $(CURDIR)
BASEDIR = $(CURDIR)/..

CC = gcc

CFLAGS = -g -Dmain=SDL_main -D__SDL_WRAPPER__


CFLAGS += -I$(BASEDIR) \
          -I$(WRAPPERDIR) \
          -Ic:/mingw/include/SDL

LFLAGS += -g -lm -mwindows -lmingw32 -lSDL_ttf -lSDLmain -lSDL -lSDL_image -lfreetype -lz

TARGET = hivelytracker.exe

OBJECTS = sdl_wrapper.o \
          ht.o \
	  win32.o \
	  ../gui.o \
	  ../replay.o \
	  ../util.o \
	  ../undo.o



$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d
