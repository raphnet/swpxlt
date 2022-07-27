#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "sprite.h"
#include "globals.h"
#include "tilecatalog.h"
#include "tilemap.h"
#include "tilereducer.h"
#include "anim.h"
#include "smsanimencoder.h"

int g_verbose = 0;

#define CODE_BANK			2
#define MAX_OTIR_TILES		16

enum {
	OPT_NOPAN = 256,
	OPT_AGRESSIVE_PAN,
};

static void printHelp()
{
	printf("Usage: ./img2sms [options] files...\n");
	printf("\nConvert images to SMS VDP tiles format for VRAM\n");

	printf("\nGeneral / Global options:\n");
	printf(" -h                       Print usage information\n");
	printf(" -v                       Enable verbose output\n");
	printf(" -o, --out=file.asm       Set output file\n");
	printf(" -m, --maxframes=count    Limit max. no of frames\n");
	printf(" -t, --maxtiles=count     Set max vdp tiles used for animation\n");
	printf(" --nopan                  Disable use of scrollX register\n");
	printf(" --agressivepan           Search agressively for best pan (slow)\n");
}

static struct option long_options[] = {
	{ "help",         no_argument,       0, 'h' },
	{ "verbose",      no_argument,       0, 'v' },
	{ "out",          required_argument, 0, 'o' },
	{ "maxframes",    required_argument, 0, 'm' },
	{ "maxtiles",     required_argument, 0, 't' },
	{ "nopan",        no_argument,       0, OPT_NOPAN },
	{ "agressivepan", no_argument,       0,	OPT_AGRESSIVE_PAN },
	{ },
};

#define MAX_STRIDES	65536

struct asmdemo_ctx {
	FILE *fptr;

	int n_load_tiles;
	uint32_t cat_ids[512];
	uint16_t sms_ids[512];

	int n_strides;
	uint8_t *strides[MAX_STRIDES]; // [0] word count, [1...] data
	int unique_strides; // stats

	int cur_codebank;
	int used_in_codebank;
	int max_bank;

	// used to save cycles in horizTileUpdate
	uint16_t last_de, last_hl;
	uint8_t last_stride_bank;

};

void asmdemo_animStart(void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	context->cur_codebank = CODE_BANK;

	fprintf(context->fptr, ";;; -- BEGIN SMS ANIMATION --\n");
	fprintf(context->fptr, ".define ANIMATION_ENTRY_BANK %d\n", CODE_BANK);

	fprintf(context->fptr, ".bank %d slot 1\n", context->cur_codebank);
	fprintf(context->fptr, ".org 0\n");

	fprintf(context->fptr, "animation_entry:\n");

}

void asmdemo_frameStart(int no, int panx, void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	if (context->used_in_codebank > 0x3E00) {
		context->cur_codebank++;
		context->used_in_codebank = 0;

		fprintf(context->fptr, "	ld a,bank(@anim_frame%d)\n", no);
		fprintf(context->fptr, "	ld hl,@anim_frame%d\n", no);
		fprintf(context->fptr, "	jp @slot1bankswitch\n");

		fprintf(context->fptr, ".bank %d slot 1\n", context->cur_codebank);
		fprintf(context->fptr, ".org 0\n");

	//	printf("Code bank: %d\n", context->cur_codebank);
	}

	fprintf(context->fptr, "@anim_frame%d:", no);
	fprintf(context->fptr, "	halt\n");

	fprintf(context->fptr,
		"	; Pan screen\n"
		"	ld a,%d			\n"
		"	out ($bf), a		\n"
		"	ld a,$88 		\n"
		"	out ($bf), a		\n"
		,
		-panx
	);

}

void asmdemo_frameEnd(void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	fprintf(context->fptr, "; frame end\n\n");
}


