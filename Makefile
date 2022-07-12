CC=gcc
LD=$(CC)
CFLAGS=-Wall -g `libpng-config --cflags` -O3
LDFLAGS=`libpng-config --libs` -lm

PROG=paltool png2vga png2cga swpxlt plasmagen dither flicinfo flic2png flicplay
OBJS=*.o
COMMON=palette.o sprite.o builtin_palettes.o rgbimage.o sprite_transform.o

all: $(PROG)

clean:
	rm -f $(PROG) $(OBJS)

paltool: paltool.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

png2vga: png2vga.o
	$(LD) $(LDFLAGS) $^ -o $@

png2cga: png2cga.o
	$(LD) $(LDFLAGS) $^ -o $@

swpxlt: swpxlt.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

dither: dither.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

plasmagen: plasmagen.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

flicinfo: flicinfo.o flic.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

flic2png: flic2png.o flic.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

flicplay: flicplay.o flic.o $(COMMON)
	$(LD) `sdl-config --libs` $(LDFLAGS) $^ -o $@

flicplay.o: flicplay.c
	$(CC) `sdl-config --cflags` $(CFLAGS) -c -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^
