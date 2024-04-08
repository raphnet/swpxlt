#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tilemap.h"
#include "tilecatalog.h"
#include "smsanimencoder.h"

typedef struct tiledFrame {
	sprite_t *img; // pointer to anim->frames[x]
	tilemap_t *map;
	int panx;
} tiled_frame_t;

#define DIFF_TILE	1
#define DIFF_FLAGS	2

#define TILES_OFFSET	0x0000
#define PNT_OFFSET		0x3800

typedef struct encoderContext {
	int w, h;
	int num_frames;
	uint32_t flags;

	tiled_frame_t *frames;
	tilecatalog_t *catalog;

	struct encoderOutputFuncs *funcs;
	void *ctx;

	// buffers and vars for encoding
	uint8_t *diff;

	// what the system has in VRAM now (previous frame)
	uint16_t vram_pnt[1024];
	// what the system should have in VRAM next (current frame)
	uint16_t vram_pnt_next[1024];

	// holds which tile id (from catalog) are loaded where in VRAM.
	uint32_t tiles_in_vram[512];

	// When non-zero, indicates the tile is used by the current frame - i.e
	// it should not be replaced when loading new tiles!
	uint8_t vram_tile_in_use[512];

	// keep tiles in vram for as long as possible by tracking which one was
	// added last, and starting the search for free tiles after that one.
	int vram_last_loaded;

	int vram_max_tiles;
	int vram_used_tiles;
	uint8_t scrollx, scrolly;
	int last_best_pan;
} encoder_t;

encoder_t *encoder_init(int frames, int w, int h, int vram_max_tiles, uint32_t flags)
{
	encoder_t *enc;

	enc = calloc(1, sizeof(encoder_t));
	if (!enc) {
		perror("encoder_t");
		return NULL;
	}

	enc->vram_max_tiles = vram_max_tiles;
	enc->flags = flags;

	enc->w = (w + 7) / 8;
	enc->h = (h + 7) / 8;
	printf("Encoder screen size (in tiles): %d x %d\n", enc->w, enc->h);
	enc->diff = calloc(enc->w * enc->h, sizeof(uint8_t));
	if (!enc->diff) {
		perror("diff");
		free(enc);
		return NULL;
	}

	enc->frames = calloc(frames, sizeof(tiled_frame_t));
	if (!enc->frames) {
		perror("tiled frames");
		free(enc->diff);
		free(enc);
		return NULL;
	}

	enc->catalog = tilecat_new();
	if (!enc->catalog) {
		free(enc->frames);
		free(enc->diff);
		free(enc);
		return NULL;
	}


	return enc;
}

void encoder_free(encoder_t *encoder)
{
	if (encoder) {
		if (encoder->frames) {
			free(encoder->frames);
		}
		if (encoder->catalog) {
			tilecat_free(encoder->catalog);
		}
		if (encoder->diff) {
			free(encoder->diff);
		}
		free(encoder);
	}
}

static int encoder_diffFrames(encoder_t *enc, int frame1, int frame2)
{
	int i;
	tilemap_t *map1, *map2;
	uint8_t diff = 0;
	int diffcount = 0;

	map1 = enc->frames[frame1].map;
	map2 = enc->frames[frame2].map;

	for (i=0; i<enc->w * enc->h; i++) {
		diff = 0;
		if (map1->data[i] != map2->data[i]) {
			diff |= DIFF_TILE;
		}
		if (map1->flags[i] != map2->flags[i]) {
			diff |= DIFF_FLAGS;
		}

		enc->diff[i] = diff;

		if (diff) {
			diffcount++;
		}
	}

	return diffcount;
}

// IF we were to call encoder_addFrame with image img, compute how many:
//
// - Tiles would need to be added to the catalog (tiles_to_add)
// - Cells would not need updating (matching_cells)
//
int encoder_tryAddFrame(encoder_t *enc, sprite_t *img, int *tiles_to_add, int *matching_cells)
{
	tilemap_t *map;
	int missing, match;

	map = tilemap_allocate((img->w+7)/8, (img->h+7)/8);
	if (!map) {
		return -1;
	}

	missing = tilemap_populateFromCatalog(map, img, enc->catalog);

	if (tiles_to_add)  {
		*tiles_to_add = missing;
	}

	if (enc->num_frames == 0) {
		// on the first frame, all the cells need updating
		match = 0;
	} else {
		match = tilemap_compare(map, enc->frames[enc->num_frames-1].map);
	}

	if (matching_cells) {
		*matching_cells = match;
	}

	tilemap_free(map);

	return 0;
}

#define PANX_SEARCHWIDTH	128

