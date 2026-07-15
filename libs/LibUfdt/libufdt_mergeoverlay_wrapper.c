/** @file libufdt_mergeoverlay_wrapper.c
 *
 * Qualcomm Silicon External package declaration file.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
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
 *   1. Validate primary blob is a proper DTB (no __fixups__ node)
 *   2. pre_overlay_malloc() to initialize the bump-pointer allocator
 *   3. ufdt_install_blob() to validate the primary DTB
 *   4. ufdt_apply_overlay() to apply the overlay (handles __fixups__)
 *   5. Copy result to output buffer
 *   6. post_overlay_free() to release all allocations
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
 * fdt_merge_overlay - Merge overlay into primary blob using libufdt
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
  int                ret_value     = FDT_ERR_QC_NOERROR;
  struct fdt_header  *main_fdt_hdr = NULL;
  struct fdt_header  *result_fdt   = NULL;
  int                fixups_off;

  /* Validate inputs */
  PTR_CHECK (primary_blob);
  PTR_CHECK (overlay_blob);
  PTR_CHECK (merge_blob);

  /* Validate primary DTB header */
  ret_value = fdt_check_header (primary_blob);
  if (ret_value != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt] ERROR: Primary DTB header check failed: %d (magic=0x%x)\n",
      ret_value,
      fdt_magic (primary_blob)
      ));
    return -FDT_ERR_BADMAGIC;
  }

  DEBUG ((DEBUG_INFO, "[libufdt] ===== Overlay Input Sizes =====\n"));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Primary DTB size : %d bytes (%d KB)\n",
    (int)fdt_totalsize (primary_blob),
    (int)(fdt_totalsize (primary_blob) / 1024)
    ));

  /*
   * Reject primary_blob if it contains __fixups__ - that would make it a
   * DTBO, not a valid base DTB. A base DTB without __symbols__ would cause
   * fixup resolution to fail.
   */
  fixups_off = fdt_path_offset (primary_blob, "/__fixups__");
  if (fixups_off != -FDT_ERR_NOTFOUND) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt] ERROR: primary_blob contains /__fixups__ - "
      "this is a DTBO, not a valid base DTB (size=%d)\n",
      (int)fdt_totalsize (primary_blob)
      ));
    return -FDT_ERR_BADOVERLAY;
  }

  /* Validate overlay DTBO header */
  ret_value = fdt_check_header (overlay_blob);
  if (ret_value != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt] ERROR: Overlay DTB header check failed: %d (magic=0x%x)\n",
      ret_value,
      fdt_magic (overlay_blob)
      ));
    return -FDT_ERR_BADMAGIC;
  }

  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Overlay DTBO size : %d bytes (%d KB)\n",
    (int)fdt_totalsize (overlay_blob),
    (int)(fdt_totalsize (overlay_blob) / 1024)
    ));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Output buf size   : %d bytes (%d KB)  [primary + overlay]\n",
    (int)(fdt_totalsize (primary_blob) + fdt_totalsize (overlay_blob)),
    (int)((fdt_totalsize (primary_blob) + fdt_totalsize (overlay_blob)) / 1024)
    ));
  DEBUG ((DEBUG_INFO, "[libufdt] =================================\n"));

  /* Step 1: Initialize libufdt bump-pointer allocator */
  if (!pre_overlay_malloc ()) {
    DEBUG ((DEBUG_ERROR, "[libufdt] FATAL: pre_overlay_malloc failed\n"));
    return -FDT_ERR_NOSPACE;
  }

  /* Step 2: Validate and install the primary blob into libufdt */
  main_fdt_hdr = ufdt_install_blob ((void *)primary_blob, pb_size);
  if (!main_fdt_hdr) {
    DEBUG ((DEBUG_ERROR, "[libufdt] ERROR: ufdt_install_blob failed for primary DTB\n"));
    ret_value = -FDT_ERR_BADMAGIC;
    goto cleanup;
  }

  /* Step 3: Apply overlay - builds ufdt trees and merges fragments */
  result_fdt = ufdt_apply_overlay (
                 main_fdt_hdr,
                 fdt_totalsize (main_fdt_hdr),
                 overlay_blob,
                 ob_size
                 );

  if (result_fdt == NULL) {
    DEBUG ((DEBUG_ERROR, "[libufdt] ERROR: ufdt_apply_overlay FAILED\n"));
    DEBUG ((DEBUG_ERROR, "[libufdt]   Possible causes:\n"));
    DEBUG ((DEBUG_ERROR, "[libufdt]   1. __fixups__ references symbol not in main DTB __symbols__\n"));
    DEBUG ((DEBUG_ERROR, "[libufdt]   2. Memory allocation failure (buffer too small)\n"));
    DEBUG ((DEBUG_ERROR, "[libufdt]   3. Malformed overlay DTB\n"));
    ret_value = -FDT_ERR_INTERNAL;
    goto cleanup;
  }

  /* Validate result FDT header */
  ret_value = fdt_check_header (result_fdt);
  if (ret_value != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "[libufdt] ERROR: Result FDT header check failed: %d (magic=0x%x)\n",
      ret_value,
      fdt_magic (result_fdt)
      ));
    ret_value = -FDT_ERR_BADMAGIC;
    goto cleanup;
  }

  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Merged DTB size   : %d bytes (%d KB)\n",
    (int)fdt_totalsize (result_fdt),
    (int)(fdt_totalsize (result_fdt) / 1024)
    ));
  DEBUG ((
    DEBUG_INFO,
    "[libufdt] Overhead vs primary: +%d bytes (+%d KB)\n",
    (int)(fdt_totalsize (result_fdt) - fdt_totalsize (primary_blob)),
    (int)((fdt_totalsize (result_fdt) - fdt_totalsize (primary_blob)) / 1024)
    ));

  /* Step 4: Copy merged result to caller's output buffer */
  {
    size_t  result_size = fdt_totalsize (result_fdt);

    if (result_size > mb_size) {
      DEBUG ((
        DEBUG_ERROR,
        "[libufdt] ERROR: Result too large: %ld > %ld\n",
        result_size,
        mb_size
        ));
      ret_value = -FDT_ERR_QC_BUF2SMALL;
      goto cleanup;
    }

    CopyMem (merge_blob, result_fdt, result_size);
  }

  /* Final sanity check on output buffer */
  ret_value = fdt_check_header (merge_blob);
  if (ret_value != FDT_ERR_QC_NOERROR) {
    DEBUG ((DEBUG_ERROR, "[libufdt] ERROR: Final output header check failed: %d\n", ret_value));
    ret_value = -FDT_ERR_BADMAGIC;
  }

cleanup:
  /* Step 5: Zero and release all libufdt allocations */
  post_overlay_free ();

  return ret_value;
}
