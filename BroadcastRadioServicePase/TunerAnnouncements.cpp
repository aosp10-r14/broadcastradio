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
 
#define LOG_TAG "BcRadio.pase.module"
#define LOG_NDEBUG 0

#include <log/log.h>
#include <android-base/logging.h>
#include <cutils/properties.h>
#include "TunerAnnouncements.h"




namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_0 {
namespace implementation {

using std::lock_guard;
using std::map;
using std::mutex;
using std::vector;

template <typename Type>
static uint32_t hidl_vec_push_back2(hidl_vec<Type>* vec, const Type& value) {
    const uint32_t index = vec->size();
    vec->resize(index + 1);
    (*vec)[index] = value;
    return index;
}
  
TunerAnnouncements::TunerAnnouncements(){
        
   char hwId[256] = {0};
   int ret = 0; 
   ALOGV("%s", __func__);
   //annouementt support enbaled or not
   ret = property_get("product.feature.annoucements", hwId, NULL); 
   if(-1 == ret)
    {
     ALOGE("scandebug - unable to get productfeature  annoucements, errno = %d", errno);
    }
   /*initialize empty string*/
   lock_guard<mutex> lk(mAnnouncementsGuard);
   mAnnouncementListener= hidl_vec<AnnouncementListener>({});

}

TunerAnnouncements::~TunerAnnouncements()
{
 //unregister all
}


Return<Result> TunerAnnouncements::registerAnnouncementListener(const hidl_vec<AnnouncementType>& enabled, 
                                                                const sp<IAnnouncementListener>& listenerCb,
                                                                sp<CloseHandle> *pcloseHandle) {

 
 
 if (listenerCb == nullptr) {
       
        return  Result::INVALID_ARGUMENTS;
    }
/*
   TODO: only allow registation when radioservice is active
    if (!mIsUp) {
        _hidl_cb(Result::INTERFACE_DOWN, nullptr);
        return {};
    }
*/
   std::lock_guard<std::mutex> lckListeners(mAnnounListenersGuard);

  sp<CloseHandle> closeHandle = new CloseHandle([this, listenerCb]() {
        std::lock_guard<std::mutex> lck(mAnnounListenersGuard);
    	//std::erase_if(mAnnouncementListener, [&](const auto& e) { return e.callback == listenerCb; });  
       mAnnouncementListener.erase(std::remove_if(mAnnouncementListener.begin(), mAnnouncementListener.end(), [&](const auto& e) { return e.callback == listenerCb; }), mAnnouncementListener.end());
    });

    mAnnouncementListener.emplace_back(AnnouncementListener{listenerCb, enabled, closeHandle});
    *pcloseHandle = closeHandle;
    auto& listener = mAnnouncementListener.back();
  
  return Result::OK;
}

Return<Result> TunerAnnouncements::unregisterAnnouncementListener(const sp<IAnnouncementListener>& listenerCb) {

 std::lock_guard<std::mutex> lck(mAnnounListenersGuard);
 mAnnouncementListener.erase(std::remove_if(mAnnouncementListener.begin(), mAnnouncementListener.end(), [&](const auto& e) { return e.callback == listenerCb; }), mAnnouncementListener.end());

  return Result::OK;
}


Return<void> TunerAnnouncements::notifyAnnouncement(const Announcement &annoucement) {

/*for all listeners*/
 for (auto& it : mAnnouncementListener) {
     /*one listener may be interested multiple types*/
     for (auto& type : it.filter) {
         if (type == annoucement.type) {
         ALOGE("Type matches");
          hidl_vec<Announcement> announcements = hidl_vec<Announcement>({});
          hidl_vec_push_back2(&announcements,annoucement);
          (it.callback)->onListUpdated(announcements);
          }
     }
 }

  return{};

}

}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android
