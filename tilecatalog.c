#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tilecatalog.h"


tilecatalog_t *tilecat_new(void)
{
	tilecatalog_t *tc;

	tc = calloc(1, sizeof(tilecatalog_t));
	if (!tc) {
		perror("could not allocate tile catalog");
		return NULL;
	}

	return tc;
}

void tilecat_free(tilecatalog_t *tc)
{
	if (tc) {
		if (tc->tiles) {
			free(tc->tiles);
		}
		free(tc);
	}
}

void tilecat_clear(tilecatalog_t *tc, int freemem)
{
	if (freemem) {
		tc->num_tiles = 0;
		tc->alloc_count = 0;
		free(tc->tiles);
		tc->tiles = NULL;
	}
	else {
		tc->num_tiles = 0;
	}
}

int tilecat_isInCatalog(tilecatalog_t *tc, uint8_t *image_8bpp, uint32_t *tid)
{
	uint32_t i;

	for (i=0; i<tc->num_tiles; i++) {
		if (0 == memcmp(tc->tiles[i].image_8bpp, image_8bpp, 64)) {
			if (tid)
				*tid = i;
			return 1;
		}
	}

	return 0;
}

static void tilecat_flipX(const uint8_t *image_8bpp, uint8_t *dst)
{
	int x,y;

	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			dst[y*8+x] = image_8bpp[y*8+(7-x)];
		}
	}
}

static void tilecat_flipY(const uint8_t *image_8bpp, uint8_t *dst)
{
	int x,y;

	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			dst[y*8+x] = image_8bpp[(7-y)*8+x];
		}
	}
}

static void tilecat_flipXY(const uint8_t *image_8bpp, uint8_t *dst)
{
	int x,y;

	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			dst[y*8+x] = image_8bpp[(7-y)*8+(7-x)];
		}
	}
}

static int tilecat_isInCatalogFlags(tilecatalog_t *tc, uint8_t *image_8bpp, uint32_t *tid, uint8_t *flags)
{
	uint32_t i;

	for (i=0; i<tc->num_tiles; i++) {
		if (0 == memcmp(tc->tiles[i].image_8bpp, image_8bpp, 64)) {
			if (tid) { *tid = i; }
			if (flags) { *flags = 0; }
			return 1;
		}
		if (0 == memcmp(tc->tiles[i].image_8bpp_x, image_8bpp, 64)) {
			if (tid) { *tid = i; }
			if (flags) { *flags = TILEMAP_FLIP_X; }
			return 1;
		}
		if (0 == memcmp(tc->tiles[i].image_8bpp_y, image_8bpp, 64)) {
			if (tid) { *tid = i; }
			if (flags) { *flags = TILEMAP_FLIP_Y; }
			return 1;
		}
		if (0 == memcmp(tc->tiles[i].image_8bpp_xy, image_8bpp, 64)) {
			if (tid) { *tid = i; }
			if (flags) { *flags = TILEMAP_FLIP_XY; }
			return 1;
		}

	}

	return 0;
}


int tilecat_isTileInCatalog(tilecatalog_t *tc, sprite_t *img, int x, int y, uint32_t *tid, uint8_t *flags)
{
	uint8_t image_8bpp[64];

	sprite_getPixels8x8(img, x, y, image_8bpp);

	if (tilecat_isInCatalogFlags(tc, image_8bpp, tid, flags)) {
		return 1;
	}

	return 0;
}

static int tilecat_addTile(tilecatalog_t *tc, uint8_t *image_8bpp, uint32_t *tid)
{
	size_t tmp;

	if (tc->num_tiles >= 0xfffffffe)
		return -1;

	if (tc->num_tiles >= tc->alloc_count) {
//		printf("Cur alloc count: %d\n",tc->alloc_count);
		tc->alloc_count += 128;
		tmp = sizeof(sms_tile_t) * tc->alloc_count;
//		printf("Alloc size: %d\n", tmp);
		tc->tiles = realloc(tc->tiles, tmp);
		if (!tc->tiles) {
			perror("Could not increase size of tile catalog\n");
			return -1;
		}
//		printf("New alloc count: %d\n",tc->alloc_count);
	}

	memset(&tc->tiles[tc->num_tiles], 0, sizeof(sms_tile_t));
	memcpy(tc->tiles[tc->num_tiles].image_8bpp, image_8bpp, 64);

	// Add pre-flipped versions for faster search
	tilecat_flipX(image_8bpp, tc->tiles[tc->num_tiles].image_8bpp_x);
	tilecat_flipY(image_8bpp, tc->tiles[tc->num_tiles].image_8bpp_y);
	tilecat_flipXY(image_8bpp, tc->tiles[tc->num_tiles].image_8bpp_xy);

	tc->tiles[tc->num_tiles].usage_count = 1;

	if (tid) {
		*tid = tc->num_tiles;
	}

	tc->num_tiles++;

	return 0;
}

