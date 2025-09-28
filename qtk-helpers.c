/* qtk-helpers.c
 *
   Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
 *
 * getbithuff() heavily inspired from dcraw.c.
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

/* Write a basic .qtk header. This is imperfect and may not allow to open the
 * raw files we generate with the official, vintage Quicktake software, but it
 * is enough for dcraw to open and convert it.
 */
void
qtk_raw_header(unsigned char *data, const char *pic_format)
{
	char hdr[] = {0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x04,0x00,0x00,0x73,0xE4,0x00,0x01};

	memcpy(hdr, pic_format, 4);
	memcpy(data, hdr, sizeof hdr);
}

char *qtk_ppm_header(int width, int height) {
	char *header = malloc(128);
	if (header == NULL)
		return NULL;

	snprintf(header, 127,
					 "P5\n#test\n%d %d\n%d\n",
					 width, height, 255);

	return header;
}

int qtk_ppm_size(int width, int height) {
	char *header;
	int len;

	header = qtk_ppm_header(width, height);
	if (header == NULL) {
		return -ENOMEM;
	}

	len = (width * height) + strlen(header);
	free(header);

	return len;
}

unsigned char bitbuf=0;
unsigned char vbits=0;

void initbithuff(void) {
  /* Consider we won't run out of cache there (at the very start). */
    bitbuf = *(input_buffer++);
    vbits = 8;
}

void refill(void) {
  bitbuf = *(input_buffer++);

  vbits = 8;
}

unsigned char getbit(void) {
  unsigned char r;
  if (vbits == 0) {
    refill();
  }
  r = bitbuf & 0x80 ? 1:0;

  bitbuf <<= 1;
  vbits--;

  return r;
}

unsigned char getbits6 (void) {
  unsigned char r = 0;
  unsigned char n = 6;
  while (n--) {
    r = (r<<1) | getbit();
  }
  // printf("has %8b\n", r);
  return r;
}

unsigned char getctrlhuff (unsigned char huff_num) {
  unsigned char r = 0;
  unsigned char n = 0;

  do {
    n++;
    // printf(" %8b not valid\n", r);
    r = (r<<1) | getbit();
  } while (huff_ctrl[huff_num][r] != n);

  // printf("value for [%02d][%8b] = %d\n", huff_num, r, huff_split[huff_num][r]);
  return huff_ctrl[huff_num+1][r];
}

unsigned char getdatahuff (unsigned char huff_num) {
  unsigned char r = 0;
  unsigned char n = 0;

  do {
    n++;
    // printf(" %8b not valid\n", r);
    r = (r<<1) | getbit();
  } while (huff_data[huff_num][r] != n);

  // printf("value for [%02d][%8b] = %d\n", huff_num, r, huff_split[huff_num][r]);
  return huff_data[huff_num][r+128];
}

/* Last huff data table is not really Huffman codes, rather a 5 bits value
 * is shifted left 3 and 4 is added. */
unsigned char getdatahuff8 (void) {
  unsigned char r = 0;
  unsigned char n = 5;
  while (n--) {
    r = (r<<1) | getbit();
  }
  return (r<<3)|0x04;
}
