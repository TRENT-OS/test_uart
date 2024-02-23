/*
 * Platform defaults for the SiFive HiFive board.
 *
 * Copyright (C) 2021-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * For commercial licensing, contact: info.cyber@hensoldt.net
 */

// Kernel log uses UART_0, so we can use UART_1 for I/O tests.
#define UART_IO     UART_1
