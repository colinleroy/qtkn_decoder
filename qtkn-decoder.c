/* qtkn-decoder.c
 *
   Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 *
 * QTKN (RADC) decoder heavily inspired from dcraw.c.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "quicktake1x0.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#define FINAL_WIDTH 320
#define FINAL_HEIGHT 240

unsigned char *output, *output_line;
unsigned char mul_m;

static unsigned char divtable[256];

static void init_divtable(unsigned char factor) {
  unsigned char r = 0;

	/* init a approximated division table */
  do {
    signed short approx = ((r<<8)|0x80)/factor;
		if (approx < 0) {
			divtable[r] = 0;
		} else if (approx > 255) {
			divtable[r] = 255;
		} else {
			divtable[r] = approx;
		}
  } while (++r);
}

#define BUF_SIZE FINAL_WIDTH+2
unsigned char huff_ctrl[9*2][256];
unsigned char huff_data[9][256];

signed short next_line[BUF_SIZE];
unsigned char *input_buffer;
unsigned char *header;
unsigned int output_len;
unsigned char last_m = 16;

static void init_huff(void) {
	/* Huff tables initializer */
	static const char src[] = {
		1,1, 2,3, 3,4, 4,2, 5,7, 6,5, 7,6, 7,8,
		1,0, 2,1, 3,3, 4,4, 5,2, 6,7, 7,6, 8,5, 8,8,
		2,1, 2,3, 3,0, 3,2, 3,4, 4,6, 5,5, 6,7, 6,8,
		2,0, 2,1, 2,3, 3,2, 4,4, 5,6, 6,7, 7,5, 7,8,
		2,1, 2,4, 3,0, 3,2, 3,3, 4,7, 5,5, 6,6, 6,8,
		2,3, 3,1, 3,2, 3,4, 3,5, 3,6, 4,7, 5,0, 5,8,
		2,3, 2,6, 3,0, 3,1, 4,4, 4,5, 4,7, 5,2, 5,8,
		2,4, 2,7, 3,3, 3,6, 4,1, 4,2, 4,5, 5,0, 5,8,
		2,6, 3,1, 3,3, 3,5, 3,7, 3,8, 4,0, 5,2, 5,4,
		2,0, 2,1, 3,2, 3,3, 4,4, 4,5, 5,6, 5,7, 4,8,
		1,0, 2,2, 2,-2,
		1,-3, 1,3,
		2,-17, 2,-5, 2,5, 2,17,
		2,-7, 2,2, 2,9, 2,18,
		2,-18, 2,-9, 2,-2, 2,7,
		2,-28, 2,28, 3,-49, 3,-9, 3,9, 4,49, 5,-79, 5,79,
		2,-1, 2,13, 2,26, 3,39, 4,-16, 5,55, 6,-37, 6,76,
		2,-26, 2,-13, 2,1, 3,-39, 4,16, 5,-55, 6,-76, 6,37
	};

  static unsigned char l, h;
  static unsigned short val, src_idx;
  l = 0;
  h = 1;

	/* Initialize "control" huff tables. These ones can have
	 * up to 8-bits codes.
	 */
  for (val = src_idx = 0; l < 18; src_idx += 2) {
    unsigned char code = val & 0xFF;
    unsigned char numbits, incr;

    numbits = src[src_idx];
    incr = 256 >> numbits;
    code >>= 8-numbits;
    huff_ctrl[h][code] = src[src_idx+1];
    huff_ctrl[l][code] = numbits;
    // printf("huff_ctrl[%d][%.*b] = %d (r%d)\n",
    //        l, numbits, code, src[src_idx+1], numbits);

    if (val >> 8 != (val+incr) >> 8) {
      l += 2;
      h += 2;
    }
    val += incr;
  }

	/* Initialize "data" huff tables. These ones can have
	 * up to 7-bits codes so pack them tighter.
	 */

  l = 0;
  h = 1;
  for (; l < 9; src_idx += 2) {
		unsigned char code = val & 0xFF;
    unsigned char numbits, incr;
    numbits = src[src_idx];
    incr = 256 >> numbits;

    code >>= 8-numbits;
    huff_data[l][code+128] = src[src_idx+1];
    huff_data[l][code] = numbits;
    // printf("huff_data[%d][%.*b] = %d (%d bits)\n", l, numbits, code, src[src_idx+1], numbits);

    if (val >> 8 != (val+incr) >> 8) {
      l++;
    }
    val += incr;
  }
}

