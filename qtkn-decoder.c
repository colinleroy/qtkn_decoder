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
#include "bayer.h"

#define radc_token(tree, ptr) ((signed char) getbithuff(8, ptr, huff[tree]))
#define PREDICTOR (c ? (buf[c][y-1][x] + buf[c][y][x+1]) / 2 \
									 : (buf[c][y-1][x+1] + 2*buf[c][y-1][x] + buf[c][y][x+1]) / 4)

int qtkn_decode(unsigned char *raw, int width, int height, unsigned char **out) {
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
  static const unsigned short pt[] =
    { 0,0, 1280,1344, 2320,3616, 3328,8000, 4095,16383, 65535,16383 };

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

	#define BUF_SIZE 321
	unsigned short curve[0x10000];
	unsigned short huff[19][256];
	unsigned char huff_l[19][256], huff_h[19][256];
	int row, col, tree, nreps, rep, step, i, c, s, r, x, y, val, len;
	short last[3] = { 16,16,16 }, mul[3], buf[3][3][BUF_SIZE];
	short buf_m[3][BUF_SIZE];
	char *header;
	unsigned char  last_m = 16, mul_m;
	unsigned short *tmp;
	unsigned char *tmp_c, *ptr;

	header = qtk_ppm_header(width, height);
	if (header == NULL)
		return -ENOMEM;

	len = qtk_ppm_size(width, height);

	tmp = malloc((size_t)width * (size_t)height * sizeof(unsigned short));
	if (tmp == NULL) {
		free(header);
		return -ENOMEM;
	}

	tmp_c = malloc((size_t)width * (size_t)height);
	if (tmp_c == NULL) {
		free(header);
		free(tmp);
		return -ENOMEM;
	}

	/* Magic initialisation number 1 */
  for (i=2; i < 12; i+=2) {
    for (c=pt[i-2]; c <= pt[i]; c++) {
      curve[c] = (float) (c-pt[i-2]) / (pt[i]-pt[i-2]) * (pt[i+1]-pt[i-1]) + pt[i-1] + 0.5;
		}
	}

	/* Huffman tree structure init, heavily obfuscated */
	for (s=i=0; i < (int)sizeof src; i+=2) {
		unsigned short tmp = src[i] << 8 | (unsigned char) src[i+1];
		for (c=0; c < 256 >> src[i]; c++) {
			((unsigned short *)huff)[s] = tmp;
			((unsigned char *)huff_l)[s] = tmp & 0xFF;
			((unsigned char *)huff_h)[s] = tmp >> 8;
			s++;
		}
	}

	s = 3;
	for (c=0; c < 256; c++) {
		unsigned short tmp = (8-s) << 8 | c >> s << s | 1 << (s-1);
		huff[18][c] = tmp;
		huff_l[18][c] = tmp & 0xFF;
		huff_h[18][c] = tmp >> 8;
	}

	for (c = 0; c < 19; c++) {
		for (i = 0; i < 256; i++) {
			unsigned short test = huff_l[c][i] | (huff_h[c][i] << 8);
			if (test != huff[c][i]) {
				printf("unexpected val at [%d][%d]: %04X vs %04X\n", c, i, huff[c][i], test);
			}
		}
	}

	/* Init the bitbuffer */
	getbits(-1, &raw);

	/* Init the raw data buffer */
#ifdef COLOR
	for (i=0; i < (int)(sizeof(buf)/sizeof(short)); i++) {
		((short *)buf)[i] = 2048;
	}
	for (row=0; row < height; row+=4) {
		for (c=0; c < 3; c++) {
			mul[c] = getbits(6, &raw);
		}
		for (c=0; c < 3; c++) {
			val = val_from_last[last[c]] * mul[c];

			for (i=0; i < (int)(sizeof(buf[0])/sizeof(short)); i++) {
				((short *)buf[c])[i] = (((short *)buf[c])[i] * val - 1) >> 12;
			}
			last[c] = mul[c];

			for (r=0; r <= !c; r++) {
				buf[c][1][width/2] = buf[c][2][width/2] = mul[c] << 7;
				for (tree=1, col=width/2; col > 0; ) {
					if ((tree = radc_token(tree, &raw))) {
						col -= 2;
						if (tree == 8) {
							for (y=1; y < 3; y++) {
								for (x=col+1; x >= col; x--) {
									buf[c][y][x] = (unsigned char) radc_token(18, &raw) * mul[c];
								}
							}
						} else {
							for (y=1; y < 3; y++) {
								for (x=col+1; x >= col; x--) {
									unsigned short predictor;
									if (c) {
										predictor = (buf[c][y-1][x] + buf[c][y][x+1]) / 2;
									} else {
										predictor = (buf[c][y-1][x+1] + 2*buf[c][y-1][x] + buf[c][y][x+1]) / 4;
									}
									buf[c][y][x] = radc_token(tree+10, &raw) * 16 + predictor;
								}
							}
						}
					} else
						do {
							nreps = (col > 2) ? radc_token(9, &raw) + 1 : 1;
							for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
								col -= 2;
								for (y=1; y < 3; y++) {
									for (x=col+1; x >= col; x--) {
										if (c) {
											buf[c][y][x] = (buf[c][y-1][x] + buf[c][y][x+1]) / 2;
										} else {
											buf[c][y][x] = (buf[c][y-1][x+1] + 2*buf[c][y-1][x] + buf[c][y][x+1]) / 4;
										}
									}
								}
								if (rep & 1) {
									step = radc_token(10, &raw) << 4;
									for (y=1; y < 3; y++) {
										for (x=col+1; x >= col; x--) {
											buf[c][y][x] += step;
										}
									}
								}
							}
						} while (nreps == 9);
				}
				for (y=0; y < 2; y++) {
					for (x=0; x < width/2; x++) {
						val = (buf[c][y+1][x] << 4) / mul[c];
						if (val < 0) val = 0;
						if (c) RAW(tmp, row+y*2+c-1,x*2+2-c) = val;
						else   RAW(tmp, row+r*2+y,x*2+y) = val;
					}
				}
				memcpy (buf[c][0]+!c, buf[c][2], sizeof buf[c][0]-2*!c);
			}
		}
		for (y=row; y < row+4; y++) {
			for (x=0; x < width; x++) {
				if ((x+y) & 1) {
					r = x ? x-1 : x+1;
					s = x+1 < width ? x+1 : x-1;
					val = (RAW(tmp, y, x)-2048) + (RAW(tmp,y,r)+RAW(tmp,y,s))/2;
					if (val < 0) val = 0;
					RAW(tmp,y,x) = val;
				}
			}
		}
	}
