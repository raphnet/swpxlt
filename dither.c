#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <png.h>
#include "sprite.h"
#include "palette.h"
#include "globals.h"
#include "rgbimage.h"

uint8_t clamp8bit(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return val;
}

uint8_t errdiff(uint8_t org, int error, int mult, int div)
{
	return clamp8bit(org + error * mult / div);
}

void distributeError(rgbimage_t *img, int x, int y, int e_r, int e_g, int e_b)
{
	uint8_t r,g,b;
	int mult, div;

	// Current pixel: #

	// 1    - # 7
	// 16   3 5 1
	div = 16;

	// 7
	mult = 7;
	getPixelRGBsafe(img, x+1, y, &r, &g, &b);
	r = errdiff(r, e_r, mult, div);
	g = errdiff(g, e_g, mult, div);
	b = errdiff(b, e_b, mult, div);
	setPixelRGBsafe(img, x+1, y, r, g, b);

	// 3
	mult = 3;
	getPixelRGBsafe(img, x-1, y+1, &r, &g, &b);
	r = errdiff(r, e_r, mult, div);
	g = errdiff(g, e_g, mult, div);
	b = errdiff(b, e_b, mult, div);
	setPixelRGBsafe(img, x-1, y+1, r, g, b);

	// 5
	mult = 5;
	getPixelRGBsafe(img, x, y+1, &r, &g, &b);
	r = errdiff(r, e_r, mult, div);
	g = errdiff(g, e_g, mult, div);
	b = errdiff(b, e_b, mult, div);
	setPixelRGBsafe(img, x, y+1, r, g, b);

	// 5
	mult = 1;
	getPixelRGBsafe(img, x+1, y+1, &r, &g, &b);
	r = errdiff(r, e_r, mult, div);
	g = errdiff(g, e_g, mult, div);
	b = errdiff(b, e_b, mult, div);
	setPixelRGBsafe(img, x+1, y+1, r, g, b);



}

int ditherImage(rgbimage_t *img, palette_t *pal)
{
	int x, y;
	pixel_t *pix;
	int e_r, e_g, e_b;
	int idx;

	if ((pal->count == 0) || (pal->count > 256)) {
		fprintf(stderr, "Bad palette color count: %d\n", pal->count);
		return -1;
	}

	for (y=0; y<img->h; y++) {
		for (x=0; x<img->w; x++) {
			pix = getPixel(img, x, y);

			idx = palette_findBestMatch(pal, pix->r, pix->g, pix->b, 0);

			e_r = pix->r - pal->colors[idx].r;
			e_g = pix->g - pal->colors[idx].g;
			e_b = pix->b - pal->colors[idx].b;

			pix->r = pal->colors[idx].r;
			pix->g = pal->colors[idx].g;
			pix->b = pal->colors[idx].b;

			/*if ((pix->r > 200) || (pix->g > 128) || (pix->b) > 250) {
				distributeError(img, x, y, e_r/3, e_g/2, e_b/6);
			} else {
			}*/

			distributeError(img, x, y, e_r, e_g, e_b);

		}
	}

	return 0;
}

static uint8_t qn(uint8_t i, int nshift)
{
	return i >> nshift;
}

static uint8_t qn8(uint8_t i, int nshift)
{
	return i * 0xff / (0xff >> nshift);
}




void quantizeRGBimage(rgbimage_t *img, int bits_per_component)
{
	int count = img->w * img->h;
	int i;
	int shift = 8-bits_per_component;

	for (i=0; i<count; i++) {
		img->pixels[i].r = qn(img->pixels[i].r, shift);
		img->pixels[i].g = qn(img->pixels[i].g, shift);
		img->pixels[i].b = qn(img->pixels[i].b, shift);
	}

	for (i=0; i<count; i++) {
		img->pixels[i].r = qn8(img->pixels[i].r, shift);
		img->pixels[i].g = qn8(img->pixels[i].g, shift);
		img->pixels[i].b = qn8(img->pixels[i].b, shift);
	}

}

uint8_t fnGamma(uint8_t in, double val)
{
	return clamp8bit(255 * pow((in / 255.0),1/val));
}

uint8_t fnGain(uint8_t in, double val)
{
	return clamp8bit(val * in);
}

uint8_t fnBias(uint8_t in, double val)
{
	return clamp8bit(val + in);
}

