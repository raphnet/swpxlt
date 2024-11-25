CC=gcc
LD=$(CC)
CFLAGS=-Wall -g `libpng-config --cflags` -O1 # -Werror
LIBS=`libpng-config --libs` -lm

# Options
WITH_GIF_SUPPORT=1


PROG=paltool png2vga png2cga swpxlt plasmagen dither flicinfo flic2png flicplay flicmerge flicfilter scrollmaker img2sms anim2sms prerot preshift flowtiles
OBJS=*.o
COMMON=palette.o sprite.o builtin_palettes.o rgbimage.o sprite_transform.o util.o growbuf.o swpxlt.o

SDL_CFLAGS=`sdl-config --cflags`
SDL_LIBS=`sdl-config --libs`

ifeq ($(WITH_GIF_SUPPORT),1)
LIBS+=-lgif
# normally in /usr/include, so nothing needed
CFLAGS+=-DWITH_GIF_SUPPORT
endif

all: $(PROG)

clean:
	rm -f $(PROG) $(OBJS)

paltool: paltool.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

png2vga: png2vga.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

png2cga: png2cga.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

prerot: prerot.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

preshift: preshift.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

flowtiles: flowtiles.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

swpxlt: swpxlt.o swpxlt_main.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

dither: dither.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

plasmagen: plasmagen.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

flicinfo: flicinfo.o flic.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

flic2png: flic2png.o flic.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

img2sms: img2sms.o tilecatalog.o tilemap.o tilereducer.o flic.o anim.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

anim2sms: anim2sms.o tilecatalog.o tilemap.o tilereducer.o flic.o anim.o smsanimencoder.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

scrollmaker: scrollmaker.o flic.o anim.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

flicmerge: flicmerge.o flic.o anim.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

flicplay: flicplay.o flic.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) $(SDL_LIBS) -o $@

flicplay.o: flicplay.c
	$(CC) $(SDL_CFLAGS) $(CFLAGS) -c -o $@ $^

flicfilter: flicfilter.o flic.o anim.o $(COMMON)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^
