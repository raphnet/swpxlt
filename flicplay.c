#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "sprite.h"
#include "sprite_transform.h"
#include "flic.h"
#include "SDL.h"

int g_verbose = 0;
SDL_Surface *g_screen_surface;

static void printHelp()
{
	printf("Usage: ./flic2png [options] file.[fli,flc]\n");
	printf("\n");
	printf("flic2png read a .FLI or .FLC file and outputs all frames to individual PNG files.\n");

	printf("\nOptions:\n");
	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
	printf(" -s factor         Scale factor (default: 1.0)\n");
	printf(" -d delay_ms       Override delay between frames\n");
}

void sprite_to_SDL(sprite_t *spr, SDL_Surface *surface)
{
	int y;
	Uint8 *p;
	SDL_Color colors[256];

	for (y=0; y<spr->palette.count; y++) {
		colors[y].r = spr->palette.colors[y].r;
		colors[y].g = spr->palette.colors[y].g;
		colors[y].b = spr->palette.colors[y].b;
//		printf("%d,%d,%d\n", colors[y].r, colors[y].g, colors[y].b);
	}

	SDL_LockSurface(g_screen_surface);

	for (y=0; y<spr->h; y++) {
		p = (Uint8 *)surface->pixels + y * surface->pitch;
		memcpy(p, spr->pixels + y * spr->w, spr->w);
	}

	SDL_UnlockSurface(g_screen_surface);

	if (!SDL_SetColors(g_screen_surface, colors, 0, 256)) {
		fprintf(stderr, "SDL_SetColors failed\n");
	}
}

void flicFrameToSprite(FlicFile *ff, sprite_t *s)
{
	memcpy(s->pixels, ff->pixels, ff->pixels_allocsize);
	memcpy(&s->palette, &ff->palette, sizeof(palette_t));
}



int main(int argc, char **argv)
{
	int opt;
	const char *infilename;
	char *e;
	FlicFile *flic;
	int loop = 1;
	double scale_factor = 1.0;
	int w,h,mustrun=1;
	int frame_delay = 0;
	sprite_t *flicsprite;
	sprite_t *scaledsprite;

	while ((opt = getopt(argc, argv, "hvo:s:d:")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose++; break;
			case 's':
				scale_factor = strtod(optarg, &e);
				if (e == optarg) {
					perror(optarg);
					return -1;
				}
				break;
			case 'd':
				frame_delay = strtol(optarg, &e, 0);
				if (e == optarg) {
					perror(optarg);
					return -1;
				}
				break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No file specified. See -h for usage.\n");
		return -1;
	}

	infilename = argv[optind];
	if (g_verbose) {
		printf("Opening %s...\n", infilename);
	}

	flic = flic_open(infilename);
	if (!flic) {
		fprintf(stderr, "Error in flic file or unsupported flic file\n");
		return -1;
	}

	printFlicInfo(flic);

	if (SDL_Init(SDL_INIT_VIDEO)) {
		flic_close(flic);
		fprintf(stderr, "SDL error\n");
		return -1;
	}

	flicsprite = allocSprite(flic->header.width, flic->header.height, 256, 0);

	w = flic->header.width * scale_factor;
	h = flic->header.height * scale_factor;
	scaledsprite = allocSprite(w, h, 256, 0);

	g_screen_surface = SDL_SetVideoMode(w, h, 8, SDL_SWSURFACE);
	if (!g_screen_surface) {
		flic_close(flic);
		SDL_Quit();
		fprintf(stderr, "Could not create screen surface\n");
		return -1;
	}

	while ((0 == flic_readOneFrame(flic, loop)) && mustrun) {
		SDL_Event ev;

		// Convert Flic frame to sprite_t
		flicFrameToSprite(flic, flicsprite);

		// Scale it up
		sprite_copyPalette(flicsprite, scaledsprite);
		sprite_scaleNear(flicsprite, scaledsprite, scale_factor);

		// Send to SDL surface
		sprite_to_SDL(scaledsprite, g_screen_surface);

//		printf("Frame %d. Delay %d\n", flic->cur_frame, flic->header.speed);
		SDL_Flip(g_screen_surface);

		if (frame_delay != 0) {
			SDL_Delay(frame_delay);
		} else {
			SDL_Delay(flic->header.speed);
		}

		// Drain all events
		while (SDL_PollEvent(&ev)) {
			switch (ev.type)
			{
				case SDL_QUIT: mustrun = 0; break;
			}
		}

	}


	flic_close(flic);
	SDL_Quit();

	return 0;
}

