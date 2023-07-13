#ifndef _tilereducer_h__
#define _tilereducer_h__

#include "tilemap.h"
#include "tilecatalog.h"
#include "palette.h"

int tilereducer_reduceTo(tilemap_t *tm, tilecatalog_t *cat, palette_t *pal, int max_tiles, int strategy);

#endif // _tilereducer_h__

