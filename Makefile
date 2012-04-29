LIBS_FIX=-lz
LIBS_FLIP=-lpng
CC=gcc

all: fixpng

fixpng: fixpng.c
	$(CC) -Wall -o fixpng fixpng.c $(LIBS_FIX) `pkg-config --cflags --libs gdk-pixbuf-2.0`

clean:
	rm -f flipchannels fixpng
