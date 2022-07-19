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

uint8_t tilemap_getUsedFlags(tilemap_t *tm);
int tilemap_countFlag(tilemap_t *tm, uint8_t flag);

void tilemap_printInfo(tilemap_t *tm);

int tilemap_saveSMS(tilemap_t *tm, const char *filename);

#endif // _tilemap_h__
