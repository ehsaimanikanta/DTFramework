// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause Clear

/** @file libufdt_mergeoverlay_wrapper.c
 *
 * Qualcomm Silicon External package declaration file.
 *
**/

/**
 * UEFI wrapper that implements fdt_merge_overlay() using libufdt.
 *
 * This file provides the fdt_merge_overlay() API (defined in DTBExtnLib.h)
 * using the libufdt fast overlay engine instead of the libfdt-based
 * implementation in DTBExtnLib_overlay.c.
 *
 * libufdt builds an unflattened device tree (ufdt) from the FDT blobs,
 * enabling O(1) node lookups via hash tables - significantly faster than
 * the O(n) linear scans performed by libfdt's fdt_overlay_apply().
 *
 * Implementation steps:
 *   1. pre_overlay_malloc() - set up the bump-pointer allocator
 *   2. ufdt_apply_overlay() - build ufdt trees and merge fragments
 *   3. CopyMem()            - copy result to caller's output buffer
 *   4. post_overlay_free()  - release all allocations
 *
 * pre_overlay_malloc() and post_overlay_free() are provided by the
 * platform-specific allocator (libufdt_sysdeps_uefi.c).
 **/

#include "libfdt.h"
#include "DTBExtnLib.h"
#include "DTBInternals.h"
#include "DTBExtnLib_env.h"
#include <ufdt_overlay.h>
#include <Library/DebugLib.h>
#include <Library/QcomBaseLib.h>

/* External functions from the platform-specific allocator */
extern void *
pre_overlay_malloc (
  void
  );

extern void
post_overlay_free (
  void
  );

/**
 * ufdt_merge_overlay - Merge overlay into primary blob using libufdt
 *
 * @primary_blob:  Primary DTB tree (read-only input, must not have __fixups__)
 * @pb_size:       Size of primary blob in bytes
 * @overlay_blob:  Overlay DTBO tree (modified internally by libufdt)
 * @ob_size:       Size of overlay blob in bytes
 * @merge_blob:    Output buffer for the merged DTB
 * @mb_size:       Size of output buffer in bytes
 *
 * Returns FDT_ERR_QC_NOERROR on success, negative error code on failure.
 */
int
ufdt_merge_overlay (
  const void  *primary_blob,
  size_t      pb_size,
  void        *overlay_blob,
  size_t      ob_size,
  void        *merge_blob,
  size_t      mb_size
  )
{
  struct fdt_header  *result_fdt;
  size_t             result_size;

  PTR_CHECK (primary_blob);
  PTR_CHECK (overlay_blob);
  PTR_CHECK (merge_blob);

  /* Step 1: Set up the bump-pointer allocator used by dto_malloc() inside
   * ufdt_apply_overlay(). Must be called before ufdt_apply_overlay(). */
  if (!pre_overlay_malloc ()) {
    DEBUG ((DEBUG_ERROR, "[libufdt] FATAL: pre_overlay_malloc failed\n"));
    return -FDT_ERR_NOSPACE;
  }

  /* Step 2: Apply overlay directly.
   * ufdt_apply_overlay() builds ufdt trees from both blobs, resolves
   * __fixups__, merges fragments, and returns a pointer to the merged FDT
   * inside the allocator buffer. */
  result_fdt = ufdt_apply_overlay (
                 (struct fdt_header *)(UINTN)primary_blob,
                 pb_size,
                 overlay_blob,
                 ob_size
                 );

  if (result_fdt == NULL) {
    DEBUG ((DEBUG_ERROR, "[libufdt] ERROR: ufdt_apply_overlay failed\n"));
    post_overlay_free ();
    return -FDT_ERR_INTERNAL;
  }

  /* Step 3: Copy merged result to caller's output buffer before freeing.
   * result_fdt points inside the allocator buffer which post_overlay_free()
   * will release, so the copy must happen first. */
  result_size = fdt_totalsize (result_fdt);
  if (result_size > mb_size) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt] ERROR: Result (%ld B) exceeds output buffer (%ld B)\n",
      result_size,
      mb_size
      ));
    post_overlay_free ();
    return -FDT_ERR_QC_BUF2SMALL;
  }

  CopyMem (merge_blob, result_fdt, result_size);

  /* Step 4: Release the allocator buffer. */
  post_overlay_free ();

  return FDT_ERR_QC_NOERROR;
}

/**
 * fdt_merge_overlay - Compatibility wrapper that calls ufdt_merge_overlay
 *
 * @primary_blob:  Primary DTB tree (read-only input, must not have __fixups__)
 * @pb_size:       Size of primary blob in bytes
 * @overlay_blob:  Overlay DTBO tree (modified internally by libufdt)
 * @ob_size:       Size of overlay blob in bytes
 * @merge_blob:    Output buffer for the merged DTB
 * @mb_size:       Size of output buffer in bytes
 *
 * Returns FDT_ERR_QC_NOERROR on success, negative error code on failure.
 */
int
fdt_merge_overlay (
  const void  *primary_blob,
  size_t      pb_size,
  void        *overlay_blob,
  size_t      ob_size,
  void        *merge_blob,
  size_t      mb_size
  )
{
  return ufdt_merge_overlay (
           primary_blob,
           pb_size,
           overlay_blob,
           ob_size,
           merge_blob,
           mb_size
           );
}
