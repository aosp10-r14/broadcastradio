
   /*
   * Copyright (C) 2019 The Android Open Source Project
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

#include "TunerAnnouncements.h"
#include "CloseHandle.h"

#define LOG_TAG "BcRadio.pase.closeHandle"
#define LOG_NDEBUG 0

#include <log/log.h>
#include <android-base/logging.h>

extern android::hardware::broadcastradio::V2_0::implementation::TunerAnnouncements *pgObjAnnouncements; 
namespace android::hardware::broadcastradio::V2_0::implementation {

  CloseHandle::CloseHandle(Callback callback) : mCallback(callback) {}

  CloseHandle::~CloseHandle() {
    close();
  }

  Return<void> CloseHandle::close() {
    ALOGE("%s and callback ptr = %p",__func__, &mCallback);
    const auto wasClosed = mIsClosed.exchange(true);
    if (wasClosed) return{};
  
   
    if (mCallback != nullptr){
     //sp<IAnnouncementListener> listenerCb =mCallback;
     //pgObjAnnouncements->unregisterAnnouncementListener(listenerCb); 
     /*need to check how to remove the lister from our list*/
     mCallback();
    }
    return{};
  }

}  // namespace android::hardware::broadcastradio::V2_0::implementation 
