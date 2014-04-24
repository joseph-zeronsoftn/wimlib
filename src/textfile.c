/*
 * textfile.c
 */

/*
 * Copyright (C) 2014 Eric Biggers
 *
 * This file is part of wimlib, a library for working with WIM files.
 *
 * wimlib is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * wimlib is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wimlib; if not, see http://www.gnu.org/licenses/.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "wimlib/assert.h"
#include "wimlib/encoding.h"
#include "wimlib/error.h"
#include "wimlib/file_io.h"
#include "wimlib/textfile.h"
#include "wimlib/util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int
read_file_contents(const tchar *path, u8 **buf_ret, size_t *bufsize_ret)
{
	int raw_fd;
	struct filedes fd;
	struct stat st;
	u8 *buf;
	int ret;
	int errno_save;

	if (!path || !*path)
		return WIMLIB_ERR_INVALID_PARAM;

	raw_fd = topen(path, O_RDONLY | O_BINARY);
	if (raw_fd < 0) {
		ERROR_WITH_ERRNO("Can't open \"%"TS"\"", path);
		return WIMLIB_ERR_OPEN;
	}
	if (fstat(raw_fd, &st)) {
		ERROR_WITH_ERRNO("Can't stat \"%"TS"\"", path);
		close(raw_fd);
		return WIMLIB_ERR_STAT;
	}
	if ((size_t)st.st_size != st.st_size ||
	    (buf = MALLOC(st.st_size)) == NULL)
	{
		close(raw_fd);
		ERROR("Not enough memory to read \"%"TS"\"", path);
		return WIMLIB_ERR_NOMEM;
	}

	filedes_init(&fd, raw_fd);
	ret = full_read(&fd, buf, st.st_size);
	errno_save = errno;
	filedes_close(&fd);
	errno = errno_save;
	if (ret) {
		ERROR_WITH_ERRNO("Error reading \"%"TS"\"", path);
		FREE(buf);
		return ret;
	}

	*buf_ret = buf;
	*bufsize_ret = st.st_size;
	return 0;
}

static int
read_text_file_contents(const tchar *path,
			tchar **buf_ret, size_t *buflen_ret)
{
	int ret;
	u8 *buf_raw;
	size_t bufsize_raw;
	size_t offset_raw;
	bool utf8;
	tchar *buf_tstr;
	size_t bufsize_tstr;

	ret = read_file_contents(path, &buf_raw, &bufsize_raw);
	if (ret)
		return ret;

	/* Guess the encoding: UTF-8 or UTF-16LE.  (Something weirder and you're
	 * out of luck, sorry...)  */
	if (bufsize_raw >= 2 &&
	    buf_raw[0] == 0xFF &&
	    buf_raw[1] == 0xFE)
	{
		utf8 = false;
		offset_raw = 2;
	}
	else if (bufsize_raw >= 2 &&
		 buf_raw[0] <= 0x7F &&
		 buf_raw[1] == 0x00)
	{
		utf8 = false;
		offset_raw = 0;
	}
	else if (bufsize_raw >= 3 &&
		 buf_raw[0] == 0xEF &&
		 buf_raw[1] == 0xBB &&
		 buf_raw[2] == 0xBF)
	{
		utf8 = true;
		offset_raw = 3;
	}
	else
	{
		utf8 = true;
		offset_raw = 0;
	}

	if (utf8) {
		ret = utf8_to_tstr((const char *)(buf_raw + offset_raw),
				   bufsize_raw - offset_raw,
				   &buf_tstr, &bufsize_tstr);
	} else {
	#if TCHAR_IS_UTF16LE
		bufsize_tstr = bufsize_raw - offset_raw;
		buf_tstr = MALLOC(bufsize_tstr + 2);
		if (buf_tstr) {
			memcpy(buf_tstr, buf_raw + offset_raw, bufsize_tstr);
			((u8*)buf_tstr)[bufsize_tstr + 0] = 0;
			((u8*)buf_tstr)[bufsize_tstr + 1] = 0;
		} else {
			ret = WIMLIB_ERR_NOMEM;
		}
	#else
		ret = utf16le_to_tstr((const utf16lechar *)(buf_raw + offset_raw),
				      bufsize_raw - offset_raw,
				      &buf_tstr, &bufsize_tstr);
	#endif
	}
	FREE(buf_raw);
	if (ret)
		return ret;

	*buf_ret = buf_tstr;
	*buflen_ret = bufsize_tstr / sizeof(tchar);
	return 0;
}

static int
string_set_append(struct string_set *set, tchar *str)
{
	size_t num_alloc_strings = set->num_alloc_strings;

	if (set->num_strings == num_alloc_strings) {
		tchar **new_strings;

		num_alloc_strings = max(num_alloc_strings * 3 / 2,
					num_alloc_strings + 4);
		new_strings = REALLOC(set->strings,
				      sizeof(set->strings[0]) * num_alloc_strings);
		if (!new_strings)
			return WIMLIB_ERR_NOMEM;
		set->strings = new_strings;
		set->num_alloc_strings = num_alloc_strings;
	}
	set->strings[set->num_strings++] = str;
	return 0;
}

#define NOT_IN_SECTION		-1
#define IN_UNKNOWN_SECTION	-2

