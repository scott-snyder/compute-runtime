/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListFixture : public DeviceFixture {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        commandList.reset(whitebox_cast(CommandList::create(productFamily, device, false)));

        ze_event_pool_desc_t eventPoolDesc = {
            ZE_EVENT_POOL_DESC_VERSION_CURRENT,
            ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
            2};

        ze_event_desc_t eventDesc = {
            ZE_EVENT_DESC_VERSION_CURRENT,
            0,
            ZE_EVENT_SCOPE_FLAG_NONE,
            ZE_EVENT_SCOPE_FLAG_NONE};

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));
        event = std::unique_ptr<Event>(Event::create(eventPool.get(), &eventDesc, device));
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

} // namespace ult
} // namespace L0
