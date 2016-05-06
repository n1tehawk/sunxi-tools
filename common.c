/*
 * Copyright (C) 2012  Henrik Nordstrom <henrik@henriknordstrom.net>
 * Copyright (C) 2016  Bernhard Nortmann <bernhard.nortmann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

unsigned int file_size(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) != 0) {
		fprintf(stderr, "stat() error on file \"%s\": %s\n", filename,
			strerror(errno));
		exit(1);
	}
	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "error: \"%s\" is not a regular file\n", filename);
		exit(1);
	}
	return st.st_size;
}

int file_save(const char *name, void *data, size_t size)
{
	FILE *out = fopen(name, "wb");
	int rc;
	if (!out) {
		perror("Failed to open output file");
		exit(1);
	}
	rc = fwrite(data, size, 1, out);
	fclose(out);
	return rc;
}

void *file_load(const char *name, size_t *size)
{
	size_t bufsize = 8192;
	size_t offset = 0;
	char *buf = malloc(bufsize);
	FILE *in;
	if (strcmp(name, "-") == 0)
		in = stdin;
	else
		in = fopen(name, "rb");
	if (!in) {
		perror("Failed to open input file");
		exit(1);
	}

	while (true) {
		ssize_t len = bufsize - offset;
		ssize_t n = fread(buf+offset, 1, len, in);
		offset += n;
		if (n < len)
			break;
		bufsize <<= 1;
		buf = realloc(buf, bufsize);
	}
	if (size)
		*size = offset;
	if (in != stdin)
		fclose(in);
	return buf;
}
