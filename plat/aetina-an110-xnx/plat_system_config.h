/*
 *  UART defaults for the platform aetina-an110-xnx (Aetina AN110 Carrier Board + Nvidia Jetson Xavier NX SoM)
 *
 *  Copyright (C) 2022-2024, HENSOLDT Cyber GmbH
 * 
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  For commercial licensing, contact: info.cyber@hensoldt.net
 */


// kernel log uses UART_2, so we can use UART_0 for i/o test
#define UART_IO     UART_0