#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

#include <png.h>

int convertPNG(FILE *fptr_in, FILE *fptr_out, int append_palette, int value_offset, int black_trick, int for_mode_x);

static void printusage(void)
{
	printf("Usage: ./png2vga [options] input_file output_file\n");
	printf("\n");
	printf("options:\n");
	printf("  -h            Displays this\n");
	printf("  -p            Append palette data\n");
	printf("  -b            Rewrite black pixel values to 0\n");
	printf("  -o value      Offset for pixel values\n");
	printf("  -x            Output in planar format for mode X\n");
	printf("\n");
	printf("\n");
	printf("input_file must be a 8-bit color PNG file.\n");
}

int main(int argc, char **argv)
{
	FILE *fptr_in = NULL, *fptr_out = NULL;
	const char *in_filename = NULL, *out_filename = NULL;
	uint8_t header[8];
	int ret = 0, opt;
	int append_palette = 0;
	int value_offset = 0;
	int black_trick = 0;
	int for_mode_x = 0;

	while ((opt = getopt(argc, argv, "hpo:bx")) != -1) {
		switch (opt)
		{
			case 'h':
				printusage();
				return 0;
			case 'p':
				append_palette = 1;
				break;
			case 'b':
				black_trick = 1;
				break;
			case 'x':
				for_mode_x = 1;
				break;
			case 'o':
				value_offset = atoi(optarg);
				break;
		}
	}

	// Need at least input and output filenames
	if (optind >= (argc-1)) {
		fprintf(stderr, "missing filenames. Try -h\n");
		return -1;
	}

	if (argc < 3) {
		printusage();
		return 1;
	}

	in_filename = argv[optind++];
	out_filename = argv[optind++];

	printf("png2vga: %s -> %s, append palette: %s, value offset: %d\n", in_filename, out_filename, append_palette ? "Yes":"No", value_offset);

	fptr_in = fopen(in_filename, "rb");
	if (!fptr_in) {
		perror("fopen");
		return 2;
	}

	if (8 != fread(header, 1, 8, fptr_in)) {
		perror("fread");
		ret = 3;
		goto done;
	}

	if (png_sig_cmp(header, 0, 8)) {
		fprintf(stderr, "Not a PNG file\n");
		ret = 3;
		goto done;
	}

	fptr_out = fopen(out_filename, "wb");
	if (!fptr_out) {
		perror("fopen outfile");
		ret = 4;
		goto done;
	}

	ret = convertPNG(fptr_in, fptr_out, append_palette, value_offset, black_trick, for_mode_x);

done:
	if (fptr_out) {
		fclose(fptr_out);
	}

	if (fptr_in) {
		fclose(fptr_in);
	}

	return ret;
}

static void writePlanar4(const uint8_t *dat, int count, FILE *outfptr)
{
	int i,j;
	int pc = count/4;

	for (i=0; i<4; i++) {
		for (j=0; j<pc; j++) {
			fwrite(dat + i +  j * 4, 1, 1, outfptr);
		}
	}
}

int convertPNG(FILE *fptr_in, FILE *fptr_out, int append_palette, int value_offset, int black_trick, int for_mode_x)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers;
	int w,h,depth,color;
	int ret;
	int x, y, i;
	int num_palette;
	png_colorp palette;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return -1;

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return -1;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		ret = -1;
		goto done;
	}

	png_init_io(png_ptr, fptr_in);
	png_set_sig_bytes(png_ptr, 8);

	png_set_packing(png_ptr);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING, NULL);
	//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA, NULL);

	w = png_get_image_width(png_ptr, info_ptr);
	h = png_get_image_height(png_ptr, info_ptr);
	depth = png_get_bit_depth(png_ptr, info_ptr);
	color = png_get_color_type(png_ptr, info_ptr);

	printf("Image: %d x %d, ",w,h);
	printf("Bit depth: %d, ", depth);
	printf("Color type: %d\n", color);
	row_pointers = png_get_rows(png_ptr, info_ptr);

	switch(color)
	{
		case PNG_COLOR_TYPE_PALETTE:
			ret = 0;
			break;
		default:
			fprintf(stderr, "Unsupported color type. File must use a palette.\n");
			ret = -1;
			goto done;
	}

	if (!png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette)) {
		fprintf(stderr, "Error getting palette\n");
		ret = -1;
		goto done;
	}

	if (depth != 8) {
		fprintf(stderr, "Unimplemented color depth\n");
		return -1;
	}

	if (!for_mode_x)
	{
		for (y=0; y<h; y++) {
			if ((value_offset == 0) && !black_trick) {
				fwrite(row_pointers[y], w, 1, fptr_out);
			} else {
				uint8_t tmp;
				for (x=0; x<w; x++) {
					tmp = row_pointers[y][x];

					if (black_trick) {
						// black!
						if ((palette[tmp].red + palette[tmp].green + palette[tmp].blue)<5) {
							tmp = 0; // no offset
						} else {
							tmp += value_offset;
						}
					} else {
						tmp += value_offset;
					}

					fwrite(&tmp, 1, 1, fptr_out);
				}
			}
		}
	}
	else { // for mode x
		if ((w & 3) != 0) {
			fprintf(stderr, "Width must be a multiple of 4\n");
			return -1;
		}

		for (y=0; y<h; y++) {
			uint8_t tmp;
			uint8_t rowbuf[w];

			for (x=0; x<w; x++) {
				tmp = row_pointers[y][x];

				if (black_trick) {
					// black!
					if ((palette[tmp].red + palette[tmp].green + palette[tmp].blue)<5) {
						tmp = 0; // no offset
					} else {
						tmp += value_offset;
					}
				} else {
					tmp += value_offset;
				}

				rowbuf[x] = tmp;
			}

			writePlanar4(rowbuf, w, fptr_out);
		}
	}

	if (append_palette) {
		uint8_t num_entries[2];

		num_entries[0] = num_palette;
		num_entries[1] = num_palette >> 8;
		fwrite(num_entries, 2, 1, fptr_out);

		for (i=0; i<num_palette; i++)
		{
			// convert to 6 bit for VGA registers
			uint8_t color[3] = {
				palette[i].red >> 2,
				palette[i].green >> 2,
				palette[i].blue >> 2
			};

			printf("Color %d: %d,%d,%d\n", i, palette[i].red, palette[i].green, palette[i].blue);
			fwrite(color, 3, 1, fptr_out);
		}


	}

	printf("ret: %d\n", ret);

done:
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return ret;
}
