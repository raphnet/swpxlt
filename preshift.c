#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "globals.h"

#include "swpxlt.h"
#include "sprite_transform.h"

int g_verbose;
sprite_t *original_image = NULL, *target_image = NULL;


static void printHelp()
{
	printf("Usage: ./preshift [options] src.png dst.png\n");
	printf("\n");
	printf("Preshift takes a source image (typically a tile meant to be repeated\n");
	printf("in one or two directions making a seemless pattern) and generates\n");
	printf("a series of shifted variations (by the specified x/y increments) to create\n");
	printf("a sliding effect.\n");

	printf("\nValid options:\n");
	printf(" -h           Print usage information\n");
	printf(" -v           Enable verbose output\n");
	printf(" -x inc       X offset increment\n");
	printf(" -y inc       Y offset increment\n");
	printf(" -z           horiZontal (side to side) frame output, rather than vertical.\n");
}

int totalSteps(int x_inc, int y_inc, int w, int h)
{
	int count = 0;
	int x = 0, y = 0;

	do {
		x += x_inc;
		y += y_inc;
		x = x % w;
		y = y % h;
		count++;
	} while (!(x == 0 && y == 0));

	return count;
}

int main(int argc, char **argv)
{
	int opt;
	const char *sourcefn = NULL;
	const char *dstfn = NULL;
	char *e;
	int i;
	spriterect_t dstrect;
	spriterect_t srcrect;
	sprite_t *tmpsprite = NULL;
	int horizontal = 0;
	int x_inc = 1;
	int y_inc = 0;
	int steps;
	int x, y;

	while ((opt = getopt(argc, argv, "hvx:y:z")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'x': x_inc = strtol(optarg, &e, 10); break;
			case 'y': y_inc = strtol(optarg, &e, 10); break;
			case 'z': horizontal = 1; break;
		}
	}

	if ((argc - optind) < 2) {
		fprintf(stderr, "Input and output files must be specified\n");
		return -1;
	}
	sourcefn = argv[optind];
	dstfn = argv[optind + 1];

	if (g_verbose) {
		printf("Source image: %s\n", sourcefn);
		printf("Target image: %s\n", dstfn);
		printf("X offset increment: %d\n", x_inc);
		printf("Y offset increment: %d\n", y_inc);
		printf("Layout: %s\n", horizontal ? "Horizontal" : "Vertical");
	}

	original_image = sprite_loadPNG(sourcefn, 0, 0);
	if (!original_image) {
		return -1;
	}

	steps = totalSteps(x_inc, y_inc, original_image->w, original_image->h);

	if (horizontal) {
		target_image = allocSprite(original_image->w * steps,
		            original_image->h,
					original_image->palette.count,
					original_image->flags );
	} else {
		target_image = allocSprite(original_image->w,
		            original_image->h * steps,
					original_image->palette.count,
					original_image->flags );
	}
	if (!target_image) {
		fprintf(stderr, "Could not create target\n");
		return -1;
	}

	sprite_copyPalette(original_image, target_image);

	if (g_verbose) {
		printf("Source dimensions: %d x %d\n", original_image->w, original_image->h);
		printf("Target dimensions: %d x %d\n", target_image->w, target_image->h);
		printf("Steps: %d\n", steps);
	}

	// Create a 3x3 mosaic of original image, to use as a source
	tmpsprite = allocSprite(original_image->w * 3,
		            original_image->h * 3,
					original_image->palette.count,
					original_image->flags );
	if (!tmpsprite) {
		fprintf(stderr, "Could not create temporary image\n");
		return -1;
	}
	sprite_copyPalette(original_image, tmpsprite);

	dstrect.w = original_image->w;
	dstrect.h = original_image->h;
	for (y=0; y<3; y++) {
		for (x=0; x<3; x++) {
			dstrect.x = x*original_image->w;
			dstrect.y = y*original_image->h;
			sprite_copyRect(original_image, NULL, tmpsprite, &dstrect);
		}
	}

	srcrect.w = original_image->w;
	srcrect.h = original_image->h;
	dstrect.w = original_image->w;
	dstrect.h = original_image->h;
	x = 0;
	y = 0;
	for (i=0; i<steps; i++) {
		if (horizontal) {
			dstrect.x = i*original_image->w;
			dstrect.y = 0;
		} else {
			dstrect.x = 0;
			dstrect.y = i*original_image->w;
		}

		srcrect.x = original_image->w + x;
		srcrect.y = original_image->h + y;

		sprite_copyRect(tmpsprite, &srcrect, target_image, &dstrect);

		x += x_inc;
		y += y_inc;
		x = x % original_image->w;
		y = y % original_image->h;
	}

	sprite_savePNG(dstfn, target_image, 0);

	if (original_image) {
		freeSprite(original_image);
	}

	return 0;
}

