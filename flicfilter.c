/*
    FlicFilter - Apply filters and transformations to a flic file
    Copyright (C) 2022 Raphael Assenat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "sprite.h"
#include "sprite_transform.h"
#include "flic.h"
#include "anim.h"

#define DEFAULT_PNG_DELAY	24
#define DEFAULT_OUTFILE	"out.flc"

static int applyFilter(animation_t *anim, int filter, const char *filterarg);

int g_verbose = 0;

#define MAX_FILTERS	16
static int filters[MAX_FILTERS];
static const char *filters_args[MAX_FILTERS];
static int num_filters = 0;

#define FIRST_OP	512

enum {
	OPT_DENOISE_SPIX = FIRST_OP,
	OPT_DENOISE_TEMPORAL1,
	OPT_QUANTIZE_PALETTE,
	OPT_GAIN,
	OPT_GAMMA,
	OPT_RESIZE,
};

static struct option long_options[] = {
	{ "help",             no_argument,        0, 'h' },
	{ "verbose",          no_argument,        0, 'v' },
	{ "out",              required_argument,  0, 'o' },

	{ "denoise_spix",     no_argument,        0, OPT_DENOISE_SPIX },
	{ "denoise_temporal1",no_argument,        0, OPT_DENOISE_TEMPORAL1 },
	{ "quantize_palette", required_argument,  0, OPT_QUANTIZE_PALETTE },
	{ "gain",             required_argument,  0, OPT_GAIN },
	{ "gamma",            required_argument,  0, OPT_GAMMA },
	{ "resize",           required_argument,  0, OPT_RESIZE },

	{ },
};

static void printHelp()
{
	printf("Usage: ./flicfilter [options] file\n");
	printf("\nflicfilter reads a flic file, applies transformations or filters, then writes\n");
	printf("a new file containing the results.\n");

	printf("\nOptions:\n");
	printf(" -h                       Print usage information\n");
	printf(" -v                       Enable verbose output\n");
	printf(" -o,--out=file            Set output file (default: %s)\n", DEFAULT_OUTFILE);
	printf("\nFilter options:\n");
	printf(" --resize WxY             Resize video size. Eg: 160x120\n");
	printf(" --gamma value            Apply gamma to palette. Eg: 1.6\n");
	printf(" --gain value             Apply gain to palette. Eg: 2.3\n");
	printf(" --quantize_palette=bits  Quantize palette to bits per color. For instance,\n");
	printf("                          for SMS, use 2.\n");
	printf("\n");
	printf("Some attempts at denoising: (YMMV)\n");
	printf(" --denoise_spix           Try to remove single pixel noise by replacing\n");
	printf("                          pixels completely surrounded by a given color\n");
	printf("                          by that color.\n");
	printf(" --denoise_temporal1      When a pixel change, only accept the change if\n");
	printf("                          the pixel is still that color in the next frame.\n");
}

int main(int argc, char **argv)
{
	int opt;
	//char *e;
	const char *infilename;
	const char *outfilename = DEFAULT_OUTFILE;
	FlicFile *outflic = NULL;
	animation_t *anim = NULL;
	int w = 0, h = 0;
	int delay = 0;
	int i;

	while ((opt = getopt_long_only(argc, argv, "hvo:", long_options, NULL)) != -1) {
		if (opt >= FIRST_OP) {
			if (num_filters >= MAX_FILTERS) {
				fprintf(stderr, "Too many filters\n");
				return -1;
			}
			printf("Adding filter %d to chain\n", opt);
			filters[num_filters] = opt;
			filters_args[num_filters] = optarg;
			num_filters++;
			continue;
		}
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'o': outfilename = optarg; break;
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

		anim = anim_load(infilename);
		if (!anim) {
			fprintf(stderr, "Error: Unsupported input file: %s\n", infilename);
			return -1;
		}

		for (i = 0; i<num_filters; i++) {
			if (applyFilter(anim, filters[i], filters_args[i])) {
				return -1;
			}
		}

		// frames may have been resized by filters, so use size of first
		// frame instead of animatino size
		w = anim->frames[0]->w;
		h = anim->frames[0]->h;

		/* Now that the size of the source material is known, create the output */
		if (!outflic) {
			outflic = flic_create(outfilename,w,h);
			if (!outflic) {
				return -1;
			}
		}


		if (outflic->header.speed == 0) {
			outflic->header.speed = anim->delay;
		}

		anim_addAllFramesToFlic(anim, outflic);
		anim_free(anim);
	}


	flic_close(outflic);

	return 0;
}

