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
#ifndef ANDROID_HARDWARE_BROADCASTRADIO_V2_1_BROADCASTRADIO_H
#define ANDROID_HARDWARE_BROADCASTRADIO_V2_1_BROADCASTRADIO_H

#include "TunerSession.h"
#include <android/hardware/broadcastradio/2.1/IBroadcastRadio.h>
#include <android/hardware/broadcastradio/2.0/types.h>

namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_1 {
namespace implementation {

using V2_0::AnnouncementType;
using V2_0::Properties;
using V2_0::AmFmRegionConfig;



struct BroadcastRadio : public IBroadcastRadio {
    //BroadcastRadio(const VirtualRadio& virtualRadio);
    BroadcastRadio();
    // V2_0::IBroadcastRadio methods
    Return<void> getProperties(getProperties_cb _hidl_cb) override;
    Return<void> getAmFmRegionConfig(bool full, getAmFmRegionConfig_cb _hidl_cb);
    Return<void> getDabRegionConfig(getDabRegionConfig_cb _hidl_cb);
    Return<void> openSession(const sp<V2_0::ITunerCallback>& callback, openSession_cb _hidl_cb) override;
    Return<void> getImage(uint32_t id, getImage_cb _hidl_cb);
    Return<void> registerAnnouncementListener(const hidl_vec<AnnouncementType>& enabled,
                                              const sp<V2_0::IAnnouncementListener>& listener,
                                              registerAnnouncementListener_cb _hidl_cb);
    // V2_1::IBroadcastRadio methods
    Return<void> isAntennaConnected(isAntennaConnected_cb _hidl_cb) override;

    //std::reference_wrapper<const VirtualRadio> mVirtualRadio;
    Properties mProperties;

    V2_0::AmFmRegionConfig getAmFmConfig() const;

   private:
    mutable std::mutex mMut;
    AmFmRegionConfig mAmFmConfig;
    wp<TunerSession> mSession;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_BROADCASTRADIO_V2_0_BROADCASTRADIO_H
