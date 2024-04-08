#include <math.h>
#include "sprite_transform.h"

void sprite_scaleNearWH(const sprite_t *src, sprite_t *dst, int w, int h)
{
	int x,y;
	uint8_t pixel;
	double factor_x, factor_y;

	factor_x = w / (double)src->w;
	factor_y = h / (double)src->h;

	for (y=0; y<dst->h; y++) {
		for (x=0; x<dst->w; x++) {

			pixel = sprite_getPixelSafeExtend(src, x/factor_x, y/factor_y);

			sprite_setPixelSafe(dst, x, y, pixel);
		}
	}

}



void sprite_scaleNear(const sprite_t *src, sprite_t *dst, double factor)
{
	int x,y;
	uint8_t pixel;

	for (y=0; y<dst->h; y++) {
		for (x=0; x<dst->w; x++) {

			pixel = sprite_getPixelSafeExtend(src, x/factor, y/factor);

			sprite_setPixelSafe(dst, x, y, pixel);
		}
	}

}

// From https://www.scale2x.it/algorithm
void sprite_scale2x(const sprite_t *src, sprite_t *dst)
{
	int x,y;
	uint8_t B,D,E,F,H,E0,E1,E2,E3;

	for (y=0; y<src->h; y++) {
		for (x=0; x<src->w; x++) {

			B = sprite_getPixelSafeExtend(src, x, y-1);
			D = sprite_getPixelSafeExtend(src, x-1, y);
			E = sprite_getPixelSafeExtend(src, x, y);
			F = sprite_getPixelSafeExtend(src, x+1, y);
			H = sprite_getPixelSafeExtend(src, x, y+1);

			if (B != H && D != F) {
				E0 = D == B ? D : E;
				E1 = B == F ? F : E;
				E2 = D == H ? D : E;
				E3 = H == F ? F : E;
			} else {
				E0 = E;
				E1 = E;
				E2 = E;
				E3 = E;
			}

			sprite_setPixelSafe(dst, x * 2, y * 2, E0);
			sprite_setPixelSafe(dst, x * 2 + 1, y * 2, E1);
			sprite_setPixelSafe(dst, x * 2, y * 2 + 1, E2);
			sprite_setPixelSafe(dst, x * 2 + 1, y * 2 + 1, E3);
		}
	}
}

void sprite_scale3x(const sprite_t *src, sprite_t *dst)
{
	int x,y;
	uint8_t A,B,C,D,E,F,G,H,I,E0,E1,E2,E3,E4,E5,E6,E7,E8;

	for (y=0; y<src->h; y++) {
		for (x=0; x<src->w; x++) {

			A = sprite_getPixelSafeExtend(src, x-1, y-1);
			B = sprite_getPixelSafeExtend(src, x, y-1);
			C = sprite_getPixelSafeExtend(src, x+1, y-1);

			D = sprite_getPixelSafeExtend(src, x-1, y);
			E = sprite_getPixelSafeExtend(src, x, y);
			F = sprite_getPixelSafeExtend(src, x+1, y);

			G = sprite_getPixelSafeExtend(src, x-1, y+1);
			H = sprite_getPixelSafeExtend(src, x, y+1);
			I = sprite_getPixelSafeExtend(src, x+1, y+1);

			if (B != H && D != F) {
				E0 = D == B ? D : E;
				E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
				E2 = B == F ? F : E;
				E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
				E4 = E;
				E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
				E6 = D == H ? D : E;
				E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
				E8 = H == F ? F : E;
			} else {
				E0 = E;
				E1 = E;
				E2 = E;
				E3 = E;
				E4 = E;
				E5 = E;
				E6 = E;
				E7 = E;
				E8 = E;
			}


			sprite_setPixelSafe(dst, x * 3, y * 3, E0);
			sprite_setPixelSafe(dst, x * 3 + 1, y * 3, E1);
			sprite_setPixelSafe(dst, x * 3 + 2, y * 3, E2);

			sprite_setPixelSafe(dst, x * 3, y * 3 + 1, E3);
			sprite_setPixelSafe(dst, x * 3 + 1, y * 3 + 1, E4);
			sprite_setPixelSafe(dst, x * 3 + 2, y * 3 + 1, E5);

			sprite_setPixelSafe(dst, x * 3, y * 3 + 2, E6);
			sprite_setPixelSafe(dst, x * 3 + 1, y * 3 + 2, E7);
			sprite_setPixelSafe(dst, x * 3 + 2, y * 3 + 2, E8);

		}
	}

}

/* Mutiply column vector vect[2] by square matrix mat[2][2] and store the result in result[2]
 */
static void applyMatrix(const double mat[2][2], const double vect[2], double result[2])
{
	result[0] = mat[0][0] * vect[0] + mat[0][1] * vect[1];
	result[1] = mat[1][0] * vect[0] + mat[1][1] * vect[1];
}

void sprite_rotate(const sprite_t *src, sprite_t *dst, double angle)
{
	int x,y;
	uint8_t pixel, mask;
	double rads = angle * (M_PI * 2) / 360.0;
	double xc, yc; // center of rotation
	double matrix[2][2] = {
		{ cos(rads), -sin(rads) },
		{ sin(rads), cos(rads) },
	};
	double v[2];
	double res[2];
	int src_x, src_y;

	xc = src->w / 2.0 - 0.5;
	yc = src->h / 2.0 - 0.5;

	for (y=0; y<dst->h; y++) {
		for (x=0; x<dst->w; x++) {

			v[0] = x-xc;
			v[1] = y-yc;
			applyMatrix(matrix, v, res);

			src_x = round(res[0] + xc);
			src_y = round(res[1] + yc);

			pixel = sprite_getPixelSafe(src, src_x, src_y);
			mask = sprite_getPixelMaskSafe(src, src_x, src_y);

			sprite_setPixelSafe(dst, x, y, pixel);
			sprite_setPixelMaskSafe(dst, x, y, mask);
		}
	}
}