void forAllPixelsD(rgbimage_t *img, double arg, uint8_t(*fn)(uint8_t in, double arg))
{
	int count = img->w * img->h;
	int i;

	for (i=0; i<count; i++) {
		img->pixels[i].r = fn(img->pixels[i].r, arg);
		img->pixels[i].g = fn(img->pixels[i].g, arg);
		img->pixels[i].b = fn(img->pixels[i].b, arg);
	}

}

static int *tmp_countbuf;

int compareUsed(const void *a, const void *b)
{
	const int *ia = (const int *)a;
	const int *ib = (const int *)b;

	return (tmp_countbuf[*ib]-tmp_countbuf[*ia]);
}


struct colorStats {
	int countbuf[256*256*256];
	int num_unique;
	int usedlist[256*256*256];
	int num_used;
};


struct colorStats *getColorStats(rgbimage_t *img, struct colorStats *reuse)
{
	int i, idx;
	struct colorStats *stats;

	if (!reuse) {
		stats = calloc(1, sizeof(struct colorStats));
		if (!stats) {
			perror("Could not allocate colorStats");
			return NULL;
		}
	} else {
		stats = reuse;
		for (i=0; i<stats->num_used; i++) {
			stats->countbuf[stats->usedlist[i]] = 0;
		}
		stats->num_unique = 0;
		stats->num_used = 0;
	}


	for (i=0; i<img->w * img->h; i++) {
		idx = img->pixels[i].b | (img->pixels[i].g<<8) | (img->pixels[i].r<<16);

		if (stats->countbuf[idx]==0) {
			stats->num_unique++; // count this unique color

			// Add this color to the use list
			stats->usedlist[stats->num_used] = idx;
			stats->num_used++;

		}

		stats->countbuf[idx]++;
	}

	// Set global for comparison function
	tmp_countbuf = stats->countbuf;

	// Sort
	qsort(stats->usedlist, stats->num_used, sizeof(int), compareUsed);

	return stats;
}

void printColorStats(struct colorStats *stats)
{
	int i;

	printf("-- Color stats --\n");
	printf("Unique colors: %d\n", stats->num_unique);
	printf("\n");
	printf("Pos | Color   | Uses:\n");
	printf("-------------------\n");
	for (i=0; i<stats->num_used; i++) {
		printf("%3d | #%06x | %d \n", i,
			stats->usedlist[i], stats->countbuf[stats->usedlist[i]]);
	}
}

int countColors(rgbimage_t *img)
{
	struct colorStats *stats;
	int c;

	stats = getColorStats(img, NULL);
	if (!stats) {
		return -1;
	}

	c = stats->num_used;
	free(stats);

	return c;
}

int findBestColorMatch(struct colorStats *stats, int ref)
{
	int i;
	int best = 0;
	int best_diff = 255;
	int diff_abs;
	int diff;
	int first = 1;
	int dist;

	for (i=0; i<stats->num_used; i++) {

		if (stats->usedlist[i] == ref)
			continue;

#if 1 // Manhattan
		diff_abs = 0;

		dist = ((ref>>0) & 0xff) - ((stats->usedlist[i]>>0)&0xff);
		diff_abs += abs(dist);

		dist = ((ref>>8) & 0xff) - ((stats->usedlist[i]>>8)&0xff);
		diff_abs += abs(dist);

		dist = ((ref>>16) & 0xff) - ((stats->usedlist[i]>>16)&0xff);
		diff_abs += abs(dist);

#endif

#if 0	// Euclidian
		diff_abs = 0;

		dist = ((ref>>0) & 0xff) - ((stats->usedlist[i]>>0)&0xff);
		dist = dist*dist;
		diff_abs += dist;

		dist = ((ref>>8) & 0xff) - ((stats->usedlist[i]>>8)&0xff);
		dist = dist*dist;
		diff_abs += dist;

		dist = ((ref>>16) & 0xff) - ((stats->usedlist[i]>>16)&0xff);
		dist = dist*dist;
		diff_abs += dist;
#endif

		diff = diff_abs;

		if (first || (diff < best_diff)) {
			best = stats->usedlist[i];
			best_diff = diff;
			first = 0;
		}
	}

//	printf("Best match for #%06x -> #%06x  (distance: %d) \n", ref, best, best_diff);

	return best;
}

