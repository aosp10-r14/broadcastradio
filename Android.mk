#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

ifneq ($(TARGET_BUILD_PDK), true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src) $(call all-Iaidl-files-under, src)
LOCAL_AIDL_INCLUDES := $(call all-Iaidl-files-under, src)

LOCAL_PACKAGE_NAME := CarRadioApp
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_CERTIFICATE := platform

LOCAL_MODULE_TAGS := optional

LOCAL_PRIVILEGED_MODULE := true

LOCAL_USE_AAPT2 := true

LOCAL_STATIC_ANDROID_LIBRARIES += \
    $(ANDROID_SUPPORT_CAR_TARGETS)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

# Include support-v7-appcompat, if not already included
ifeq (,$(findstring android-support-v7-appcompat,$(LOCAL_STATIC_ANDROID_LIBRARIES)))
LOCAL_STATIC_ANDROID_LIBRARIES += android-support-v7-appcompat
endif

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_DEX_PREOPT := false

include packages/apps/Car/libs/car-stream-ui-lib/car-stream-ui-lib.mk
include packages/apps/Car/libs/car-apps-common/car-apps-common.mk
include packages/services/Car/car-support-lib/car-support.mk

include $(BUILD_PACKAGE)

endif