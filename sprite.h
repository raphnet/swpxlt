#ifndef _sprite_h__
#define _sprite_h__

#include <stdio.h>
#include <stdint.h>

#define SPRITE_FLAG_OPAQUE	1
#define SPRITE_FLAG_USE_TRANSPARENT_COLOR	2

#include "palette.h"
typedef struct sprite {
	uint16_t w, h;
	uint8_t transparent_color;
	palette_t palette;
	uint8_t *pixels;
	uint8_t *mask;
	uint8_t flags;
} sprite_t;

typedef struct spriterect {
	int x,y,w,h;
} spriterect_t;

sprite_t *allocSprite(uint16_t w, uint16_t h, uint16_t palsize, uint8_t flags);
sprite_t *duplicateSprite(const sprite_t *spr);
void freeSprite(sprite_t *spr);


#define SPRITE_LOADFLAG_DROP_TRANSPARENT	1
#define SPRITE_ACCEPT_RGB					2
sprite_t *sprite_loadPNG(const char *in_filename, int n_expected_colors, uint32_t flags);
int sprite_savePNG(const char *out_filename, sprite_t *spr, uint32_t flags);

void printSprite(sprite_t *spr);
void sprite_setPixel(sprite_t *spr, int x, int y, int value);
void sprite_setPixelSafe(sprite_t *spr, int x, int y, int value);
int sprite_getPixel(const sprite_t *spr, int x, int y);
int sprite_getPixelSafe(const sprite_t *spr, int x, int y);
int sprite_getPixelSafeExtend(const sprite_t *spr, int x, int y);
int sprite_fillRect(struct sprite *spr, int x, int y, int w, int h, int color);


void sprite_setPixelMask(sprite_t *spr, int x, int y, int value);
void sprite_setPixelMaskSafe(sprite_t *spr, int x, int y, int value);
int sprite_getPixelMask(const sprite_t *spr, int x, int y);
int sprite_getPixelMaskSafe(const sprite_t *spr, int x, int y);

void sprite_applyPalette(sprite_t *sprite, const palette_t *newpal);
int sprite_copyPalette(const sprite_t *spr_src, sprite_t *spr_dst);
int sprite_remapPalette(sprite_t *sprite, const palette_t *dstpal);
sprite_t *sprite_packPixels(const sprite_t *spr, int bits_per_pixel);

void sprite_getFullRect(const sprite_t *src, spriterect_t *dst);
void sprite_copyRect(const sprite_t *src, const spriterect_t *src_rect, sprite_t *dst, const spriterect_t *dst_rect);


#endif // _sprite_h__