void asmdemo_animEnd(void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	fprintf(context->fptr, "@anim_end:\n");
	fprintf(context->fptr, "	ret\n\n");

	// these helper routines need to be in bank0

	fprintf(context->fptr, ".bank 0 slot 0\n");


	fprintf(context->fptr, "	; HL: source address (must be pre-ored with $4000)\n");
	fprintf(context->fptr, "	; DE: VRAM addr (must be pre-ored with $4000)\n");
	fprintf(context->fptr, "	; B: byte count\n");
	fprintf(context->fptr, "	; Decreases C, zeroes B, increments HL, leaves DE as is\n");
	fprintf(context->fptr, "@vram_memcpy_otir:\n");

	fprintf(context->fptr,
		"	ld c,$bf					\n"
		"	out (c),e					\n"
		"	out (c),d					\n"

		"	dec c ; C = $be				\n"
		"	otir						\n"
		"	ret							\n"
	);


	fprintf(context->fptr, "	; DE: VRAM destination\n");
	fprintf(context->fptr, "	; HL: RAM/ROM source\n");
	fprintf(context->fptr, "	; B: byte count\n");
	fprintf(context->fptr, "	; Decreases C, zeroes B, increments HL, leaves DE as is\n");
	fprintf(context->fptr, "@vram_memcpy:\n");

	fprintf(context->fptr,
		"	; set VRAM address\n"
		"	ld c,$bf					\n"
		"	out (c),e					\n"
		"	out (c),d					\n"
		"	dec c ; c = $be				\n"
		"@@lp:							\n"
		"	outi	; (C) = (HL); HL++	\n"
		"	jp NZ, @@lp					\n"
	);
	fprintf(context->fptr, "	ret\n");

	// bank 1 code trampoline
	fprintf(context->fptr, "\n\n; a: bank, hl: addr\n");
	fprintf(context->fptr, "@slot1bankswitch:\n");
	fprintf(context->fptr, "	ld ($FFFE),a\n");
	fprintf(context->fptr, "	jp (hl)\n\n");

	fprintf(context->fptr, ";;; -- END SMS ANIMATION --\n");
}

void asmdemo_loadTilesBegin(void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	context->n_load_tiles = 0;
	fprintf(context->fptr, "\n; load tiles sequence\n\n");
}

void asmdemo_loadTile(uint32_t catid, uint16_t smsid, void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	if (catid > 0xffff) {
		fprintf(stderr, "Fatal: more than 64k tiles not supported\n");
		exit(1);
	}

	context->cat_ids[context->n_load_tiles] = catid;
	context->sms_ids[context->n_load_tiles] = smsid;
	context->n_load_tiles++;

}

void asmdemo_loadTilesEnd(void *ctx)
{
	struct asmdemo_ctx *context = ctx;
	int i, j, seq = 1, n, inc = 1;
	uint16_t smsid;
	uint32_t catid;
	int otir_tiles = 0;
	int totaldone = 0;
	int k;
	int last_bank = -1;

	for (i=0; i<context->n_load_tiles; i+=inc) {


		catid = context->cat_ids[i];
		smsid =  context->sms_ids[i];


		// look ahead to see how many consecutive tiles we have
		//
		// tiles must be consecutive in VRAM and in the catalog
		for (seq=1,j=i+1; j<context->n_load_tiles; j++,seq++) {
			if (context->cat_ids[j] != catid + seq )
				break;
			if (context->sms_ids[j] != smsid + seq)
				break;

			// avoid bank switch in the middle of a sequence
			if ((catid / 512) != ((catid+seq) / 512)) {
				break;
			}
		}

		// switch bank on the first tile, and then when necessary.
		//
		if ((i < 1) || (catid / 512) != last_bank) {
			if (i < 1) {
				fprintf(context->fptr, "	; initial bank switch\n");
			} else {
				fprintf(context->fptr, "	; bank switch (reached cat.tile %d\n", catid);
			}
			fprintf(context->fptr, "	ld a, bank(@tilecatalog@tile_id%d)\n", catid);
			fprintf(context->fptr, "	ld ($FFFF), a\n");
			last_bank = catid / 512;
		}

		inc = seq;
		while (seq > 0) {
			n = seq;
			if (n > 8)
				n = 8;

			if (otir_tiles + n <= MAX_OTIR_TILES) {
				fprintf(context->fptr, "	ld de, $%04x\n", 0x4000 | (smsid * 32));
				fprintf(context->fptr, "	ld hl, @tilecatalog + %d\n", (catid * 32) & 0x3fff);
				fprintf(context->fptr, "	ld b, %d\n", (32 * n)&0xff);
				fprintf(context->fptr, "	call @vram_memcpy_otir");
				otir_tiles += n;
			} else {
				fprintf(context->fptr, "	ld de, $%04x\n", 0x4000 | (smsid * 32));
				fprintf(context->fptr, "	ld hl, @tilecatalog + %d\n", (catid * 32) & 0x3fff);
				fprintf(context->fptr, "	ld b, %d\n", (32 * n)&0xff);
				fprintf(context->fptr, "	call @vram_memcpy");
			}

			fprintf(context->fptr, "       ; Update tile ID(s) (CAT->SMS) ");
			for (k=0; k<n; k++) {
				fprintf(context->fptr, "%d->%d ", catid+k, smsid+k);
			}
			fprintf(context->fptr, "\n");

			totaldone += n;

			smsid += n;
			catid += n;

			seq -= n;

			context->used_in_codebank += 20;
		}
	}

	if (totaldone != context->n_load_tiles) {
		fprintf(stderr, "Internal error\n");
		exit(1);
	}

}

