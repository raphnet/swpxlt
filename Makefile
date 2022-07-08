CC=gcc
LD=$(CC)
CFLAGS=-Wall -g `libpng-config --cflags` -O3
LDFLAGS=`libpng-config --libs` -lm

PROG=paltool png2vga swpxlt plasmagen dither
OBJS=*.o

all: $(PROG)

clean:
	rm -f $(PROG) $(OBJS)

paltool: paltool.o palette.o sprite.o
	$(LD) $(LDFLAGS) $^ -o $@

png2vga: png2vga.o
	$(LD) $(LDFLAGS) $^ -o $@

swpxlt: swpxlt.o sprite.o palette.o sprite_transform.o
	$(LD) $(LDFLAGS) $^ -o $@

dither: dither.o sprite.o palette.o rgbimage.o
	$(LD) $(LDFLAGS) $^ -o $@

plasmagen: plasmagen.o sprite.o palette.o
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CLFAGS) -c -o $@ $^
