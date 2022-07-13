#ifndef _palette_h__
#define _palette_h__

#include <stdint.h>

typedef struct sprite_palette_entry {
	uint8_t r, g, b;
	uint8_t padding;
} palent_t;

typedef struct palette {
	int count;
	palent_t colors[256];
} palette_t;

typedef struct palette_remap_lut {
	uint8_t lut[256];
} palremap_lut_t;

void palette_print(const palette_t *pal);
void palette_print_24bit(const palette_t *pal);

enum {
	COLORMATCH_METHOD_DEFAULT = 0,
};

int palette_findBestMatch(const palette_t *pal, int r, int g, int b, int method);
void palette_setColor(palette_t * pal, uint8_t index, int r, int g, int b);
void palette_addColor(palette_t * pal, int r, int g, int b);
int palette_findColor(const palette_t *pal, int r, int g, int b);
void palette_clear(palette_t * pal);

int palette_loadGimpPalette(const char *filename, palette_t *dst);
int palette_loadJascPalette(const char *filename, palette_t *dst);
int palette_loadFromPng(const char *filename, palette_t *dst);

int palette_load(const char *filename, palette_t *dst);

int palette_generateRemap(const palette_t *srcpal, const palette_t *dstpal, palremap_lut_t *dst);
int palettes_match(const palette_t *pal1, const palette_t *pal2);

#endif // _palette_h__