void asmdemo_horizTileUpdate(uint16_t first_tile, uint8_t count, uint16_t *data, void *ctx)
{
	struct asmdemo_ctx *context = ctx;
	uint8_t *buf;
	uint16_t de;
	//int i;


	fprintf(context->fptr, "	; update %d tilemap entries\n", count);


	de = (first_tile*2 + 0x3800) | 0x4000;

	if (context->n_strides == 0) {
		fprintf(context->fptr, "	ld c, $be\n");

		fprintf(context->fptr, "	ld de, $%04x\n", de);
	}
	else {
		if ((de >> 8) == (context->last_de >> 8)) {
			fprintf(context->fptr, "	ld e, $%02x\n", de & 0xff);
		} else {
			fprintf(context->fptr, "	ld de, $%04x\n", de);
		}
	}

	fprintf(context->fptr, "	ld hl, @stride%d\n", context->n_strides);
	fprintf(context->fptr, "	ld b, %d\n", count * 2);
	fprintf(context->fptr, "	ld a, bank(@stride%d)\n", context->n_strides);
	fprintf(context->fptr, "	ld ($FFFF), a\n");

	fprintf(context->fptr,
		"	inc c ; $be -> $bf			\n"
		"	out (c),e					\n"
		"	out (c),d					\n"
		"	dec c ; c = $be				\n"
		"@@pntlp%d:						\n"
		"	outi	; (C) = (HL); HL++	\n"
		"	jp NZ, @@pntlp%d			\n",
		context->n_strides,
		context->n_strides
	);


	context->last_de = de;


	buf = malloc(count * 2 + 1);
	if (!buf) {
		perror("stride buffer");
		exit(1);
	}
	buf[0] = count;
	memcpy(buf+1, data, count*2);

	// save this stride, for
	context->strides[context->n_strides] = buf;
	context->n_strides++;

	context->used_in_codebank += 25;
}

void asmdemo_writeCatalog(tilecatalog_t *catalog, void *ctx)
{
	struct asmdemo_ctx *context = ctx;
	int i,j;
	uint8_t *data;
	// this is called after all code was generated. Use the next
	// bank.
	int bank = context->cur_codebank + 1;
	int prev_bank = bank;
	int len;
	int used;


	fprintf(context->fptr, "\n\n;;;;; tile catalog\n");
	fprintf(context->fptr, ".bank %d slot 2\n", bank);
	fprintf(context->fptr, ".org 0\n");
	fprintf(context->fptr, "@tilecatalog:\n");
	for (i=0; i<catalog->num_tiles; i++) {
		bank = (context->cur_codebank + 1) + i / 512;
		if (bank != prev_bank) {
			fprintf(context->fptr, ".bank %d slot 2\n", bank);
			fprintf(context->fptr, ".org 0\n");
			prev_bank = bank;
			if (bank > context->max_bank) {
				context->max_bank = bank;
			}
		}

		fprintf(context->fptr, "	@@tile_id%d: ", i);
		fprintf(context->fptr, "	.db ");
		data = catalog->tiles[i].native;
		for (j=0; j<32; j++) {
			fprintf(context->fptr, "%s$%02x", j==0 ? "":", ", data[j]);
		}
		fprintf(context->fptr, "\n");
	}

	/* Good time to output strides too */

	// start in an empty bank
	bank++;
	prev_bank = bank;

	fprintf(context->fptr, "\n\n;;;;; strides\n");
	fprintf(context->fptr, ".bank %d slot 2\n", bank);
	fprintf(context->fptr, ".org 0\n");

	used = 0;
	for (i=0; i<context->n_strides; i++) {
		len = context->strides[i][0] * 2; // is a word count

		// merged strides are marked as 0 length and should be ignored,
		// a label has already been emitted for those
		if (len == 0) {
			continue;
		}

		if (used + len > 0x3FFF) {
			bank++;
			used = 0;

			if (bank > context->max_bank) {
				context->max_bank = bank;
			}
		}

		if (bank != prev_bank) {
			fprintf(context->fptr, ".bank %d slot 2\n", bank);
			fprintf(context->fptr, ".org 0\n");
			prev_bank = bank;
		}
#if 1
		// If there are identical strides later, find them and save space
		// by adding labels.
		for (j=i+1; j<context->n_strides; j++) {
			if (context->strides[i][0] != context->strides[j][0])
				continue;
			if (0 == memcmp(context->strides[i] + 1, context->strides[j] + 1, len)) {
				fprintf(context->fptr, "@stride%d:  ; dup\n", j);
				context->strides[j][0] = 0;
			}
		}
#endif
		fprintf(context->fptr, "@stride%d: ", i);
		fprintf(context->fptr, "	.db ");
		data = context->strides[i] + 1;
		for (j=0; j<len; j++) {
			fprintf(context->fptr, "%s$%02x", j==0 ? "":", ", data[j]);
		}
		fprintf(context->fptr, "\n");

		context->unique_strides++;

		used += len;
	}


	printf("Top used bank: %d\n", context->max_bank);
	printf("Total strides: %d\n", context->n_strides);
	printf("Unique strides: %d\n", context->unique_strides);
}

