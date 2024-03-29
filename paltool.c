#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin_palettes.h"
#include "palette.h"
#include "sprite.h"

int g_verbose = 0;


void printHelp(void)
{
	int i;
	const struct palette *pal;

	printf("Usage: ./paltool [options]\n");

	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
	printf(" -a                Next palette to be appended\n");
	printf(" -O offset         Define offset for next set color or palette import\n");
	printf("                   (default 0)\n");
	printf(" -i input          Input file\n");
	printf(" -o filename       Output file (when unspecified, simply displays palette)\n");
	printf(" -f format         Output format\n");
	printf(" -n symbol_name    Symbol name for assembly output\n");
	printf(" -x                Use Xterm-256 to display palette in color (if -o is omitted)\n");
	printf("\n");
	printf("Palette generation:\n");
	printf(" -I offset         Source palette offset (for copying/editing ranges)\n");
	printf(" -l length         Number of colors to modify\n");
	printf(" -d percent        Apply a darkening effect from -I to -O of percent\n");
	printf(" -s r,g,b          Set color at offset (0 < rgb < 256)\n");
	printf("                   Offset auto-increments after each set operation.\n");
	printf(" -B name           Load a built-in palette\n");
	printf("\n");
	printf("Supported output formats:\n");
	printf("   vga_asm         VGA format (6 bit) in nasm syntax\n");
	printf("   png             PNG file format (8-bit) with swatches\n");
	printf("   animator        Autodesk Animator COL file (256 entries / raw 768 bytes)\n");
	printf("   animator_pro    Autodesk Animator Pro COL file (Header + (n*3 bytes)\n");
	printf("   sms_wladx       Master system format, WLA-DX syntax\n");
	printf("   sms_bin         Master system format, binary (16 bytes)\n");
	printf("\n");
	printf("Supported input formats (auto detected):\n");
	printf("   gimp            Gimp palette (.GPL)\n");
	printf("   png             Palette from PNG image (.PNG)\n");
	printf("   jasc            JASC palette format (eg: Grafx2, Paint Shop Pro, etc)\n");
	printf("\n");
	printf("Built-in palettes:\n");

	for (i=0; ; i++) {
		pal = getBuiltinPalette_byId(i);
		if (!pal)
			break;

		printf("   %-16s%s\n", getBuiltinPaletteName(i), getBuiltinPaletteDescription(i));
	}
}

#define MODE_APPEND	0
#define MODE_OFFSET 1

int processPalette(const palette_t *src, int mode, int offset, palette_t *dst)
{
	switch (mode)
	{
		case MODE_OFFSET:
			if (src->count + offset > 255) {
				fprintf(stderr, "Cannot apply palette (%d colors) at offset %d: No space\n", src->count, offset);
			}
			if (src->count + offset >= dst->count) {
				dst->count = src->count + offset;
			}
			break;

		case MODE_APPEND:
			if (dst->count + src->count > 256) {
				fprintf(stderr, "Cannot append palette - no space left\n");
				return -1;
			}
			offset = dst->count;
			dst->count += src->count;
			break;

		default:
			return -1;
	}

	memcpy(dst->colors + offset, src->colors, src->count * sizeof(palent_t));

	return 0;
}

int loadBuiltInPalette(const char *name, int mode, int offset, palette_t *dst)
{
	const palette_t *src;

	src = getBuiltinPalette_byName(name);
	if (!src) {
		fprintf(stderr, "No such built-in palette\n");
		return -1;
	}

	if (g_verbose) {
		printf("Found %d colors in built-in palette %s\n", src->count, name);
	}

	return processPalette(src, mode, offset, dst);
}

int loadAndProcessPalette(const char *filename, int mode, int offset, palette_t *dst)
{
	palette_t src;


	if (palette_load(filename, &src)) {
		return -1;
	}

	return processPalette(&src, mode, offset, dst);
}

int apply_effect_darken(palette_t *pal, int source_index, int dest_index, int len, int percent)
{
	int i;
	palent_t *src, *dst;

	if (len == 0) {
		printf("Warning: Effect length is 0. Nothing to do.\n");
	}

	for (i=0; i<len; i++) {
		if (i+source_index > 255) {
			fprintf(stderr, "Darken: Source index out of range (%d[%d])\n", source_index, i);
			return -1;
		}
		if (i+dest_index > 255) {
			fprintf(stderr, "Darken: Destination index out of range (%d[%d])\n", dest_index, i);
		}

		if (i+dest_index+1 >= pal->count) {
			pal->count = i+dest_index+1;
		}

		src = &pal->colors[i+source_index];
		dst = &pal->colors[i+dest_index];

		dst->r = src->r * percent / 100;
		dst->g = src->g * percent / 100;
		dst->b = src->b * percent / 100;
	}

	return 0;
}


