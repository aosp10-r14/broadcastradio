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
 
#define LOG_TAG "BcRadio.pase.module.2_1"
#define LOG_NDEBUG 0

#include <log/log.h>
#include <android-base/logging.h>
#include <cutils/properties.h>

#include "BroadcastRadio.h"
#include "TunerSdkClient.h"
#include "resources.h"
#include "TunerAnnouncements.h"

#include <android/hardware/broadcastradio/2.0/types.h>

android::hardware::broadcastradio::V2_0::implementation::TunerAnnouncements ObjAnnouncements;
android::hardware::broadcastradio::V2_0::implementation::TunerAnnouncements *pgObjAnnouncements;
namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_1 {
namespace implementation {

using std::lock_guard;
using std::map;
using std::mutex;
using std::vector;

using V2_0::AmFmRegionConfig;
using V2_0::Deemphasis;
using V2_0::Rds;
using V2_0::IdentifierType;
using V2_0::VendorKeyValue;
using V2_0::AmFmBandRange;
using V2_0::Result;
using V2_0::DabTableEntry;
using V2_0::IAnnouncementListener;
/*using
using
using*/


static const AmFmRegionConfig gDefaultAmFmConfig = {  //
    {
        {87500, 108000, 100, 100},  // FM
        {153, 282, 3, 9},           // AM LW
        {531, 1620, 9, 9},          // AM MW
        {1600, 30000, 1, 5},        // AM SW
    },
    static_cast<uint32_t>(Deemphasis::D50),
    static_cast<uint32_t>(Rds::RDS)};


static Properties initProperties() {

    Properties prop = {};

    prop.maker = "Panasonic";
   // prop.product = virtualRadio.getName();
    prop.product = "Panasonic TunerCore";
    prop.supportedIdentifierTypes = hidl_vec<uint32_t>({
        static_cast<uint32_t>(IdentifierType::AMFM_FREQUENCY),
        static_cast<uint32_t>(IdentifierType::RDS_PI),
        static_cast<uint32_t>(IdentifierType::HD_STATION_ID_EXT),
        static_cast<uint32_t>(IdentifierType::DAB_FREQUENCY),
        static_cast<uint32_t>(IdentifierType::DAB_SID_EXT),
    });
    prop.vendorInfo = hidl_vec<VendorKeyValue>({
        {"com.panasonic.dummy", "dummy"},
    });

    return prop;
}


BroadcastRadio::BroadcastRadio()
      :mProperties(initProperties()),
      mAmFmConfig(gDefaultAmFmConfig) {
        
   char hwId[256] = {0};
   int ret = 0; 
   ALOGV("%s", __func__);
   ret = property_get("product.soc.id", hwId, NULL); 
   if(-1 == ret)
    {
     ALOGE("scandebug - unable to get product id, errno = %d", errno);
    }
    pgObjAnnouncements =&ObjAnnouncements;
 }


Return<void> BroadcastRadio::getProperties(getProperties_cb _hidl_cb) {
   ALOGV("%s", __func__);

   _hidl_cb(mProperties);
   return {};
}


AmFmRegionConfig BroadcastRadio::getAmFmConfig() const {
    ALOGV("%s", __func__);
       
    lock_guard<mutex> lk(mMut);
    return mAmFmConfig;
}


Return<void> BroadcastRadio::getAmFmRegionConfig(bool full, getAmFmRegionConfig_cb _hidl_cb) {
   ALOGV("%s(%d)", __func__, full);   
   if (full) {
        AmFmRegionConfig config = {};
        config.ranges = hidl_vec<AmFmBandRange>({
            {65000, 108000, 10, 0},  // FM
            {150, 30000, 1, 0},      // AM
        });
        config.fmDeemphasis = Deemphasis::D50 | Deemphasis::D75;
        config.fmRds = Rds::RDS | Rds::RBDS;
        _hidl_cb(Result::OK, config);
        return {};
    } else {
        _hidl_cb(Result::OK, getAmFmConfig());
        return {};
    }
}


Return<void> BroadcastRadio::getDabRegionConfig(getDabRegionConfig_cb _hidl_cb) {
     ALOGV("%s", __func__);
    hidl_vec<DabTableEntry> config = {
        {"5A", 174928},  {"7D", 194064},  {"8A", 195936},  {"8B", 197648},  {"9A", 202928},
        {"9B", 204640},  {"9C", 206352},  {"10B", 211648}, {"10C", 213360}, {"10D", 215072},
        {"11A", 216928}, {"11B", 218640}, {"11C", 220352}, {"11D", 222064}, {"12A", 223936},
        {"12B", 225648}, {"12C", 227360}, {"12D", 229072},
    };

    _hidl_cb(Result::OK, config);
    return {};
}


Return<void> BroadcastRadio::openSession(const sp<V2_0::ITunerCallback>& callback,
                                         openSession_cb _hidl_cb) {
    LOG(DEBUG) << "opening new session...";
    ALOGV("%s", __func__);


    lock_guard<mutex> lk(mMut);
    lock_guard<mutex> session_lk(g_session_mutex);
    auto oldSession = mSession.promote();
    if (oldSession != nullptr) {
        LOG(INFO) << "closing previously opened tuner";
        oldSession->close();
        mSession = nullptr;
        g_tunerSession = nullptr;
    }
    /* For the needs of default implementation it's fine to instantiate new session object
     * out of the lock scope. If your implementation needs it, use reentrant lock.
     */
    sp<TunerSession> newSession = new TunerSession(*this, callback);
    mSession = newSession;
    g_tunerSession = newSession;
    _hidl_cb(Result::OK, newSession);
    return {};
}


Return<void> BroadcastRadio::getImage(uint32_t id, getImage_cb _hidl_cb) {

  ALOGV("%s(%x)", __func__, id);
  LOG(DEBUG) << "fetching image " << std::hex << id;
  lock_guard<mutex> lk(mMut);
  uint8_t buffer[HD_IMAGE_SIZE_MAX];
  uint32_t image_size;
  if (id == V2_0::implementation::resources::demoPngId) {
    _hidl_cb(std::vector<uint8_t>(V2_0::implementation::resources::demoPng, std::end(V2_0::implementation::resources::demoPng)));
    return{};
  }
  memset(buffer, 0, sizeof(buffer));
  image_size =0;
  getImageFromId(id, &buffer[0], HD_IMAGE_SIZE_MAX, &image_size);

  if (image_size > 0) {
    LOG(INFO) << "image " << std::hex << id << " length" << image_size;
    _hidl_cb(std::vector<uint8_t>(buffer, &buffer[image_size + 1]));
  }
  else
  {
    LOG(INFO) << "image " << std::hex << id << " doesn't exists";
    _hidl_cb({});
  }


  return{};
}


Return<void> BroadcastRadio::registerAnnouncementListener(
    const hidl_vec<AnnouncementType>& enabled, const sp<IAnnouncementListener>& listenerCb,
    registerAnnouncementListener_cb _hidl_cb) {
     ALOGV("%s(%s)", __func__, toString(enabled).c_str());

    LOG(DEBUG) << "registering announcement listener for " << toString(enabled);

    sp<V2_0::implementation::CloseHandle> closeHandle;
    ObjAnnouncements.registerAnnouncementListener(enabled,listenerCb,&closeHandle);
     
     _hidl_cb(Result::OK, closeHandle);

   
    return {};
}

Return<void> BroadcastRadio::isAntennaConnected(isAntennaConnected_cb _hidl_cb) {
   ALOGV("%s", __func__);
   _hidl_cb(V2_1::Result::OK, false);
   return {};
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android