int replaceColor(rgbimage_t *img, pixel_t *original, pixel_t *replacement)
{
	int count = img->w * img->h;
	int i;

	for (i=0; i<count; i++) {
		if (pixelEqual(img->pixels + i, original)) {
			memcpy(img->pixels + i, replacement, sizeof(pixel_t));
		}
	}

	return 0;
}

int reduceColors(rgbimage_t *img, int max)
{
	struct colorStats *stats = NULL;
	int rarest, match;
	pixel_t original, replacement;

	if (max <= 1) {
		fprintf(stderr, "Cannot keep 0 (or less) colors!\n");
		return -1;
	}

	if (g_verbose) {
		printf("Begin color reduction loop...\n");
	}

	do
	{
#if 0
		if (stats) { free(stats); }
		stats = getColorStats(img, NULL);
#else
		stats = getColorStats(img, stats);
#endif
		if (!stats) {
			return -1;
		}

		if (g_verbose) {
			printf("Unique colors: %d - target %d\n", stats->num_unique, max);
		}

		rarest = stats->usedlist[stats->num_used-1];
		match = findBestColorMatch(stats, rarest);

		original.r = rarest >> 16;
		original.g = rarest >> 8;
		original.b = rarest;
		replacement.r = match >> 16;
		replacement.g = match >> 8;
		replacement.b = match;

		replaceColor(img, &original, &replacement);


		stats->num_unique--;
	}
	while (stats->num_unique > max);

	if (g_verbose) {
		printf("Recude colors finished: Colors: %d\n", stats->num_unique);
	}

	if (stats) {
		free(stats);
	}

	return 0;
}

int makePalette(rgbimage_t *img, palette_t *outpal)
{
	struct colorStats *stats = NULL;
	int i;

	stats = getColorStats(img, NULL);
	if (stats->num_used > 256) {
		fprintf(stderr, "Too many colors. Please quantize first\n");
		free(stats);
		return -1;
	}

	outpal->count = stats->num_used;

	for (i=0; i<stats->num_used; i++) {
		outpal->colors[i].r = stats->usedlist[i] >> 16;
		outpal->colors[i].g = stats->usedlist[i] >> 8;
		outpal->colors[i].b = stats->usedlist[i] >> 0;
	}

	free(stats);

	return 0;
}

int g_verbose;

enum {
	OPT_IN = 255,
	OPT_RELOAD,
	OPT_OUT,
	OPT_QUANTIZE,
	OPT_GAIN,
	OPT_BIAS,
	OPT_GAMMA,
	OPT_COUNT,
	OPT_SHOWSTATS,
	OPT_REDUCECOLORS,
	OPT_MAKEPAL,
	OPT_LOADPAL,
	OPT_DITHER,
};

static struct option long_options[] = {
	{ "help",		no_argument,	0,	'h' },
	{ "verbose",	no_argument,	0,	'v' },
	{ "in",			required_argument, 	0,	OPT_IN },
	{ "reload",		no_argument,		0,	OPT_RELOAD },
	{ "out",		required_argument, 	0,	OPT_OUT },
	{ "quantize",	required_argument, 	0,	OPT_QUANTIZE },
	{ "gain",		required_argument, 	0,	OPT_GAIN },
	{ "bias",		required_argument, 	0,	OPT_BIAS },
	{ "gamma",		required_argument,	0,	OPT_GAMMA },
	{ "count",      no_argument, 0, OPT_COUNT },
	{ "showstats",	no_argument, 0, OPT_SHOWSTATS },
	{ "reducecolors", required_argument, 0, OPT_REDUCECOLORS },
	{ "makepal",	no_argument, 0, OPT_MAKEPAL },
	{ "loadpal",	required_argument, 0, OPT_LOADPAL },
	{ "dither",		no_argument, 0, OPT_DITHER },
	{ }
};

void printHelp(void)
{
	printf("Usage: ./dither [options]\n");

	printf(" -h                 Print usage information\n");
	printf(" -v                 Enable verbose mode\n");
	printf(" -in file           Load image\n");
	printf(" -reload            Reload image (uses previous -in filename)\n");
	printf(" -out file          Write image\n");
	printf(" -quantize bits     Quantize (per component. Eg 6 for VGA).\n");
	printf("                    min 1, max 8 (no effect)\n");
	printf(" -gain val          Multiply pixels by value (float)\n");
	printf(" -bias val          Add value to pixels (float)\n");
	printf(" -gamma val         Gamma correction value (float)\n");
	printf(" -showstats         Show color stats of current image\n");
	printf(" -reducecolors max  Remove less used colors until unique colors <= max\n");
	printf(" -makepal           Build palette from current image (max 256 colors)\n");
	printf(" -loadpal file      Load palette from file\n");
	printf(" -dither            Dither the current using palette (-makepal or -loadpal)\n");
}


