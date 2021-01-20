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
#pragma once
#include <android-base/macros.h>
#include <android/hardware/broadcastradio/2.0/ICloseHandle.h>
#include <android/hardware/broadcastradio/2.0/IAnnouncementListener.h>

#include <optional>
#include "CloseHandle.h"



namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_0 {
namespace implementation {



struct AnnouncementListener{
      sp<IAnnouncementListener> callback;
      hidl_vec<AnnouncementType> filter;
      wp<CloseHandle> closeHandle;
   };


class TunerAnnouncements {
 
  public:  
  TunerAnnouncements();
  virtual  ~TunerAnnouncements();
  virtual  Return<Result> registerAnnouncementListener(const hidl_vec<AnnouncementType>& enabled,
                                                      const sp<IAnnouncementListener>& listenerCb,
                                                       sp<CloseHandle> *closeHandle);
  virtual Return<Result> unregisterAnnouncementListener(const sp<IAnnouncementListener>& listenerCb);
  virtual Return<void> notifyAnnouncement(const Announcement &annoucement);


  std::mutex mAnnouncementsGuard;
  std::mutex mAnnounListenersGuard;  
  std::vector<AnnouncementListener> mAnnouncementListener;

};


}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android