int encoder_panOptimize(encoder_t *enc, sprite_t *img)
{
	int bestpan = 0;
	int pans[PANX_SEARCHWIDTH*2+1];
	int toadd[PANX_SEARCHWIDTH*2+1];
	int matching[PANX_SEARCHWIDTH*2+1];
	int idx, pan, i;
	int bestadd = 0, bestmatch;
	sprite_t *panimg;
	int searchwidth = PANX_SEARCHWIDTH;
	int searchstart = -PANX_SEARCHWIDTH;

	if (enc->flags & SMSANIM_ENC_FLAG_AGRESSIVE_PAN) {
		searchwidth = 128;
		searchstart = -128;
	} else {
		searchwidth = 16;
		searchstart = -16; // enc->last_best_pan;
	}

	panimg = duplicateSprite(img);
	if (panimg) {
		sprite_panX(panimg, searchstart);

		for (idx = 0, pan = searchstart; pan <= searchwidth; pan++,idx++) {
			encoder_tryAddFrame(enc, panimg, &toadd[idx], &matching[idx]);
			pans[idx] = pan;
//			printf(" [idx %d] Evaluating pan %d : (new tiles: %d, tilemap matches: %d)\n", idx, pan, toadd[idx], matching[idx]);
			sprite_panX(panimg, 1);
		}

		freeSprite(panimg);

		// 1. Find the lowest count of missing (to add) tiles
		for (i=0; i<idx; i++) {
			if (i==0) {
				bestadd = toadd[i];
			} else {
				if (toadd[i] < bestadd) {
					bestadd = toadd[i];
				}
			}
		}

		// 2. If there are multiple pan values which results
		// in a same number of missing tiles, select the one
		// needing the least pattern table updates
		for (bestmatch=0,i=0; i<idx; i++) {
			if (toadd[i] == bestadd) {
				if (matching[i] > bestmatch) {
					bestmatch = matching[i];
					bestpan = pans[i];
				}
			}
		}

		printf("Ideal pan value: %d (new tiles: %d, tilemmap matches: %d)\n", bestpan, bestadd, bestmatch);
/*		if (bestadd > 200) {
			fprintf(stderr, "Too many tiles to update\n");
			exit(1);
		}*/
	}

	enc->last_best_pan = bestpan;

	return bestpan;
}

int encoder_addFrame(encoder_t *enc, sprite_t *img)
{
	tilemap_t *map;
	int bestpan = 0;

	map = tilemap_allocate((img->w+7)/8, (img->h+7)/8);
	if (!map) {
		return -1;
	}

	if (enc->flags & SMSANIM_ENC_FLAG_PANOPTIMISE) {
		bestpan = encoder_panOptimize(enc, img);
	}

	sprite_panX(img, bestpan);

	tilecat_addAllFromSprite(enc->catalog, img, map);

	enc->frames[enc->num_frames].img = img;
	enc->frames[enc->num_frames].map = map;
	enc->frames[enc->num_frames].panx = bestpan;
	enc->num_frames++;

	return 0;
}

void encoder_printInfo(encoder_t *enc)
{
	int i;
	int d;

	printf("Frames encoded: %d\n", enc->num_frames);
	for (i=0; i<enc->num_frames; i++) {
		if (i > 0) {
			d = encoder_diffFrames(enc, i, i-1);
			printf("  %d: Uses %d unique tiles - %d changes from previous frame\n", i+1, tilemap_countUniqueIDs(enc->frames[i].map), d);
		} else {
			printf("  %d: Uses %d unique tiles\n", i+1, tilemap_countUniqueIDs(enc->frames[i].map));
		}
	}
	printf("Tiles in global catalog: %d\n", enc->catalog->num_tiles);
}

static void outputHorizontalTilemapUpdate(encoder_t *enc, int first_tile, int count)
{
//	int i;

//	fprintf(out_fptr, "; VRAM_WRITEWORDS to(byte address), wordcount, data...\n");
//	fprintf(out_fptr, "> VRAM_WRITEWORDS %d %d ", first_tile * 2 + TILES_OFFSET, count);
/*
	for (i=0; i<count; i++) {
		printf("%04x", enc->vram_pnt_next[first_tile + i]);
	}
	printf("\n");
*/
	if (enc->funcs->horizTileUpdate) {
		enc->funcs->horizTileUpdate(first_tile, count, enc->vram_pnt_next + first_tile, enc->ctx);
	}
}