int tilecat_addFromSprite(tilecatalog_t *tc, sprite_t *src, int x, int y, uint32_t *id, uint8_t *flags)
{
	uint8_t image_8bpp[64];
	uint32_t tid;
	uint8_t tflags;

	//  Get 8x8 area
	sprite_getPixels8x8(src, x, y, image_8bpp);

	if (tilecat_isInCatalogFlags(tc, image_8bpp, &tid, &tflags)) {
//		printf("Already cataloged, ID is %u\n", tid);
		// already in catalog. Increase use count
		if (tc->tiles[tid].usage_count < 0xFFFFFFFF) {
			tc->tiles[tid].usage_count++;
		}

		if (id)
			*id = tid;

		if (flags)
			*flags = tflags;

		return 0;
	}

	if (tilecat_addTile(tc, image_8bpp, &tid)) {
		fprintf(stderr, "Failed to add sprite to catalog\n");
		return -1;
	}

//	printf("New tile catalogued as ID is %u\n", tid);

	if (id)
		*id = tid;

	// newly added tiles always added as-is without flipping
	if (flags)
		*flags = 0;


	return 0;
}

int tilecat_addAllFromSprite(tilecatalog_t *tc, sprite_t *src, tilemap_t *tm)
{
	int x,y;
	uint32_t tid;
	uint8_t flags;

	for (y=0; y<src->h; y+=8) {
		for (x=0; x<src->w; x+=8) {
			if (tilecat_addFromSprite(tc, src, x, y, &tid, &flags)) {
				return -1;
			}
			if (tm) {
				tilemap_setTileID(tm, x/8, y/8, tid, flags);
			}
		}
	}

	return 0;
}

void tilecat_printInfo(tilecatalog_t *tc)
{
	uint32_t i;
	uint32_t single_use=0;

	printf("Tile Catalog info {\n");
	printf("  Entries: %u\n", tc->num_tiles);

	printf("  Tiles used multiple times: {\n");
	for (i=0; i<tc->num_tiles; i++) {
		if (tc->tiles[i].usage_count > 1) {
			printf("    [%d] : Use count %d\n", i, tc->tiles[i].usage_count);
		} else {
			single_use++;
		}
	}
	printf("  }\n");
	printf("  Single use tiles: %d\n", single_use);

	printf("}\n");
}

void tilecat_drawTile(sms_tile_t *t, sprite_t *dst, int x, int y, uint8_t flags)
{
	int X,Y,i;
	uint8_t buf[64];
	uint8_t *src = t->image_8bpp;

	if ((flags & TILEMAP_FLIP_XY) == TILEMAP_FLIP_XY) {
		tilecat_flipXY(t->image_8bpp, buf);
		src = buf;
	} else if (flags & TILEMAP_FLIP_X) {
		tilecat_flipX(t->image_8bpp, buf);
		src = buf;
	} else if (flags & TILEMAP_FLIP_Y) {
		tilecat_flipY(t->image_8bpp, buf);
		src = buf;
	}

	i = 0;
	for (Y=0; Y<8; Y++) {
		for (X=0; X<8; X++) {
			sprite_setPixelSafe(dst, x + X, y + Y, src[i]);
			i++;
		}
	}
}

#define TILES_PER_ROW	32

