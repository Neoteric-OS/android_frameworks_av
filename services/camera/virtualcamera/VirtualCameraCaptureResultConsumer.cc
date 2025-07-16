/*
 * Copyright (C) 2024 The Android Open Source Project
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

 #define LOG_TAG "VirtualCameraCaptureResultConsumer"

 #include "VirtualCameraCaptureResultConsumer.h"

#include "util/AidlUtil.h"
#include "utils/Log.h"

namespace android {
namespace companion {
namespace virtualcamera {

using ::aidl::android::companion::virtualcamera::VirtualCameraMetadata;
using ::aidl::android::hardware::camera::device::CameraMetadata;

 ndk::ScopedAStatus VirtualCameraCaptureResultConsumer::acceptCaptureResult(
     int64_t timestamp,
     const VirtualCameraMetadata& captureResultMetadata) {
    auto metadata = std::make_unique<CameraMetadata>();
    status_t ret =
        convertVirtualToDeviceCameraMetadata(captureResultMetadata, *metadata);
    if (ret != OK) {
      ALOGE("Failed to convert virtual to device CaptureResult!");
      // Return OK to the client, we just log the error and drop the metadata.
      return ndk::ScopedAStatus::ok();
    }

    {
      std::lock_guard<std::mutex> lock(mLock);
      mLastTimestamp = timestamp;
      mLastMetadata = std::move(metadata);
    }

    // TODO: b/371167033 merge the capture result metadata in submitCaptureResult

   return ndk::ScopedAStatus::ok();
 }

 }  // namespace virtualcamera
 }  // namespace companion
 }  // namespace android