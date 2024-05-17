#ifndef _sms_anim_encoder_h__
#define _sms_anim_encoder_h__

#include <stdint.h>
#include "sprite.h"

struct encoderOutputFuncs {
	void (*animStart)(void *ctx);
	void (*frameStart)(int frameno, int panx, void *ctx);
	void (*frameEnd)(void *ctx);
	void (*animEnd)(void *ctx);
	void (*loadTilesBegin)(void *ctx);
	void (*loadTile)(uint32_t catid, uint16_t smsid, void *ctx);
	void (*loadTilesEnd)(void *ctx);

	void (*horizTileUpdate)(uint16_t first_tile, uint8_t count, uint16_t *data, void *ctx);
	void (*writeCatalog)(tilecatalog_t *catalog, void *ctx);
	void (*updatePalette)(palette_t *pal, void *ctx);
};

typedef struct encoderContext encoder_t;

#define SMSANIM_ENC_FLAG_PANOPTIMISE	1
#define SMSANIM_ENC_FLAG_AGRESSIVE_PAN	2

// When a tile needs to be uploaded to VRAM only to update
// one place in the tilemap, try replacing the old tile directly
// instead of loading the tile to a new VRAM location.
//
// Reduces pattern table updates for small detail animation
#define SMSANIM_ENC_UPDATE_IN_PLACE		4
encoder_t *encoder_init(int frames, int w, int h, int vram_max_tiles, uint32_t flags);
void encoder_free(encoder_t *encoder);
int encoder_addFrame(encoder_t *enc, sprite_t *img);
int encoder_skipFrame(encoder_t *enc);
void encoder_printInfo(encoder_t *enc);

void encoder_outputScript(encoder_t *enc, struct encoderOutputFuncs *funcs, void *ctx);

#endif // _sms_anim_encoder_h__
