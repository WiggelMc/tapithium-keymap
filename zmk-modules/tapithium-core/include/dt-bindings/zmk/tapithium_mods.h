/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define TP_ENABLE_CMD 0
#define TP_STICKY_CMD 1
#define TP_CANCEL_CMD 2
#define TP_RESET_CMD 3
#define TP_MPRESS_CMD 4
#define TP_NONE_CMD 5
#define TP_NEXT_CMD 6
#define TP_MOD_CMD 7
#define TP_LAY_CMD 8

/*
Note: Some future commands will include additional parameters, so we
defines these aliases up front.
*/

#define TP_ENABLE TP_ENABLE_CMD 0
#define TP_STICKY TP_STICKY_CMD 0
#define TP_CANCEL TP_CANCEL_CMD 0
#define TP_RESET TP_RESET_CMD 0
#define TP_MPRESS TP_MPRESS_CMD 0
#define TP_NONE TP_NONE_CMD 0
#define TP_NEXT TP_NEXT_CMD 0
#define TP_MOD TP_MOD_CMD
#define TP_LAY TP_LAY_CMD

#define TP_MP TP_MPRESS
#define TP_ TP_NONE
#define TP___ TP_NEXT
