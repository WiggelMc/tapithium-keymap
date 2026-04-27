/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#define TP_ENABLE_CMD 0
#define TP_STICKY_CMD 1
#define TP_RESET_CMD 2
#define TP_MOD_CMD 3
#define TP_LAY_CMD 4

/*
Note: Some future commands will include additional parameters, so we
defines these aliases up front.
*/

#define TP_ENABLE TP_ENABLE_CMD 0
#define TP_STICKY TP_STICKY_CMD 0
#define TP_RESET TP_RESET_CMD 0
#define TP_MOD TP_MOD_CMD
#define TP_LAY TP_LAY_CMD