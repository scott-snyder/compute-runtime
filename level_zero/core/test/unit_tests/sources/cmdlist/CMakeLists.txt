#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_barrier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_event_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_signal_event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_wait_on_events.cpp
)