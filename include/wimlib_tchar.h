#ifndef _WIMLIB_TCHAR_H
#define _WIMLIB_TCHAR_H

/* Functions to act on "tchar" strings, which have a platform-dependent encoding
 * and character size. */

#ifdef __WIN32__
#include <wchar.h>
/* For Windows builds, the "tchar" type will be 2 bytes and will be equivalent
 * to "wchar_t" and "utf16lechar".  All indicate one code unit of a UTF16-LE
 * string. */
typedef wchar_t tchar;
#  define TCHAR_IS_UTF16LE 1
#  define _T(text) L##text
#  define T(text) _T(text) /* Make a string literal into a wide string */
#  define TS "ls" /* Format a string of "tchar" */
#  define TC "lc" /* Format a "tchar" */

/* For Windows builds, the following definitions replace the "tchar" functions
 * with the "wide-character" functions. */
#  define tmemchr	wmemchr
#  define tmemcpy	wmemcpy
#  define tmemmove	wmemmove
#  define tmempcpy	wmempcpy
#  define tstrcpy	wcscpy
#  define tprintf	wprintf
#  define tsprintf	swprintf
#  define tfprintf	fwprintf
#  define tvfprintf	vfwprintf
#  define istalpha	iswalpha
#  define istspace	iswspace
#  define tstrcmp	wcscmp
#  define tstrncmp	wcsncmp
#  define tstrchr	wcschr
#  define tstrpbrk	wcspbrk
#  define tstrrchr	wcsrchr
#  define tstrlen	wcslen
#  define tmemcmp	wmemcmp
#  define tstrcasecmp   _wcsicmp
#  define tstrftime	wcsftime
#  define tputchar	putwchar
#  define tputc		putwc
#  define tputs		_putws
#  define tfputs	fputws
#  define tfopen	_wfopen
#  define topen		_wopen
#  define tstat		_wstati64
#  define tstrtol	wcstol
#  define tstrtod	wcstod
#  define tstrtoul	wcstoul
#  define tunlink	_wunlink
#  define tstrerror	_wcserror
#  define taccess	_waccess
#  define tstrdup	wcsdup
#  define ttempnam      _wtempnam
#  define tgetenv	_wgetenv
#  define totlower(c)	towlower((wchar_t)(c))
/* The following "tchar" functions do not have exact wide-character equivalents
 * on Windows so require parameter rearrangement or redirection to a replacement
 * function defined ourselves. */
#  define TSTRDUP	WCSDUP
#  define tmkdir(path, mode) _wmkdir(path)
#  define tstrerror_r   win32_strerror_r_replacement
#  define trename	win32_rename_replacement
#  define tglob		win32_wglob
#else /* __WIN32__ */
/* For non-Windows builds, the "tchar" type will be one byte and will specify a
 * string in the locale-dependent multibyte encoding.  However, only UTF-8 is
 * well supported in this library. */
typedef char tchar;
#  define TCHAR_IS_UTF16LE 0
#  define T(text) text /* In this case, strings of "tchar" are simply strings of
			  char */
#  define TS "s"       /* Similarly, a string of "tchar" is printed just as a
			  normal string. */
#  define TC "c"       /* Print a single character */
/* For non-Windows builds, replace the "tchar" functions with the regular old
 * string functions. */
#  define tmemchr	memchr
#  define tmemcpy	memcpy
#  define tmemmove	memmove
#  define tmempcpy	mempcpy
#  define tstrcpy	strcpy
#  define tprintf	printf
#  define tsprintf	sprintf
#  define tfprintf	fprintf
#  define tvfprintf	vfprintf
#  define istalpha	isalpha
#  define istspace	isspace
#  define tstrcmp	strcmp
#  define tstrncmp	strncmp
#  define tstrchr	strchr
#  define tstrpbrk	strpbrk
#  define tstrrchr	strrchr
#  define tstrlen	strlen
#  define tmemcmp	memcmp
#  define tstrcasecmp   strcasecmp
#  define tstrftime	strftime
#  define tputchar	putchar
#  define tputc		putc
#  define tputs		puts
#  define tfputs	fputs
#  define tfopen	fopen
#  define topen		open
#  define tstat		stat
#  define tunlink	unlink
#  define tstrerror	strerror
#  define tstrtol	strtol
#  define tstrtod	strtod
#  define tstrtoul	strtoul
#  define tmkdir	mkdir
#  define tstrdup	strdup
#  define ttempnam      tempnam
#  define tgetenv	getenv
#  define totlower(c)	tolower((unsigned char)(c))
#  define TSTRDUP	STRDUP
#  define tstrerror_r	strerror_r
#  define trename	rename
#  define taccess	access
#  define tglob		glob
#endif /* !__WIN32__ */

#endif /* _WIMLIB_TCHAR_H */