int main(int argc, char **argv)
{
	int opt;
	int mode = MODE_APPEND;
	int percent = 100;
	int length = 0;
	int offset = 0, input_offset = 0;
	int output_format = PALETTE_FORMAT_NONE;
	const char *output_filename = "-";
	const char *symbol_name = "palette";
	FILE *out_fptr;
	int printpal_colored = 0;

	palette_t palette;

	// Start with an empty palette
	palette_clear(&palette);

	while ((opt = getopt(argc, argv, "haO:i:o:if:n:vI:l:d:s:B:x")) != -1) {
		switch(opt)
		{
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'a': mode = MODE_APPEND; break;
			case 'n': symbol_name = optarg; break;
			case 'v': g_verbose = 1; break;
			case 'O':
				mode = MODE_OFFSET;
				offset = atoi(optarg);
				if (offset > 255) {
					fprintf(stderr, "Invalid offset > 255\n");
					return -1;
				}
				break;
			case 'I':
				input_offset = atoi(optarg);
				if (input_offset > 255 || input_offset < 0) {
					fprintf(stderr, "Invalid offset (out of range)\n");
					return -1;
				}
				break;
			case 'o':
				output_filename = optarg;
				break;
			case 'f':
				output_format = palette_parseOutputFormat(optarg);
				if (output_format == PALETTE_FORMAT_NONE) {
					fprintf(stderr, "Unsupported or invalid output format\n");
					return -1;
				}
				break;
			case 'B':
				if (loadBuiltInPalette(optarg, mode, offset, &palette)) {
					return -1;
				}
				break;
			case 'i':
				if (loadAndProcessPalette(optarg, mode, offset, &palette)) {
					return -1;
				}
				break;
			case 'd':
				percent = atoi(optarg);
				if (percent < 0 || percent > 100) {
					fprintf(stderr, "Percentage must be between 0 and 100\n");
					return -1;
				}
				apply_effect_darken(&palette, input_offset, offset, length, percent);
				break;
			case 'l':
				length = atoi(optarg);
				if (length < 0 || length > 255) {
					fprintf(stderr, "Length must be between 0 and 255\n");
					return -1;
				}
				break;
			case 's':
				{
					int r,g,b,count;

					count = sscanf(optarg, "%d,%d,%d", &r, &g, &b);
					if (count != 3) {
						fprintf(stderr, "Incorrect value passed to -s. Must be a triplet (r,g,b) without spaces.\n");
						return -1;
					}

					if (offset > 255) {
						fprintf(stderr, "Invalid offset > 255\n");
						return -1;
					}

					palette_setColor(&palette, offset, r, g, b);
					offset++;
				}
				break;
			case 'x':
				printpal_colored = 1;
				break;
		}
	}


	if (g_verbose) {
		printf("Resulting palette:\n");
		palette_print(&palette);
	}


	if (output_format == PALETTE_FORMAT_NONE) {
		if (printpal_colored) {
			palette_print_24bit(&palette);
		} else {
			palette_print(&palette);
		}
	}
	else if (output_format == PALETTE_FORMAT_PNG) {
		sprite_t *palimage;
		int i,x,y;

		palimage = allocSprite(256,256,palette.count,0);
		if (!palimage) {
			fprintf(stderr, "Could not allocate image\n");
			return -1;
		}

		sprite_applyPalette(palimage, &palette);

		for (i=0; i<palette.count; i++) {
			x = i % 8;
			y = i / 8;
			sprite_fillRect(palimage, x * 32, y * 32, 31, 31, i);
		}

		sprite_savePNG(output_filename, palimage, 0);
		freeSprite(palimage);
	}
	else
	{
		if (0 == strcmp(output_filename, "-")) {
			out_fptr = stdout;
		} else {
			out_fptr = fopen(output_filename, "wb");
		}
		if (!out_fptr) {
			perror(output_filename);
			return -1;
		}

		palette_saveFPTR(out_fptr, &palette, output_format, symbol_name);

		if (out_fptr != stdout) {
			fclose(out_fptr);
		}
	}

	return 0;
}
