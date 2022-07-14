CC=gcc
LD=$(CC)
CFLAGS=-Wall -g `libpng-config --cflags` -O3
LDFLAGS=`libpng-config --libs` -lm

# Options
WITH_GIF_SUPPORT=1


PROG=paltool png2vga png2cga swpxlt plasmagen dither flicinfo flic2png flicplay flicmerge
OBJS=*.o
COMMON=palette.o sprite.o builtin_palettes.o rgbimage.o sprite_transform.o

SDL_CFLAGS=`sdl-config --cflags`
SDL_LIBS=`sdl-config --libs`

ifeq ($(WITH_GIF_SUPPORT),1)
LDFLAGS+=-lgif
# normally in /usr/include, so nothing needed
CFLAGS+=-DWITH_GIF_SUPPORT
endif

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

flicmerge: flicmerge.o flic.o anim.o $(COMMON)
	$(LD) $(LDFLAGS) $^ -o $@

flicplay: flicplay.o flic.o $(COMMON)
	$(LD) $(SDL_LIBS) $(LDFLAGS) $^ -o $@

flicplay.o: flicplay.c
	$(CC) $(SDL_CFLAGS) $(CFLAGS) -c -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^
