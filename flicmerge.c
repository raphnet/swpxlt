#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "sprite.h"
#include "flic.h"
#include "anim.h"

#define DEFAULT_PNG_DELAY	24
#define DEFAULT_OUTFILE	"out.flc"

int g_verbose = 0;

static void printHelp()
{
	printf("Usage: ./flicmerge [options] files...\n");
	printf("\nflicmerge reads 1 or more files (FLC/FLI/PNG) and outputs a single\n");
	printf(".FLC file.\n");

	printf("\nOptions:\n");
	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
	printf(" -o outfile        Set output file (default: %s)\n", DEFAULT_OUTFILE);
	printf(" -d delay_ms       Delay between frames in ms. Default: auto\n");
}

sprite_t *spriteFromFlicFrame(FlicFile *ff)
{
	sprite_t *s;

	s = allocSprite(ff->header.width, ff->header.height, 256, 0);
	if (!s)
		return NULL;

	memcpy(s->pixels, ff->pixels, ff->pixels_allocsize);
	memcpy(&s->palette, &ff->palette, sizeof(palette_t));

	return s;
}

int main(int argc, char **argv)
{
	int opt;
	char *e;
	const char *infilename;
	const char *outfilename = DEFAULT_OUTFILE;
	FlicFile *outflic = NULL;
	FlicFile *flic = NULL;
	sprite_t *img = NULL;
	animation_t *anim = NULL;
	int w = 0, h = 0;
	int delay = 0;

	while ((opt = getopt(argc, argv, "hvo:")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'o': outfilename = optarg; break;
			case 'd':
					delay = strtol(optarg, &e, 0);
					if ((e == optarg)||(delay<0)) {
						fprintf(stderr, "invalid delay\n");
						return 1;
					}
					break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No input file(s) specified. See -h for usage.\n");
		return -1;
	}

	/* If a size was specified, create the output context now, otherwise
	 * wait after loading the first image/frame. */
	if ((w!=0)&&(h!=0)) {
		outflic = flic_create(outfilename,w,h);
		if (!outflic) {
			return -1;
		}
		outflic->header.speed = delay;
	}

	/* Append all frames/images from arg list */
	for (;optind < argc; optind++) {
		infilename = argv[optind];
//		printf("Reading %s...\n", infilename);

		/* Load a new file*/

		if (isFlicFile(infilename)) {
			flic = flic_open(infilename);
			if (!flic) {
				fprintf(stderr, "Error in flic file or unsupported flic file\n");
				return -1;
			}
			printFlicInfo(flic);
			w = flic->header.width;
			h = flic->header.height;
		} else if ((img = sprite_loadPNG(infilename, 0, 0))) {
//			printf("Not a FLC/FLI file.\n");
			img = sprite_loadPNG(infilename, 0, 0);
			w = img->w;
			h = img->h;
		} else if ((anim = anim_load(infilename))) {
			w = anim->w;
			h = anim->h;
		} else {
			fprintf(stderr, "Error: Unsupported input file: %s\n", infilename);
			return -1;
		}

		/* Now that the size of the source material is known, create the output */
		if (!outflic) {
			outflic = flic_create(outfilename,w,h);
			if (!outflic) {
				return -1;
			}

		}

		if ((w != outflic->header.width) || (h != outflic->header.height)) {
			fprintf(stderr, "Error: Source material must all be of the same size\n");
			return -1;
		}

		/* Operate */
		if (flic) {
			if (outflic->header.speed == 0) {
				outflic->header.speed = flic->header.speed;
			}
			while (flic->cur_frame != flic->header.frames) {
				printf("  Reading frame %d\n", flic->cur_frame+1);
				flic_readOneFrame(flic, 0);
				if (flic_appendFrame(outflic, flic->pixels, &flic->palette)) {
					return -1;
				}
			}
		}
		if (img) {
			if (outflic->header.speed == 0) {
				outflic->header.speed = DEFAULT_PNG_DELAY;
			}
			if (flic_appendFrame(outflic, img->pixels, &img->palette)) {
				return -1;
			}
		}
		if (anim) {
			if (outflic->header.speed == 0) {
				outflic->header.speed = anim->delay;
			}
			anim_addAllFramesToFlic(anim, outflic);
		}


		/* Clean up */
		if (flic) {
			flic_close(flic);
			flic = NULL;
		}
		if (img) {
			freeSprite(img);
			img = NULL;
		}
		if (anim) {
			anim_free(anim);
			anim = NULL;
		}
	}


	flic_close(outflic);

	return 0;
}

