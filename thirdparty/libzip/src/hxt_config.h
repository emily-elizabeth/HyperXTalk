#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H

/*
   config.h for the libzip that OXT-Beyond builds with gyp.

   libzip's CMake build generates this file from config.h.in by probing
   the system; gyp runs no configure step, so this copy answers the same
   questions for Windows (MSVC), macOS, Linux and Android. What is left
   undefined is absent there, an option that is off, or something libzip
   has a plain fallback for. The options that are off: bzip2, xz and zstd
   compression, the crypto libraries that WinZip AES encryption needs (as
   before, revZip reads and writes Deflate and stored entries; traditional
   PKWARE encryption is built in), and zip_fdopen on Windows (ENABLE_FDOPEN,
   which revZip does not use). The fallbacks: libzip clears passwords
   itself rather than with explicit_bzero, and copies files the ordinary
   way rather than with clonefile or FICLONERANGE.
 */

#ifndef _HAD_ZIPCONF_H
#include "zipconf.h"
#endif

#if defined(_WIN32)

/* The MSVC runtime's underscored names, and its 64-bit file offsets */
#define HAVE__CLOSE
#define HAVE__DUP
#define HAVE__FDOPEN
#define HAVE__FILENO
#define HAVE__FSEEKI64
#define HAVE__FSTAT64
#define HAVE__FTELLI64
#define HAVE__SETMODE
#define HAVE__SNPRINTF
#define HAVE__SNPRINTF_S
#define HAVE__SNWPRINTF_S
#define HAVE__STAT64
#define HAVE__STRDUP
#define HAVE__STRICMP
#define HAVE__STRTOI64
#define HAVE__STRTOUI64
#define HAVE__UNLINK
#define HAVE_GETSECURITYINFO
#define HAVE_LOCALTIME_S
#define HAVE_MEMCPY_S
/* strerror_s, but not strerrorlen_s: the MSVC runtime has only the first. */
#define HAVE_STRERROR_S
#define HAVE_STRNCPY_S
#define HAVE_STDBOOL_H
#define HAVE_STRTOLL
#define HAVE_STRTOULL
#define HAVE_SNPRINTF
#define SIZEOF_OFF_T 4

#else /* POSIX */

#define ENABLE_FDOPEN
#define HAVE_FILENO
#define HAVE_FCHMOD
#define HAVE_FSEEKO
#define HAVE_FTELLO
#define HAVE_LOCALTIME_R
#define HAVE_MKSTEMP
#define HAVE_SNPRINTF
#define HAVE_STRCASECMP
#define HAVE_STRDUP
#define HAVE_STRTOLL
#define HAVE_STRTOULL
#define HAVE_STDBOOL_H
#define HAVE_STRINGS_H
#define HAVE_UNISTD_H
#define HAVE_STRUCT_TM_TM_ZONE

/* arc4random on macOS and Android; glibc has it only from 2.36, and the
   Linux builds must run on glibc 2.31, so Linux reads /dev/urandom. */
#if defined(__APPLE__) || defined(__ANDROID__)
#define HAVE_ARC4RANDOM
#endif

/* The Linux builds define _FILE_OFFSET_BITS=64 (config/linux-settings.gypi);
   macOS and Android's 64-bit ABIs have a 64-bit off_t. */
#if defined(__APPLE__) || defined(__LP64__) || \
    (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64)
#define SIZEOF_OFF_T 8
#else
#define SIZEOF_OFF_T 4
#endif

#endif /* POSIX */

#if defined(_WIN64) || defined(__LP64__)
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN
#endif

#define PACKAGE "libzip"
#define VERSION "1.12"

#endif /* HAD_CONFIG_H */
