#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include "sprite.h"
#include "globals.h"

int g_verbose;

enum {
	OPT_LOADDAT = 256,
	OPT_LOADPNG,
	OPT_SAVEDAT,
	OPT_SAVEPNG,
};

static void printHelp()
{
	printf("Usage: ./s58tool [options]\n");
	printf("\nConvert MA-2055 cash register .DAT images to PNG and vis. versa\n");

	printf("\nGeneral / Global options:\n");
	printf(" -h                      Print usage information\n");
	printf(" -v                      Enable verbose output\n");

	printf(" -loaddat file.dat       Load .DAT image data\n");
	printf(" -loadpng file.png       Load .PNG image\n");
	printf(" -savedat file.dat       Save .DAT image\n");
	printf(" -savepng file.png       Save .PNG image\n");
}

static struct option long_options[] = {
	{ "help",    no_argument,       0, 'h' },
	{ "verbose", no_argument,       0, 'v' },
	{ "loaddat", required_argument, 0, OPT_LOADDAT },
	{ "loadpng", required_argument, 0, OPT_LOADPNG },
	{ "savedat", required_argument, 0, OPT_SAVEDAT },
	{ "savepng", required_argument, 0, OPT_SAVEPNG },
	{ },
};

enum {
	FORMAT_UNDEFINED = 0,
	FORMAT_DAT,
	FORMAT_PNG,
};


// // Header is 32 bytes ASCII
// (spaces added for clarity)
//
// S58LOGO.DAT
//
// 004QH 20241125 01 1445 00120  00000000
// ????? YYYYMMDD ?? hhmm height ????????
//
// not sure if height is really 5 digits.
//
// S58INSHI.DAT
//
// 004p> 20241125 01 1446 00160  00000000
//
// S58RYOSY.DAT
//
// 004p? 20241125 01 1445 00128  00000000
//

sprite_t *loadDat(const char *filename)
{
	FILE *fptr;
	long sz;
	uint8_t *buf;
	char *e;
	int w = 416, h;
	int rowbytes;
	int x, y, c;
	char hstr[6];
	uint8_t header[32];
	sprite_t *img;

	fptr = fopen(filename, "rb");
	if (!fptr) {
		perror(filename);
		return NULL;
	}

	fseek(fptr, 0, SEEK_END);
	sz = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);

	if (1 != fread(header, 32, 1, fptr)) {
		perror("could not read header");
		fclose(fptr);
		return NULL;
	}

	memcpy(hstr, header + 19, 5);
	hstr[5] = 0;
	h = strtol(hstr, &e,  10);
	if (e == hstr) {
		perror("Invalid height");
		fclose(fptr);
		return NULL;
	}


	// Subtract the 32 byte header from file size
	sz -= 32;
	w = sz / h * 8;
	if (w & 7) {
		fprintf(stderr, "Image width is not multiple of 8\n");
		fclose(fptr);
		return NULL;
	}

	rowbytes = w / 8;

	if (g_verbose) {
		printf("DAT size (excluding header) is %ld bytes\n", sz);
		printf("DAT header height %d\n", h);
		printf("Implied image width %d\n", w);
	}

	buf = malloc(rowbytes * h);
	if (!buf) {
		perror("Could not allocate memory for image");
		fclose(fptr);
		return NULL;
	}

	if (1 != fread(buf, rowbytes * h, 1, fptr)) {
		perror("Could not read image data");
		free(buf);
		fclose(fptr);
		return NULL;
	}

	img = allocSprite(w, h, 2, SPRITE_FLAG_OPAQUE);
	if (!img) {
		fprintf(stderr, "could not create sprite\n");
		free(buf);
		fclose(fptr);
		return NULL;
	}

	// Color 0 white : Paper
	palette_setColor(&img->palette, 0, 0xff, 0xff, 0xff);
	// Color 1 black : Thermal print
	palette_setColor(&img->palette, 1, 0, 0, 0);

	for (y=0; y<h; y++) {
		for(x=0; x<w; x++) {
			if (buf[y*(w/8)+(x/8)] & (0x80 >> (x & 7))) {
				c = 1;
			} else {
				c = 0;
			}
			sprite_setPixel(img, x, y, c);
		}
	}

	free(buf);
	fclose(fptr);

	return img;
}

