#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <png.h>
#include "sprite.h"
#include "globals.h"
#ifdef WITH_GIF_SUPPORT
#include "gif_lib.h"
#endif

static jmp_buf jmpbuf;

sprite_t *allocSprite(uint16_t w, uint16_t h, uint16_t palsize, uint8_t flags)
{
	sprite_t *spr;

	spr = calloc(1, sizeof(sprite_t));
	if (!spr) {
		perror("Could not allocate sprite_t");
		return NULL;
	}

	spr->pixels = calloc(1, w*h);
	if (!spr->pixels) {
		perror("Could not allocate pixels");
		free(spr);
		return NULL;
	}

	spr->mask = calloc(1, w*h);
	if (!spr->mask) {
		perror("Could not allocate mask");
		free(spr->pixels);
		free(spr);
		return NULL;
	}

	spr->w = w;
	spr->h = h;
	spr->palette.count = palsize;
	spr->flags = flags;

	return spr;
}

sprite_t *duplicateSprite(const sprite_t *spr)
{
	sprite_t *s;

	s = allocSprite(spr->w, spr->h, spr->palette.count, 0);
	if (!s)
		return NULL;

	s->transparent_color = spr->transparent_color;
	s->flags = spr->flags;
	sprite_copyPalette(spr, s);
	memcpy(s->pixels, spr->pixels, spr->w * spr->h);

	return s;
}

// Set the mask to 1 (transparent) for pixels of a particular color.
void spriteUpdateTransparent(sprite_t *spr, uint8_t id)
{
	int i, size = spr->w * spr->h;
	uint8_t *pixel = spr->pixels, *mask = spr->mask;

	for (i=0; i<size; i++) {
		if (*pixel == id) {
			*mask = 0xff;
		}
		pixel++;
		mask++;
	}
}

// remove a color from the palette, and update
// the pixel data.
//
// Useful for PNGs with transparence where ID 0
// was transparent and the rest of the palette is
// in a specific order (eg: EGA) which must normally
// start at 0
int spriteDeleteColor(sprite_t *spr, uint8_t id)
{
	uint8_t *pix = spr->pixels;
	int size = spr->w * spr->h, i;

	if (id > 0) {
		fprintf(stderr, "Error: Removing non-zero color not implemented\n");
		return -1;
	} else {
		memmove(spr->palette.colors,
				spr->palette.colors + 1,
				sizeof(palent_t) * (spr->palette.count - 1));
		spr->palette.count--;
	}

	for (i=0; i<size; i++) {
		if (*pix > id) {
			*pix = *pix-1;
		}
		pix++;
	}

	return 0;
}

void freeSprite(sprite_t *spr)
{
	if (spr) {
		if (spr->mask) {
			free(spr->mask);
		}
		if (spr->pixels) {
			free(spr->pixels);
		}
		free(spr);
	}
}

void printSprite(sprite_t *spr)
{
	int i, y;
	uint8_t *pix, *mask;
	printf("Sprite size: %d x %d\n", spr->w, spr->h);
	printf("Palette colors: %d\n", spr->palette.count);
	printf("Palette: ");
	palette_print(&spr->palette);

	pix = spr->pixels;
	mask = spr->mask;
	for (y=0; y<spr->h; y++) {
		for (i=0; i<spr->w; i++) {
			if (*mask == 0xff) {
				printf("...");
			} else {
				printf("%02x ", *pix & (0xff^*mask));
				//printf("*");
			}
			mask++;
			pix++;
		}
		printf("\n");
	}
}

static void writepng_error_handler(png_structp png_ptr, png_const_charp msg)
{
	fprintf(stderr, "writepng libpng error: %s\n", msg);
	fflush(stderr);

	longjmp(jmpbuf, 1);
}

