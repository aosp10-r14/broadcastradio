/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "BcRadio.pase.service"
#define LOG_NDEBUG 0
 
#include <log/log.h>
#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "BroadcastRadio.h"
#include "TunerSdkClient.h"


using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::broadcastradio::V2_1::implementation::BroadcastRadio;

CTUNSDK_BroadcastRadio client;


int main() {
    android::base::SetDefaultTag("BcRadioDef");
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);
    configureRpcThreadpool(4, true);

    ALOGV("BroadcastRadioServicePase - %s", __func__);
    
    TUNSDK_CreateThreadInit(client);
    
    BroadcastRadio broadcastRadio;

    
    auto amFmStatus = broadcastRadio.registerAsService();
    CHECK_EQ(amFmStatus, android::OK)
        << "Failed to register Broadcast Radio AM/FM HAL implementation";

    joinRpcThreadpool();
    return 1;  // joinRpcThreadpool shouldn't exit
}
