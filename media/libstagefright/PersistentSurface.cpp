/*
 * Copyright 2025 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "PersistentSurface"

#include <aidl/android/hardware/media/c2/IInputSurface.h>
#include <android/binder_libbinder.h>
#include <android/native_window_aidl.h>
#include <gui/Surface.h>
#include <log/log.h>
#include <media/stagefright/PersistentSurface.h>

namespace android {

namespace c2_aidl = ::aidl::android::hardware::media::c2;

sp<IGraphicBufferProducer> PersistentSurface::getBufferProducer() {
    if (mType != TYPE_INPUTSURFACE) {
        return mBufferProducer;
    }
    std::shared_ptr<c2_aidl::IInputSurface> interface =
        c2_aidl::IInputSurface::fromBinder(mHalInputSurface);
    if (!interface) {
        ALOGE("hal interface does not exist");
        return nullptr;
    }
    ::aidl::android::view::Surface surface;
    ::ndk::ScopedAStatus transStatus = interface->getSurface(&surface);
    if (!transStatus.isOk()) {
        ALOGE("getSurface() failed");
        return nullptr;
    }
    ANativeWindow *window = surface.release();
    if (!window) {
        ALOGE("window does not exist");
        return nullptr;
    }
    return Surface::getIGraphicBufferProducer(window);
}

sp<hidl::base::V1_0::IBase> PersistentSurface::getHidlTarget() const {
    return mType == TYPE_HIDLSOURCE ? mHidlGraphicBufferSource : nullptr;
}

::ndk::SpAIBinder PersistentSurface::getAidlTarget() const {
    return mType == TYPE_AIDLSOURCE ? mAidlGraphicBufferSource : nullptr;
}

::ndk::SpAIBinder PersistentSurface::getHalInputSurface() const {
    return mType == TYPE_INPUTSURFACE ? mHalInputSurface : nullptr;
}

status_t PersistentSurface::writeToParcel(Parcel *parcel) const {
    bool parceled = false;
    bool invalidSurface = false;
    switch (mType) {
        case TYPE_HIDLSOURCE: {
            parcel->writeInt32(TYPE_HIDLSOURCE);
            parcel->writeStrongBinder(IInterface::asBinder(mBufferProducer));
            HalToken token;
            bool result = createHalToken(mHidlGraphicBufferSource, &token);
            parcel->writeBool(result);
            if (result) {
                parcel->writeByteArray(token.size(), token.data());
                parceled = true;
            }
        }
        break;
        case TYPE_AIDLSOURCE: {
            parcel->writeInt32(TYPE_AIDLSOURCE);
            parcel->writeStrongBinder(IInterface::asBinder(mBufferProducer));
            AIBinder *binder = mAidlGraphicBufferSource.get();
            if (binder != nullptr) {
                ::android::sp<::android::IBinder> intf = AIBinder_toPlatformBinder(binder);
                if (intf) {
                    parcel->writeBool(true);
                    parcel->writeStrongBinder(intf);
                    parceled = true;
                    break;
                }
            }
            parcel->writeBool(false);
        }
        break;
        case TYPE_INPUTSURFACE: {
            parcel->writeInt32(TYPE_INPUTSURFACE);
            AIBinder *binder = mHalInputSurface.get();
            if (binder != nullptr) {
                ::android::sp<::android::IBinder> intf = AIBinder_toPlatformBinder(binder);
                if (intf) {
                    parcel->writeBool(true);
                    parcel->writeStrongBinder(intf);
                    parceled = true;
                    break;
                }
            }
            parcel->writeBool(false);
        }
        break;
        default: {
            invalidSurface = true;
            parcel->writeInt32(TYPE_UNKNOWN);
        }
        break;
    }

    if (!parceled) {
        if (invalidSurface) {
            ALOGW("surface type invalid");
        } else {
            ALOGE("binder is not parceled %d", mType);
        }
    }
    return NO_ERROR;
}

status_t PersistentSurface::readFromParcel(const Parcel *parcel) {
    mHidlGraphicBufferSource.clear();
    mAidlGraphicBufferSource.set(nullptr);
    mHalInputSurface.set(nullptr);
    SurfaceType type = static_cast<SurfaceType>(parcel->readInt32());
    switch (type) {
        case TYPE_HIDLSOURCE: {
            mType = TYPE_HIDLSOURCE;
            mBufferProducer = interface_cast<IGraphicBufferProducer>(
                    parcel->readStrongBinder());
            bool haveHidlSource = parcel->readBool();
            if (haveHidlSource) {
                std::vector<uint8_t> tokenVector;
                parcel->readByteVector(&tokenVector);
                HalToken token = HalToken(tokenVector);
                mHidlGraphicBufferSource = retrieveHalInterface(token);
                deleteHalToken(token);
            }
        }
        break;
        case TYPE_AIDLSOURCE: {
            mType = TYPE_AIDLSOURCE;
            mBufferProducer = interface_cast<IGraphicBufferProducer>(
                    parcel->readStrongBinder());
            bool haveAidlSource = parcel->readBool();
            if (haveAidlSource) {
                ::android::sp<::android::IBinder> intf = parcel->readStrongBinder();
                AIBinder *ndkBinder = AIBinder_fromPlatformBinder(intf);
                if (ndkBinder) {
                    mAidlGraphicBufferSource.set(ndkBinder);
                }
            }
        }
        break;
        case TYPE_INPUTSURFACE: {
            mType = TYPE_INPUTSURFACE;
            bool haveHalInputSurface = parcel->readBool();
            if (haveHalInputSurface) {
                ::android::sp<::android::IBinder> intf = parcel->readStrongBinder();
                AIBinder *ndkBinder = AIBinder_fromPlatformBinder(intf);
                if (ndkBinder) {
                    mHalInputSurface.set(ndkBinder);
                }
            }
        }
        break;
        default: {
            mType = TYPE_UNKNOWN;
            ALOGE("A parcel of unknown type was read %d", mType);
        }
        break;
    }
    return NO_ERROR;
}

}  // namespace android