int sprite_savePNG(const char *out_filename, sprite_t *spr, uint32_t flags)
{
	png_structp  png_ptr;
	png_infop  info_ptr;
	int color_type = PNG_COLOR_TYPE_PALETTE;
	int w = spr->w;
	int h = spr->h;
	png_color palette[256] = { };
	int y, i;
	FILE *fptr;

	// Copy sprite palette to png_color array
	for (i=0; i<spr->palette.count; i++) {
		palette[i].red = spr->palette.colors[i].r;
		palette[i].green = spr->palette.colors[i].g;
		palette[i].blue = spr->palette.colors[i].b;
	}

	fptr = fopen(out_filename, "wb");
	if (!fptr) {
		perror(out_filename);
		return -1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, writepng_error_handler, NULL);
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

	if (setjmp(jmpbuf)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fptr);
		return 2;
	}

	png_init_io(png_ptr, fptr);

	png_set_IHDR(png_ptr, info_ptr, w, h, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_set_PLTE(png_ptr, info_ptr, palette, spr->palette.count);
	if (spr->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR) {
		png_byte trans = spr->transparent_color;
		png_set_tRNS(png_ptr, info_ptr, &trans, 1, NULL);
		if (g_verbose) {
			printf("Transparent color in output: %d\n", trans);
		}
	}

	png_write_info(png_ptr, info_ptr);

	for (y=0; y<h; y++) {
		png_write_row(png_ptr, spr->pixels + y * spr->w);
	}

	png_write_end(png_ptr, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fptr);

	return 0;


}