static void denoise_sprite_spix(sprite_t *spr)
{
	int x,y,X,Y;
	int i;
	int block[9];
	int color, newcolor;
	// 0 1 2
	// 3 4 5
	// 6 7 8

	for (y=0; y<spr->h; y++) {
		for (x=0; x<spr->w; x++) {

			// Get the 3x3 block of pixels
			i = 0;
			for (Y=0; Y<3; Y++) {
				for (X=0; X<3; X++) {
					block[i] = sprite_getPixelSafeExtend(spr, x+X, y+Y);
					i++;
				}
			}

			// Check if the surrounding pixels are all the same color
			color = block[0];
			newcolor = color;
			for (i=1; i<9; i++) {
				if (i == 4) // skip middle pixel
					continue;

				if (block[i] != color) {
					// keep original color
					newcolor = block[4];
					break;
				}
			}

			sprite_setPixel(spr, x, y, newcolor);
		}
	}
}

static void apply_denoise_spix(animation_t *anim)
{
	int i;

	printf("Appying SPIX denoise...\n");

	for (i=0; i<anim->num_frames; i++) {
		denoise_sprite_spix(anim->frames[i]);
	}
}

static void apply_denoise_tempral1(animation_t *anim)
{
	int i;
	int x,y;
	int color_prev, color_now, color_next;

	printf("Appying Temporal 1 denoise...\n");

	// start at the second frame
	for (i=1; i<anim->num_frames; i++) {

		for (y=0; y<anim->h; y++) {
			for (x=0; x<anim->w; x++) {

				color_prev = sprite_getPixel(anim->frames[i-1],x,y);
				color_now = sprite_getPixel(anim->frames[i],x,y);
				// ring frame?
				if (i == anim->num_frames-1) {
					color_next = sprite_getPixel(anim->frames[0],x,y);
				} else {
					color_next = sprite_getPixel(anim->frames[i+1],x,y);
				}

				if (color_now != color_prev) {
					if (color_next == color_now) {
						// ok, new color is present in at least 2 frames
					} else {
						// color is only in current frame, revert to previous
						// color
						sprite_setPixel(anim->frames[i],x,y,color_prev);
					}
				}

			}
		}

	}
}

static void apply_quantize_palette(animation_t *anim, int bits_per_component)
{
	int i;

	printf("Quantizing palette...\n");

	for (i=0; i<anim->num_frames; i++) {
		palette_quantize(&anim->frames[i]->palette, bits_per_component);
	}
}

static void apply_gain(animation_t *anim, double gain)
{
	int i;

	printf("Applying gain...\n");

	for (i=0; i<anim->num_frames; i++) {
		palette_gain(&anim->frames[i]->palette, gain);
	}
}

static void apply_gamma(animation_t *anim, double gamma)
{
	int i;

	printf("Applying gamma...\n");

	for (i=0; i<anim->num_frames; i++) {
		palette_gamma(&anim->frames[i]->palette, gamma);
	}
}

static void apply_resize(animation_t *anim, int w, int h)
{
	int i;
	sprite_t *orig, *replacement;

	printf("Resizing frames to %dx%d...\n",w,h);

	for (i=0; i<anim->num_frames; i++) {
		orig = anim->frames[i];

		replacement = allocSprite(w, h, 256, SPRITE_FLAG_OPAQUE);
		if (!replacement) {
			fprintf(stderr, "could not allocate scaled frame\n");
			return;
		}
		sprite_copyPalette(orig, replacement);

		sprite_scaleNearWH(orig, replacement, w, h);
		anim->frames[i] = replacement;

		freeSprite(orig);
	}

}

static int applyFilter(animation_t *anim, int filter, const char *filterarg)
{
	char *e;
	int val;
	double vald;
	int w,h,i;

	switch (filter)
	{
		case OPT_DENOISE_SPIX:
			apply_denoise_spix(anim);
			break;

		case OPT_DENOISE_TEMPORAL1:
			apply_denoise_tempral1(anim);
			break;

		case OPT_QUANTIZE_PALETTE:
			val = strtol(filterarg, &e, 0);
			if (e==filterarg) {
				fprintf(stderr, "Invalid filter argument\n");
				return -1;
			}
			if (val < 1 || val > 8) {
				fprintf(stderr, "Invalid filter argument\n");
				return -1;
			}
			apply_quantize_palette(anim, val);
			break;

		case OPT_GAIN:
			vald = strtod(filterarg, &e);
			if (e==filterarg) {
				fprintf(stderr, "Invalid filter argument\n");
				return -1;
			}
			apply_gain(anim, vald);
			break;

		case OPT_GAMMA:
			vald = strtod(filterarg, &e);
			if (e==filterarg) {
				fprintf(stderr, "Invalid filter argument\n");
				return -1;
			}
			apply_gamma(anim, vald);
			break;

		case OPT_RESIZE:
			i = sscanf(filterarg, "%dx%d", &w, &h);
			if (i != 2) {
				fprintf(stderr, "Invalid size\n");
				return -1;
			}
			if ((w < 1)||(h < 1)) {
				fprintf(stderr, "Invalid size\n");
				return -2;
			}
			apply_resize(anim, w, h);
			break;
	}

	return 0;
}

