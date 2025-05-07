/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef PERSISTENT_SURFACE_H_

#define PERSISTENT_SURFACE_H_

#include <android/binder_auto_utils.h>
#include <binder/Parcel.h>
#include <hidl/HidlSupport.h>
#include <hidl/HybridInterface.h>
#include <gui/IGraphicBufferProducer.h>
#include <media/stagefright/foundation/ABase.h>

namespace android {

struct PersistentSurface : public RefBase {

    enum SurfaceType : int {
        TYPE_UNKNOWN = 0,
        TYPE_HIDLSOURCE,    // android::hardware::media::omx::V1_0::IGraphicBufferSource
        TYPE_AIDLSOURCE,    // aidl::android::media::IAidlGraphicBufferSource
        TYPE_INPUTSURFACE,  // aidl::android::hardware::media::c2::IInputSurface
    };

    PersistentSurface() : mType(TYPE_UNKNOWN) {}

    // create a persistent surface with HIDL IGraphicBufferSource.
    PersistentSurface(
            const sp<IGraphicBufferProducer>& bufferProducer,
            const sp<hidl::base::V1_0::IBase>& hidlSource) :
        mType(TYPE_HIDLSOURCE),
        mBufferProducer(bufferProducer),
        mHidlGraphicBufferSource(hidlSource),
        mAidlGraphicBufferSource(nullptr),
        mHalInputSurface(nullptr) {}

    // create a persistent surface with AIDL IAidlGraphicBufferSource.
    PersistentSurface(
            const sp<IGraphicBufferProducer>& bufferProducer,
            const ::ndk::SpAIBinder& aidlSource) :
        mType(TYPE_AIDLSOURCE),
        mBufferProducer(bufferProducer),
        mHidlGraphicBufferSource(nullptr),
        mAidlGraphicBufferSource(aidlSource),
        mHalInputSurface(nullptr) {}

    // create a persistent surface with Codec2 AIDL IInputSurface.
    PersistentSurface(
            const ::ndk::SpAIBinder& inputSurface) :
        mType(TYPE_INPUTSURFACE),
        mBufferProducer(nullptr),
        mHidlGraphicBufferSource(nullptr),
        mAidlGraphicBufferSource(nullptr),
        mHalInputSurface(inputSurface) {}

    sp<IGraphicBufferProducer> getBufferProducer();

    SurfaceType getType() const {
        return mType;
    }

    sp<hidl::base::V1_0::IBase> getHidlTarget() const;

    ::ndk::SpAIBinder getAidlTarget() const;

    ::ndk::SpAIBinder getHalInputSurface() const;

    status_t writeToParcel(Parcel *parcel) const;

    status_t readFromParcel(const Parcel *parcel);

private:
    SurfaceType mType;

    sp<IGraphicBufferProducer> mBufferProducer;

    // Either one of the below three is valid according to mType.
    sp<hidl::base::V1_0::IBase> mHidlGraphicBufferSource;
    ::ndk::SpAIBinder mAidlGraphicBufferSource;
    ::ndk::SpAIBinder mHalInputSurface;

    DISALLOW_EVIL_CONSTRUCTORS(PersistentSurface);
};

}  // namespace android

#endif  // PERSISTENT_SURFACE_H_
