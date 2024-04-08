#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "globals.h"

#include "swpxlt.h"
#include "sprite_transform.h"




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



