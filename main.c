/* main.c
 *
 * Copyright 2023, Colin Leroy-Mira <colin@colino.net>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quicktake1x0.h"

static int get_uint16_at(const unsigned char *buf, size_t offset) {
  return (buf[offset] << 8) | buf[offset+1];
}

int main(int argc, char *argv[]) {
  FILE *in_fp = NULL, *out_fp = NULL;
  unsigned char *in_buf = NULL, *out_buf = NULL;
  size_t in_size = 0, data_offset;
  unsigned int width, height, type;

  if (argc < 2) {
    printf("Usage: %s [input.qtk] [output.ppm]\n", argv[0]);
    goto done;
  }

  in_fp = fopen(argv[1], "r");
  if (!in_fp) {
    printf("Can not open %s: %s\n", argv[1], strerror(errno));
    goto done;
  }

  out_fp = fopen(argv[2], "wb");
  if (!out_fp) {
    printf("Can not open %s: %s\n", argv[2], strerror(errno));
    goto done;
  }

  if (fseek(in_fp, 0, SEEK_END) == 0) {
    in_size = ftell(in_fp);
    in_buf = malloc(in_size);
    rewind(in_fp);
  } else {
    printf("Can not find out file size: %s\n", strerror(errno));
    goto done;
  }

  if (fread(in_buf, 1, in_size, in_fp) < in_size) {
    printf("Can not read input file: %s\n", strerror(errno));
    goto done;
  }

  if (strncmp(in_buf, "qktn", 4)) {
    printf("File is not a Quicktake 150 picture.\n");
    goto done;
  }

  height = get_uint16_at(in_buf, 544);
  width  = get_uint16_at(in_buf, 546);
  type   = get_uint16_at(in_buf, 552);
  printf("Size: %dx%d, type: %d\n", width, height, type);

  if (type == 30) {
    data_offset = 738;
  } else {
    data_offset = 736;
  }
  if (qtkn_decode(in_buf + data_offset, width, height, &out_buf)) {
    printf("Error converting picture.\n");
    goto done;
  }

  fwrite(out_buf, 1, qtk_ppm_size(width, height), out_fp);

done:
  free(out_buf);
  free(in_buf);
  if (in_fp != NULL) {
    fclose(in_fp);
  }
  if (out_fp != NULL) {
    fclose(out_fp);
  }
}
