#
# Test UART driver server
#
# Copyright (C) 2019-2020, HENSOLDT Cyber GmbH
#

cmake_minimum_required(VERSION 3.7.2)

os_set_config_file(system_config.h CREATE_CONFIG_PROJECT)


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


#ROOT SERVER
DeclareAndCreateCamkESSystem(main.camkes)
GenerateSimulateScript()
