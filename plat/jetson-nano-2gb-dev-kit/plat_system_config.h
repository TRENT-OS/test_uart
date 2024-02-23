/*
 *  UART defaults for the platform jetson-nano-2gb-dev-kit (Nvidia Jetson Nano 2GB Developer Kit)
 *
 *  Copyright (C) 2022-2024, HENSOLDT Cyber GmbH
 * 
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  For commercial licensing, contact: info.cyber@hensoldt.net
 */


// kernel log uses UART_0, so we can use UART_1 for i/o test
#define UART_IO     UART_1