#include <stdlib.h>
#include "tilemap.h"
#include "tilecatalog.h"

#if 0
int compare_tileCounts(const void *a, const void *b)
{
	const struct tileUseEntry *enta = a;
	const struct tileUseEntry *entb = b;

	return (enta->usecount > entb->usecount) - (enta->usecount < entb->usecount);
}

static void sortEntries(struct tileUseEntry *entries, int count)
{
	qsort(entries, count, sizeof(struct tileUseEntry), compare_tileCounts);
}

static void listEntries(struct tileUseEntry *entries, int count)
{
	int i;
	for (i=0; i<count; i++) {
		printf("Tile ID %d used %d times\n", entries[i].id, entries[i].usecount);
	}
}
#endif

int tilereducer_reduceTo(tilemap_t *tm, tilecatalog_t *cat, palette_t *pal, int max_tiles, int strategy)
{
	struct tileUseEntry *entries = NULL;
	int count, i,j;
	uint32_t difference, best_difference;
	int best_index1 = 0, best_index2 = 1;
	int first;
	uint8_t best_flags = 0;
	uint8_t flags;

	count = getAllUsedTiles(tm, &entries);
	if (!entries) {
		return -1;
	}
	if (count < max_tiles) {
		free(entries);
		return 0;
	}

	if (max_tiles < 2) {
		fprintf(stderr, "max tiles must be at least 2\n");
		return -1;
	}

	do
	{
		// Find the two most similar tiles
		first = 1;
		for (i=0; i<count; i++) {
			if (entries[i].usecount == 0)
				continue;

			for (j=i+1; j<count; j++) {
				if (entries[j].usecount == 0)
					continue;

				difference = tilecat_getDifferenceScore(cat, entries[i].id, entries[j].id, pal, &flags);
				if (first || (difference < best_difference)) {
					best_difference = difference;
					best_index1 = i;
					best_index2 = j;
					best_flags = flags;
					first = 0;
				}

			}
		}

//		printf("Best candidate for replacing tile %d is tile %d - score %d\n",
//			entries[best_index1].id, entries[best_index2].id, best_difference);

		if (entries[best_index1].usecount > entries[best_index2].usecount) {
			// replace arg1 by arg2
			tilemap_replaceID(tm, entries[best_index2].id, entries[best_index1].id, best_flags);
			entries[best_index1].usecount += entries[best_index2].usecount;
			entries[best_index2].usecount = 0;
			count--;
		} else {
			tilemap_replaceID(tm, entries[best_index1].id, entries[best_index2].id, best_flags);
			entries[best_index2].usecount += entries[best_index1].usecount;
			entries[best_index1].usecount = 0;
			count--;
		}


		if (best_index1 == best_index2) {
			fprintf(stderr, "Internal error\n");
			return -1;
		}

//		printf("Cur count: %d, target: %d\n", count, max_tiles);

	} while (count > max_tiles);

	free(entries);

	return 0;
}
