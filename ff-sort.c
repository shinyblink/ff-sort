// ┳━┓┳━┓  ┓━┓┏━┓┳━┓┏┓┓
// ┣━ ┣━ ━━┗━┓┃ ┃┃┳┛ ┃
// ┇  ┇    ━━┛┛━┛┇┗┛ ┇
// ff-sort
// Usage: <farbfeld source> | ff-sort [options] | <farbfeld sink>
// made by vifino. ISC (C) vifino 2018

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#if FARBHERD == 1
#include <farbherd.h>
#endif

#include "conversion.h"

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define PPOS(x, y) (x + (y*w))
#define COPY4(src, srci, dst, dsti)							\
	if (src != dst) { \
		dst[dsti + 0] = src[srci + 0];							\
		dst[dsti + 1] = src[srci + 1];							\
		dst[dsti + 2] = src[srci + 2];							\
		dst[dsti + 3] = src[srci + 3];							\
	}

static void usage(char* self, int status) {
	eprintf(
		"Usage: <farbfeld source> | %s [-x|-y] [-l min] [-u max] type [args..] | <farbfeld sink>\n"
		"Type is one of:\n"
		"\tred/green/blue - color channel\n"
		"\tsum - sum of the above\n"
		"\thue/saturation/value - HSV\n", self);
	exit(status);
}

// IO helpers.
static inline void chew(FILE* file, void* buffer, size_t bytes) {
#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fileno(file), 0, bytes, POSIX_FADV_SEQUENTIAL);
#endif

	do {
		size_t b = fread(buffer, 1, bytes, file);
		if (!b) {
			fprintf(stderr, "Failed to read %lu more bytes.\n", bytes);
			exit(2);
		}
		bytes -= b;
		buffer = (char*) buffer + b;
	} while (bytes != 0);
}
static inline void spew(FILE* file, void* buffer, size_t bytes) {
	if (file)
		if (!fwrite(buffer, bytes, 1, file)) {
			eprintf("write failed.\n");
			exit(1);
		}
}

static void ffparse(FILE* food, FILE* out, uint32_t* w, uint32_t* h) {
	char buf[8];
	chew(food, buf, 8);
	if (strncmp(buf, "farbfeld", 8) != 0) {
		eprintf("file is not a farbfeld image?\n");
		exit(1);
	}
	spew(out, buf, 8);

	chew(food, buf, 8);
	*w = be32toh(*(uint32_t*)buf);
	*h = be32toh(*(uint32_t*)(buf + 4));
	if (!w || !h) {
		eprintf("image has zero dimension?\n");
		exit(1);
	}
	spew(out, buf, 8);
}

// Global crap.
static uint32_t w, h;
static int farbherd = 0;

// Selection options
static int options;
#define SEL_X 0
#define SEL_Y 1

#define SEL_BRUTE 0
#define SEL_EDGES 1 // soon.

static int dir = 1;
#define DIR_NORMAL 1
#define DIR_REVERSE -1

double min = 0;
double max = 1;

// Sorting modes.
static int mode;
static int hsv = 0;
#define MODE_SUM 1
#define MODE_RED 2
#define MODE_GREEN 3
#define MODE_BLUE 4
#define MODE_HUE 5
#define MODE_SATURATION 6
#define MODE_VALUE 7

// datatypes
typedef struct {
	FP px[4]; // format is whatever the mode wants.
	uint16_t orig[4]; // original kept.
} pixel;

#define PIXEL(pix, original) ((pixel) { px = (pix), orig = (original) })

// sort an array of pixels
#define COMP(a, b) \
	((a) == (b)) ? 0 : ((a) > (b)) ? 1 : -1
static int comp(const void* e1, const void* e2) {
	pixel s1 = *(pixel*) e1;
	pixel s2 = *(pixel*) e2;
	int sum1, sum2;
	switch (mode) {
		// for these modes it doesn't matter if it's RGB or HSV
	case MODE_RED:
	case MODE_HUE:
		return COMP(s1.px[0], s2.px[0]);
	case MODE_GREEN:
	case MODE_SATURATION:
		return COMP(s1.px[1], s2.px[1]);
	case MODE_BLUE:
	case MODE_VALUE:
		return COMP(s1.px[2], s2.px[2]);

	case MODE_SUM:
		sum1 = s1.px[0] + s1.px[1] + s1.px[2];
		sum2 = s2.px[0] + s2.px[1] + s2.px[2];
		return COMP(sum1, sum2);
	default:
		fprintf(stderr, "Invalid sorting mode???\n");
		exit(2);
	}
}

static inline void sort(pixel* queue, int amount, uint16_t* img, int x, int y) {
	if (!amount)
		return;

	qsort(queue, amount, sizeof(pixel), comp);

	int i;
	for (i = 0; i < amount; i++) {
		int p = ((options == SEL_X) ? PPOS((x - amount + i), y) : PPOS(x, (y - amount + i))) * 4;
		int op = (dir == DIR_NORMAL) ? i : (amount - i);
		COPY4(queue[op].orig, 0, img, p);
	}
}

static inline void compare(int x, int y, uint16_t* img, pixel* queue, int* amount, pixel* current, pixel* px_min, pixel* px_max) {
	int p = PPOS(x, y) * 4;
	COPY4(img, p, current->orig, 0);
#ifdef DOCONVERT
	uint16_t conv[4];
	qbeush2ush(current->orig, conv);
	qush2fp(conv, current->px);
#else
	qush2fp(current->orig, current->px);
#endif
	if (hsv) {
		rgb2hsv(current->px, current->px);
	}
	if ((comp(current, px_min) >= 0) && (comp(current, px_max) < 1)) {
		// fits in selection scheme
		queue[(*amount)++] = *current;
	} else {
		sort(queue, *amount, img, x, y);
		*amount = 0;
	}
}

