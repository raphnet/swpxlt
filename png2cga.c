#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <png.h>

int convertPNG(FILE *fptr_in, FILE *fptr_out);

static void printusage(void)
{
	printf("Usage: ./png2cga input_file output_file\n");
	printf("\n");
	printf("input_file must be a 16x16 2-bit color PNG file.\n");
}

int main(int argc, char **argv)
{
	FILE *fptr_in = NULL, *fptr_out = NULL;
	uint8_t header[8];
	int ret = 0;

	if (argc < 3) {
		printusage();
		return 1;
	}

	fptr_in = fopen(argv[1], "rb");
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

	fptr_out = fopen(argv[2], "wb");
	if (!fptr_out) {
		perror("fopen outfile");
		ret = 4;
		goto done;
	}

	ret = convertPNG(fptr_in, fptr_out);

done:
	if (fptr_out) {
		fclose(fptr_out);
	}

	if (fptr_in) {
		fclose(fptr_in);
	}

	return ret;
}

int convertPNG(FILE *fptr_in, FILE *fptr_out)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers;
	int w,h,depth,color;
	int ret=0;
	int x,y;

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

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING, NULL);

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
			//printf("Processing paletized image\n");
			if (depth != 2) {
				if (depth == 8) {
					for (y=0; y<h; y++) {
						for (x=0; x<w; x++) {
							if (row_pointers[y][x]>3) {
								fprintf(stderr, "Palette has too many colors.\n");
							}
						}
					}
					// Only indexes 0-3 are used, we can go on
					break;
				} // depth = 8

				fprintf(stderr, "Palette has too many colors.\n");
				ret = -1;
				goto done;
			}
			break;
		default:
			fprintf(stderr, "Unsupported color type. File must use a 2bit palette.\n");
			ret = -1;
			goto done;
	}

	switch(depth)
	{
		case 2:
			for (y=0; y<h; y++) {
				if (y &1) {
					fseek(fptr_out, (w/4) * (y/2) + (h * (w/4)/2), SEEK_SET);
				} else {
					fseek(fptr_out, (w/4) * (y/2), SEEK_SET);
				}
				fwrite(row_pointers[y], w/4, 1, fptr_out);
			}
			break;
		case 8:
			for (y=0; y<h; y++) {
				unsigned char row[w/4];
				memset(row, 0, sizeof(row));

				for (x=0; x<w; x++) {
					unsigned char b;

					b = row_pointers[y][x];

					row[x/4] |= (b<<6)>>((x&3)<<1);
				}

				if (y &1) {
					fseek(fptr_out, (w/4) * (y/2) + (h * (w/4)/2), SEEK_SET);
				} else {
					fseek(fptr_out, (w/4) * (y/2), SEEK_SET);
				}
				fwrite(row, w/4, 1, fptr_out);
			}

			break;
	}

done:
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return ret;
}