static void init_decoder(void) {
	unsigned short c, i, s;
	/* Huff tables initializer */
	static const char src[] = {
		1,1, 2,3, 3,4, 4,2, 5,7, 6,5, 7,6, 7,8,
		1,0, 2,1, 3,3, 4,4, 5,2, 6,7, 7,6, 8,5, 8,8,
		2,1, 2,3, 3,0, 3,2, 3,4, 4,6, 5,5, 6,7, 6,8,
		2,0, 2,1, 2,3, 3,2, 4,4, 5,6, 6,7, 7,5, 7,8,
		2,1, 2,4, 3,0, 3,2, 3,3, 4,7, 5,5, 6,6, 6,8,
		2,3, 3,1, 3,2, 3,4, 3,5, 3,6, 4,7, 5,0, 5,8,
		2,3, 2,6, 3,0, 3,1, 4,4, 4,5, 4,7, 5,2, 5,8,
		2,4, 2,7, 3,3, 3,6, 4,1, 4,2, 4,5, 5,0, 5,8,
		2,6, 3,1, 3,3, 3,5, 3,7, 3,8, 4,0, 5,2, 5,4,
		2,0, 2,1, 3,2, 3,3, 4,4, 4,5, 5,6, 5,7, 4,8,
		1,0, 2,2, 2,-2,
		1,-3, 1,3,
		2,-17, 2,-5, 2,5, 2,17,
		2,-7, 2,2, 2,9, 2,18,
		2,-18, 2,-9, 2,-2, 2,7,
		2,-28, 2,28, 3,-49, 3,-9, 3,9, 4,49, 5,-79, 5,79,
		2,-1, 2,13, 2,26, 3,39, 4,-16, 5,55, 6,-37, 6,76,
		2,-26, 2,-13, 2,1, 3,-39, 4,16, 5,-55, 6,-76, 6,37
	};

	header = qtk_ppm_header(FINAL_WIDTH, FINAL_HEIGHT);
	if (header == NULL)
		exit(1);

	output_len = qtk_ppm_size(FINAL_WIDTH, FINAL_HEIGHT);

	output = malloc((size_t)FINAL_WIDTH * (size_t)FINAL_HEIGHT);
	if (output == NULL) {
		free(header);
		exit(1);
	}

	init_huff();

	/* Init the bitbuffer */
	initbithuff();

	for (i=0; i < BUF_SIZE; i++) {
		next_line[i] = 2048;
	}

	output_line = output - FINAL_WIDTH;
}

static void finalize_decoder(unsigned char **out) {
	unsigned char *ptr;
	*out = calloc(1, output_len);
	if (*out == NULL) {
		free(header);
		free(output);
		exit(1);
	}

	strcpy((char *)*out, header);
	ptr = *out + strlen(header);
	free(header);
	memcpy(ptr, output, FINAL_WIDTH*FINAL_HEIGHT);
	free(output);
}

static void init_row(void) {
	static unsigned short val_from_last[256] = {
	  0x0000, 0x1000, 0x0800, 0x0555, 0x0400, 0x0333, 0x02ab, 0x0249, 0x0200, 0x01c7, 0x019a, 0x0174, 0x0155, 0x013b, 0x0125, 0x0111, 0x0100,
	  0x00f1, 0x00e4, 0x00d8, 0x00cd, 0x00c3, 0x00ba, 0x00b2, 0x00ab, 0x00a4, 0x009e, 0x0098, 0x0092, 0x008d, 0x0089, 0x0084, 0x0080,
	  0x007c, 0x0078, 0x0075, 0x0072, 0x006f, 0x006c, 0x0069, 0x0066, 0x0064, 0x0062, 0x005f, 0x005d, 0x005b, 0x0059, 0x0057, 0x0055,
	  0x0054, 0x0052, 0x0050, 0x004f, 0x004d, 0x004c, 0x004a, 0x0049, 0x0048, 0x0047, 0x0045, 0x0044, 0x0043, 0x0042, 0x0041, 0x0040,
	  0x003f, 0x003e, 0x003d, 0x003c, 0x003b, 0x003b, 0x003a, 0x0039, 0x0038, 0x0037, 0x0037, 0x0036, 0x0035, 0x0035, 0x0034, 0x0033,
	  0x0033, 0x0032, 0x0031, 0x0031, 0x0030, 0x0030, 0x002f, 0x002f, 0x002e, 0x002e, 0x002d, 0x002d, 0x002c, 0x002c, 0x002b, 0x002b,
	  0x002a, 0x002a, 0x0029, 0x0029, 0x0029, 0x0028, 0x0028, 0x0027, 0x0027, 0x0027, 0x0026, 0x0026, 0x0026, 0x0025, 0x0025, 0x0025,
	  0x0024, 0x0024, 0x0024, 0x0023, 0x0023, 0x0023, 0x0022, 0x0022, 0x0022, 0x0022, 0x0021, 0x0021, 0x0021, 0x0021, 0x0020, 0x0020,
	  0x0020, 0x0020, 0x001f, 0x001f, 0x001f, 0x001f, 0x001e, 0x001e, 0x001e, 0x001e, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001c,
	  0x001c, 0x001c, 0x001c, 0x001c, 0x001b, 0x001b, 0x001b, 0x001b, 0x001b, 0x001b, 0x001a, 0x001a, 0x001a, 0x001a, 0x001a, 0x001a,
	  0x0019, 0x0019, 0x0019, 0x0019, 0x0019, 0x0019, 0x0019, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0017, 0x0017,
	  0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0016, 0x0016, 0x0016, 0x0016, 0x0016, 0x0016, 0x0016, 0x0016, 0x0015, 0x0015,
	  0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014,
	  0x0014, 0x0014, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0012, 0x0012, 0x0012,
	  0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011,
	  0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010
	};
	unsigned short val, i;

	mul_m = getbits6();
	/* Ignore the two next ones */
	getbits6();
	getbits6();

	/* Init the div table to ease setting each value */
	init_divtable(mul_m);

	val = (val_from_last[last_m] * mul_m) >> 4;
	last_m = mul_m;

	for (i=0; i < BUF_SIZE; i++) {
		next_line[i] = (next_line[i] * val - 1) >> 8;
	}
}