static void sort_img(uint16_t* img, uint16_t* out, pixel* queue, uint32_t w, uint32_t h) {
	pixel px_min;
	pixel px_max;
	px_min.px[0] = min; px_min.px[1] = min;
	px_min.px[2] = min; px_min.px[3] = min;
	px_max.px[0] = max; px_max.px[1] = max;
	px_max.px[2] = max; px_max.px[3] = max;

	if (img != out) {
		memcpy(out, img, w * h * 8);
	}

	// process
	pixel current;
	unsigned int x, y;
	if (options == SEL_X) {
		for (y = 0; y < h; y++) {
			int amount = 0;
			for (x = 0; x < w; x++) {
				//COPY4(img, PPOS(x, y) * 4, out, PPOS(x, y) * 4);
				compare(x, y, out, queue, &amount, &current, &px_min, &px_max);
			}
			sort(queue, amount, out, w, y);
		}
	} else {
		for (x = 0; x < w; x++) {
			int amount = 0;
			for (y = 0; y < h; y++) {
				//COPY4(img, PPOS(x, y) * 4, out, PPOS(x, y) * 4);
				compare(x, y, out, queue, &amount, &current, &px_min, &px_max);
			}
			sort(queue, amount, out, x, h);
		}
	}
}

static int nom_farbfeld(void) {
	// parse input image
	ffparse(stdin, stdout, &w, &h);

	uint16_t* img = malloc(w * h * 8);
	if (!img) {
		fprintf(stderr, "Error: Failed to alloc framebuffer.\n");
		return 2;
	}

	pixel* queue = calloc((options == SEL_X) ? w : h, sizeof(pixel));
	if (!queue) {
		fprintf(stderr, "Error: Failed to alloc sorting queue.\n");
		return 2;
	}

	chew(stdin, img, w * h * 8);
	sort_img(img, img, queue, w, h);
	spew(stdout, img, w * h * 8);
	fflush(stdout);
	return 0;
}

#if FARBHERD == 1
static int nom_farbherd(void) {
	farbherd_header_t head;
	if (farbherd_read_farbherd_header(stdin, &head)) {
		fprintf(stderr, "Failed to read farbherd header.\n");
		return 1;
	}
	farbherd_write_farbherd_header(stdout, head);
	fflush(stdout);

	size_t datasize = farbherd_datasize(head.imageHead);
	uint16_t* work = calloc(1, datasize);
	uint16_t* input = calloc(1, datasize);
	uint16_t* output = malloc(datasize);
	if (!work || !input || !output) {
		fprintf(stderr, "Failed to allocate work memory.\n");
		return 1;
	}

	w = head.imageHead.width;
	h = head.imageHead.height;

	pixel* queue = calloc((options == SEL_X) ? w : h, sizeof(pixel));
	if (!queue) {
		fprintf(stderr, "Error: Failed to alloc sorting queue.\n");
		return 2;
	}

	farbherd_frame_t inputframe;
	if (farbherd_init_farbherd_frame(&inputframe, head)) {
		fprintf(stderr, "Failed to allocate incoming frame memory\n");
		return 1;
	}

	while (1) {
		if (farbherd_read_farbherd_frame(stdin, &inputframe, head))
			return 0;
		farbherd_apply_delta(input, inputframe.deltas, datasize);

		// do the sorting.
		sort_img(input, output, queue, w, h);

		farbherd_calc_apply_delta(work, output, datasize);
		fwrite(output, datasize, 1, stdout);
		fflush(stdout);
	}
	return 0;
}
#endif

// entry point.
int main(int argc, char* argv[]) {
	if (argc < 2)
		usage(argv[0], 1);

	// parse arguments
	int opt;
	while ((opt = getopt(argc, argv, "xyrhu:l:")) != -1) {
		switch (opt) {
		case 'x':
			options = SEL_X;
			break;
		case 'y':
			options = SEL_Y;
			break;

		case 'r':
			dir = DIR_REVERSE;
			break;

		case 'h':
			farbherd = 1;
			break;

		case 'l':
			min = atof(optarg);
			break;

		case 'u':
			max = atof(optarg);
			break;

		default:
			usage(argv[0], 1);
		}
	}

	if (optind >= argc)
		usage(argv[0], 1);

	char* modestr = argv[optind];
	if (strcmp(modestr, "sum") == 0) {
		mode = MODE_SUM;
	} else if (strcmp(modestr, "red") == 0) {
		mode = MODE_RED;
	} else if (strcmp(modestr, "green") == 0) {
		mode = MODE_GREEN;
	} else if (strcmp(modestr, "blue") == 0) {
		mode = MODE_BLUE;
	} else if (strcmp(modestr, "hue") == 0) {
		mode = MODE_HUE; hsv = 1;
	} else if (strcmp(modestr, "saturation") == 0) {
		mode = MODE_SATURATION; hsv = 1;
	} else if (strcmp(modestr, "value") == 0) {
		mode = MODE_VALUE; hsv = 1;
	} else {
		usage(argv[0], 1);
	}

	if (farbherd) {
#if FARBHERD == 1
		return nom_farbherd();
#endif
		fprintf(stderr, "Farbherd is not supported in this build. :(\n");
		return 2;
	}
	return nom_farbfeld();
}
