// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause Clear

/** @file inttypes.h
 *
 * Qualcomm Silicon External package declaration file.
 *
**/

/**
 *
 *  UEFI-compatible inttypes.h stub for libufdt.
 *  libufdt_sysdeps.h includes <inttypes.h> for PRIu32, PRIx32, etc.
 *  In the UEFI environment these format specifiers are not used by the
 *  vendor sysdeps implementation, so this stub satisfies the include
 *  without pulling in POSIX headers.
 *
 **/

#pragma once

#include <LibUfdtSupport.h>

/* Printf format macros for fixed-width integer types.
 * These are defined here to satisfy any source that includes <inttypes.h>
 * but the vendor sysdeps implementation does not use them at runtime.
 */
#ifndef PRId8
#define PRId8  "d"
#endif
#ifndef PRId16
#define PRId16  "d"
#endif
#ifndef PRId32
#define PRId32  "d"
#endif
#ifndef PRId64
#define PRId64  "lld"
#endif

#ifndef PRIu8
#define PRIu8  "u"
#endif
#ifndef PRIu16
#define PRIu16  "u"
#endif
#ifndef PRIu32
#define PRIu32  "u"
#endif
#ifndef PRIu64
#define PRIu64  "llu"
#endif

#ifndef PRIx8
#define PRIx8  "x"
#endif
#ifndef PRIx16
#define PRIx16  "x"
#endif
#ifndef PRIx32
#define PRIx32  "x"
#endif
#ifndef PRIx64
#define PRIx64  "llx"
#endif

#ifndef PRIX8
#define PRIX8  "X"
#endif
#ifndef PRIX16
#define PRIX16  "X"
#endif
#ifndef PRIX32
#define PRIX32  "X"
#endif
#ifndef PRIX64
#define PRIX64  "llX"
#endif

#ifndef PRIuMAX
#define PRIuMAX  "llu"
#endif
#ifndef PRIxMAX
#define PRIxMAX  "llx"
#endif