static void decode_row(void) {
	int col, tree, nreps, rep, step, r;
	signed short val1, val0;

	/* Decode data */
	for (r=0; r < 2; r++) {
		output_line += FINAL_WIDTH;

		val0 = next_line[FINAL_WIDTH+1] = mul_m << 7;

		for (tree=1, col=FINAL_WIDTH; col > 0; ) {
			if (tree = getctrlhuff(tree*2)) {
				col -= 2;

				if (tree == 8) {
					unsigned char token;
					token = (unsigned char) getdatahuff8();
					val1 = token * mul_m;
					output_line[col+1] = token;

					token = (unsigned char) getdatahuff8();
					val0 = token * mul_m;
					output_line[col] = token;

					token = (unsigned char) getdatahuff8();
					next_line[col+2] = token * mul_m;
					token = (unsigned char) getdatahuff8();
					next_line[col+1] = token * mul_m;

				} else {
					unsigned short predictor;
					signed int token1, token2, token3, token4;

					token1 = (signed char)getdatahuff(tree+1) << 4;
					token2 = (signed char)getdatahuff(tree+1) << 4;
					token3 = (signed char)getdatahuff(tree+1) << 4;
					token4 = (signed char)getdatahuff(tree+1) << 4;

					val1 = ((((val0 + next_line[col+2]) >> 1)
									+ next_line[col+1]) >> 1)
									+ token1;
					output_line[col+1] = divtable[val1 >> 8];

					next_line[col+2] = ((((val0 + next_line[col+3]) >> 1)
									+ val1) >> 1)
									+ token3;

					val0 = ((((val1 + next_line[col+1]) >> 1)
									+ next_line[col+0]) >> 1)
									+ token2;
					output_line[col] = divtable[val0 >> 8];

					next_line[col+1] = ((((val1 + next_line[col+2]) >> 1)
								+ val0) >> 1)
								+ token4;
				}
			} else
				do {
					nreps = (col > 2) ? getdatahuff(0) + 1 : 1;
					for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
						col -= 2;

						val1 = ((((val0 + next_line[col+2]) >> 1)
											+ next_line[col+1]) >> 1);
						output_line[col+1] = divtable[val1 >> 8];

						next_line[col+2] = ((((val0 + next_line[col+3]) >> 1)
																+ val1) >> 1);

						val0 = ((((val1 + next_line[col+1]) >> 1)
										+ next_line[col+0]) >> 1);
						output_line[col] = divtable[val0 >> 8];

						next_line[col+1] = ((((val1 + next_line[col+2]) >> 1)
																+ val0) >> 1);

						if (rep & 1) {
							step = (signed char)getdatahuff(1) << 4;
							val1 += step;
							output_line[col+1] = divtable[val1 >> 8];

							val0 += step;
							output_line[col] = divtable[val0 >> 8];

							next_line[col+2] += step;
							next_line[col+1] += step;
						}
					}
				} while (nreps == 9);
		}
	}
}

static void discard_data(void) {
	int col, tree, nreps, rep, r;

  /* Consume RADC tokens but discard them. */
	for (r=0; r < 2; r++) {
		tree = 1;
		col = FINAL_WIDTH/2;

		while (col > 0) {
			if (tree = getctrlhuff(tree*2)) {
				col --;
				if (tree == 8) {
					getdatahuff8();
					getdatahuff8();
					getdatahuff8();
					getdatahuff8();
				} else {
					getdatahuff(tree+1);
					getdatahuff(tree+1);
					getdatahuff(tree+1);
					getdatahuff(tree+1);
				}
			} else
				do {
					unsigned char rep_loop;
					nreps = (col > 1) ? getdatahuff(0) + 1 : 1;

					rep_loop = nreps > 8 ? 8 : nreps;
					col -= rep_loop;
					rep_loop /= 2;
					while (rep_loop--) {
						getdatahuff(1);
					}
				} while (nreps == 9);
		}
	}
}
int qtkn_decode(unsigned char *raw, unsigned char **out) {
	unsigned char row;

	input_buffer = raw;

	init_decoder();

	for (row=0; row < FINAL_HEIGHT; row+=2) {
		init_row();

		decode_row();
		discard_data();
	}

	finalize_decoder(out);

	return 0;
}
