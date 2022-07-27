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

enum {
	PALETTE_FORMAT_NONE = 0,
	PALETTE_FORMAT_VGAASM,
	PALETTE_FORMAT_PNG,
	PALETTE_FORMAT_ANIMATOR,
	PALETTE_FORMAT_ANIMATOR_PRO,
	PALETTE_FORMAT_SMS_WLADX,
	PALETTE_FORMAT_SMS_BIN,
};

// returns PALETTE_FORMAT_NONE if unknown
int palette_parseOutputFormat(const char *arg);

int palette_findBestMatch(const palette_t *pal, int r, int g, int b, int method);
void palette_setColor(palette_t * pal, uint8_t index, int r, int g, int b);
void palette_addColor(palette_t * pal, int r, int g, int b);
void palette_addColorEnt(palette_t *pal, palent_t *entry);
int palette_findColor(const palette_t *pal, int r, int g, int b);
int palette_findColorEnt(const palette_t *pal, palent_t *entry);
void palette_clear(palette_t * pal);

int palette_compareColorsManhattan(const palette_t *pal, int color1, int color2);
int palette_compareColorsEuclidian(const palette_t *pal, int color1, int color2);

int palette_loadGimpPalette(const char *filename, palette_t *dst);
int palette_loadJascPalette(const char *filename, palette_t *dst);
int palette_loadFromPng(const char *filename, palette_t *dst);

int palette_load(const char *filename, palette_t *dst);

int palette_output_sms_bin(FILE *fptr, palette_t *pal);
int palette_output_sms_wladx(FILE *fptr, palette_t *pal, const char *symbol_name);
int palette_output_animator_pro_col(FILE *fptr, palette_t *pal);
int palette_output_animator_col(FILE *fptr, palette_t *pal);
int palette_output_vgaasm(FILE *fptr, palette_t *pal, const char *symbol_name);
int palette_saveFPTR(FILE *outfptr, palette_t *src, uint8_t format, const char *name);
int palette_save(const char *outfilename, palette_t *src, uint8_t format, const char *name);

int palette_generateRemap(const palette_t *srcpal, const palette_t *dstpal, palremap_lut_t *dst);
int palettes_match(const palette_t *pal1, const palette_t *pal2);

int palette_quantize(palette_t *pal, int bits_per_component);
int palette_gain(palette_t *pal, double gain);
int palette_gamma(palette_t *pal, double gamma);

int palette_dropDuplicateColors(palette_t *pal);

#endif // _palette_h__