int tilecat_toPNG(tilecatalog_t *tc, palette_t *palette, const char *savecat_filename)
{
	sprite_t *img;
	int h;
	int i;
	int x,y;

	if (tc->num_tiles < 1) {
		fprintf(stderr, "Cannot write %s - catalog empty\n", savecat_filename);
		return -1;
	}

	h = ((tc->num_tiles + (TILES_PER_ROW-1)) / TILES_PER_ROW) * 9;
	if (h == 0) {
		h = 9;
	}


	img = allocSprite(32*9, h, palette->count, 0);
	if (!img) {
		return -1;
	}

	sprite_applyPalette(img, palette);

	for (x=0,y=0,i=0; i<tc->num_tiles; i++) {
		tilecat_drawTile(&tc->tiles[i], img, x * 9, y * 9, 0);
		x++;
		if (x >= TILES_PER_ROW) {
			x = 0;
			y++;
		}
	}

	sprite_savePNG(savecat_filename, img, 0);

	freeSprite(img);

	return 0;
}

uint32_t tilecat_getDifferenceScore(tilecatalog_t *tc, uint32_t tile1, uint32_t tile2, palette_t *palette, uint8_t *flags)
{
	uint32_t score = 0;
	int i;
	uint8_t buf[64];
	uint8_t *src;
	int j;
	uint32_t best_score = 0;
	uint8_t best_flags = 0;
	uint8_t cur_flags;

	for (j=0; j<4; j++)
	{
		if (j == 3) {
			tilecat_flipX(tc->tiles[tile1].image_8bpp, buf);
			src = buf;
			cur_flags = TILEMAP_FLIP_X;
		} else if (j == 2) {
			tilecat_flipY(tc->tiles[tile1].image_8bpp, buf);
			src = buf;
			cur_flags = TILEMAP_FLIP_Y;
		} else if (j == 1) {
			tilecat_flipXY(tc->tiles[tile1].image_8bpp, buf);
			src = buf;
			cur_flags = TILEMAP_FLIP_XY;
		} else {
			src = tc->tiles[tile1].image_8bpp;
			cur_flags = 0;
		}

		for (score=0,i=0; i<64; i++)
		{
#if 0
			score += palette_compareColorsManhattan(palette,
									src[i],
									tc->tiles[tile2].image_8bpp[i]);
#else
			score += palette_compareColorsEuclidian(palette,
									src[i],
									tc->tiles[tile2].image_8bpp[i]);
#endif
		}

		if (j == 0) {
			best_score = score;
			best_flags = 0;
		} else {
			if (score < best_score) {
				best_score = score;
				best_flags = cur_flags;
			}
		}
	}

	if (flags) {
		*flags = best_flags;
	}

	return best_score;
}

void tilecat_mixTiles(tilecatalog_t *tc, uint32_t tile1, uint32_t tile2)
{
	int i;
	sms_tile_t *t1, *t2;

	t1 = &tc->tiles[tile1];
	t2 = &tc->tiles[tile2];

	for (i=0; i<64; i++) {
		t1->image_8bpp[i] = (i & 1) ? t2->image_8bpp[i] : t1->image_8bpp[i];
	}
}

static void convert8bppTo2bppPlanar(const uint8_t src[64], uint8_t dst[32])
{
	int row, col;
	uint8_t ob = 0, bit;
	int i;

	i = 0;
	for (row = 0; row < 8; row++) {
		for (bit = 1; bit <= 8; bit <<= 1) {
			for (col = 0; col < 8; col++) {
				ob <<= 1;
				if (src[row*8+col] & bit) {
					ob |= 1;
				}
			}
			dst[i] = ob;
			i++;
		}
	}
}

int tilecat_updateNative(tilecatalog_t *tc)
{
	uint32_t id;
	sms_tile_t *tile;

	tile = tc->tiles;
	for (id = 0; id<tc->num_tiles; id++, tile++) {
		convert8bppTo2bppPlanar(tile->image_8bpp, tile->native);
	}

	return 0;
}

int tilecat_saveTiles(tilecatalog_t *tc, const char *filename)
{
	FILE *fptr;
	uint32_t id;
	sms_tile_t *tile;

	fptr = fopen(filename, "wb");
	if (!fptr) {
		perror(filename);
		return -1;
	}

	tile = tc->tiles;
	for (id = 0; id<tc->num_tiles; id++, tile++) {
		fwrite(tile->native, 32, 1, fptr);
	}

	fclose(fptr);
	return 0;
}

