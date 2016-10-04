# Copyright (C) 2009 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)


LOCAL_CFLAGS    += -DANDROID
LOCAL_CFLAGS    += -fvisibility=hidden
#LOCAL_CFLAGS    += -std=c99
LOCAL_CPPFLAGS  += -Wno-conversion-null
LOCAL_CPPFLAGS  += -std=c++11

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog

LOCAL_MODULE    := InputEvent
LOCAL_SRC_FILES := android/src/Input.cpp android/src/InputEventJNI.cpp


#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)

