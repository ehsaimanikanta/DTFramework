// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause Clear

/** @file libufdt_sysdeps_uefi.c
 *
 * Qualcomm Silicon External package declaration file.
 *
**/

/**
 * UEFI-specific sysdeps implementation for libufdt.
 *
 * The open-source libufdt/sysdeps/libufdt_sysdeps_vendor.c is compiled with:
 *   -DDTO_DISABLE_DEFAULT_VENDOR_LIBC_ALLOCATION  (disables dto_malloc/dto_free)
 *   -DDTO_DISABLE_DEFAULT_VENDOR_LIBC_PRINT       (disables dto_print)
 *
 * That file provides: qsort, dto_qsort, dto_strchr, dto_strtoul, dto_strlen,
 *   dto_memcmp, dto_memcpy, dto_strcmp, dto_strncmp, dto_memchr, dto_memset
 *   (all resolved to UEFI equivalents via LibUfdtSupport.h / LibFdtSupport.h)
 *
 * This file provides the UEFI-specific implementations for functions that
 * the open-source file cannot supply:
 *   - dto_print             : Disabled - no libc printf in SEC phase
 *   - dto_malloc            : Bump-pointer allocator from PCD-configured buffer
 *   - dto_free              : No-op - memory freed in bulk by post_overlay_free()
 *   - dto_malloc_reset      : No-op
 *   - dto_strdup            : Uses dto_malloc + CopyMem
 *   - dto_AsciiStrLen       : Maps to AsciiStrLen
 *   - pre_overlay_malloc    : Initializes the bump-pointer allocator
 *   - post_overlay_free     : Prints usage stats and resets the allocator
 *
 * Allocator design:
 *   Uses a fixed physical address buffer (PcdLibUfdtBufBase, default 1MB)
 *   that is pre-mapped in the MMU by EarlyCacheInit() before the DTB overlay
 *   runs in SEC phase. This avoids AllocatePages() which conflicts with the
 *   UEFI_Stack region in SEC phase.
 *
 *   Buffer sizing (measured on target):
 *     Primary DTB: ~233 KB, Overlay DTBO: ~51 KB
 *     Output FDT:  ~277 KB (1 large allocation)
 *     Node structs: ~293 KB (~10,393 small allocations)
 *     Total used:  ~570 KB - 1MB buffer provides ~430 KB headroom
 **/

#include <PiPei.h>
#include <Base.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/HobLib.h>
#include <Library/PrePiLib.h>
#include "libufdt/sysdeps/include/libufdt_sysdeps.h"

#define EFI_DTBO_ERROR  -1

/* =========================================================================
 * Bump-pointer allocator state
 * ========================================================================= */

STATIC void    *g_buffer  = NULL;  ///< Pre-allocated buffer base address
STATIC size_t  g_ssize    = 0;     ///< Current bump offset (bytes used)
STATIC size_t  g_buf_size = 0;     ///< Total buffer size (from PcdLibUfdtBufSize)

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * dto_print()
 *
 * Disabled in UEFI SEC phase - no libc printf available.
 *
 * @param[in]  fmt  Format string (unused).
 *
 * @retval  -1  Always returns error.
 **/
int
dto_print (
  const char  *fmt,
  ...
  )
{
  (void)fmt;
  return EFI_DTBO_ERROR;
}

/**
 * pre_overlay_malloc()
 *
 * Initializes the libufdt bump-pointer allocator before an overlay operation.
 *
 * Reads the buffer base address and size from PCDs:
 *   PcdLibUfdtBufBase - Physical address of the pre-allocated buffer
 *                       (mapped in EarlyCacheInit() before DTB overlay runs)
 *   PcdLibUfdtBufSize - Size of the buffer (default 1MB)
 *
 * Zeroes the entire buffer before use to ensure a clean state.
 * Must be called once before each overlay operation.
 *
 * @retval  Pointer to buffer base on success.
 * @retval  NULL if PCDs are not configured.
 **/
void *
pre_overlay_malloc (
  void
  )
{
  UINT64  buf_base = FixedPcdGet64 (PcdLibUfdtBufBase);
  UINT64  buf_size = FixedPcdGet64 (PcdLibUfdtBufSize);

  if ((buf_base == 0) || (buf_size == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt_sysdeps] pre_overlay_malloc FAILED: "
      "PcdLibUfdtBufBase=0x%lx PcdLibUfdtBufSize=0x%lx (not configured)\n",
      buf_base,
      buf_size
      ));
    return NULL;
  }

  g_buffer   = (void *)buf_base;
  g_ssize    = 0;
  g_buf_size = (size_t)buf_size;

  /* Zero the entire buffer before use to ensure a clean state */
  SetMem (g_buffer, g_buf_size, 0);

  DEBUG ((
    DEBUG_INFO,
    "[libufdt_sysdeps] pre_overlay_malloc: "
    "buffer at 0x%lx size %ld KB (from PCDs)\n",
    buf_base,
    buf_size / 1024
    ));
  return g_buffer;
}

