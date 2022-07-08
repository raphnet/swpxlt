#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "globals.h"

#include "swpxlt.h"
#include "sprite_transform.h"

int g_verbose = 0;


#define DEFAULT_SCALE_ALGO	SCALE_ALGO_NEAR
#define DEFAULT_ROT_ALGO	ROT_ALGO_NICE6X

struct algomap {
	int algo;
	const char *name;
};

struct algomap scale_algos[] = {
	{	SCALE_ALGO_NOP, "NOP"	},
	{	SCALE_ALGO_NEAR, "NEAR"	},
	{	SCALE_ALGO_2X, "SCALE2X" },
	{	} // terminator
};

struct algomap rot_algos[] = {
	{	ROT_ALGO_NOP, "NOP"	},
	{	ROT_ALGO_NEAR, "NEAR" },
	{	ROT_ALGO_NICE2X, "NICE2X"	},
	{	ROT_ALGO_NICE3X, "NICE3X"	},
	{	ROT_ALGO_NICE4X, "NICE4X"	},
	{	ROT_ALGO_NICE6X, "NICE6X"	},
	{	ROT_ALGO_NICE8X, "NICE8X"	},
	{	} // terminator
};


int parseAlgoName(const struct algomap *map, const char *name)
{
	int i=0;

	for (i=0; map[i].name; i++) {
		if (0==strcasecmp(map[i].name,name)) {
			return map[i].algo;
		}
	}

	return -1;
}

const char *getAlgoName(const struct algomap *map, int id)
{
	int i=0;

	for (i=0; map[i].name; i++) {
		if (id==map[i].algo) {
			return map[i].name;
		}
	}

	return NULL;
}

void listAlgos(const struct algomap *map)
{
	int i;

	for (i=0; map[i].name; i++) {
		printf("  %s\n", map[i].name);
	}
}

void printHelp(void)
{
	printf("Usage: ./scaler [options]\n");

	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
	printf(" -i input_file     Replace working buffer by image\n");
	printf(" -T idx            Specify transparent color index\n");
	printf(" -o output_file    Write working buffer to file\n");
	printf(" -S algo           Scaling algorithm to use (default: %s)\n", getAlgoName(scale_algos, DEFAULT_SCALE_ALGO));
	printf(" -s scale_factor   Scale by factor (1.0 = same size)\n");
	printf(" -R algo		   Rotation algorithm to use (defualt: %s)\n", getAlgoName(rot_algos, DEFAULT_ROT_ALGO));
	printf(" -r angle          Rotate by angle (degrees)\n");
	printf("\n\n");

	printf("Available scaling altorithms:\n");
	listAlgos(scale_algos);
	printf("\n");

	printf("Available scaling altorithms:\n");
	listAlgos(rot_algos);
}

