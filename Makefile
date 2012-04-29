LIBS_FIX=-lz
LIBS_FLIP=-lpng
INCLUDES_FLIP=-I/usr/local/include
CC=gcc

all: fixpng flipchannels

fixpng: fixpng.c
	$(CC) -o fixpng fixpng.c $(LIBS_FIX)

flipchannels: flipchannels.c
	$(CC) -o flipchannels flipchannels.c $(LIBS_FLIP) $(INCLUDES_FLIP)
