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
#ifndef ANDROID_HARDWARE_BROADCASTRADIO_V2_1_TUNER_H
#define ANDROID_HARDWARE_BROADCASTRADIO_V2_1_TUNER_H

#include <android/hardware/broadcastradio/2.1/ITunerCallback.h>
#include <android/hardware/broadcastradio/2.1/ITunerSession.h>
#include <broadcastradio-utils/WorkerThread.h>
#include <optional>

#include "tunsdk.h"


// Enables/Disables PASA additions
// e.g. Sending of vendor specific keys related to HD
#define CFG_BCRADIO_PASA_ADDITIONS

// Enables/Disables Combined DAB/FM station list
// If enabled, a combined list of DAB and FM stations is sent to HMI 
// on each update of DAB or FM station list from tuner core.
#define CFG_BCRADIO_COMBINED_STL_DAB_FM


#define CFG_BCRADIO_PASE_VENDOR_KEYS

#define PASE_KEY_DAB_AUDIO_STATUS       "com.panasonic.broadcastradio.dabaudiostatus"


namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_1 {
namespace implementation {

using V2_0::ProgramFilter;
using V2_0::Result;
using V2_0::ProgramSelector;
using V2_0::ConfigFlag;
using V2_0::VendorKeyValue;
using V2_0::ProgramInfo;
using V2_0::AmFmBandRange;

struct BroadcastRadio;

typedef struct _ProgListReqInfo
{
   ProgramFilter filter;
   bool isProgramListRequested;
}ProgramListReqInfo;

struct TunerSession : public ITunerSession {
    TunerSession(BroadcastRadio& module, const sp<V2_0::ITunerCallback>& callback);

    // V2_0::ITunerSession methods
    virtual Return<Result> tune(const ProgramSelector& program) override;
    virtual Return<Result> scan(bool directionUp, bool skipSubChannel) override;
    virtual Return<Result> step(bool directionUp) override;
    virtual Return<void> cancel() override;
    virtual Return<Result> startProgramListUpdates(const ProgramFilter& filter);
    virtual Return<void> stopProgramListUpdates();
    virtual Return<void> isConfigFlagSet(ConfigFlag flag, isConfigFlagSet_cb _hidl_cb);
    virtual Return<Result> setConfigFlag(ConfigFlag flag, bool value);
    virtual Return<void> setParameters(const hidl_vec<VendorKeyValue>& parameters,
                                       setParameters_cb _hidl_cb) override;
    virtual Return<void> getParameters(const hidl_vec<hidl_string>& keys,
                                       getParameters_cb _hidl_cb) override;
    virtual Return<void> close() override;
    virtual Return<void> close2_1() override;
        // V2_1::IBroadcastRadio methods
    Return<V2_1::Result> cancelAnnouncement() override;
    Return<V2_1::Result> setConfiguration(const BandConfig& config) override;
    Return<void> getConfiguration(getConfiguration_cb _hidl_cb) override;
    Return<V2_1::Result> startBackgroundScan() override;


    std::optional<AmFmBandRange> getAmFmRangeLocked() const;

#ifdef CFG_BCRADIO_PASA_ADDITIONS    
    void updateMulticastInfo(Ts_tunsdk_station_Handle* pStation);   
#endif
        
    const sp<V2_0::ITunerCallback> mCallback;
    const sp<V2_1::ITunerCallback> mCallback2_1;
    ProgramListReqInfo progListReqInfo = {};
//    std::vector<VirtualProgram> mProgramsAM;   
//    std::vector<VirtualProgram> mProgramsFM;    
//    std::vector<VirtualProgram> mProgramsDAB;   
    
    ProgramInfo mSelectedProgramInfo = {};
    
    std::vector<ProgramInfo> mProgramListAM;   
    std::vector<ProgramInfo> mProgramListFM;   
    std::vector<ProgramInfo> mProgramListDAB;   
    
    
    // TunerSDK
    uint32_t mMode;
    Te_tunsdk_radioState mRadioState;
    
    bool     mIsDabQualityValid = false;
    uint8_t  mDabQuality = 0;
    bool     mIsFmQualityValid = false;
    uint8_t  mFmQuality = 0;
    
    Te_tunsdk_dab_audio_status mDabAudioStatus = TUNSDK_DAB_AUDIO_INVALID;
    
    bool mCombinedStlDabFm = false;

    
   private:
    std::mutex mMut;
    WorkerThread mThread;
    bool mIsClosed = false; 
    std::reference_wrapper<BroadcastRadio> mModule;
    bool mIsTuneCompleted = false;
    ProgramSelector mCurrentProgram = {};

    
//    void cancelLocked();
    void tuneInternalLocked(const ProgramSelector& sel);
   // const VirtualRadio& virtualRadio() const;
    const BroadcastRadio& module() const;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_BROADCASTRADIO_V2_0_TUNER_H
