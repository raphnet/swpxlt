#ifndef _rgb_image_h__
#define _rgb_image_h__

#include <stdint.h>

#include "palette.h"

typedef struct _pixel {
	uint8_t r, g, b;
	uint8_t reserved; // or alpha
} pixel_t;


typedef struct _rgbimage {
	int w, h;
	pixel_t *pixels;
} rgbimage_t;


pixel_t *getPixel(rgbimage_t *img, int x, int y);
void getPixelRGBsafe(rgbimage_t *img, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);
void setPixelRGB(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void setPixelRGBA(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void setPixelRGBsafe(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b);

rgbimage_t *allocRGBimage(int w, int h);
rgbimage_t *duplicateRGBimage(const rgbimage_t *original);
rgbimage_t *rgbi_loadPNG(const char *in_filename);
int rgbi_savePNG(const char *out_filename, rgbimage_t *img);
int rgbi_savePNG_indexed(const char *out_filename, rgbimage_t *img, const palette_t *pal);

void freeRGBimage(rgbimage_t *img);
void printRGBimage(rgbimage_t *img);
int pixelEqual(const pixel_t *a, const pixel_t *b);

enum {
	DITHERALGO_NONE = 0,
	DITHERALGO_FLOYD_STEINBERG,
	DITHERALGO_ERROR_DIFF_THRESHOLD,
};

// those will return NULL if the algo is invalid
const char *getDitherAlgoName(uint8_t algo);
const char *getDitherAlgoShortName(uint8_t algo);
// returns algo ID if found, otherwise -1
int getDitherAlgoByShortName(const char *sname);

int ditherImage_none(rgbimage_t *img, palette_t *pal);
int ditherImage_floyd_steinberg(rgbimage_t *img, palette_t *pal);
int ditherImage(rgbimage_t *img, palette_t *pal, uint8_t algo);

void quantizeRGBimage(rgbimage_t *img, int bits_per_component);


#endif // _rgb_image_h__

