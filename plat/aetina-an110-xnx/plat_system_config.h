/*
 *  UART defaults for the platform aetina-an110-xnx (Aetina AN110 Carrier Board + Nvidia Jetson Xavier NX SoM)
 *
 *  Copyright (C) 2022, HENSOLDT Cyber GmbH
 *
*/


// kernel log uses UART_2, so we can use UART_0 for i/o test
#define UART_IO     UART_0