/*
 * UART platform zynq7000
 *
 * Copyright (C) 2020-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * For commercial licensing, contact: info.cyber@hensoldt.net
 */

// kernel log uses UART_1, so we can use UART_0 for i/o test
#define UART_IO     UART_0
