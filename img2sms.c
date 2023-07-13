#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "sprite.h"
#include "globals.h"
#include "tilecatalog.h"
#include "tilemap.h"
#include "tilereducer.h"

int g_verbose = 0;

enum {
	OPT_SAVECAT = 256,
	OPT_MAXTILES,
	OPT_SAVEIMAGE,
	OPT_SAVETILES,
	OPT_SAVEMAP,
	OPT_SAVEPAL,
};

static void printHelp()
{
	printf("Usage: ./img2sms [options] files...\n");
	printf("\nConvert images to SMS VDP tiles format for VRAM\n");

	printf("\nGeneral / Global options:\n");
	printf(" -h                      Print usage information\n");
	printf(" -v                      Enable verbose output\n");

	printf(" -savecat file.png       Save the tile catalog to a .png\n");
	printf(" -maxtiles count         Try to reduce total tiles by replacing\n");
	printf("                         similar tiles. (lossy)\n");
	printf(" -saveimage file.png     Save the resulting image\n");
	printf(" -savetiles tiles.bin    Save the final tiles (for SMS VDP)\n");
	printf(" -savemap image.map      Save the final tilemap (for SMS VDP)\n");
	printf(" -savepal palette.bin    Save the final palette\n");
}

static struct option long_options[] = {
	{ "help",      no_argument,       0, 'h' },
	{ "verbose",   no_argument,       0, 'v' },
	{ "savecat",   required_argument, 0, OPT_SAVECAT },
	{ "maxtiles",  required_argument, 0, OPT_MAXTILES },
	{ "saveimage", required_argument, 0, OPT_SAVEIMAGE },
	{ "savetiles", required_argument, 0, OPT_SAVETILES },
	{ "savemap",   required_argument, 0, OPT_SAVEMAP },
	{ "savepal",   required_argument, 0, OPT_SAVEPAL },
	{ },
};

int main(int argc, char **argv)
{
	int opt;
	char *e;
	const char *infilename;
	sprite_t *img = NULL;
	tilecatalog_t *catalog = NULL;
	int maxtiles = -1;
	const char *savecat_filename = NULL;
	const char *saveimage_filename = NULL;
	const char *savetiles_filename = NULL;
	const char *savemap_filename = NULL;
	const char *savepal_filename = NULL;
	palette_t palette;
	tilemap_t *map = NULL;

	while ((opt = getopt_long_only(argc, argv, "hv", long_options, NULL)) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose++; break;
			case OPT_MAXTILES:
				maxtiles = strtol(optarg, &e, 0);
				if (e == optarg) {
					perror(optarg);
					return -1;
				}
				break;
			case OPT_SAVECAT:
				savecat_filename = optarg;
				break;
			case OPT_SAVEIMAGE:
				saveimage_filename = optarg;
				break;
			case OPT_SAVETILES:
				savetiles_filename = optarg;
				break;
			case OPT_SAVEMAP:
				savemap_filename = optarg;
				break;
			case OPT_SAVEPAL:
				savepal_filename = optarg;
				break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No file specified. See -h for usage.\n");
		return -1;
	}

	catalog = tilecat_new();
	if (!catalog) {
		return -1;
	}

	infilename = argv[optind];
	printf("Processing %s...\n", infilename);

	img = sprite_load(infilename, 0, SPRITE_LOADFLAG_DROP_TRANSPARENT);
	if (!img) {
		fprintf(stderr, "could not load %s\n", infilename);
		return -1;
	}

	if (img->palette.count > 16) {
		// TODO : Inspect pixels to check actual use
		fprintf(stderr, "more than 16 colors\n");
		return -1;
	}

	memcpy(&palette, &img->palette, sizeof(palette_t));

	printf("Image is %d x %d\n", img->w, img->h);

	map = tilemap_allocate((img->w+7)/8, (img->h+7)/8);
	if (!map) {
		return -1;
	}

	printf("Adding all tiles to catalog...\n");
	tilecat_addAllFromSprite(catalog, img, map);
	tilecat_printInfo(catalog);

	printf("Tilemap unique tiles: %d\n", tilemap_countUniqueIDs(map));

	if (maxtiles > 0) {
		printf("Reducing tiles use... (this may take a while...)\n");
		tilereducer_reduceTo(map, catalog, &palette, maxtiles, 0);
		tilemap_printInfo(map);
	}

	freeSprite(img);

	// If tile reduction was some times may have been replaced and some
	// may not be used anymore. Build a new visual (intput image) and use
	// it to build new catalog and tilemap.
	img = tilemap_toSprite(map, catalog, &palette);
	if (!img) {
		fprintf(stderr, "Could not build updated visual\n");
		return -1;
	}
	// drop the old catalog and tilemap
	tilemap_free(map);
	tilecat_free(catalog);

	// prepare the final tilemap
	map = tilemap_allocate((img->w+7)/8, (img->h+7)/8);
	if (!map) {
		fprintf(stderr, "Could not allocate final tilemap\n");
		return -1;
	}
	// build catalog
	catalog = tilecat_new();
	if (!catalog) {
		fprintf(stderr, "Could not allocate final catalog\n");
		return -1;
	}

	tilecat_addAllFromSprite(catalog, img, map);
	tilecat_printInfo(catalog);
	tilemap_printInfo(map);


	if (savecat_filename) {
		printf("Saving catalog image: %s\n", savecat_filename);
		tilecat_toPNG(catalog, &palette, savecat_filename);
	}

	if (saveimage_filename) {
		printf("Saving resulting image: %s\n", saveimage_filename);
		sprite_savePNG(saveimage_filename, img, 0);
	}

	if (savepal_filename) {
		printf("Saving palette: %s\n", savepal_filename);
		palette_save(savepal_filename, &palette, PALETTE_FORMAT_SMS_BIN, "palette");
	}

	if (savetiles_filename) {
		printf("Saving tiles: %s\n", savetiles_filename);
		tilecat_updateNative(catalog);
		tilecat_saveTiles(catalog, savetiles_filename);
	}

	if (savemap_filename) {
		printf("Saving tilemap: %s\n", savemap_filename);
		tilemap_saveSMS(map, savemap_filename);
	}


	freeSprite(img);
	tilecat_free(catalog);

	return 0;

}
