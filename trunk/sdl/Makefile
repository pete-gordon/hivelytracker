
WRAPPERDIR := $(CURDIR)
BASEDIR = $(CURDIR)/..

CC = gcc

CFLAGS = -g -Dmain=SDL_main -D__SDL_WRAPPER__


CFLAGS += -I$(BASEDIR) \
          -I$(WRAPPERDIR) \
          -Ic:/mingw/include/SDL

LFLAGS += -g -lm -mwindows -lmingw32 -lSDL_ttf -lSDLmain -lSDL -lSDL_image -lfreetype -lz -lole32 -loleaut32

TARGET = hivelytracker.exe

OBJECTS = sdl_wrapper.o \
          ht.o \
	  win32.o \
	  ../gui.o \
	  ../replay.o \
	  ../util.o \
	  ../undo.o \
	  ../about.o

all: $(TARGET)

-include $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) winicon.o
	$(CC) -o $(TARGET) $(OBJECTS) winicon.o $(LFLAGS)

winicon.o: winicon.ico hivelytracker.rc
	windres -i hivelytracker.rc -o winicon.o

$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d

clean:
	rm $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET) winicon.o
