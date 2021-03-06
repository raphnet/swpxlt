#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <png.h>
#include "rgbimage.h"
#include "globals.h"
#include "palette.h"
#include "util.h"

pixel_t *getPixel(rgbimage_t *img, int x, int y)
{
	return img->pixels + img->w * y + x;
}


void getPixelRGBsafe(rgbimage_t *img, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
	pixel_t *p;

	if ((x < 0)||(y<0)||(x>=img->w)||(y>=img->h)) {
		return;
	}

	p = getPixel(img, x, y);
	*r = p->r;
	*g = p->g;
	*b = p->b;
}

void setPixelRGBA(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	pixel_t *dst = getPixel(img, x, y);
	dst->r = r;
	dst->g = g;
	dst->b = b;
	dst->reserved = a;
}

void setPixelRGB(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	pixel_t *dst = getPixel(img, x, y);
	dst->r = r;
	dst->g = g;
	dst->b = b;
}

void setPixelRGBsafe(rgbimage_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	if ((x < 0)||(y<0)||(x>=img->w)||(y>=img->h)) {
		return;
	}
	setPixelRGB(img, x, y, r, g, b);
}

rgbimage_t *allocRGBimage(int w, int h)
{
	rgbimage_t *i;

	i = calloc(1, sizeof(rgbimage_t));
	if (!i)
		return NULL;

	i->w = w;
	i->h = h;
	i->pixels = calloc(1, sizeof(pixel_t) * w * h);
	if (!i->pixels) {
		perror("Could not allocate image buffer\n");
		free(i);
		return NULL;
	}

	return i;
}

rgbimage_t *duplicateRGBimage(const rgbimage_t *original)
{
	rgbimage_t *i;

	i = calloc(1, sizeof(rgbimage_t));
	if (!i)
		return NULL;

	i->w = original->w;
	i->h = original->h;
	i->pixels = calloc(1, sizeof(pixel_t) * i->w * i->h);
	if (!i->pixels) {
		perror("Could not allocate image buffer\n");
		free(i);
		return NULL;
	}
	memcpy(i->pixels, original->pixels, i->w * i->h * sizeof(pixel_t));

	return i;

}

void freeRGBimage(rgbimage_t *img)
{
	if (img) {
		free(img->pixels);
		free(img);
	}
}

void printRGBimage(rgbimage_t *img)
{
	int x,y;
	pixel_t *p;

	for (y=0; y<img->h; y++) {
		for (x=0; x<img->w; x++) {
			p = getPixel(img, x, y);
			printf("(%02x,%02x,%02x)", p->r, p->g, p->b);
		}
		printf("\n");
	}
}

int pixelEqual(const pixel_t *a, const pixel_t *b)
{
	return (a->r == b->r) &&  (a->g == b->g) && (a->b == b->b);
}

rgbimage_t *rgbi_loadPNG(const char *in_filename)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers;
	int w,h,depth,color;
	int y, i;
	rgbimage_t *img = NULL;
	uint8_t header[8];
	FILE *fptr_in;

	fptr_in = fopen(in_filename, "rb");
	if (!fptr_in) {
		perror(in_filename);
		return NULL;
	}

	if (8 != fread(header, 1, 8, fptr_in)) {
		fclose(fptr_in);
		return NULL;
	}

	if (png_sig_cmp(header, 0, 8)) {
		fclose(fptr_in);
		fprintf(stderr, "Not a PNG file\n");
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fptr_in);
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fptr_in);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		goto error;
	}

	png_init_io(png_ptr, fptr_in);
	png_set_sig_bytes(png_ptr, 8);

	png_set_packing(png_ptr);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16|PNG_TRANSFORM_PACKING|PNG_TRANSFORM_EXPAND, NULL);

	w = png_get_image_width(png_ptr, info_ptr);
	h = png_get_image_height(png_ptr, info_ptr);
	depth = png_get_bit_depth(png_ptr, info_ptr);
	color = png_get_color_type(png_ptr, info_ptr);

	if (g_verbose) {
		printf("Image: %d x %d, ",w,h);
		printf("Bit depth: %d, ", depth);
		printf("Color type: %d\n", color);
	}
	row_pointers = png_get_rows(png_ptr, info_ptr);

	img = allocRGBimage(w, h);
	if (!img) {
		goto error;
	}

	switch(color)
	{
		case PNG_COLOR_TYPE_PALETTE:
			break;

		case PNG_COLOR_TYPE_GRAY:
			for (y=0; y<h; y++) {
				for (i=0; i<w; i++) {
					uint8_t *src = row_pointers[y] + i;
					setPixelRGB(img, i, y, src[0], src[0], src[0]);
				}
			}
			break;

		case PNG_COLOR_TYPE_RGBA:
			for (y=0; y<h; y++) {
				for (i=0; i<w; i++) {
					uint8_t *src = row_pointers[y] + i*4;

					setPixelRGBA(img, i, y, src[0], src[1], src[2], src[3]);
				}
			}
			break;

		case PNG_COLOR_TYPE_RGB:
			for (y=0; y<h; y++) {
				for (i=0; i<w; i++) {
					uint8_t *src = row_pointers[y] + i*3;

					setPixelRGB(img, i, y, src[0], src[1], src[2]);
				}
			}
			break;

		default:
			fprintf(stderr, "Unsupported color type.\n");
			goto error;
	}



	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (fptr_in) {
		fclose(fptr_in);
	}
	return img;

error:
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (img) {
		freeRGBimage(img);
	}
	if (fptr_in) {
		fclose(fptr_in);
	}

	return NULL;
}