int output_pnt_updates(encoder_t *enc)
{
	int x,y;
	int off;
	int left, right;

	for (y=0; y<enc->h; y++) {
		printf(";   Line %2d {", y);

		left = -1;
		right = -1;
		for (x=0; x<enc->w; x++) {
			off = y * 32 + x;

			if (enc->vram_pnt[off] != enc->vram_pnt_next[off]) {
				printf("O");
				if (left < 0) {
					left = x;
				}
				right = x;
			} else {
				printf(".");
			}
		}
		if (right < 0) {
			printf("  : No changes\n");
		} else {
			printf("  : Columns %2d to %2d changed [%d]\n",left,right,right-left+1);
			outputHorizontalTilemapUpdate(enc, y * 32 + left, right-left+1);
		}
	}

	memcpy(enc->vram_pnt, enc->vram_pnt_next, sizeof(enc->vram_pnt));

	return 0;
}

#define NOT_IN_VRAM	0xffff

// Check if the tile id (from catalog) is loaded somewhere in VRAM. If it is,
// return the SMS tile ID.
//
// returns 0xFFFF (NOT IN VRAM) if not
uint16_t tileInVRAM(encoder_t *enc, uint32_t tid)
{
	int i;

	for (i=0; i<enc->vram_max_tiles; i++) {
		if (enc->tiles_in_vram[i] == tid) {
			return i;
		}
	}
	return NOT_IN_VRAM;
}

static void tilemapToPNTnext(encoder_t *enc, tilemap_t *map)
{
	int x, y;
	uint32_t tid;
	uint8_t flags;
	uint16_t smstile;

	for (y=0; y<map->h; y++) {
		for (x=0; x<map->w; x++) {
			tid = tilemap_getTileID(map,x,y);
			flags = tilemap_getTileFlags(map, x,y);

			smstile = tileInVRAM(enc, tid);
			if (smstile == NOT_IN_VRAM) {
				fprintf(stderr, "Warning: skipping missing tile id %d\n", tid);
exit(1);
				continue;
			}

			smstile |= (flags & TILEMAP_FLIP_X) ? 0x200 : 0;
			smstile |= (flags & TILEMAP_FLIP_Y) ? 0x400 : 0;

			enc->vram_pnt_next[y * 32 + x] = smstile;
		}
	}
}

static int isInPreviousPNT(encoder_t *enc, int id)
{
	int i;

	for (i=0; i<enc->w * enc->h; i++) {
		if ((enc->vram_pnt[i] & 0x1ff) == id) {
			return 1;
		}
	}
	return 0;
}

static uint16_t loadTile(encoder_t *enc, uint32_t tid, int frame)
{
	int i, x, y, smstile;
	tiled_frame_t *tframe;

	if (enc->flags & SMSANIM_ENC_UPDATE_IN_PLACE && frame > 0)
	{
		tframe = &enc->frames[frame];


		// Check if this is a tile update for a unique spot
		i = tilemap_findUseOf(tframe->map, tid, &x, &y);
		if (i) {
			i = y * 32 + x;

			// Get the SMS tile at that location
			smstile = enc->vram_pnt[i] & 0x1FF;

			if (!enc->vram_tile_in_use[smstile]) {
				enc->vram_tile_in_use[smstile] = 1;
				enc->tiles_in_vram[smstile] = tid;
				printf("Replacing tile ID %d in-place (no PNT update) at %d,%d (%d)\n", tid, x, y, smstile);
				return smstile;
			}

		}
	}


	// Attempt 1 : Only look for tiles not on screen at the moment!
#if 1
	// look for a free tile slot
	for (i=enc->vram_last_loaded+1; i<enc->vram_max_tiles; i++) {
		if (enc->vram_tile_in_use[i] == 0 && !isInPreviousPNT(enc, i)) {
			enc->vram_tile_in_use[i] = 1;
			enc->tiles_in_vram[i] = tid;
			enc->vram_last_loaded = i;
			return i;
		}
	}

	// look for a free tile slot
	for (i=0; i<enc->vram_last_loaded; i++) {
		if (enc->vram_tile_in_use[i] == 0 && !isInPreviousPNT(enc, i)) {
			enc->vram_tile_in_use[i] = 1;
			enc->tiles_in_vram[i] = tid;
			enc->vram_last_loaded = i;
			return i;
		}
	}
#endif

#if 0

	for (i=0; i<enc->vram_max_tiles; i++) {
		if (enc->vram_tile_in_use[i] == 0 && !isInPreviousPNT(enc, i)) {
			enc->vram_tile_in_use[i] = 1;
			enc->tiles_in_vram[i] = tid;
			enc->vram_last_loaded = i;
			return i;
		}
	}
#endif


	// Attempt 2 : race condition between tile update and tilemap update

//	exit(1);

	// look for a free tile slot
	for (i=enc->vram_last_loaded+1; i<enc->vram_max_tiles; i++) {
		if (enc->vram_tile_in_use[i] == 0) {
			enc->vram_tile_in_use[i] = 1;
			enc->tiles_in_vram[i] = tid;
			enc->vram_last_loaded = i;
			return i;
		}
	}

	// look for a free tile slot
	for (i=0; i<enc->vram_last_loaded; i++) {
		if (enc->vram_tile_in_use[i] == 0) {
			enc->vram_tile_in_use[i] = 1;
			enc->tiles_in_vram[i] = tid;
			enc->vram_last_loaded = i;
			return i;
		}
	}

	return NOT_IN_VRAM;
}

