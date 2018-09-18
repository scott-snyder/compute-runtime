/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/os_library.h"

namespace Os {
extern const char *gmmDllName;
extern const char *gmmEntryName;
} // namespace Os

namespace OCLRT {

void GmmHelper::loadLib() {
    gmmLib.reset(OsLibrary::load(Os::gmmDllName));
    bool isLoaded = false;
    UNRECOVERABLE_IF(!gmmLib);
    if (gmmLib->isLoaded()) {
        auto openGmmFunc = reinterpret_cast<decltype(&OpenGmm)>(gmmLib->getProcAddress(Os::gmmEntryName));
        auto status = openGmmFunc(&gmmEntries);
        if (status == GMM_SUCCESS) {
            isLoaded = gmmEntries.pfnCreateClientContext &&
                       gmmEntries.pfnCreateSingletonContext &&
                       gmmEntries.pfnDeleteClientContext &&
                       gmmEntries.pfnDestroySingletonContext;
        }
    }
    UNRECOVERABLE_IF(!isLoaded);
}
} // namespace OCLRT
