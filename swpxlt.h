#ifndef _scaler_h__
#define _scaler_h__

#include "sprite.h"

enum {
	SCALE_ALGO_NOP = 0, // Do nothing
	SCALE_ALGO_NEAR, // Nearest neighboor
	SCALE_ALGO_2X, // Scale2x
};

enum {
	ROT_ALGO_NOP = 0, // Do nothing
	ROT_ALGO_NEAR, // Nearest neighbor (naive)
	ROT_ALGO_NICE2X, // Scale2x first, rotate, then scale 1/2
	ROT_ALGO_NICE3X, // Scale3x first, rotate, then scale 1/3
	ROT_ALGO_NICE4X, // Scale2x twice, rotate, then scale 1/4
	ROT_ALGO_NICE6X, // Scale3x, scale2x, rotate, then scale down 1/6
	ROT_ALGO_NICE8X, // Scale2x thrice, rotate, then scale 1/8
};


sprite_t *createScaledSprite(const sprite_t *spr_orig, int algo, double factor);
sprite_t *createRotatedSprite(const sprite_t *spr_orig, int algo, double angle);

#endif // _scaler_h__
