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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

#define STDIN_INITIAL_CHUNK	8192

inline bool file_exists(const char *filename)
{
	return access(filename, F_OK) == 0;
}

ssize_t file_size(const char *filename)
{
	ssize_t rc;
	struct stat st;
	if (stat(filename, &st) != 0) {
		rc = errno;
		pr_error("stat() error on file \"%s\": %s\n",
			 filename, strerror(rc));
		return -rc;
	}
	if (!S_ISREG(st.st_mode)) {
		pr_error("file_size() error: \"%s\" is not a regular file\n",
			 filename);
		return -EINVAL;
	}
	return st.st_size;
}

ssize_t file_save(const char *filename, void *data, size_t size)
{
	ssize_t rc;
	FILE *out = fopen(filename, "wb");
	if (!out) {
		rc = errno;
		pr_error("file_save() failed to open \"%s\": %s\n",
			 filename, strerror(rc));
		return -rc;
	}
	rc = fwrite(data, 1, size, out);
	if (ferror(out))
		rc = -errno;
	fclose(out);
	return rc;
}

static void *file_load_stdin(ssize_t *size)
{
	char *buffer = NULL;
	size_t bufsize = STDIN_INITIAL_CHUNK, offset;
	ssize_t rc;

	for (offset = 0; !feof(stdin); offset += rc) {
		buffer = realloc(buffer, bufsize);
		if (!buffer) {
			rc = -ENOMEM;
		} else {
			rc = fread(buffer + offset, 1, bufsize - offset, stdin);
			if (ferror(stdin))
				rc = -errno;
		}
		if (rc < 0) {
			pr_error("error reading <stdin>: %s\n", strerror(-rc));
			if (size)
				*size = rc;
			free(buffer);
			return NULL;
		}
		bufsize <<= 1; /* double buffer size for next reallocation */
	}

	if (offset != bufsize)
		buffer = realloc(buffer, offset); /* allocate actual size */
	if (size)
		*size = offset;
	return buffer;
}

void *file_load(const char *filename, ssize_t *size)
{
	char *buffer = NULL;
	size_t bufsize;
	FILE *in;
	ssize_t rc;

	if (strcmp(filename, "-") == 0)
		return file_load_stdin(size);

	rc = file_size(filename);
	if (rc > 0) {
		bufsize = rc;
		in = fopen(filename, "rb");
		if (!in) {
			rc = -errno;
			pr_error("file_load() failed to open \"%s\": %s\n",
				 filename, strerror(-rc));
		}
	}
	if (rc <= 0) {
		if (size)
			*size = rc;
		return NULL; /* error or empty file */
	}

	buffer = malloc(bufsize);
	if (!buffer) {
		rc = -ENOMEM;
	} else {
		rc = fread(buffer, 1, bufsize, in);
		if (ferror(in))
			rc = -errno;
	}
	if (rc < 0) {
		pr_error("error reading \"%s\": %s\n", filename, strerror(-rc));
		if (size)
			*size = rc;
		fclose(in);
		free(buffer);
		return NULL;
	}

	if (size)
		*size = rc;
	fclose(in);
	return buffer;
}
