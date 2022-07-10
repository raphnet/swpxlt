#ifndef _builtin_palettes_h__
#define _builtin_palettes_h__

#include "palette.h"

const struct palette *getBuiltinPalette_byId(int id);
const struct palette *getBuiltinPalette_byName(const char *name);
const char *getBuiltinPaletteDescription(int id);
const char *getBuiltinPaletteName(int id);

#endif // _builtin_palettes_h__
