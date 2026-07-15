/** @file LibUfdtSupport.h
 *
 * Qualcomm Silicon External package declaration file.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
**/

/**
 * Root include file to support building the third-party libufdt library
 * in a UEFI environment.
 *
 * Provides ONLY the additional type definitions that libufdt needs beyond
 * what LibFdtSupport.h (from edk2/MdePkg/Library/BaseFdtLib) already supplies.
 *
 * This header adds only what is missing for libufdt:
 *   int64_t, ssize_t, ptrdiff_t, uintmax_t, SIZE_MAX
 *   strcmp, strncmp  (used by libufdt_sysdeps_vendor.c via dto_strcmp etc.)
 **/

#pragma once

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

/* Additional integer types not provided by LibFdtSupport.h
 *
 * Each typedef is guarded by the file-level or type-level macros that the
 * relevant system/compiler headers set, so that if those headers are already
 * visible (e.g. when compiled without -nostdinc) the typedef is skipped and
 * no "redefinition with different types" error is raised.
 */
/* int64_t
 * Guards cover:
 *   _INT64_T_DEFINED      : MSVC / EDK2 convention (already present)
 *   __int8_t_defined      : glibc stdint.h (defines all signed types under this guard)
 *   _BITS_STDINT_INTN_H   : glibc bits/stdint-intn.h file guard
 */
#if !defined(_INT64_T_DEFINED) && !defined(__int8_t_defined) && !defined(_BITS_STDINT_INTN_H)
  #define _INT64_T_DEFINED
  typedef INT64 int64_t;
#endif

#if !defined(_SSIZE_T_DEFINED) && !defined(_SSIZE_T)
  #define _SSIZE_T_DEFINED
  #define _SSIZE_T
  typedef INTN ssize_t;
#endif

/* ptrdiff_t
 * Guards cover:
 *   _PTRDIFF_T_DEFINED    : MSVC / GCC convention (already present)
 *   _PTRDIFF_T            : Clang built-in __stddef_ptrdiff_t.h
 *   _PTRDIFF_T_           : GCC built-in stddef.h
 */
#if !defined(_PTRDIFF_T_DEFINED) && !defined(_PTRDIFF_T) && !defined(_PTRDIFF_T_)
  #define _PTRDIFF_T_DEFINED
  #define _PTRDIFF_T
  #define _PTRDIFF_T_
  typedef INTN ptrdiff_t;
#endif

/* uintmax_t
 * Guards cover:
 *   _UINTMAX_T_DEFINED    : MSVC / EDK2 convention (already present)
 *   _STDINT_H             : glibc stdint.h file guard (uintmax_t has no own guard in glibc)
 *   _STDINT_H_            : alternate stdint.h guard used by some toolchains
 */
#if !defined(_UINTMAX_T_DEFINED) && !defined(_STDINT_H) && !defined(_STDINT_H_)
  #define _UINTMAX_T_DEFINED
  typedef UINT64 uintmax_t;
#endif

/* SIZE_MAX - maximum value of size_t */
#ifndef SIZE_MAX
#define SIZE_MAX  MAX_UINTN
#endif

/* strcmp / strncmp - not provided by LibFdtSupport.h, needed by open-source
 * libufdt_sysdeps_vendor.c */
#ifndef strcmp
#define strcmp(s1, s2)  ((int)AsciiStrCmp((s1), (s2)))
#endif
#ifndef strncmp
#define strncmp(s1, s2, n)  ((int)AsciiStrnCmp((s1), (s2), (n)))
#endif