#else
	for (i=0; i < BUF_SIZE; i++) {
		(buf_m[0])[i] = 2048;
	}
	for (row=0; row < height; row+=4) {
		c = 0;
		mul_m = getbits(6, &raw);
		getbits(6, &raw);
		getbits(6, &raw);

		val = val_from_last[last_m] * mul_m;

		for (i=0; i < (int)(sizeof(buf[0])/sizeof(short)); i++) {
			((short *)buf_m)[i] = (((short *)buf_m)[i] * val - 1) >> 12;
		}
		last_m = mul_m;

		for (r=0; r < 2; r++) {
			buf_m[1][width/2] = buf_m[2][width/2] = mul_m << 7;
			for (tree=1, col=width/2; col > 0; ) {
				if ((tree = radc_token(tree, &raw))) {
					col -= 2;
					if (tree == 8) {
						unsigned char token;
						token = (unsigned char) radc_token(18, &raw);
						buf_m[1][col+1] = token * mul_m;
						token = (unsigned char) radc_token(18, &raw);
						buf_m[1][col] = token * mul_m;
						token = (unsigned char) radc_token(18, &raw);
						buf_m[2][col+1] = token * mul_m;
						token = (unsigned char) radc_token(18, &raw);
						buf_m[2][col] = token * mul_m;
					} else {
						unsigned short predictor;
						unsigned short token;

						predictor = ((buf_m[0][col+1] << 1) + buf_m[0][col+2] + buf_m[1][col+2]) >> 2;
						token = radc_token(tree+10, &raw);
						buf_m[1][col+1] = (token << 4) + predictor;
						predictor = ((buf_m[0][col] << 1) + buf_m[0][col+1] + buf_m[1][col+1]) >> 2;
						token = radc_token(tree+10, &raw);
						buf_m[1][col] = (token << 4) + predictor;

						predictor = ((buf_m[1][col+1] << 1) + buf_m[1][col+2] + buf_m[2][col+2]) >> 2;
						token = radc_token(tree+10, &raw);
						buf_m[2][col+1] = (token << 4) + predictor;
						predictor = ((buf_m[1][col] << 1) + buf_m[1][col+1] + buf_m[2][col+1]) >> 2;
						token = radc_token(tree+10, &raw);
						buf_m[2][col] = (token << 4) + predictor;
					}
				} else
					do {
						nreps = (col > 2) ? radc_token(9, &raw) + 1 : 1;
						for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
							col -= 2;
							buf_m[1][col+1] = ((buf_m[0][col+1] << 1) + buf_m[0][col+2] + buf_m[1][col+2]) >> 2;
							buf_m[2][col+1] = ((buf_m[1][col+1] << 1) + buf_m[1][col+2] + buf_m[2][col+2]) >> 2;
							buf_m[1][col] = ((buf_m[0][col] << 1) + buf_m[0][col+1] + buf_m[1][col+1]) >> 2;
							buf_m[2][col] = ((buf_m[1][col] << 1) + buf_m[1][col+1] + buf_m[2][col+1]) >> 2;

							if (rep & 1) {
								step = radc_token(10, &raw) << 4;
								buf_m[1][col+1] += step;
								buf_m[2][col+1] += step;
								buf_m[1][col] += step;
								buf_m[2][col] += step;
							}
						}
					} while (nreps == 9);
			}
			for (y=0; y < 2; y++) {
				for (x=0; x < width/2; x++) {
					val = (buf_m[y+1][x] << 4) / mul_m;
					if (val < 0)
						val = 0;
					RAW(tmp, row+r*2+y,x*2+y) = val;
				}
			}
			memcpy (buf_m[0]+1, buf_m[2], sizeof buf_m[0]-2);
		}

		/* Copy the data we got */
		for (y=row; y < row+4; y++) {
			for (x=0; x < width; x++) {
				if ((x+y) & 1) {
					r = x ? x-1 : x+1;
					s = x+1 < width ? x+1 : x-1;
					val = (RAW(tmp, y, x)-2048) + (RAW(tmp,y,r)+RAW(tmp,y,s))/2;
					if (val < 0) val = 0;
					RAW(tmp,y,x) = val;
				}
			}
		}

    /* Consume RADC tokens but don't care about them. */
		for (c=1; c != 3; c++) {
			for (tree=1, col=width/2; col > 0; ) {
				if ((tree = radc_token(tree, &raw))) {
					col -= 2;
					if (tree == 8) {
						for (y=1; y < 3; y++) {
							for (x=col+1; x >= col; x--) {
								radc_token(18, &raw);
							}
						}
					} else {
						for (y=1; y < 3; y++) {
							for (x=col+1; x >= col; x--) {
								radc_token(tree+10, &raw);
							}
						}
					}
				} else
					do {
						nreps = (col > 2) ? radc_token(9, &raw) + 1 : 1;
						for (rep=0; rep < 8 && rep < nreps && col > 0; rep++) {
							col -= 2;
							if (rep & 1) {
								radc_token(10, &raw);
							}
						}
					} while (nreps == 9);
			}
		}

	}
#endif

  for (i=0; i < height*width; i++) {
    tmp_c[i] = LIM(curve[tmp[i]] >> 4, 0, 255);
	}

	*out = calloc(1, len);
	if (*out == NULL) {
		free(header);
		free(tmp);
		free(tmp_c);
		return -ENOMEM;
	}

	strcpy((char *)*out, header);
	ptr = *out + strlen(header);
	free(header);

	gp_bayer_decode(tmp_c, width, height, ptr, BAYER_TILE_GBRG);
	free(tmp);
	free(tmp_c);

	return 0;
}

#undef PREDICTOR
