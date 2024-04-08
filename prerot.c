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
	printf("Usage: ./prerot [options] src.png dst.png\n");
	printf("\nValid options:\n");
	printf(" -h           Print usage information\n");
	printf(" -v           Enable verbose output\n");
	printf(" -r angle     Rotation angle per frame in degrees\n");
	printf(" -f count     Frame count (including identity)\n");
	printf(" -a           Auto-repair or add a black sprite contour\n");
	printf(" -z           horiZontal (side to side) frame output, rather than vertical.\n");
}

int is_multiple_of_90(double angle)
{
	angle /= 90.0;

	if (fabs(angle - (int)angle) < 0.1) {
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	const char *sourcefn = NULL;
	const char *dstfn = NULL;
	double rotate_step = 22.5;
	int frame_count = 4;
	char *e;
	int i;
	double angle;
	spriterect_t dstrect;
	sprite_t *tmpsprite;
	uint8_t rotalgo;
	int autorepair = 0;
	int horizontal = 0;

	while ((opt = getopt(argc, argv, "hvr:f:az")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'a': autorepair = 1; break;
			case 'z': horizontal = 1; break;
			case 'r':
				rotate_step = strtod(optarg, &e);
				if (e == optarg) {
					perror(optarg);
					return -1;
				}
				break;
			case 'f':
				frame_count = strtol(optarg, &e, 10);
				if (frame_count <= 0) {
					fprintf(stderr, "Frame count must be >=1\n");
					return -1;
				}
				break;
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
		printf("Rotate step angle: %.2f\n", rotate_step);
		printf("Frame count: %d\n", frame_count);
		printf("Layout: %s\n", horizontal ? "Horizontal" : "Vertical");
		printf("Auto-repair border: %s\n", autorepair ? "Yes" : "No");
	}

	original_image = sprite_loadPNG(sourcefn, 0, 0);
	if (!original_image) {
		return -1;
	}

	if (horizontal) {
		target_image = allocSprite(original_image->w * frame_count,
		            original_image->h,
					original_image->palette.count,
					original_image->flags );
	} else {
		target_image = allocSprite(original_image->w,
		            original_image->h * frame_count,
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
	}

	angle = 0.0;
	dstrect.x = 0;
	dstrect.y = 0;
	dstrect.w = original_image->w;
	dstrect.h = original_image->h;
	for (i=0; i<frame_count; i++) {

		// TODO : This should probably be configurable...
		rotalgo = ROT_ALGO_NICE2X;
		if (is_multiple_of_90(angle)) {
			rotalgo = ROT_ALGO_NEAR;
		}

		if (g_verbose) {
			printf("Angle... %.2f algo %d\n", angle, rotalgo);
		}

		tmpsprite = createRotatedSprite(original_image, rotalgo, angle);

		if (autorepair) {
			sprite_autoBlackContour(tmpsprite);
		}

		sprite_copyRect(tmpsprite, NULL, target_image, &dstrect);
		freeSprite(tmpsprite);

		angle += rotate_step;

		if (horizontal) {
			dstrect.x += dstrect.w;
		} else {
			dstrect.y += dstrect.h;
		}
	}

	sprite_savePNG(dstfn, target_image, 0);

	if (original_image) {
		freeSprite(original_image);
	}

	return 0;
}