/**
 * post_overlay_free()
 *
 * Finalizes the libufdt allocator after an overlay operation completes.
 *
 * Prints memory usage statistics at DEBUG_INFO level, zeroes the used
 * portion of the buffer to prevent data leakage, and resets the allocator
 * state. The physical buffer at PcdLibUfdtBufBase is NOT freed (fixed address).
 *
 * Must be called after each overlay operation (success or failure).
 **/
void
post_overlay_free (
  void
  )
{
  DEBUG ((DEBUG_INFO, "[libufdt] ===== Memory Usage Summary =====\n"));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Buffer size (PCD)      : %ld bytes (%ld KB)\n",
    g_buf_size,
    g_buf_size / 1024
    ));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Bytes used             : %ld bytes (%ld KB)\n",
    g_ssize,
    g_ssize / 1024
    ));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Bytes free             : %ld bytes (%ld KB)\n",
    g_buf_size - g_ssize,
    (g_buf_size - g_ssize) / 1024
    ));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Buffer utilization     : %ld%%\n",
    g_buf_size ? (g_ssize * 100) / g_buf_size : 0
    ));
  DEBUG ((DEBUG_INFO, "[libufdt] =================================\n"));

  /* Zero the used portion to prevent DTB content leakage */
  if ((g_buffer != NULL) && (g_ssize > 0)) {
    SetMem (g_buffer, g_ssize, 0);
  }

  /* Reset state - fixed address, no free needed */
  g_buffer   = NULL;
  g_ssize    = 0;
  g_buf_size = 0;
}

/**
 * dto_malloc()
 *
 * O(1) bump-pointer sub-allocator from the pre-allocated buffer.
 * Allocations are 8-byte aligned for ARM64 to prevent alignment faults.
 * Zero-initializes each allocation to prevent uninitialized struct field issues.
 * All memory is freed in bulk by post_overlay_free().
 *
 * @param[in]  size  Number of bytes to allocate.
 *
 * @retval  Pointer to allocated memory on success.
 * @retval  NULL on failure (buffer overflow or not initialized).
 **/
void *
dto_malloc (
  size_t  size
  )
{
  void  *retbuf;

  if (size == 0) {
    return NULL;
  }

  if (g_buffer == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt_sysdeps] dto_malloc FAILED: pre_overlay_malloc not called\n"
      ));
    return NULL;
  }

  /* Check for overflow before alignment */
  if (((MAX_UINT64 - g_ssize) < size) ||
      ((g_ssize + size) > g_buf_size))
  {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt_sysdeps] dto_malloc FAILED: "
      "buffer overflow (used=%ld, requested=%ld, total=%ld)\n",
      g_ssize,
      size,
      g_buf_size
      ));
    return NULL;
  }

  /* Align to 8 bytes for ARM64 to prevent alignment faults */
  size = (size + 7) & ~(size_t)7;

  /* Re-check after alignment padding */
  if ((g_ssize + size) > g_buf_size) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt_sysdeps] dto_malloc FAILED: "
      "buffer overflow after alignment (used=%ld, requested=%ld, total=%ld)\n",
      g_ssize,
      size,
      g_buf_size
      ));
    return NULL;
  }

  retbuf = (char *)g_buffer + g_ssize;
  SetMem (retbuf, size, 0);
  g_ssize += size;
  return retbuf;
}

/**
 * dto_free()
 *
 * No-op. Memory is freed in bulk by post_overlay_free().
 *
 * @param[in]  ptr  Pointer to free (ignored).
 **/
void
dto_free (
  void  *ptr
  )
{
  (void)ptr;
}

/**
 * dto_malloc_reset()
 *
 * No-op. Reset is handled by post_overlay_free().
 **/
void
dto_malloc_reset (
  void
  )
{
  /* No-op */
}

/**
 * dto_strdup()
 *
 * Duplicates a string using dto_malloc().
 *
 * @param[in]  s  Null-terminated ASCII string to duplicate.
 *
 * @retval  Pointer to duplicated string on success.
 * @retval  NULL if dto_malloc() fails.
 **/
char *
dto_strdup (
  const char  *s
  )
{
  char    *RetPtr = NULL;
  UINT32  Len     = AsciiStrLen (s) + 1;

  RetPtr = dto_malloc (Len);
  if (!RetPtr) {
    return NULL;
  }

  SetMem (RetPtr, Len, 0);
  CopyMem (RetPtr, s, AsciiStrLen (s));
  return RetPtr;
}

/**
 * dto_AsciiStrLen()
 *
 * Returns the length of a null-terminated ASCII string.
 * Wrapper for UEFI AsciiStrLen().
 *
 * @param[in]  s  Null-terminated ASCII string.
 *
 * @return  Length of the string in characters (excluding null terminator).
 **/
size_t
dto_AsciiStrLen (
  const char  *s
  )
{
  return AsciiStrLen (s);
}