int performRotateOperation(const sprite_t *spr_orig, sprite_t *spr_dst, int algo, double angle)
{
	sprite_t *tmp1, *tmp2;

	switch(algo)
	{
		case ROT_ALGO_NOP:
			sprite_copyRect(spr_orig, NULL, spr_dst, NULL);
			return 0;

		case ROT_ALGO_NEAR:
			sprite_rotate(spr_orig, spr_dst, angle);
			return 0;

		case ROT_ALGO_NICE2X:
			tmp1 = createScaledSprite(spr_orig, SCALE_ALGO_2X, 2.0);
			tmp2 = createRotatedSprite(tmp1, ROT_ALGO_NEAR, angle);
			freeSprite(tmp1);
			sprite_scaleNear(tmp2, spr_dst, 0.5);
			freeSprite(tmp2);
			return 0;

		case ROT_ALGO_NICE3X:
			tmp1 = createScaledSprite(spr_orig, SCALE_ALGO_2X, 3.0);
			tmp2 = createRotatedSprite(tmp1, ROT_ALGO_NEAR, angle);
			freeSprite(tmp1);
			sprite_scaleNear(tmp2, spr_dst, 0.3333334);
			freeSprite(tmp2);
			return 0;

		case ROT_ALGO_NICE4X:
			tmp1 = createScaledSprite(spr_orig, SCALE_ALGO_2X, 4.0);
			tmp2 = createRotatedSprite(tmp1, ROT_ALGO_NEAR, angle);
			freeSprite(tmp1);
			sprite_scaleNear(tmp2, spr_dst, 0.25);
			freeSprite(tmp2);
			return 0;

		case ROT_ALGO_NICE6X:
			// Scale 3x
			tmp1 = createScaledSprite(spr_orig, SCALE_ALGO_2X, 3.0);

			// Scale 2x
			tmp2 = createScaledSprite(tmp1, SCALE_ALGO_2X, 2.0);
			freeSprite(tmp1);

			// Rotate
			tmp1 = createRotatedSprite(tmp2, ROT_ALGO_NEAR, angle);
			freeSprite(tmp2);

			// Scale down 1/8
			sprite_scaleNear(tmp1, spr_dst, 0.166667);
			freeSprite(tmp1);

			return 0;


		case ROT_ALGO_NICE8X:
			// Scale 4x
			tmp1 = createScaledSprite(spr_orig, SCALE_ALGO_2X, 4.0);

			// Scale 2x
			tmp2 = createScaledSprite(tmp1, SCALE_ALGO_2X, 2.0);
			freeSprite(tmp1);

			// Rotate
			tmp1 = createRotatedSprite(tmp2, ROT_ALGO_NEAR, angle);
			freeSprite(tmp2);

			// Scale down 1/8
			sprite_scaleNear(tmp1, spr_dst, 0.125);
			freeSprite(tmp1);

			return 0;


	}

	return -1;
}


int performScaleOperation(const sprite_t *spr_orig, sprite_t *spr_dst, int algo, double factor)
{
	sprite_t *tmp;

	switch(algo)
	{
		case SCALE_ALGO_NOP:
			sprite_copyRect(spr_orig, NULL, spr_dst, NULL);
			return 0;

		case SCALE_ALGO_NEAR:
			sprite_scaleNear(spr_orig, spr_dst, factor);
			return 0;

		case SCALE_ALGO_2X:
			if (factor == 2.0) {
				sprite_scale2x(spr_orig, spr_dst);
			} else if (factor == 3.0) {
				sprite_scale3x(spr_orig, spr_dst);
			} else if (factor == 4.0) {
				tmp = duplicateSprite(spr_dst);
				if (!tmp)
					return -1;

				sprite_scale2x(spr_orig, tmp);
				sprite_scale2x(tmp, spr_dst);
			} else {
				fprintf(stderr, "Error: Scale2x does not support factor %.2f\n", factor);
				return -1;
			}
			return 0;

	}

	return -1;
}

sprite_t *createScaledSprite(const sprite_t *spr_orig, int algo, double factor)
{
	sprite_t *s;

	s = allocSprite(spr_orig->w * factor,
						spr_orig->h * factor,
						spr_orig->palette.count, 0);
	if (!s)
		return NULL;

	sprite_copyPalette(spr_orig, s);
	s->transparent_color = spr_orig->transparent_color;
	s->flags = spr_orig->flags;

	if (performScaleOperation(spr_orig, s, algo, factor)) {
		fprintf(stderr, "Scale failed - %d - %.2f\n", algo, factor);
		freeSprite(s);
		return NULL;
	}

	return s;
}

sprite_t *createRotatedSprite(const sprite_t *spr_orig, int algo, double angle)
{
	sprite_t *s;

	s = duplicateSprite(spr_orig);
	if (!s)
		return NULL;


	if (performRotateOperation(spr_orig, s, algo, angle)) {
		fprintf(stderr, "Rotate failed - %d - %.2f\n", algo, angle);
		freeSprite(s);
		return NULL;
	}


	return s;
}


