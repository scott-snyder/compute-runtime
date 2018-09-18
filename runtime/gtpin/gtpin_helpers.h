/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinCreateBuffer(gtpin::context_handle_t context, uint32_t size, gtpin::resource_handle_t *pResource);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinFreeBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinMapBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource, uint8_t **pAddress);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinUnmapBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource);
} // namespace OCLRT
