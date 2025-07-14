/*
 * Copyright (C) 2025 The Android Open Source Project
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

 #ifndef ANDROID_COMPANION_VIRTUALCAMERA_VIRTUALCAMERACAPTURERESULTCONSUMER_H
 #define ANDROID_COMPANION_VIRTUALCAMERA_VIRTUALCAMERACAPTURERESULTCONSUMER_H

#include <mutex>

#include "aidl/android/hardware/camera/device/CameraMetadata.h"
#include "aidl/android/companion/virtualcamera/BnCaptureResultConsumer.h"
#include "android-base/thread_annotations.h"

 namespace android {
 namespace companion {
 namespace virtualcamera {

 // Consumer for capture results, filled by the client application and consumed
 // by the VirtualCameraRenderThread.
 class VirtualCameraCaptureResultConsumer
     : public aidl::android::companion::virtualcamera::BnCaptureResultConsumer {
  public:
   ndk::ScopedAStatus acceptCaptureResult(
       int64_t timestamp,
       const aidl::android::companion::virtualcamera::VirtualCameraMetadata&
           captureResultMetadata);

  private:
    std::mutex mLock;
    int64_t mLastTimestamp GUARDED_BY(mLock);
    std::unique_ptr<aidl::android::hardware::camera::device::CameraMetadata>
        mLastMetadata GUARDED_BY(mLock);
 };

 }  // namespace virtualcamera
 }  // namespace companion
 }  // namespace android

 #endif  // ANDROID_COMPANION_VIRTUALCAMERA_VIRTUALCAMERACAPTURERESULTCONSUMER_H