sprite_t *sprite_loadPNG(const char *in_filename, int n_expected_colors, uint32_t flags)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers;
	int w,h,depth,color;
	int y, i;
	int num_palette;
	png_colorp palette;
	sprite_t *spr = NULL;
	uint8_t header[8];
	FILE *fptr_in;
	int num_trans = 0;
	png_bytep trans;

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
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING, NULL);
	//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA, NULL);

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

	switch(color)
	{
		case PNG_COLOR_TYPE_PALETTE:
			break;
		default:
			fprintf(stderr, "Unsupported color type. File must use a palette.\n");
			goto error;
	}

	if (!png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette)) {
		fprintf(stderr, "Error getting palette\n");
		goto error;
	}

	if (depth != 8) {
		fprintf(stderr, "Error, expected 8 bit depth\n");
		goto error;
	}

	spr = allocSprite(w, h, num_palette, 0);
	if (!spr) {
		goto error;
	}

	// copy pixel data
	for (y=0; y<h; y++) {
		memcpy(spr->pixels + y*w, row_pointers[y], w);
	}

	// copy palette
	for (i=0; i<num_palette; i++)
	{
		spr->palette.colors[i].r = palette[i].red;
		spr->palette.colors[i].g = palette[i].green;
		spr->palette.colors[i].b = palette[i].blue;
	}

	if (g_verbose) {
		printf("PNG palette size: %d\n", num_palette);
	}

	// Check transparency
	png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
	if (g_verbose) {
		printf("Num transparent colors: %d\n", num_trans);
	}
	if (num_trans > 0)
	{
		// build mask based on transparent color
		for (i=0; i<num_trans; i++) {
			if (g_verbose) {
				printf("Transparent color: %d\n", trans[i]);
			}
			spriteUpdateTransparent(spr, trans[i]);
		}

		if (flags & SPRITE_LOADFLAG_DROP_TRANSPARENT) {
			if (g_verbose) {
				printf("Dropping transparent color(s)...\n");
			}
			// drop transparent colors from palette and pixel data
			if ((n_expected_colors == 0) || ((n_expected_colors != 0) && (spr->palette.count != n_expected_colors))) {
				for (i=0; i<num_trans; i++) {
					spriteDeleteColor(spr, trans[i]);
				}
			}
		}
		else {
			if (num_trans > 2) {
				fprintf(stderr, "Only one transparent color is supported\n");
				goto error;
			}
			spr->transparent_color = trans[0];
			spr->flags |= SPRITE_FLAG_USE_TRANSPARENT_COLOR;
		}
	}

	if (n_expected_colors != 0) {
		if (spr->palette.count != n_expected_colors) {
			fprintf(stderr, "Expected %d colors but found %d\n", n_expected_colors, spr->palette.count);
			goto error;
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (fptr_in) {
		fclose(fptr_in);
	}
	return spr;

error:
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (spr) {
		freeSprite(spr);
	}
	if (fptr_in) {
		fclose(fptr_in);
	}

	return NULL;
}

#ifdef WITH_GIF_SUPPORT
sprite_t *sprite_loadGIF(const char *in_filename, int n_expected_colors, uint32_t flags)
{
	GifFileType *gf;
	GifColorType *color;
	sprite_t *sprite = NULL;
	int err = 0;
	int i;
	int y;

	gf = DGifOpenFileName(in_filename, &err);
	if (!gf) {
		if (err == D_GIF_ERR_NOT_GIF_FILE) {
			return NULL;
		}
		fprintf(stderr, "Could not open gif: %s\n", GifErrorString(err));
		return NULL;
	}

	if (GIF_OK != DGifSlurp(gf)) {
		fprintf(stderr, "Could not read gif: %s\n", GifErrorString(err));
		goto error;
	}

	if (gf->ImageCount < 1) {
		fprintf(stderr, "No images in GIF!?\n");
		goto error;
	}

	// Create sprite with same size as GIF
	sprite = allocSprite(gf->SWidth, gf->SHeight, gf->SColorMap->ColorCount, 0);
	if (!sprite) {
		fprintf(stderr, "failed to allocate sprite\n");
		goto error;
	}

	// Copy colormap
	color = gf->SColorMap->Colors;
	for (i=0; i<gf->SColorMap->ColorCount; i++,color++) {
		palette_setColor(&sprite->palette, i, color->Red, color->Green, color->Blue);
	}

	// Set background color
	memset(sprite->pixels, gf->SBackGroundColor, sprite->w * sprite->h);

	// Copy image
	for (i=0,y = gf->SavedImages[0].ImageDesc.Top; y < gf->SavedImages[0].ImageDesc.Top + gf->SavedImages[0].ImageDesc.Height; y++,i++) {
		memcpy(sprite->pixels + gf->SavedImages[0].ImageDesc.Left + y * sprite->w,
				gf->SavedImages[0].RasterBits + gf->SavedImages[0].ImageDesc.Width * i,
				gf->SavedImages[0].ImageDesc.Width);
	}

	if (GIF_OK != DGifCloseFile(gf, &err)) {
		fprintf(stderr, "Warning: Could not close gif: %s\n", GifErrorString(err));
	}

	return sprite;

error:
	if (gf) {
		if (GIF_OK != DGifCloseFile(gf, &err)) {
			fprintf(stderr, "Could not close gif: %s\n", GifErrorString(err));
		}
	}
	if (sprite) {
		freeSprite(sprite);
	}

	return NULL;
}
#endif

sprite_t *sprite_load(const char *in_filename, int n_expected_colors, uint32_t flags)
{
	sprite_t *spr;

	spr = sprite_loadPNG(in_filename, n_expected_colors, flags);
	if (spr)
		return spr;

#ifdef WITH_GIF_SUPPORT
	spr = sprite_loadGIF(in_filename, n_expected_colors, flags);
	if (spr)
		return spr;
#endif

	return NULL;
}

void sprite_setPixelSafe(sprite_t *spr, int x, int y, int value)
{
	if (x >= spr->w)
		return;
	if (y >= spr->h)
		return;
	if (x < 0)
		return;
	if (y < 0)
		return;
	spr->pixels[y * spr->w + x] = value;
}


void sprite_setPixel(sprite_t *spr, int x, int y, int value)
{
	spr->pixels[y * spr->w + x] = value;
}

int sprite_getPixel(const sprite_t *spr, int x, int y)
{
	return spr->pixels[y * spr->w + x];
}

int sprite_getPixels8x8(const sprite_t *spr, int x, int y, uint8_t *dst)
{
	int Y,X;
	uint8_t *src;

	if ( ((x + 8) <= spr->w) && ((y + 8) <= spr->h)) {

		src = spr->pixels + y * spr->w + x;
		for (Y=0; Y<8; Y++) {
			memcpy(dst, src, 8);
			dst += 8;
			src += spr->w;
		}
		return 0;
	}

	for (Y=0; Y<8; Y++) {
		for (X=0; X<8; X++) {
			*dst = sprite_getPixelSafeExtend(spr, x + X, y + Y);
			dst++;
		}
	}
	return 0;
}

int sprite_getPixelSafe(const sprite_t *spr, int x, int y)
{
	if ((x < 0)||(y < 0)||(x >= spr->w)||(y >= spr->h)) {
		if (spr->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR) {
			return spr->transparent_color;
		}
		else {
			return sprite_getPixelSafeExtend(spr,x,y);
		}
	}

	return spr->pixels[y * spr->w + x];
}


int sprite_getPixelSafeExtend(const sprite_t *spr, int x, int y)
{
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= spr->w)
		x = spr->w-1;
	if (y >= spr->h)
		y = spr->h-1;

	return spr->pixels[y * spr->w + x];
}

void sprite_setPixelMaskSafe(sprite_t *spr, int x, int y, int value)
{
	if (x >= spr->w)
		return;
	if (y >= spr->h)
		return;

	spr->mask[y * spr->w + x] = value;
}

int sprite_getPixelMaskSafe(const sprite_t *spr, int x, int y)
{
	if (x >= spr->w)
		return 0;
	if (y >= spr->h)
		return 0;

	return spr->mask[y * spr->w + x];
}