static int
parse_text_file(const tchar *path, tchar *buf, size_t buflen,
		const struct text_file_section *pos_sections,
		int num_pos_sections, int flags, line_mangle_t mangle_line)
{
	int current_section = NOT_IN_SECTION;
	bool have_named_sections = false;
	tchar *p;
	tchar *nl;
	unsigned long line_no = 1;

	for (int i = 0; i < num_pos_sections; i++) {
		if (*pos_sections[i].name)
			have_named_sections = true;
		else
			current_section = i;
	}

	for (p = buf; p != buf + buflen; p = nl + 1, line_no++) {
		tchar *line_begin, *line_end;
		size_t line_len;
		int ret;

		nl = tmemchr(p, T('\n'), buf + buflen - p);
		if (!nl)
			break;

		line_begin = p;
		line_end = nl;

		/* Ignore leading whitespace.  */
		while (line_begin < nl && istspace(*line_begin))
			line_begin++;

		/* Ignore trailing whitespace.  */
		while (line_end > line_begin && istspace(*(line_end - 1)))
			line_end--;

		line_len = line_end - line_begin;

		/* Ignore comments and empty lines.  */
		if (line_len == 0 || *line_begin == T(';') || *line_begin == T('#'))
			continue;

		line_begin[line_len] = T('\0');

		/* Check for beginning of new section.  */
		if (line_begin[0] == T('[') &&
		    line_begin[line_len - 1] == T(']') &&
		    have_named_sections)
		{
			line_begin[line_len - 1] = T('\0');
			current_section = IN_UNKNOWN_SECTION;
			for (int i = 0; i < num_pos_sections; i++) {
				if (!tstrcmp(line_begin + 1,
					     pos_sections[i].name))
				{
					current_section = i;
					break;
				}
			}
			line_begin[line_len - 1] = T(']');
			if (current_section < 0)
				WARNING("%"TS":%lu: Unrecognized section \"%"TS"\"",
					path, line_no, line_begin);
			continue;
		}

		if (current_section < 0) {
			if (current_section == NOT_IN_SECTION)
				WARNING("%"TS":%lu: Not in a bracketed section!",
					path, line_no);
			continue;
		}

		if (flags & LOAD_TEXT_FILE_REMOVE_QUOTES) {
			if (line_begin[0] == T('"') || line_begin[0] == T('\'')) {
				tchar quote = line_begin[0];
				if (line_len >= 2 &&
				    line_begin[line_len - 1] == quote)
				{
					line_begin++;
					line_len -= 2;
					line_begin[line_len] = T('\0');
				}
			}
		}

		if (mangle_line) {
			ret = (*mangle_line)(line_begin, path, line_no);
			if (ret)
				return ret;
		}

		ret = string_set_append(pos_sections[current_section].strings,
					line_begin);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * do_load_text_file -
 *
 * Read and parse lines from a text file from an on-disk file or a buffer.
 * The file may contain sections, like in an INI file.
 *
 * @path
 *	Path to the file on disk to read, or a dummy name for the buffer.
 * @buf
 *	If NULL, the data will be read from the @path file.  Otherwise the data
 *	will be read from this buffer, which must be newline-terminated.
 * @buflen
 *	Length of buffer in 'tchars'; ignored if @buf is NULL.
 * @buf_ret
 *	On success, a pointer to a buffer backing the parsed lines is stored
 *	here.  If @buf is not NULL, this will be @buf.  Otherwise, this will be
 *	an allocated buffer that must be freed when finished with the lines.
 * @pos_sections
 *	Specifications of allowed sections in the file.  Each such specification
 *	consists of the name of the section (e.g. [ExclusionList], like in the
 *	INI file format), along with a pointer to the list of lines parsed for
 *	that section.  Use an empty name to indicate the destination of lines
 *	not in any section.
 * @num_pos_sections
 *	Length of @pos_sections array.
 * @flags
 *	LOAD_TEXT_FILE_REMOVE_QUOTES or 0.
 * @mangle_line
 *	Optional callback to modify each line being read.
 *
 * Returns 0 on success or a positive error code on failure.
 *
 * Unknown sections are ignored (warning printed).
 */
int
do_load_text_file(const tchar *path,
		  tchar *buf, size_t buflen,
		  tchar **buf_ret,
		  const struct text_file_section *pos_sections,
		  int num_pos_sections,
		  int flags,
		  line_mangle_t mangle_line)
{
	int ret;
	bool pathmode = (buf == NULL);

	if (pathmode) {
		ret = read_text_file_contents(path, &buf, &buflen);
		if (ret)
			return ret;

		/* Overwrite '\0' with '\n' to avoid special case of last line
		 * not terminated with '\n'.  */
		buf[buflen++] = T('\n');
	} else {
		wimlib_assert(buflen > 0 && buf[buflen - 1] == T('\n'));
	}

	ret = parse_text_file(path, buf, buflen, pos_sections,
			      num_pos_sections, flags, mangle_line);
	if (ret) {
		for (int i = 0; i < num_pos_sections; i++)
			FREE(pos_sections[i].strings->strings);
		if (pathmode)
			FREE(buf);
		return ret;
	}

	*buf_ret = buf;
	return 0;
}
