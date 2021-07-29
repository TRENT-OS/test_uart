/**
 *
 * UART Test Configuration
 *
 * Copyright (C) 2020, HENSOLDT Cyber GmbH
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

#if !defined(Debug_Config_LOG_LEVEL)
#define Debug_Config_LOG_LEVEL              Debug_LOG_LEVEL_DEBUG
#endif
#define Debug_Config_INCLUDE_LEVEL_IN_MSG
#define Debug_Config_LOG_WITH_FILE_LINE


//-----------------------------------------------------------------------------
// Memory
//-----------------------------------------------------------------------------

#define Memory_Config_USE_STDLIB_ALLOC

//-----------------------------------------------------------------------------
// UART
//-----------------------------------------------------------------------------
#define Uart_INPUT_FIFO_DATAPORT_SIZE 4096