int main(int argc, char **argv)
{
	int opt, res;
	rgbimage_t *image_in = NULL;
	const char *input_filename = NULL;
	struct colorStats *stats;
	char *e;
	int val;
	double vald;
	palette_t refpal = { };

	while ((opt = getopt_long_only(argc, argv, "hv", long_options, NULL)) != -1) {
		switch(opt)
		{
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case OPT_IN:
				if (image_in) {
					freeRGBimage(image_in);
				}
				image_in = rgbi_loadPNG(optarg);
				if (!image_in) {
					fprintf(stderr, "Could not load image (%s)\n", optarg);
					return -1;
				}
				input_filename = optarg;
				break;

			case OPT_RELOAD:
				if (!input_filename) {
					fprintf(stderr ,"No image to reload\n");
					return -1;
				}
				if (image_in) {
					freeRGBimage(image_in);
				}
				image_in = rgbi_loadPNG(input_filename);
				if (!image_in) {
					fprintf(stderr, "Could not load image (%s)\n", input_filename);
					return -1;
				}
				break;

			case OPT_QUANTIZE:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				val = strtol(optarg, &e, 0);
				if (e == optarg) {
					fprintf(stderr, "Invalid value\n");
					return -1;
				}
				if ((val<1)||(val>8)) {
					fprintf(stderr, "Value out of range\n");
					return 1;
				}
				if (g_verbose) {
					printf("Quantize to %d bits per color\n", val);
				}
				quantizeRGBimage(image_in, val);
				break;

			case 'X':
				if (image_in) {
					printRGBimage(image_in);
				}
				break;

			case OPT_OUT:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				if (refpal.count == 0) {
					rgbi_savePNG(optarg, image_in);
				} else {
					rgbi_savePNG_indexed(optarg, image_in, &refpal);
				}
				break;

			case OPT_GAMMA:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				vald = strtod(optarg, &e);
				if (optarg == e) {
					fprintf(stderr, "Invalid value\n");
					return -1;
				}
				forAllPixelsD(image_in, vald, fnGamma);
				break;

				break;

			case OPT_GAIN:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				vald = strtod(optarg, &e);
				if (optarg == e) {
					fprintf(stderr, "Invalid value\n");
					return -1;
				}
				forAllPixelsD(image_in, vald, fnGain);
				break;

			case OPT_BIAS:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				vald = strtod(optarg, &e);
				if (optarg == e) {
					fprintf(stderr, "Invalid value\n");
					return -1;
				}
				forAllPixelsD(image_in, vald, fnBias);
				break;

			case OPT_COUNT:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				val = countColors(image_in);
				printf("Unique colors: %d\n", val);
				break;

			case OPT_SHOWSTATS:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				stats = getColorStats(image_in, NULL);
				printColorStats(stats);
				free(stats);
				break;

			case OPT_REDUCECOLORS:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				val = strtol(optarg, &e, 0);
				if (e == optarg) {
					fprintf(stderr, "Invalid value\n");
					return -1;
				}
				res = reduceColors(image_in, val);
				if (res < 0) {
					fprintf(stderr ,"Color reduction failed\n");
					return -1;
				}
				break;

			case OPT_LOADPAL:
				if (palette_load(optarg, &refpal)) {
					fprintf(stderr ,"Load palette failed\n");
					return -1;
				}

				if (g_verbose) {
					printf("Reference palette for dither:\n");
					palette_print(&refpal);
				}

				break;

			case OPT_MAKEPAL:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}

				res = makePalette(image_in, &refpal);
				if (res < 0) {
					fprintf(stderr,"Failed to build palette\n");
					return -1;
				}

				if (g_verbose) {
					printf("Reference palette for dither:\n");
					palette_print(&refpal);
				}
				break;

			case OPT_DITHER:
				if (!image_in) {
					fprintf(stderr ,"No image loaded\n");
					return -1;
				}
				if (refpal.count == 0) {
					fprintf(stderr, "Cannot dither without a palette.\n");
					return -1;
				}

				ditherImage(image_in, &refpal);
				break;

		}
	}


	return 0;
}