void sprite_setPixelMask(sprite_t *spr, int x, int y, int value)
{
	spr->mask[y * spr->w + x] = value;
}

int sprite_getPixelMask(const sprite_t *spr, int x, int y)
{
	return spr->mask[y * spr->w + x];
}

int sprite_pixelIsOpaque(const sprite_t *spr, int x, int y)
{
	if (spr->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR) {
		return sprite_getPixel(spr, x, y) != spr->transparent_color;
	} else {
		return sprite_getPixelMask(spr, x, y);
	}
}

int sprite_copyPalette(const sprite_t *spr_src, sprite_t *spr_dst)
{
	memcpy(&spr_dst->palette, &spr_src->palette, sizeof(palette_t));
	return 0;
}

/**
 * Change the sprite palette without touching the pixels.
 */
void sprite_applyPalette(sprite_t *sprite, const palette_t *newpal)
{
	memcpy(&sprite->palette, newpal, sizeof(palette_t));
}

/**
 * Change the sprite palette and update pixels so colors stay the same.
 *
 * Note: All colors from the original palette must exist in the new palette.
 */
int sprite_remapPalette(sprite_t *sprite, const palette_t *dstpal)
{
	int y, x;
	palremap_lut_t remap_lut;

	if (palette_generateRemap(&sprite->palette, dstpal, &remap_lut) < 0) {
		return -1;
	}

	sprite_applyPalette(sprite, dstpal);

	for (y=0; y<sprite->h; y++) {
		for (x=0; x<sprite->w; x++) {
			int c;
			c = sprite_getPixel(sprite, x, y);
			sprite_setPixel(sprite, x, y, remap_lut.lut[c]);
		}
	}

	return 0;
}

sprite_t *sprite_packPixels(const sprite_t *spr, int bits_per_pixel)
{
	sprite_t *packedSprite;
	int source_pixels[8];
	int source_mask[8];
	int pixels_per_byte = 8/bits_per_pixel;
	int buf_w = spr->w / pixels_per_byte;
	uint8_t pixmask = 0xff >> (8-bits_per_pixel);
	int x,y,i;
	uint8_t pack, packed_mask;

	printf("Input width: %d, packed output width: %d\n", spr->w, buf_w);
	printf("Bits per pixel: %d\n", bits_per_pixel);
	printf("Pixels per byte: %d\n", pixels_per_byte);

	// Pack the the sprite pixels
	packedSprite = allocSprite(buf_w, spr->h, spr->palette.count, 0);
	if (!packedSprite)
		return NULL;

	for (y=0; y<spr->h; y++) {
		for (x=0; x<spr->w; x+=pixels_per_byte) {
			// Get source pixels to pack
			for (i=0; i<pixels_per_byte; i++) {
				source_pixels[i] = sprite_getPixel(spr, x+i, y);
				source_mask[i] = sprite_getPixelMask(spr, x+i, y);
			}

			// Pack them in one byte
			for (pack=0,packed_mask=0,i=0; i<pixels_per_byte; i++) {
				pack <<= bits_per_pixel;
				pack |= source_pixels[i] & pixmask;
				packed_mask <<= bits_per_pixel;
				packed_mask |= source_mask[i] & pixmask;
			}

			// write to output
			sprite_setPixel(packedSprite, x/pixels_per_byte, y, pack);
			sprite_setPixelMask(packedSprite, x/pixels_per_byte, y, packed_mask);
		}
	}

	// pre-apply mask to all pixels (so tranparent pixles are zero. Assumed later.
	for (i=0; i<packedSprite->w * packedSprite->h; i++) {
		packedSprite->pixels[i] = packedSprite->pixels[i] & (packedSprite->mask[i] ^ 0xff);
	}

	sprite_copyPalette(spr, packedSprite);

	return packedSprite;
}

void sprite_getFullRect(const sprite_t *sprite, spriterect_t *dst)
{
	if (!dst)
		return;

	dst->x = 0; dst->y = 0;
	dst->w = sprite->w;
	dst->h = sprite->h;
}


