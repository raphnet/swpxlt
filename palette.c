#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "palette.h"
#include "globals.h"
#include "sprite.h"
#include "builtin_palettes.h"

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

