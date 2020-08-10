/**
 *
 * UART Test Configuration
 *
 * Copyright (C) 2020, Hensoldt Cyber GmbH
 *
 */
#pragma once


//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

#if !defined(NDEBUG)
#   define Debug_Config_STANDARD_ASSERT
#   define Debug_Config_ASSERT_SELF_PTR
#else
#   define Debug_Config_DISABLE_ASSERT
#   define Debug_Config_NO_ASSERT_SELF_PTR
#endif

#define Debug_Config_LOG_LEVEL              Debug_LOG_LEVEL_DEBUG
#define Debug_Config_INCLUDE_LEVEL_IN_MSG
#define Debug_Config_LOG_WITH_FILE_LINE


//-----------------------------------------------------------------------------
// Memory
//-----------------------------------------------------------------------------

#define Memory_Config_USE_STDLIB_ALLOC


//-----------------------------------------------------------------------------
// UART
//-----------------------------------------------------------------------------

// zynq7000
#define CFG_UART_PHYS_ADDR  0xe0000000
#define CFG_UART_INTR       59

// imx6
// #define CFG_UART_PHYS_ADDR  0x02020000
// #define CFG_UART_INTR       58
