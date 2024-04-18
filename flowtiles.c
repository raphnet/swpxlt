#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "globals.h"

#include "swpxlt.h"
#include "sprite_transform.h"

#define MAX_SOURCE_IMAGES	256
#define DEFAULT_TILE_SIZE	8
#define DEFAULT_COLUMNS		16

int g_verbose;


static void printHelp()
{
	printf("Usage: ./flowtiles [options] -o output.png input_file1.png [input_file2.png] ...\n\n");

	printf("flowtiles reads one or more input images, extracts tiles (left-to-right, downwards)\n");
	printf("and writes them to the output image of specified width in tiles, left-to-right, downwards.\n");

	printf("\nValid options:\n");
	printf(" -h           Print usage information\n");
	printf(" -v           Enable verbose output\n");
	printf(" -o file      Output image file\n");
	printf(" -s tilesize  Set the working tile size (default %d)\n", DEFAULT_TILE_SIZE);
	printf(" -w width     Output image width (or column count) in tiles (default: %d)\n", DEFAULT_COLUMNS);
}

int main(int argc, char **argv)
{
	int retval = 0;
	int opt;
	const char *dstfn = NULL;
	char *e;
	int i, x, y;
	spriterect_t srcrect;
	spriterect_t dstrect;
	int tilesize = DEFAULT_TILE_SIZE;
	int columns = DEFAULT_COLUMNS;
	sprite_t *source_images[MAX_SOURCE_IMAGES];
	sprite_t *target_image = NULL;
	int source_images_count;
	int total_input_tiles;
	int target_w, target_h;
	int outcol, outrow;

	while ((opt = getopt(argc, argv, "hvo:s:w:")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'o': dstfn = optarg; break;
			case 's':
				tilesize = strtol(optarg, &e, 10);
				if (tilesize < 1) {
					fprintf(stderr, "tilesize should be >= 1\n");
					return -1;
				}
				break;
			case 'w':
				columns = strtol(optarg, &e, 10);
				if (columns < 1) {
					fprintf(stderr, "at least one column of tiles is required\n");
					return -1;
				}
				break;
		}
	}

	source_images_count = argc - optind;
	if (source_images_count < 1) {
		fprintf(stderr, "At least one input file must be specified\n");
		return -1;
	}
	if (!dstfn) {
		fprintf(stderr, "No output specified (-o)\n");
		return -1;
	}

	memset(source_images, 0, sizeof(source_images));

	total_input_tiles = 0;
	for (i=0; i<source_images_count; i++) {
		if (g_verbose) {
			printf("Loading: %s\n", argv[optind+i]);
		}

		// Load the image
		source_images[i] = sprite_loadPNG(argv[optind+i], 0, 0);
		if (!source_images[i]) {
			fprintf(stderr, "Could not load %s\n", argv[optind+i]);
			retval = 1;
			goto error;
		}

		// Validate dimensions
		if ((source_images[i]->w % tilesize) != 0 || ((source_images[i]->h % tilesize) != 0)) {
			fprintf(stderr, "Error, image %s size (%d x %d) is not a multiple of tilesize (%d)\n",
				argv[optind+i], source_images[i]->w, source_images[i]->h, tilesize);
			retval = 1;
			goto error;
		}

		total_input_tiles += (source_images[i]->w / 8) * (source_images[i]->h / 8);
	}

	target_w = columns * tilesize;
	target_h = tilesize * ((total_input_tiles + columns - 1) / columns);

	if (g_verbose) {
		printf("Tile size: %d\n", tilesize);
		printf("Output columns: %d\n", columns);
		printf("Input images loaded: %d\n", source_images_count);
		printf("Total input tiles: %d\n", total_input_tiles);
		printf("Target image: %s\n", dstfn);
		printf("Target size: %d x %d (%d x %d tiles)\n", target_w, target_h, target_w / tilesize, target_h / tilesize);
	}

	target_image = allocSprite(target_w, target_h,
					source_images[0]->palette.count,
					source_images[0]->flags );
	if (!target_image) {
		fprintf(stderr, "Could not create target image\n");
		retval = 1;
		goto error;
	}

	// TODO / Future features
	// Check that all palette match
	// Support auto-remap of colors (strict and best effort modes)
	// Add command-line option to use a master palette instead
	// of just picking the first one.
	sprite_copyPalette(source_images[0], target_image);

	srcrect.w = tilesize;
	srcrect.h = tilesize;
	dstrect.w = tilesize;
	dstrect.h = tilesize;
	outcol = 0;
	outrow = 0;
	for (i=0; i<source_images_count; i++)
	{
		sprite_t *src;
		src = source_images[i];
		for (y=0; y<src->h; y+= tilesize) {
			for (x=0; x<src->w; x+= tilesize) {

				srcrect.x = x;
				srcrect.y = y;
				dstrect.x = outcol * tilesize;
				dstrect.y = outrow * tilesize;

				sprite_copyRect(src, &srcrect, target_image, &dstrect);

				outcol++;
				if (outcol >= columns) {
					outcol = 0;
					outrow++;
				}
			}
		}
	}

	sprite_savePNG(dstfn, target_image, 0);
	retval = 0;

error:
	// Free all images
	for (i=0; i<MAX_SOURCE_IMAGES; i++) {
		if (source_images[i]) {
			freeSprite(source_images[i]);
		}
	}
	if (target_image) {
		freeSprite(target_image);
	}

	return retval;
}

