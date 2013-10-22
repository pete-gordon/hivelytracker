CC = m68k-amigaos-gcc
AR = ar
RANLIB = ranlib

ifeq ($(CC),vc)
	WARN = -dontwarn=153 -dontwarn=214
	OPT = 
	CPU = -cpu=68060 -fpu=68881 +aos68k_warpos
	LFLAGS = -lauto -lm040 -lamiga

	ifeq ($(TARG),morphos)
		DEFS = -D__MORPHOS__ -DNO_INLINE_STDARG
		CPU = +morphos_warpos
		LFLAGS = +morphos_warpos -Lvbcc:PosixLib/Morphos -lposix -lamiga -lm -lauto
	endif

	ifeq ($(TARG),warpos)
		DEFS = -D__WARPOS__
		CPU = +warpos_warpos
		LFLAGS = +warpos_warpos -lamiga -lm -lauto
	endif

	CFLAGS = -Ivbcc:PosixLib/include -c99 $(WARN) $(CPU) $(OPT) $(DEFS) -Iinclude
endif

ifneq (,$(findstring gcc,$(CC)))
	CPU = -m68020-60 -m68881 -O2 -fomit-frame-pointer
	DEFS = -D__OS3_WRAPPER__
	OPT =
	CFLAGS = -Iinclude -I.. -I. $(CPU) $(OPT) $(DEFS)
	LFLAGS = -noixemul -lm
endif

all: ht

ht: ht.o gui.o util.o replay.o about.o undo.o os3_wrapper.o
	$(CC) -o ht ht.o gui.o util.o replay.o about.o undo.o os3_wrapper.o $(LFLAGS)

ht.o: ../ht.c ../gui.h ../replay.h ../about.h
	$(CC) $(CFLAGS) -c ../ht.c -o ht.o

gui.o: ../gui.c ../gui.h ../util.h ../replay.h
	$(CC) -c ../gui.c -o gui.o $(CFLAGS)

util.o: ../util.c ../util.h
	$(CC) -c ../util.c -o util.o $(CFLAGS)

replay.o: ../replay.c ../replay.h ../undo.h
	$(CC) $(CFLAGS) -c ../replay.c -o replay.o

about.o: ../about.c ../about.h ../gui.h
	$(CC) -c ../about.c -o about.o $(CFLAGS)

undo.o: ../undo.c ../undo.h ../replay.h
	$(CC) -c ../undo.c -o undo.o $(CFLAGS)

os3_wrapper.o: os3_wrapper.c os3_wrapper.h
	$(CC) -c os3_wrapper.c -o os3_wrapper.o $(CFLAGS)
