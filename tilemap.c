#include <stdio.h>
#include <stdlib.h>
#include "tilemap.h"

tilemap_t *tilemap_allocate(int tiles_w, int tiles_h)
{
	tilemap_t *tmap;

	tmap = calloc(1, sizeof(tilemap_t));
	if (!tmap) {
		perror("tilemap");
		return tmap;
	}

	tmap->data = calloc(1, tiles_w * tiles_h * sizeof(uint32_t));
	if (!tmap->data) {
		perror("tilemap data");
		free(tmap);
		return NULL;
	}

	tmap->flags = calloc(1, tiles_w * tiles_h * sizeof(uint8_t));
	if (!tmap->flags) {
		perror("tilemap flags");
		free(tmap->data);
		free(tmap);
		return NULL;
	}


	tmap->w = tiles_w;
	tmap->h = tiles_h;

	return tmap;
}

void tilemap_free(tilemap_t *tm)
{
	if (tm) {
		if (tm->data) {
			free(tm->data);
		}
		free(tm);
	}
}

void tilemap_setTileID(tilemap_t *tm, int x, int y, uint32_t id, uint8_t flags)
{
	tm->data[tm->w * y + x] = id;
	tm->flags[tm->w * y + x] = flags;
}

uint32_t tilemap_getTileID(tilemap_t *tm, int x, int y)
{
	return tm->data[tm->w * y + x];
}

uint8_t tilemap_getTileFlags(tilemap_t *tm, int x, int y)
{
	return tm->flags[tm->w * y + x];
}


int getAllUsedTiles(tilemap_t *tm, struct tileUseEntry **dst)
{
	struct tileUseEntry *entries;
	int n_entries;
	int i, j;
	uint32_t id;

	entries = calloc(1, tm->w * tm->h * sizeof(struct tileUseEntry));
	if (!entries) {
		perror(__func__);
		return 1;
	}

	n_entries = 0;
	for (i=0; i<tm->w * tm->h; i++) {
		// Get tile in current cell
		id = tm->data[i];

		// look if it has an entry
		for (j=0; j<n_entries; j++) {
			// yes it has, increase use count
			if (entries[j].id == id) {
				entries[j].usecount++;
				break;
			}
		}
		if (j>=n_entries) {
			// no entry found, create one
			entries[n_entries].id = id;
			entries[n_entries].usecount = 1;
			n_entries++;
		}

	}

	if (dst) {
		*dst = entries;
	}

	return n_entries;
}

int tilemap_countUniqueIDs(tilemap_t *tm)
{
	return getAllUsedTiles(tm, NULL);
}

int tilemap_replaceID(tilemap_t *tm, int orig_id, int new_id, uint8_t new_flags)
{
	int i;
	for (i=0; i<tm->w * tm->h; i++) {
		if (tm->data[i] == orig_id) {
			tm->data[i] = new_id;
			tm->flags[i] = new_flags;
		}
	}
	return 0;
}

sprite_t *tilemap_toSprite(tilemap_t *tm, tilecatalog_t *cat, palette_t *pal)
{
	sprite_t *spr;
	int x,y;
	uint32_t id;
	uint8_t flags;

	spr = allocSprite(tm->w * 8, tm->h * 8, pal->count, SPRITE_FLAG_OPAQUE);
	if (!spr)
		return NULL;

	sprite_applyPalette(spr, pal);

	for (y=0; y<tm->h; y++) {
		for (x=0; x<tm->w; x++) {
			id = tilemap_getTileID(tm, x, y);
			flags = tilemap_getTileFlags(tm, x, y);
//			printf("Drawing tile %d at %d,%d\n", id, x, y);
			tilecat_drawTile(&cat->tiles[id],spr,x*8,y*8,flags);
		}
	}

	return spr;
}

uint8_t tilemap_getUsedFlags(tilemap_t *tm)
{
	int i;
	uint8_t flags = 0;

	for (i=0; i<tm->w * tm->h; i++) {
		flags |= tm->flags[i];
	}

	return flags;
}

int tilemap_countFlag(tilemap_t *tm, uint8_t flag)
{
	int c, i;

	for (c=0,i=0; i<tm->w * tm->h; i++) {
		if ((tm->flags[i] & flag) == flag) {
			c++;
		}
	}

	return c;
}

void tilemap_printInfo(tilemap_t *map)
{
	uint8_t used_flags = tilemap_getUsedFlags(map);
	int total_tiles = map->w * map->h;
	int unique_tiles = tilemap_countUniqueIDs(map);

	printf("Tilemap info: {\n");
	printf("  Tilemap size: %d x %d\n", map->w, map->h);
	printf("  Total tiles: %d\n", total_tiles);
	printf("  Tilemap unique tiles: %d\n", unique_tiles);
	if (total_tiles > 0) {
		printf("  Reuse ratio: %.1f%%\n", unique_tiles * 100.0 / total_tiles);
	}
	printf("  Used flags: ");
	if (used_flags == 0) {
		printf("None (no flipping)\n");
	} else {
		if ((used_flags & TILEMAP_FLIP_XY) == (TILEMAP_FLIP_XY)) {
			printf("FLIP_XY(%d) ", tilemap_countFlag(map, TILEMAP_FLIP_XY));
		}
		else
		{
			if (used_flags & TILEMAP_FLIP_X) {
				printf("FLIP_X(%d) ", tilemap_countFlag(map, TILEMAP_FLIP_X));
			}
			if (used_flags & TILEMAP_FLIP_Y) {
				printf("FLIP_Y(%d) ", tilemap_countFlag(map, TILEMAP_FLIP_Y));
			}
		}
		printf("\n");
	}
	printf("}\n");
}

int tilemap_saveSMS(tilemap_t *tm, const char *filename)
{
	FILE *fptr;
	uint16_t smstile;
	uint8_t buf[2];
	int i;
	uint8_t flags;

	fptr = fopen(filename, "wb");
	if (!fptr) {
		perror(filename);
		return -1;
	}

	for (i=0; i<tm->w * tm->h; i++) {
		flags = tm->flags[i];
		smstile = tm->data[i];

		if (flags & TILEMAP_FLIP_X) {
			smstile |= 0x200;
		}
		if (flags & TILEMAP_FLIP_Y) {
			smstile |= 0x400;
		}

		buf[0] = smstile;
		buf[1] = smstile >> 8;
		fwrite(buf, 2, 1, fptr);
	}

	fclose(fptr);

	return 0;
}