int saveDat(const char *filename, sprite_t *img)
{
	FILE *fptr;
	int rowsize;
	uint8_t *rowbuf;
	int y, x;
	struct palette outpal;
	int src_color, dst_color;
	uint8_t r, g, b;
	char header[33];
	int year, month, day, hour, minute;

	if (img->w & 0x7) {
		fprintf(stderr, "Error : Image width is not a multiple of 8\n");
		return -1;
	}

	// TODO : Build timestamp from current time
	year = 2024;
	month = 11;
	day = 25;
	hour = 14;
	minute = 45;

	if (32 != snprintf(header, sizeof(header), "004QH%04d%02d%02d01%02d%02d%05d00000000", year, month, day, hour, minute, img->h))
	{
		fprintf(stderr, "Error : Could not build a valid header\n");
		return -1;
	}

	outpal.count = 2;
	palette_setColor(&outpal, 0, 0xff, 0xff, 0xff);
	palette_setColor(&outpal, 1, 0, 0, 0);

	fptr = fopen(filename, "wb");
	if (!fptr) {
		perror(filename);
		return -1;
	}

	fwrite(header, 32, 1, fptr);

	rowsize = img->w / 8;
	rowbuf = malloc(rowsize);
	if (!rowbuf) {
		perror("could not allocate row buffer");
		fclose(fptr);
		return -1;
	}

	for (y=0; y<img->h; y++) {
		memset(rowbuf, 0, rowsize);
		for (x=0; x<img->w; x++) {
			src_color = sprite_getPixel(img, x, y);
			r = img->palette.colors[src_color].r;
			g = img->palette.colors[src_color].g;
			b = img->palette.colors[src_color].b;
			dst_color = palette_findBestMatch(&outpal, r, g, b, 0);
			if (dst_color) {
				rowbuf[x/8] |= 0x80 >> (x & 7);
			}
		}
		fwrite(rowbuf, rowsize, 1, fptr);
	}

	free(rowbuf);
	fclose(fptr);

	return 0;
}


int main(int argc, char **argv)
{
	int opt, res;
//	char *e;
	sprite_t *img = NULL;
	const char *load_filename = NULL;
	const char *save_filename = NULL;
	int load_format = FORMAT_UNDEFINED;
	int save_format = FORMAT_UNDEFINED;

	while ((opt = getopt_long_only(argc, argv, "hv", long_options, NULL)) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose++; break;

			case OPT_LOADDAT:
				load_filename = optarg;
				load_format = FORMAT_DAT;
				break;

			case OPT_LOADPNG:
				load_filename = optarg;
				load_format = FORMAT_PNG;
				break;

			case OPT_SAVEDAT:
				save_filename = optarg;
				save_format = FORMAT_DAT;
				break;

			case OPT_SAVEPNG:
				save_filename = optarg;
				save_format = FORMAT_PNG;
				break;
		}
	}

	if (load_format == FORMAT_UNDEFINED ||
		save_format == FORMAT_UNDEFINED ||
		load_filename == NULL ||
		save_filename == NULL)
	{
		fprintf(stderr, "A file to load and a file to save must be specified\n");
		return -1;
	}

	if (g_verbose) {
		printf("Loading %s\n", load_filename);
	}

	switch(load_format)
	{
		case FORMAT_PNG:
			img = sprite_load(load_filename, 0, 0);
			break;

		case FORMAT_DAT:
			img = loadDat(load_filename);
			break;
	}

	if (!img) {
		fprintf(stderr, "Error: Could not load image\n");
		return -1;
	}

	if (g_verbose) {
		printf("Loaded %s\n", load_filename);
	}

	switch(save_format)
	{
		case FORMAT_PNG:
			res = sprite_savePNG(save_filename, img, 0);
			break;

		case FORMAT_DAT:
			res = saveDat(save_filename, img);
			break;

		default:
			res = 1;
	}

	if (g_verbose && res == 0) {
		printf("Saved %s\n", save_filename);
	}

	if (res) {
		fprintf(stderr, "Could not save to %s\n", save_filename);
	}

	return 0;
}

