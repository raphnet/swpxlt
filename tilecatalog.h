#ifndef _tilecatalog_h__
#define _tilecatalog_h__

#include <stdint.h>
#include "sprite.h"
#include "tilemap.h"

typedef struct _tilemap tilemap_t;

typedef struct _SMS_Tile {
	// The 8x8 sprite in 8bpp format. For finding already existing sprites
	uint8_t image_8bpp[64];
	// SMS planar
	uint8_t native[32];
	// How many time this tile was added to the catalog
	uint32_t usage_count;
} sms_tile_t;

typedef struct _tilecatalog {
	uint32_t num_tiles;
	sms_tile_t *tiles;
	uint32_t alloc_count;
} tilecatalog_t;

tilecatalog_t *tilecat_new(void);
void tilecat_free(tilecatalog_t *tc);

// id : Tile ID it was added or found as (can be NULL)
// flags : Flip x/y flags (when found in catalog but flipped) (can be NULL)
//
int tilecat_addFromSprite(tilecatalog_t *tc, sprite_t *src, int x, int y, uint32_t *id, uint8_t *flags);

// tm can be NULL
int tilecat_addAllFromSprite(tilecatalog_t *tc, sprite_t *src, tilemap_t *tm);

void tilecat_printInfo(tilecatalog_t *tc);
int tilecat_toPNG(tilecatalog_t *tc, palette_t *palette, const char *savecat_filename);

void tilecat_drawTile(sms_tile_t *t, sprite_t *dst, int x, int y, uint8_t flags);

// Return a measure of the difference between tile1 and tile2. Flags will be set to
// the flip bits which returns the least difference.
uint32_t tilecat_getDifferenceScore(tilecatalog_t *tc, uint32_t tile1, uint32_t tile2, palette_t *palette, uint8_t *flags);

void tilecat_mixTiles(tilecatalog_t *tc, uint32_t tile1, uint32_t tile2);

// Convert 8bpp tiles to 2bpp SMS tiles.
//
// i.e. update the native[32] member from the image_8bpp[64] member for all tiles.
int tilecat_updateNative(tilecatalog_t *tc);

// save the catalog in SMS format. Call tilecat_updateNative first!
int tilecat_saveTiles(tilecatalog_t *tc, const char *filename);

#endif // _tilecatalog_h__