int rgbi_savePNG(const char *out_filename, rgbimage_t *img)
{
	png_structp  png_ptr;
	png_infop  info_ptr;
	int color_type = PNG_COLOR_TYPE_RGB;
	int w = img->w;
	int h = img->h;
	int y, i;
	FILE *fptr;
	unsigned char rowbuf[w*3];

	fptr = fopen(out_filename, "wb");
	if (!fptr) {
		perror(out_filename);
		return -1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fptr);
		return 4;   /* out of memory */
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fptr);
		return 4;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fptr);
		return 2;
	}

	png_init_io(png_ptr, fptr);

	png_set_IHDR(png_ptr, info_ptr, w, h, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);


	png_write_info(png_ptr, info_ptr);

	for (y=0; y<h; y++) {
		for (i=0; i<w; i++) {
			pixel_t *p = getPixel(img, i, y);
			rowbuf[i*3+0] = p->r;
			rowbuf[i*3+1] = p->g;
			rowbuf[i*3+2] = p->b;
		}

		png_write_row(png_ptr, rowbuf);
	}

	png_write_end(png_ptr, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fptr);

	return 0;
}


int rgbi_savePNG_indexed(const char *out_filename, rgbimage_t *img, const palette_t *pal)
{
	png_structp  png_ptr;
	png_infop  info_ptr;
	int color_type = PNG_COLOR_TYPE_PALETTE;
	int w = img->w;
	int h = img->h;
	int y, i;
	FILE *fptr;
	unsigned char rowbuf[w];
	png_color palette[256] = { };

	// Copy sprite palette to png_color array
	for (i=0; i<pal->count; i++) {
		palette[i].red = pal->colors[i].r;
		palette[i].green = pal->colors[i].g;
		palette[i].blue = pal->colors[i].b;
	}

	fptr = fopen(out_filename, "wb");
	if (!fptr) {
		perror(out_filename);
		return -1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fptr);
		return 4;   /* out of memory */
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fptr);
		return 4;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fptr);
		return 2;
	}

	png_init_io(png_ptr, fptr);

	png_set_IHDR(png_ptr, info_ptr, w, h, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_set_PLTE(png_ptr, info_ptr, palette, pal->count);

	png_write_info(png_ptr, info_ptr);

	for (y=0; y<h; y++) {
		for (i=0; i<w; i++) {
			pixel_t *p = getPixel(img, i, y);
			rowbuf[i] = palette_findBestMatch(pal, p->r, p->g, p->b, COLORMATCH_METHOD_DEFAULT);
		}

		png_write_row(png_ptr, rowbuf);
	}

	png_write_end(png_ptr, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fptr);

	return 0;
}

static uint8_t errdiff(uint8_t org, int error, int mult, int div)
{
	return clamp8bit(org + error * mult / div);
}

static void distributeError(rgbimage_t *img, int x, int y, int e_r, int e_g, int e_b)
{
	uint8_t r=0,g=0,b=0;
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

int ditherImage_none(rgbimage_t *img, palette_t *pal)
{
	int x, y;
	pixel_t *pix;
	int idx;

	if ((pal->count == 0) || (pal->count > 256)) {
		fprintf(stderr, "Bad palette color count: %d\n", pal->count);
		return -1;
	}

	for (y=0; y<img->h; y++) {
		for (x=0; x<img->w; x++) {
			pix = getPixel(img, x, y);

			idx = palette_findBestMatch(pal, pix->r, pix->g, pix->b, 0);

			pix->r = pal->colors[idx].r;
			pix->g = pal->colors[idx].g;
			pix->b = pal->colors[idx].b;
		}
	}

	return 0;
}

int ditherImage_err1(rgbimage_t *img, palette_t *pal)
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

			// an attempt to generate less noise - mixed results...
			if ((pix->r > 200) || (pix->g > 128) || (pix->b) > 250) {
				distributeError(img, x, y, e_r/3, e_g/2, e_b/6);
			} else {
			}

		}
	}

	return 0;
}


int ditherImage_floyd_steinberg(rgbimage_t *img, palette_t *pal)
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

struct ditherAlgo {
	const char *shortname;
	const char *name;
	int (*algoFunc)(rgbimage_t *img, palette_t *pal);
};

static struct ditherAlgo ditherAlgos[] = {
	[DITHERALGO_NONE] = {
		"nop",   "None - best match", ditherImage_none
	},
	[DITHERALGO_FLOYD_STEINBERG] = {
		"fs",    "Floyd-Steinberg error diffusion (standard)", ditherImage_floyd_steinberg
	},
	[DITHERALGO_ERROR_DIFF_THRESHOLD] = {
		"err1",    "Error diffusion with threshold (only dither large errors)", ditherImage_err1
	},

};

const char *getDitherAlgoName(uint8_t algo)
{
	if (algo >= ARRAY_SIZE(ditherAlgos)) {
		return NULL;
	}
	return ditherAlgos[algo].name;
}

const char *getDitherAlgoShortName(uint8_t algo)
{
	if (algo >= ARRAY_SIZE(ditherAlgos)) {
		return NULL;
	}
	return ditherAlgos[algo].shortname;
}

int getDitherAlgoByShortName(const char *sname)
{
	int i;
	for (i=0; i<ARRAY_SIZE(ditherAlgos); i++) {
		if (0 == strcasecmp(ditherAlgos[i].shortname, sname)) {
			return i;
		}
	}
	return -1;
}

int ditherImage(rgbimage_t *img, palette_t *pal, uint8_t algo)
{
	if (algo >= ARRAY_SIZE(ditherAlgos)) {
		fprintf(stderr, "Invalid dithering algorithm id (%d)\n", algo);
		return -1;
	}

	return ditherAlgos[algo].algoFunc(img, pal);
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


