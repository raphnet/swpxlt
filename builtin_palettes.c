#include <stdio.h>
#include <string.h>
#include "builtin_palettes.h"

static const struct palette builtin_vga16 = {
	.count = 16,
	.colors = {
		{ 0x00, 0x00, 0x00 }, // Black
		{ 0x00, 0x00, 0xAA }, // Blue
		{ 0x00, 0xAA, 0x00 }, // Green
		{ 0x00, 0xAA, 0xAA }, // Cyan
		{ 0xAA, 0x00, 0x00 }, // Red
		{ 0xAA, 0x00, 0xAA }, // Magenta
		{ 0xAA, 0x55, 0x00 }, // Brown
		{ 0xAA, 0xAA, 0xAA }, // Light gray

		{ 0x55, 0x55, 0x55 }, // Dark Gray
		{ 0x55, 0x55, 0xFF }, // Light blue
		{ 0x55, 0xFF, 0x55 }, // Light green
		{ 0x55, 0xFF, 0xFF }, // Light cyan
		{ 0xFF, 0x55, 0x55 }, // Light red
		{ 0xFF, 0x55, 0xFF }, // Light magenta
		{ 0xFF, 0xFF, 0x55 }, // Yellow
		{ 0xFF, 0xFF, 0xFF }, // White
	},
};

static const struct palette builtin_cga0_low = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x00, 0xAA, 0x00 }, // Green
		{ 0xAA, 0x00, 0x00 }, // Red
		{ 0xAA, 0x55, 0x00 }, // Brown
	},
};

static const struct palette builtin_cga0_high = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x55, 0xFF, 0x55 }, // Light green
		{ 0xFF, 0x55, 0x55 }, // Light red
		{ 0xFF, 0xFF, 0x55 }, // Yellow
	},
};

static const struct palette builtin_cga1_low = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x00, 0xAA, 0xAA }, // Cyan
		{ 0xAA, 0x00, 0xAA }, // Magenta
		{ 0xAA, 0xAA, 0xAA }, // Light gray
	},
};

static const struct palette builtin_cga1_high = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x55, 0xFF, 0xFF }, // Light cyan
		{ 0xFF, 0x55, 0xFF }, // Light magenta
		{ 0xFF, 0xFF, 0xFF }, // White
	},
};

static const struct palette builtin_cga_mode5_low = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x00, 0xAA, 0xAA }, // Cyan
		{ 0xAA, 0x00, 0x00 }, // Red
		{ 0xAA, 0xAA, 0xAA }, // Light gray
	},
};

static const struct palette builtin_cga_mode5_high = {
	.count = 4,
	.colors = {
		{ 0x00, 0x00, 0x00 },
		{ 0x55, 0xFF, 0xFF }, // Light cyan
		{ 0xFF, 0x55, 0x55 }, // Light red
		{ 0xFF, 0xFF, 0xFF }, // White
	},
};

struct builtin_palette {
	const char *name;
	const char *description;
	const struct palette *palette;
};

struct builtin_palette palettes[] = {
	{ "vga16", "Standard CGA/EGA/VGA text mode 16 color palette", &builtin_vga16 },
	{ "cga0_low", "CGA Pal. 0 (green/red/brown) low int.", &builtin_cga0_low},
	{ "cga0_high", "CGA Pal. 0 (green/red/yellow) high int.", &builtin_cga0_high, },
	{ "cga1_low", "CGA Pal. 1 (cyan/magenta/gray) low int.", &builtin_cga1_low },
	{ "cga1_high", "CGA Pal. 1 (cyan/magenta/white) high int.", &builtin_cga1_high, },
	{ "cga_m5_low", "CGA Mode 5 (cyan/red/gray) low int.", &builtin_cga_mode5_low, },
	{ "cga_m5_high", "CGA Mode 5 (cyan/red/white) high int.", &builtin_cga_mode5_high },
	{ }, // terminator
};

const char *getBuiltinPaletteName(int id)
{
	return palettes[id].name;
}

const char *getBuiltinPaletteDescription(int id)
{
	return palettes[id].description;
}


const struct palette *getBuiltinPalette_byId(int id)
{
	int i;

	for (i=0; palettes[i].name; i++) {
		if (i == id)
			return palettes[i].palette;
	}

	return NULL;
}

const struct palette *getBuiltinPalette_byName(const char *name)
{
	struct builtin_palette *pal;

	for (pal = palettes; pal->name; pal++) {
		if (0 == strcasecmp(name, pal->name)) {
			return pal->palette;
		}
	}

	return NULL;
}