// NULL src means full surface
int sprite_copyRect(const sprite_t *src, const spriterect_t *src_rect, sprite_t *dst, const spriterect_t *dst_rect)
{
	int x,y,w,h,src_x,src_y,dst_x,dst_y;
	uint8_t pixel;

	if (src_rect) {
		src_x = src_rect->x;
		src_y = src_rect->y;
		w = src_rect->w;
		h = src_rect->h;
	} else {
		src_x = 0;
		src_y = 0;
		w = src->w;
		h = src->h;
	}

	if (dst_rect) {
		dst_x = dst_rect->x;
		dst_y = dst_rect->y;
	} else {
		dst_x = 0;
		dst_y = 0;
	}

//	printf("Before clipping %d,%d -> %d,%d [%d x %d]\n", src_x, src_y, dst_x, dst_y, w, h);

	// Validate/trim the destination
	if (dst_x + w > dst->w) { w -= (dst_x + w)-dst->w; }
	if (dst_y + h > dst->h) { h -= (dst_y + h)-dst->h; }
	if (dst_x < 0) { w += dst_x; src_x += -dst_x; dst_x = 0; }
	if (dst_y < 0) { h += dst_y; src_y += -dst_y; dst_y = 0; }
	if ((w <= 0) || (h <= 0)) { return -1; }

	if (src_x + w > src->w) { w -= (src_x + w)-src->w; }
	if (src_y + h > src->h) { h -= (src_y + h)-src->h; }
	if (src_x < 0) { w += src_x; dst_x += -src_x; src_x = 0; }
	if (src_y < 0) { h += src_y; dst_y += -src_y; src_y = 0; }
	if ((w <= 0) || (h <= 0)) { return -1; }

//	printf("After clipping %d,%d -> %d,%d [%d x %d]\n", src_x, src_y, dst_x, dst_y, w, h);

//	printf("Copyrect loop: %d x %d\n", w,h);

	// Note: This really is not optimised!

	if (src->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR)
	{
		// Blitting with a transparent color source
		for (y=0; y<h; y++) {
			for (x=0; x<w; x++) {
				pixel = sprite_getPixel(src, x + src_x, y + src_y);
				if (pixel == src->transparent_color)
					continue;
				sprite_setPixel(dst, x + dst_x, y + dst_y, pixel);
			}
		}
	}
	else if (src->flags & SPRITE_FLAG_OPAQUE)
	{
		// Blitting from fully opaque sprite
		for (y=0; y<h; y++) {
			for (x=0; x<w; x++) {
				pixel = sprite_getPixel(src, x + src_x, y + src_y);
				sprite_setPixel(dst, x + dst_x, y + dst_y, pixel);
			}
		}
	} else {
		// Blitting using mask
		for (y=0; y<h; y++) {
			for (x=0; x<w; x++) {
				if (!sprite_getPixelMask(src, x + src_x, y + src_y)) {
					pixel = sprite_getPixel(src, x + src_x, y + src_y);
					sprite_setPixel(dst, x + dst_x, y + dst_y, pixel);
				}
			}
		}
	}

	return 0;
}

void sprite_fill(struct sprite *spr, int color)
{
	memset(spr->pixels, color, spr->w * spr->h);
}

int sprite_fillRect(struct sprite *spr, int x, int y, int w, int h, int color)
{
	int X,Y;

	// lazy low performance implementation
	for (Y=0; Y<h; Y++)
	{
		for (X=0; X<w; X++) {
			sprite_setPixelSafe(spr, x+X, y+Y, color);
		}
	}

	return 0;
}

int sprite_setPixelsStrip(struct sprite *spr, int x, int y, uint8_t *data, int count)
{
	if ((y < 0) || (y >= spr->h)) {
		return -1;
	}

	if (x < 0) {
		// skip out of range pixels
		data += -x;
		// shorten count
		count += x;
		// start at the beginning of the row
		x = 0;
	}

	// strip was fully outside the area
	if (count < 1) {
		return -1;
	}

	memcpy(spr->pixels + y * spr->w + x, data, count);

	return 0;
}

int sprite_panX(struct sprite *spr, int pan)
{
	int y;
	uint8_t linebuf[spr->w];
	int len1, len2;

	pan = pan % spr->w;

	if (pan == 0) { // nop
		return 0;
	}

	if (pan < 0) {
		pan = spr->w + pan;
	}

	// consider w = 100
	//
	// A-----------------------------------------B
	//
	// if pan=10
	// -----BA------------------------------------
	// len2          len1
	//
	// if pan=90
	// -----------------------------------BA------
	// len2                                len1
	//
	len1 = spr->w - pan;
	len2 = spr->w - len1;

	for (y=0; y<spr->h; y++) {
		memcpy(linebuf + pan, spr->pixels + y * spr->w , len1);
		memcpy(linebuf, spr->pixels + y * spr->w + len1 , len2);
		memcpy(spr->pixels + y * spr->w, linebuf, spr->w);
	}

	return 0;
}

