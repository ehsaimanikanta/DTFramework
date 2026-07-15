/** @file stdlib.h
 *
 * Qualcomm Silicon External package declaration file.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
**/

/**
 *  UEFI stub for open-source libufdt_sysdeps_vendor.c <stdlib.h> include.
 *  malloc/free are disabled via DTO_DISABLE_DEFAULT_VENDOR_LIBC_ALLOCATION.
 *  strtoul is provided via the strtoul macro in LibFdtSupport.h.
 *  This stub satisfies the include without pulling in POSIX headers.
 **/
#pragma once

/* Empty stub - malloc/free disabled by DTO_DISABLE_DEFAULT_VENDOR_LIBC_ALLOCATION
 * strtoul resolved via LibFdtSupport.h macro */