void asmdemo_updatePalette(palette_t *pal, void *ctx)
{
	struct asmdemo_ctx *context = ctx;

	fprintf(context->fptr, "	jp @@palette_end\n");
	palette_output_sms_wladx(context->fptr, pal, "@@palette");

	// CRAM 0
	fprintf(context->fptr, "	ld a,$00\n");
	fprintf(context->fptr, "	out ($bf),a\n");
	fprintf(context->fptr, "	ld a,$c0\n");
	fprintf(context->fptr, "	out ($bf),a\n");

	fprintf(context->fptr, "	ld hl, @@palette\n");
	fprintf(context->fptr, "	ld b, 16\n");
	fprintf(context->fptr, "	ld c, $be\n");
	fprintf(context->fptr, "	otir\n");
}

struct encoderOutputFuncs asmdemo = {
	.animStart = asmdemo_animStart,
	.frameStart = asmdemo_frameStart,
	.frameEnd = asmdemo_frameEnd,
	.animEnd = asmdemo_animEnd,
	.loadTilesBegin = asmdemo_loadTilesBegin,
	.loadTile = asmdemo_loadTile,
	.loadTilesEnd = asmdemo_loadTilesEnd,
	.horizTileUpdate = asmdemo_horizTileUpdate,
	.writeCatalog = asmdemo_writeCatalog,
	.updatePalette = asmdemo_updatePalette,
};


int main(int argc, char **argv)
{
	int opt;
	char *e;
	const char *infilename;
	const char *outfilename = NULL;
	animation_t *anim = NULL;
	encoder_t *encoder = NULL;
	struct asmdemo_ctx *asmctx;
	int frame;
	int max_frames = 0xffff;
	int max_vram_tiles = 440;
	FILE *fptr = NULL;
	uint32_t encflags = SMSANIM_ENC_FLAG_PANOPTIMISE;

	while ((opt = getopt_long_only(argc, argv, "hvo:m:t:", long_options, NULL)) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose++; break;
			case 'o': outfilename = optarg; break;
			case 'm':
				max_frames = strtol(optarg, &e, 0);
				if (e == optarg) {
					fprintf(stderr, "invalid number\n");
					return -1;
				}
				break;
			case 't':
				max_vram_tiles = strtol(optarg, &e, 0);
				if (e == optarg) {
					fprintf(stderr, "invalid number\n");
					return -1;
				}
				break;
			case OPT_NOPAN:
				encflags &= ~SMSANIM_ENC_FLAG_PANOPTIMISE;
				break;
			case OPT_AGRESSIVE_PAN:
				encflags |= SMSANIM_ENC_FLAG_AGRESSIVE_PAN;
				break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No file specified. See -h for usage.\n");
		return -1;
	}

	for (;optind < argc; optind++) {

		infilename = argv[optind];
		printf("Loading %s...\n", infilename);
		anim = anim_load(infilename);
		if (!anim) {
			return -1;
		}

		printf("Loaded %d frames\n", anim->num_frames);

		encoder = encoder_init(anim->num_frames, anim->w, anim->h, max_vram_tiles, encflags);
		if (!encoder) {
			return -1;
		}

		for (frame = 0; frame < anim->num_frames; frame++) {

			if (frame >= max_frames) {
				printf("Reached max frames\n");
				break;
			}

			printf("Frame %d:\n", frame+ 1);
			encoder_addFrame(encoder, anim->frames[frame]);
		}

		//encoder_printInfo(encoder);


		asmctx = calloc(1, sizeof(struct asmdemo_ctx));
		if (!asmctx) {
			perror("asmdemo_ctx");
			return -1;
		}

		asmctx->fptr = stdout;

		if (outfilename) {
			fptr = fopen(outfilename, "w");
			if (!fptr) {
				perror(outfilename);
				return -1;
			}
			asmctx->fptr = fptr;
		}

		encoder_outputScript(encoder, &asmdemo, asmctx);

		if (fptr) {
			fclose(fptr);
		}

		encoder_free(encoder);
		free(asmctx);
		anim_free(anim);

		break; // only handle one input file for now
	}

	return 0;
}