int main(int argc, char **argv)
{
	int opt, scale_algo = DEFAULT_SCALE_ALGO, rot_algo = DEFAULT_ROT_ALGO;
	int retcode = 0;
	char *e;
	double scale_factor = 1.0;
	double rotate_angle = 0;
	int force_transparent_color = 0;
	int transparent_color = 0;
	sprite_t *working_image = NULL, *tmp_image = NULL;

	while ((opt = getopt(argc, argv, "hvi:o:S:s:R:r:T:")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;

			case 'T':
					transparent_color = strtol(optarg, &e, 0);
					if ((e == optarg) || (transparent_color<0) || (transparent_color>255) ) {
						fprintf(stderr, "Invalid color specified\n");
						retcode = 1;
						goto error;
					}
					force_transparent_color = 1;
					break;

			case 'i': // Load an input image
					if (working_image) {
						fprintf(stderr, "Warning: Replacing working image by %s", optarg);
						freeSprite(working_image);
					}
					working_image = sprite_loadPNG(optarg, 0, 0);
					if (!working_image) {
						retcode = 1;
						goto error;
					}

					if (force_transparent_color) {
						if (working_image->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR) {
							if (working_image->transparent_color != transparent_color) {
								fprintf(stderr, "Warning: File-defined transparent color (%d) overridden from command line (-C %d)\n",
									working_image->transparent_color, transparent_color);
							}
						}
						working_image->transparent_color = transparent_color;
						working_image->flags |= SPRITE_FLAG_USE_TRANSPARENT_COLOR;
					}

					if (g_verbose) {
						printf("Loaded image %s (%d x %d), flags 0x%02x - t %d\n",
							optarg,
							working_image->w,
							working_image->h,
							working_image->flags,
							working_image->transparent_color);
					}
					break;

			case 'o': // Write current buffer to file
					sprite_savePNG(optarg, working_image, 0);

					if (g_verbose) {
						printf("Write image %s (%d x %d), flags 0x%02x - t %d\n",
							optarg,
							working_image->w,
							working_image->h,
							working_image->flags,
							working_image->transparent_color);
					}
					break;

			case 'S':
				scale_algo = parseAlgoName(scale_algos, optarg);
				if (scale_algo < 0) {
					fprintf(stderr, "Unknown scale algo: %s\n", optarg);
					retcode = 1;
					goto error;
				}
				break;

			case 's': // Scale working image
				if (!working_image) {
					fprintf(stderr, "Error: No image loaded.\n");
					retcode = 1;
					goto error;
				}
				scale_factor = strtod(optarg, &e);
				if (e == optarg) {
					perror(optarg);
					retcode = 1;
					goto error;
				}
				tmp_image = createScaledSprite(working_image, scale_algo, scale_factor);
				if (!tmp_image) {
					retcode = 1;
					goto error;
				}
				freeSprite(working_image);
				working_image = tmp_image;

				break;

			case 'R':
				rot_algo = parseAlgoName(rot_algos, optarg);
				if (scale_algo < 0) {
					fprintf(stderr, "Unknown rotation algo: %s\n", optarg);
					retcode = 1;
					goto error;
				}
				break;

			case 'r': // Rotate working image
				if (!working_image) {
					fprintf(stderr, "Error: No image loaded.\n");
					retcode = 1;
					goto error;
				}

				rotate_angle = strtod(optarg, &e);
				if (e == optarg) {
					perror(optarg);
					retcode = 1;
					goto error;
				}

				tmp_image = createRotatedSprite(working_image, rot_algo, rotate_angle);
				if (!tmp_image) {
					retcode = 1;
					goto error;
				}
				freeSprite(working_image);
				working_image = tmp_image;

				break;

		}
	}

	retcode = 0;
error:

	if (working_image) {
		freeSprite(working_image);
	}

	return retcode;
}
