#ifndef _tilemap_h__
#define _tilemap_h__

#include <stdint.h>

#include "tilecatalog.h"

typedef struct _tilecatalog tilecatalog_t;

typedef struct _tilemap {
	int w, h;
	uint32_t *data;
	uint8_t *flags; // TILEMAP_FLIP_*
} tilemap_t;

struct tileUseEntry {
	uint32_t id;
	int usecount;
};

#define TILEMAP_FLIP_X	1
#define TILEMAP_FLIP_Y	2
#define TILEMAP_FLIP_XY	(TILEMAP_FLIP_X|TILEMAP_FLIP_Y)

tilemap_t *tilemap_allocate(int tiles_w, int tiles_h);
void tilemap_free(tilemap_t *tm);

void tilemap_setTileID(tilemap_t *tm, int x, int y, uint32_t id, uint8_t flags);
uint32_t tilemap_getTileID(tilemap_t *tm, int x, int y);
uint8_t tilemap_getTileFlags(tilemap_t *tm, int x, int y);

int tilemap_countUniqueIDs(tilemap_t *tm);
int getAllUsedTiles(tilemap_t *tm, struct tileUseEntry **dst);

int tilemap_replaceID(tilemap_t *tm, int orig_id, int new_id, uint8_t new_flags);

sprite_t *tilemap_toSprite(tilemap_t *tm, tilecatalog_t *cat, palette_t *pal);

// Fill a tilemap with catalog tile IDs for image. Missing tiles are NOT
// added to catalog, those are set to 0xFFFF.
//
// return the number of unique tiles that were missing and would need to be added
// to the catalog.
//
int tilemap_populateFromCatalog(tilemap_t *tm, sprite_t *img, tilecatalog_t *cat);

uint8_t tilemap_getUsedFlags(tilemap_t *tm);
int tilemap_countFlag(tilemap_t *tm, uint8_t flag);

// compare two tilemaps. Must have the same size and refer tiles IDs from the
// same catalog.
//
// Returns the number of cells using the same tile id and flags.
//
int tilemap_compare(tilemap_t *tm1, tilemap_t *tm2);

void tilemap_printInfo(tilemap_t *tm);

int tilemap_saveSMS(tilemap_t *tm, const char *filename);
void tilemap_listEntries(struct tileUseEntry *entries, int count);
void tilemap_sortEntries(struct tileUseEntry *entries, int count);

// Return true and sets x/y if tile is used only in one place
int tilemap_findUseOf(tilemap_t *tm, uint32_t tile, int *x, int *y);

#endif // _tilemap_h__
