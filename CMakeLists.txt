#
# Test UART driver server
#
# Copyright (C) 2019-2024, HENSOLDT Cyber GmbH
# 
# SPDX-License-Identifier: GPL-2.0-or-later
#
# For commercial licensing, contact: info.cyber@hensoldt.net
#

cmake_minimum_required(VERSION 3.7.2)

#-------------------------------------------------------------------------------
# use the SDK
find_package("os-sdk" REQUIRED)
os_sdk_set_defaults()
os_sdk_setup(CONFIG_FILE "system_config.h" CONFIG_PROJECT "system_config")
CAmkESAddCPPInclude("plat/${PLATFORM}")

#-------------------------------------------------------------------------------
project(tests_uart C)

DeclareCAmkESComponent(
    UART_tester
    SOURCES
        uart_tester.c
    C_FLAGS
        -Wall
        -Werror
    LIBS
        system_config
        os_core_api
        lib_debug
        lib_io
)

DeclareCAmkESComponents_for_UARTs()

if(("${KernelPlatform}" STREQUAL "tx1") OR ("${KernelPlatform}" STREQUAL "tx2") OR ("${KernelPlatform}" STREQUAL "xavier"))
    SystemController_DeclareCAmkESComponent(
        SystemController
    )
endif()

#ROOT SERVER
os_sdk_create_CAmkES_system("main.camkes")
