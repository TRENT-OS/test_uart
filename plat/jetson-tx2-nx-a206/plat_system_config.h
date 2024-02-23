/*
 *  UART defaults for the platform jetson-tx2-nx-a206 (SeeedStudio A206 Carrier Board + Nvidia Jetson TX2 NX SoM)
 *
 *  Copyright (C) 2022-2024, HENSOLDT Cyber GmbH
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  For commercial licensing, contact: info.cyber@hensoldt.net
 */


// kernel log uses UART_0, so we can use UART_2 for i/o test
#define UART_IO     UART_2
