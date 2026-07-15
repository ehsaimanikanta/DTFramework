/** @file LibUfdt.h
 *
 * Public API header for the LibUfdt EDK2 library.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
**/

/**
 * Public interface for the LibUfdt EDK2 wrapper library.
 *
 * This header is the single include point for any EDK2 module that needs
 * to use libufdt overlay functionality.  It exposes:
 *
 *   1. struct fdt_entry_node  – generic linked-list node for FDT/DTBO
 *                               address+size pairs, usable by any target.
 *
 *   2. pre_overlay_malloc()   – initialise the libufdt bump-pointer
 *                               allocator before an overlay operation.
 *
 *   3. post_overlay_free()    – release all allocations and print usage
 *                               statistics after an overlay operation.
 *
 *   4. ufdt_apply_multi_overlay() – apply a linked list of DTBO overlays
 *                               to a base DTB in sequence.
 *
 * Placing these declarations here (rather than in a library-internal
 * header) means that any future target or library that links LibUfdt
 * only needs to include <Library/LibUfdt.h> to access the full API.
 *
 * EDK2 include path convention:
 *   LibUfdt.dec adds Include/ to the compiler include path.
 *   This file lives at Include/Library/LibUfdt.h so that consumers
 *   can use the standard EDK2 form:  #include <Library/LibUfdt.h>
 **/

#ifndef LIB_UFDT_H_
#define LIB_UFDT_H_

#include <LibUfdtSupport.h>
#include "libufdt.h"
#include "ufdt_overlay.h"

/// Convenience typedef for the libfdt FDT header structure.
typedef struct fdt_header FDT_HEADER;

/**
 * struct fdt_entry_node - Generic linked-list node for FDT/DTBO blobs.
 *
 * Tracks a single FDT or DTBO blob by its physical address and size.
 * Nodes are chained into a singly-linked list so that a caller can
 * pass an ordered sequence of overlays to ufdt_apply_multi_overlay().
 *
 * This structure is intentionally kept in the LibUfdt public header so
 * that any EDK2 module (DXE driver, library, or SEC module) that uses
 * libufdt overlay operations can share the same definition without
 * pulling in platform-specific driver headers.
 **/
struct fdt_entry_node {
  fdt64_t                address; ///< Physical address of the FDT/DTBO blob.
  fdt64_t                size;    ///< Size of the blob in bytes.
  struct fdt_entry_node  *next;   ///< Next node in the list, or NULL.
};

/**
 * pre_overlay_malloc() - Initialise the libufdt bump-pointer allocator.
 *
 * Must be called once before each overlay operation.  Reads the buffer
 * base address and size from PCDs (PcdLibUfdtBufBase / PcdLibUfdtBufSize),
 * zeroes the entire buffer, and initialises the internal allocator state.
 *
 * The buffer must already be mapped in the MMU (handled by EarlyCacheInit()
 * in the SEC phase, or by the normal DXE memory map in later phases).
 *
 * @retval  Pointer to the buffer base on success.
 * @retval  NULL if PcdLibUfdtBufBase or PcdLibUfdtBufSize is zero
 *          (PCDs not configured for this platform).
 **/
void *
pre_overlay_malloc (
  void
  );

/**
 * post_overlay_free() - Finalise the libufdt allocator after an overlay.
 *
 * Prints memory-usage statistics at DEBUG_INFO level, zeroes the used
 * portion of the buffer to prevent DTB content leakage, and resets the
 * allocator state.  The physical buffer at PcdLibUfdtBufBase is NOT freed
 * (it is a fixed-address region).
 *
 * Must be called after every overlay operation, whether it succeeded or
 * failed, to ensure the allocator is in a clean state for the next call.
 **/
void
post_overlay_free (
  void
  );

/**
 * ufdt_apply_multi_overlay() - Apply a list of DTBO overlays to a base DTB.
 *
 * Iterates the fdt_entry_node singly-linked list and applies each overlay
 * in sequence using ufdt_apply_overlay().  The libufdt bump-pointer
 * allocator must already be initialised by pre_overlay_malloc() before
 * calling this function.
 *
 * Each call to ufdt_apply_overlay() may allocate from the bump-pointer
 * buffer; all allocations are released in bulk by post_overlay_free().
 *
 * @param[in]  MainFdtHdr   Pointer to the base DTB header, as returned by
 *                          ufdt_install_blob().
 * @param[in]  MainFdtSize  Size of the base DTB buffer in bytes.
 * @param[in]  DtsList      Head of the fdt_entry_node linked list.  Each
 *                          node supplies the address and size of one DTBO.
 *
 * @retval  Pointer to the merged FDT (inside the allocator buffer) on success.
 * @retval  NULL if any ufdt_apply_overlay() call fails.
 **/
static inline void *
ufdt_apply_multi_overlay (
  struct fdt_header      *MainFdtHdr,
  size_t                 MainFdtSize,
  struct fdt_entry_node  *DtsList
  )
{
  struct fdt_header      *CurrentFdt  = MainFdtHdr;
  size_t                 CurrentSize  = MainFdtSize;
  struct fdt_entry_node  *Node        = DtsList;

  while (Node != NULL) {
    struct fdt_header  *ResultFdt;

    ResultFdt = ufdt_apply_overlay (
                  CurrentFdt,
                  CurrentSize,
                  (void *)(UINTN)Node->address,
                  (size_t)Node->size
                  );
    if (ResultFdt == NULL) {
      return NULL;
    }

    CurrentFdt  = ResultFdt;
    CurrentSize = (size_t)fdt_totalsize (CurrentFdt);
    Node        = Node->next;
  }

  return (void *)CurrentFdt;
}

#endif /* LIB_UFDT_H_ */