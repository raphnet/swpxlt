/*
    swpxlt - A collection of image/palette manipulation tools

    Copyright (C) 2022 Raphael Assenat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "palette.h"
#include "globals.h"
#include "sprite.h"
#include "builtin_palettes.h"
#include "util.h"

static void printEnt_hex(const palent_t *ent, const char *suf)
{
	printf("%02x%02x%02x%s",
		ent->r, ent->g, ent->b,
		suf);
}

void palette_print(const palette_t *pal)
{
	int i;

	for (i=0; i<pal->count; i++) {
		printf("%d: ", i);
		printEnt_hex(&pal->colors[i], "\n");
	}
}

static void setTermBgColorRGB(const palent_t *ent)
{
	printf("\033[48;2;%d;%d;%dm", ent->r, ent->g, ent->b);
}

static void resetTermColor(void)
{
	printf("\033[0m");
}

void palette_print_24bit(const palette_t *pal)
{
	int i;
	int j;

	// Top Header
	printf("    ");
	for (i=0; i<16; i++)  {
		if (i >= pal->count) {
			break;
		}
		printf("%X ", i);
	}
	printf("\n");

	// Rows
	for (i=0; i<16; i++) {
		if (i*16 >= pal->count) {
			break;
		}

		// Row label
		printf("%X : ", i);

		// Colors
		for (j=0; j<16; j++) {
			if (i*16+j >= pal->count)
				break;
			setTermBgColorRGB(&pal->colors[i*16+j]);
			printf("  ");
			resetTermColor();
		}
		resetTermColor();
		printf("\n");
	}

}

void palette_setColor(palette_t * pal, uint8_t index, int r, int g, int b)
{
	pal->colors[index].r = r;
	pal->colors[index].g = g;
	pal->colors[index].b = b;

	if (index >= pal->count) {
		pal->count = index + 1;
	}
}

void palette_addColor(palette_t * pal, int r, int g, int b)
{
	if (pal->count < 256) {
		palette_setColor(pal, pal->count, r, g, b);
		pal->count++;
	}
}

void palette_addColorEnt(palette_t *pal, palent_t *entry)
{
	palette_addColor(pal, entry->r, entry->g, entry->b);
}

void palette_clear(palette_t * pal)
{
	memset(pal, 0, sizeof(palette_t));
}

int palette_loadGimpPalette(const char *filename, palette_t *dst)
{
	FILE *fptr;
	char linebuf[256];
	int linecount;
	int hash_seen = 0;
	int r,g,b;

	fptr = fopen(filename, "r");
	if (!fptr) {
		perror(filename);
		return -1;
	}

	// Sample:
	//
	// GIMP Palette
	// Name: Tandy
	// Columns: 0
	// #
	//   0   0   0 Untitled
	//   0   0 170 Untitled
	//   ...
	//
	//

	linecount = 1;
	while (fgets(linebuf, sizeof(linebuf), fptr))
	{
		// Make sure line 1 reads GIMP Palette
		if (linecount == 1) {
			if (!strstr(linebuf, "GIMP Palette")) {
				fclose(fptr);
				if (g_verbose) {
					fprintf(stderr, "%s: Not a gimp palette\n", filename);
				}
				return -1;
			}
		}

		if (linebuf[0] == '#') {
			// I'm not sure if comments are valid in this format. Generate an error for investigation if it evers happens.
			if (hash_seen) {
				fprintf(stderr, "%s: Unexpected comment? at line %d\n", filename, linecount);
				fclose(fptr);
			}
			hash_seen = 1;
			palette_clear(dst);
		} else {
			// Once the line with a single # has been
			// seen, assume each line is a color.
			if (hash_seen)
			{
				if (3 != sscanf(linebuf, "%d %d %d", &r, &g, &b)) {
					fclose(fptr);
					fprintf(stderr, "Error reading color %s:%d\n", filename, linecount);
					return -1;
				}

				palette_addColor(dst, r, g, b);
			}
		}

		linecount++;
	}

	if (!feof(fptr)) {
		perror("error reading palette");
		fclose(fptr);
		return -1;
	}


	fclose(fptr);

	return 0;
}

static int getColorDistance(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2)
{
	// Euclidian
	return sqrt( (r2-r1)*(r2-r1) + (g2-g1)*(g2-g1) + (b2-b1)*(b2-b1)	);
}

int palette_findBestMatch(const palette_t *pal, int r, int g, int b, int method)
{
	int best_index = 0;
	int best_dist;
	int dist;
	int i;

	for (i=0; i<pal->count; i++) {
		dist = getColorDistance(pal->colors[i].r, pal->colors[i].g, pal->colors[i].b, r, g, b);

		if (i==0) {
			best_index = i;
			best_dist = dist;
		} else {
			if (dist < best_dist) {
				best_dist = dist;
				best_index = i;
			}
		}
	}

	return best_index;
}

int palette_findColor(const palette_t *pal, int r, int g, int b)
{
	int i;

	for (i=0; i<pal->count; i++) {
		if ((pal->colors[i].r == r) && (pal->colors[i].g == g) && (pal->colors[i].b == b)) {
			return i;
		}
	}
	return -1;
}

int palette_findColorEnt(const palette_t *pal, palent_t *entry)
{
	return palette_findColor(pal, entry->r, entry->g, entry->b);
}

//
// Populate a look-up table to remap the pixels using a source palette to a different destination palette.
//
// Note: The source colors must exist in the destination palette.
//
int palette_generateRemap(const palette_t *srcpal, const palette_t *dstpal, palremap_lut_t *dst)
{
	int i, idx;

	// For each color in the source palette, try to find a match in the destination palette.
	for (i=0; i<srcpal->count; i++) {
		idx = palette_findColor(dstpal, srcpal->colors[i].r, srcpal->colors[i].g, srcpal->colors[i].b);
		if (idx < 0) {
			fprintf(stderr, "Source color index %d does not exist in destination palette\n", i);
			return idx;
		}

		dst->lut[i] = idx;
	}

	return 0;
}

// https://liero.nl/lierohack/docformats/other-jasc.html
int palette_loadJascPalette(const char *filename, palette_t *dst)
{
	char linebuf[128];
	int lineno = 0;
	int processed_colors = 0;
	int r,g,b;
	FILE *jasc;

	jasc = fopen(filename, "r");
	if (!jasc) {
		perror(filename);
		return -1;
	}

	while (fgets(linebuf, sizeof(linebuf), jasc)) {

		switch(lineno)
		{
			case 0:
				if (strncmp(linebuf, "JASC-PAL", 8)) {
					if (g_verbose) {
						fprintf(stderr, "Header not found - Not a JASC PAL file\n");
					}
					return -1;
				}
				break;
			case 1: // ignore
				break;
			case 2:
				dst->count = strtol(linebuf, NULL, 10);
				break;
			default:
				if (3 != sscanf(linebuf, "%d %d %d", &r, &g, &b)) {
					fprintf(stderr, "Line %d error: missing values\n", lineno);
					return -1;
				}

				dst->colors[processed_colors].r = r;
				dst->colors[processed_colors].g = g;
				dst->colors[processed_colors].b = b;
				processed_colors++;

				if (processed_colors >= dst->count) {
					goto done; // jump out of this while loop
				}
				break;
		}

		lineno++;
	}

done:
	fclose(jasc);

	return 0;
}

int palette_loadFromImage(const char *filename, palette_t *dst)
{
	sprite_t *spr;

	spr = sprite_load(filename, 0, 0);
	if (!spr) {
		return -1;
	}

	memcpy(dst, &spr->palette, sizeof(palette_t));
	freeSprite(spr);

	return 0;
}

int palette_load(const char *filename, palette_t *dst)
{
	palette_t src;
	struct {
		int (*load)(const char *filename, palette_t *dst);
		const char *name;
	} paletteLoaders[] = {
		{ palette_loadGimpPalette, "Gimp" },
		{ palette_loadJascPalette, "Jasc" },
		{ palette_loadFromImage, "Image" },
		{ } // terminator
	};
	int i;

	if (strncmp(filename, "builtin:", 8)==0) {
		const palette_t *builtin;
		builtin = getBuiltinPalette_byName(filename+8);
		if (!builtin) {
			fprintf(stderr, "No such built-in palette");
			return -1;
		}
		memcpy(dst, builtin, sizeof(palette_t));
		return 0;
	}

	for (i=0; paletteLoaders[i].load; i++) {
		if (g_verbose) {
			printf("Checking for %s palette\n", paletteLoaders[i].name);
		}

		if (0 == paletteLoaders[i].load(filename, &src)) {
			break;
		}
	}

	if (!paletteLoaders[i].load) {
		fprintf(stderr, "Unsupported palette file\n");
		return -1;
	}

	if (g_verbose) {
		printf("Loaded %d colors from %s palette\n", src.count, paletteLoaders[i].name);

	}

	memcpy(dst, &src, sizeof(palette_t));

	return 0;
}

int palettes_match(const palette_t *pal1, const palette_t *pal2)
{
	int i;

	if (pal1->count != pal2->count)
		return 0;

	for (i=0; i<pal1->count; i++) {
		if (pal1->colors[i].r != pal2->colors[i].r)
			return 0;
		if (pal1->colors[i].g != pal2->colors[i].g)
			return 0;
		if (pal1->colors[i].b != pal2->colors[i].b)
			return 0;
	}

	return 1;
}

int palette_compareColorsManhattan(const palette_t *pal, int color1, int color2)
{
	int total = 0, dist;

	dist = pal->colors[color1].r - pal->colors[color2].r;
	total += abs(dist);
	dist = pal->colors[color1].g - pal->colors[color2].g;
	total += abs(dist);
	dist = pal->colors[color1].b - pal->colors[color2].b;
	total += abs(dist);

	return total;
}

int palette_compareColorsEuclidian(const palette_t *pal, int color1, int color2)
{
	int total = 0, dist;

	dist = pal->colors[color1].r - pal->colors[color2].r;
	dist = dist*dist;
	total += abs(dist);

	dist = pal->colors[color1].g - pal->colors[color2].g;
	dist = dist*dist;
	total += abs(dist);

	dist = pal->colors[color1].b - pal->colors[color2].b;
	dist = dist*dist;
	total += abs(dist);

	return total;
}

int palette_output_sms_bin(FILE *fptr, palette_t *pal)
{
	int i;
	uint8_t color;

	for (i=0; i<pal->count; i++) {
		color = (pal->colors[i].b >> 6) << 4;
		color |= (pal->colors[i].g >> 6) << 2;
		color |= (pal->colors[i].r >> 6) << 0;

		fwrite(&color, 1, 1, fptr);
	}

	return 0;
}


int palette_output_sms_wladx(FILE *fptr, palette_t *pal, const char *symbol_name)
{
	int i;
	uint8_t color;

	fprintf(fptr, "%s:\n", symbol_name);
	for (i=0; i<pal->count; i++) {
		color = (pal->colors[i].b >> 6) << 4;
		color |= (pal->colors[i].g >> 6) << 2;
		color |= (pal->colors[i].r >> 6) << 0;

		fprintf(fptr, ".db $%02x ; idx %d, rgb=%02x%02x%02x\n",
					color,
					pal->colors[i].r,
					pal->colors[i].g,
					pal->colors[i].b,
					i);
	}
	fprintf(fptr, "%s_end:\n", symbol_name);

	return 0;
}


int palette_output_vgaasm(FILE *fptr, palette_t *pal, const char *symbol_name)
{
	int i;

	fprintf(fptr, "%s:\n", symbol_name);
	for (i=0; i<pal->count; i++) {
		fprintf(fptr, "	db %2d,%2d,%2d ; idx %d\n",
					pal->colors[i].r / 4,
					pal->colors[i].g / 4,
					pal->colors[i].b / 4,
					i);
	}
	fprintf(fptr, "%s_end:\n", symbol_name);
	fprintf(fptr, "%%define %s_size %d\n", symbol_name, pal->count);

	return 0;
}

// https://www.fileformat.info/format/animator-col/corion.htm
int palette_output_animator_pro_col(FILE *fptr, palette_t *pal)
{
	int i;
	uint8_t header[8];
	uint8_t tmp[3];
	int size;

	size = 8 + pal->count * 3;

	//  1 dword  File size, including this header
	header[0] = size;
	header[1] = size >> 8;
	header[2] = 0;
	header[3] = 0;
	// 1 word   ID=0B123h
	header[4] = 0x23;
	header[5] = 0xB1;
	// 1 word   Version, currently 0
	header[6] = 0x00;
	header[7] = 0x00;

	fwrite(header, 8, 1, fptr);

	for (i=0; i<pal->count; i++) {
		tmp[0] = pal->colors[i].r;
		tmp[1] = pal->colors[i].g;
		tmp[2] = pal->colors[i].b;
		fwrite(tmp, 3, 1, fptr);
	}

	return 0;
}

// https://www.fileformat.info/format/animator-col/corion.htm
int palette_output_animator_col(FILE *fptr, palette_t *pal)
{
	int i;
	uint8_t tmp[3];

	for (i=0; i<256; i++) {
		if (i < pal->count) {
			tmp[0] = pal->colors[i].r;
			tmp[1] = pal->colors[i].g;
			tmp[2] = pal->colors[i].b;
		} else {
			memset(tmp, 0, 3);
		}

		fwrite(tmp, 3, 1, fptr);
	}

	return 0;
}

int palette_saveFPTR(FILE *outfptr, palette_t *src, uint8_t format, const char *name)
{
	switch (format)
	{
		case PALETTE_FORMAT_SMS_BIN:
			return palette_output_sms_bin(outfptr, src);
		case PALETTE_FORMAT_SMS_WLADX:
			return palette_output_sms_wladx(outfptr, src, name);
		case PALETTE_FORMAT_VGAASM:
			return palette_output_vgaasm(outfptr, src, name);
		case PALETTE_FORMAT_ANIMATOR:
			return palette_output_animator_col(outfptr, src);
		case PALETTE_FORMAT_ANIMATOR_PRO:
			return palette_output_animator_pro_col(outfptr, src);
	}

	return -1;
}

int palette_save(const char *outfilename, palette_t *src, uint8_t format, const char *name)
{
	FILE *outfptr;
	char *mode = "wb";
	int res;

	if ((format == PALETTE_FORMAT_SMS_WLADX) || (format == PALETTE_FORMAT_VGAASM)) {
		mode = "w";
	}

	outfptr = fopen(outfilename, mode);
	if (!outfptr) {
		perror(outfilename);
		return -1;
	}

	res = palette_saveFPTR(outfptr, src, format, name);

	fclose(outfptr);

	return res;
}

int palette_parseOutputFormat(const char *arg)
{
	if (0 == strcasecmp(arg, "vga_asm")) { return PALETTE_FORMAT_VGAASM; }
	if (0 == strcasecmp(arg, "png")) { return PALETTE_FORMAT_PNG; }
	if (0 == strcasecmp(arg, "animator")) { return PALETTE_FORMAT_ANIMATOR; }
	if (0 == strcasecmp(arg, "animator_pro")) { return PALETTE_FORMAT_ANIMATOR_PRO; }
	if (0 == strcasecmp(arg, "sms_wladx")) { return PALETTE_FORMAT_SMS_WLADX; }
	if (0 == strcasecmp(arg, "sms_bin")) { return PALETTE_FORMAT_SMS_BIN; }
	return PALETTE_FORMAT_NONE;
}

static uint8_t qn(uint8_t i, int nshift)
{
	return i >> nshift;
}

static uint8_t qn8(uint8_t i, int nshift)
{
	return i * 0xff / (0xff >> nshift);
}

int palette_quantize(palette_t *pal, int bits_per_component)
{
	int shift = 8-bits_per_component;
	int i;

	for (i=0; i<pal->count; i++) {
		pal->colors[i].r = qn(pal->colors[i].r, shift);
		pal->colors[i].g = qn(pal->colors[i].g, shift);
		pal->colors[i].b = qn(pal->colors[i].b, shift);
	}

	for (i=0; i<pal->count; i++) {
		pal->colors[i].r = qn8(pal->colors[i].r, shift);
		pal->colors[i].g = qn8(pal->colors[i].g, shift);
		pal->colors[i].b = qn8(pal->colors[i].b, shift);
	}

	return 0;
}

int palette_gain(palette_t *pal, double gain)
{
	int i;

	for (i=0; i<pal->count; i++) {
		pal->colors[i].r = clamp8bit(pal->colors[i].r * gain);
		pal->colors[i].g = clamp8bit(pal->colors[i].g * gain);
		pal->colors[i].b = clamp8bit(pal->colors[i].b * gain);
	}

	return 0;
}

static uint8_t fnGamma(uint8_t in, double val)
{
	return clamp8bit(255 * pow((in / 255.0),1/val));
}

int palette_gamma(palette_t *pal, double gamma)
{
	int i;

	for (i=0; i<pal->count; i++) {
		pal->colors[i].r = fnGamma(pal->colors[i].r, gamma);
		pal->colors[i].g = fnGamma(pal->colors[i].g, gamma);
		pal->colors[i].b = fnGamma(pal->colors[i].b, gamma);
	}

	return 0;
}

int palette_dropDuplicateColors(palette_t *pal)
{
	int i;
	palette_t newpal;

	newpal.count = 0;

	for (i=0; i<pal->count; i++) {
		if (-1 == palette_findColorEnt(&newpal, &pal->colors[i])) {
			palette_addColorEnt(&newpal, &pal->colors[i]);
		}
	}

	memcpy(pal, &newpal, sizeof(palette_t));

	return 0;
}