void printLoadedTiles(encoder_t *enc)
{
	int i;
	int prev = -1, cur;

	printf("Loaded tiles:\n");
	for (i=0; i<enc->vram_max_tiles; i++) {
		cur = enc->tiles_in_vram[i];
		if (cur != prev) {
			printf("  SMS_%d: %d\n", i, cur);
		}
		prev = cur;
	}
}

static void encodeFrame(encoder_t *enc, int frame)
{
	tiled_frame_t *tframe;
	int i;
	uint32_t tid;
	uint16_t smstile;

	struct tileUseEntry *ent;
	int usecount;

	int reuse_count = 0;
	int load_count = 0;

	tframe = &enc->frames[frame];

	if (enc->funcs->frameStart) {
		enc->funcs->frameStart(frame+1, tframe->panx, enc->ctx);
	}
	printf("\n;;;;;;; frame %d / %d\n", frame + 1, enc->num_frames);

	if (frame == 0) {
		if (enc->funcs->updatePalette) {
			enc->frames[0].img->palette.count = 16;
			enc->funcs->updatePalette(&enc->frames[0].img->palette, enc->ctx);
		}
	}

	// List required tiles to display this frame.
	usecount = getAllUsedTiles(tframe->map, &ent);

	printf("; used tiles: %d\n", usecount);

	// Tiles already in VRAM must be kept there (i.e. not get replaced
	// when loading missing tiles further down). So mark those as
	// being "in use".
	memset(enc->vram_tile_in_use, 0, sizeof(enc->vram_tile_in_use));
	for (i=0; i<usecount; i++) {
		tid = ent[i].id;
		smstile = tileInVRAM(enc, tid);
		if (smstile != NOT_IN_VRAM) {
//			printf("tile %d already in VRAM\n", smstile);
			enc->vram_tile_in_use[smstile] = 1;
			reuse_count++;
		}
	}
	printf("; tiles already in VRAM: %d\n", reuse_count);

	// Now load missing tiles to unused locations in VRAM
	//

	if (enc->funcs->loadTile) {
		enc->funcs->loadTilesBegin(enc->ctx);
	}
	//
	for (i=0; i<usecount; i++) {
		tid = ent[i].id;
		smstile = tileInVRAM(enc, tid);
		if (smstile == NOT_IN_VRAM) {
			smstile = loadTile(enc, tid, frame);
			if (smstile != NOT_IN_VRAM) {
				if (enc->funcs->loadTile) {
					enc->funcs->loadTile(tid, smstile, enc->ctx);
				}
				load_count++;
			} else {
				fprintf(stderr, "Warning: out of VRAM space!\n");
			}
		}
	}

	if (enc->funcs->loadTile) {
		enc->funcs->loadTilesEnd(enc->ctx);
	}


	printf("; new tiles loaded to VRAM: %d\n", load_count);
	printf("; last added: %d\n", enc->vram_last_loaded);

	free(ent);

	//printLoadedTiles(enc);

	// sync VRAM pattern table according to map
	tilemapToPNTnext(enc, tframe->map);

	output_pnt_updates(enc);

	if (enc->funcs->frameEnd) {
		enc->funcs->frameEnd(enc->ctx);
	}
}

void encoder_outputScript(encoder_t *enc, struct encoderOutputFuncs *funcs, void *ctx)
{
	int frame;

	enc->funcs = funcs;
	enc->ctx = ctx;

	if (enc->funcs->animStart) {
		enc->funcs->animStart(enc->ctx);
	}

	// fill vram with invalid data, forces a complete redraw of the screen on the first frame
	memset(enc->vram_pnt, 0xff, sizeof(enc->vram_pnt));

	// impossible tiles in VRAM - force loading
	memset(enc->tiles_in_vram, 0xff, sizeof(enc->vram_pnt));

	for (frame = 0; frame < enc->num_frames; frame++) {
		encodeFrame(enc, frame);
	}

	tilecat_updateNative(enc->catalog);

	if (enc->funcs->animEnd) {
		enc->funcs->animEnd(enc->ctx);
	}

	if (enc->funcs->writeCatalog) {
		enc->funcs->writeCatalog(enc->catalog, enc->ctx);
	}
}

