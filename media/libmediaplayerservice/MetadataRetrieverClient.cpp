/*
**
** Copyright (C) 2008 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
// QTI_BEGIN: 2023-06-21: Video: libmediaplayerservice: Enable perfboost during heif decode
**
** Changes from Qualcomm Innovation Center are provided under the following license:
** Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
** SPDX-License-Identifier: BSD-3-Clause-Clear
**
// QTI_END: 2023-06-21: Video: libmediaplayerservice: Enable perfboost during heif decode
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "MetadataRetrieverClient"
#include <utils/Log.h>

// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
#include <inttypes.h>
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <string.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <media/DataSource.h>
#include <media/IMediaHTTPService.h>
#include <media/MediaMetadataRetrieverInterface.h>
#include <media/MediaPlayerInterface.h>
#include <media/stagefright/InterfaceUtils.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/FoundationUtils.h>
#include <private/media/VideoFrame.h>
#include "MetadataRetrieverClient.h"
#include "StagefrightMetadataRetriever.h"
#include "MediaPlayerFactory.h"

namespace android {

MetadataRetrieverClient::MetadataRetrieverClient(pid_t pid)
{
    ALOGV("MetadataRetrieverClient constructor pid(%d)", pid);
    mPid = pid;
// QTI_BEGIN: 2023-06-21: Video: libmediaplayerservice: Enable perfboost during heif decode
    // Trigger Perf boost to support faster HEIF decoding. Set the duration
    // to indefinite. This perflock will be released when MetadataRetrieverClient
    // is released.
    mPerfBoost = std::make_unique<HeifPerfBoost>(true, 0);
// QTI_END: 2023-06-21: Video: libmediaplayerservice: Enable perfboost during heif decode
    mAlbumArt = NULL;
    mRetriever = NULL;
}

MetadataRetrieverClient::~MetadataRetrieverClient()
{
    ALOGV("MetadataRetrieverClient destructor");
    disconnect();
}

status_t MetadataRetrieverClient::dump(int fd, const Vector<String16>& /*args*/)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    result.append(" MetadataRetrieverClient\n");
    snprintf(buffer, 255, "  pid(%d)\n", mPid);
    result.append(buffer);
    write(fd, result.c_str(), result.size());
    write(fd, "\n", 1);
    return NO_ERROR;
}

void MetadataRetrieverClient::disconnect()
{
    ALOGV("disconnect from pid %d", mPid);
    Mutex::Autolock lock(mLock);
    mRetriever.clear();
    mAlbumArt.clear();
    IPCThreadState::self()->flushCommands();
}

static sp<MediaMetadataRetrieverBase> createRetriever(player_type playerType)
{
    sp<MediaMetadataRetrieverBase> p;
    switch (playerType) {
        case STAGEFRIGHT_PLAYER:
        case NU_PLAYER:
        {
            p = new StagefrightMetadataRetriever;
            break;
        }
        default:
            // TODO:
            // support for TEST_PLAYER
            ALOGE("player type %d is not supported",  playerType);
            break;
    }
    if (p == NULL) {
        ALOGE("failed to create a retriever object");
    }
    return p;
}

status_t MetadataRetrieverClient::setDataSource(
        const sp<IMediaHTTPService> &httpService,
        const char *url,
        const KeyedVector<String8, String8> *headers)
{
    ALOGV("setDataSource(%s)", url);
    Mutex::Autolock lock(mLock);
    if (url == NULL) {
        return UNKNOWN_ERROR;
    }

    // When asking the MediaPlayerFactory subsystem to choose a media player for
    // a given URL, a pointer to an outer IMediaPlayer can be passed to the
    // factory system to be taken into consideration along with the URL.  In the
    // case of choosing an instance of a MediaPlayerBase for a
    // MetadataRetrieverClient, there is no outer IMediaPlayer which will
    // eventually encapsulate the result of this selection.  In this case, just
    // pass NULL to getPlayerType to indicate that there is no outer
    // IMediaPlayer to consider during selection.
    player_type playerType =
        MediaPlayerFactory::getPlayerType(NULL /* client */, url);
    ALOGV("player type = %d", playerType);
    sp<MediaMetadataRetrieverBase> p = createRetriever(playerType);
    if (p == NULL) return NO_INIT;
    status_t ret = p->setDataSource(httpService, url, headers);
    if (ret == NO_ERROR) mRetriever = p;
    return ret;
}

status_t MetadataRetrieverClient::setDataSource(int fd, int64_t offset, int64_t length)
{
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    ALOGV("setDataSource fd=%d, offset=%" PRId64 ", length=%" PRId64 "", fd, offset, length);
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    Mutex::Autolock lock(mLock);
    struct stat sb;
    int ret = fstat(fd, &sb);
    if (ret != 0) {
        ALOGE("fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
        return BAD_VALUE;
    }
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    ALOGV("st_dev  = %" PRIu64 "", static_cast<uint64_t>(sb.st_dev));
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    ALOGV("st_mode = %u", sb.st_mode);
    ALOGV("st_uid  = %lu", static_cast<unsigned long>(sb.st_uid));
    ALOGV("st_gid  = %lu", static_cast<unsigned long>(sb.st_gid));
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    ALOGV("st_size = %" PRIu64 "", sb.st_size);
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions

    if (offset >= sb.st_size) {
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
        ALOGE("offset (%" PRId64 ") bigger than file size (%" PRIu64 ")", offset, sb.st_size);
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
        return BAD_VALUE;
    }
    if (offset + length > sb.st_size) {
        length = sb.st_size - offset;
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
        ALOGV("calculated length = %" PRId64 "", length);
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    }

    player_type playerType =
        MediaPlayerFactory::getPlayerType(NULL /* client */,
                                          fd,
                                          offset,
                                          length);
    ALOGV("player type = %d", playerType);
    sp<MediaMetadataRetrieverBase> p = createRetriever(playerType);
    if (p == NULL) {
        return NO_INIT;
    }
    status_t status = p->setDataSource(fd, offset, length);
    if (status == NO_ERROR) mRetriever = p;
    return status;
}

status_t MetadataRetrieverClient::setDataSource(
        const sp<IDataSource>& source, const char *mime)
{
    ALOGV("setDataSource(IDataSource)");
    Mutex::Autolock lock(mLock);

    sp<DataSource> dataSource = CreateDataSourceFromIDataSource(source);
    player_type playerType =
        MediaPlayerFactory::getPlayerType(NULL /* client */, dataSource);
    ALOGV("player type = %d", playerType);
    sp<MediaMetadataRetrieverBase> p = createRetriever(playerType);
    if (p == NULL) return NO_INIT;
    status_t ret = p->setDataSource(dataSource, mime);
    if (ret == NO_ERROR) mRetriever = p;
    return ret;
}

Mutex MetadataRetrieverClient::sLock;

sp<IMemory> MetadataRetrieverClient::getFrameAtTime(
        int64_t timeUs, int option, int colorFormat, bool metaOnly)
{
// QTI_BEGIN: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    ALOGV("getFrameAtTime: time(%" PRId64 "us) option(%d) colorFormat(%d), metaOnly(%d)",
            timeUs, option, colorFormat, metaOnly);
// QTI_END: 2018-01-23: Audio: stagefright: Make classes customizable and add AV extensions
    Mutex::Autolock lock(mLock);
    Mutex::Autolock glock(sLock);
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }
    sp<IMemory> frame = mRetriever->getFrameAtTime(timeUs, option, colorFormat, metaOnly);
    if (frame == NULL) {
        ALOGE("failed to capture a video frame");
        return NULL;
    }
    return frame;
}

sp<IMemory> MetadataRetrieverClient::getImageAtIndex(
        int index, int colorFormat, bool metaOnly, bool thumbnail) {
    ALOGV("getImageAtIndex: index(%d) colorFormat(%d), metaOnly(%d) thumbnail(%d)",
            index, colorFormat, metaOnly, thumbnail);
    Mutex::Autolock lock(mLock);
    Mutex::Autolock glock(sLock);
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }
    sp<IMemory> frame = mRetriever->getImageAtIndex(index, colorFormat, metaOnly, thumbnail);
    if (frame == NULL) {
        ALOGE("failed to extract image");
        return NULL;
    }
    return frame;
}

sp<IMemory> MetadataRetrieverClient::getImageRectAtIndex(
        int index, int colorFormat, int left, int top, int right, int bottom) {
    ALOGV("getImageRectAtIndex: index(%d) colorFormat(%d), rect {%d, %d, %d, %d}",
            index, colorFormat, left, top, right, bottom);
    Mutex::Autolock lock(mLock);
    Mutex::Autolock glock(sLock);
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }
    sp<IMemory> frame = mRetriever->getImageRectAtIndex(
            index, colorFormat, left, top, right, bottom);
    if (frame == NULL) {
        ALOGE("failed to extract image at index %d", index);
    }
    return frame;
}

sp<IMemory> MetadataRetrieverClient::getFrameAtIndex(
            int index, int colorFormat, bool metaOnly) {
    ALOGV("getFrameAtIndex: index(%d), colorFormat(%d), metaOnly(%d)",
            index, colorFormat, metaOnly);
    Mutex::Autolock lock(mLock);
    Mutex::Autolock glock(sLock);
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }

    sp<IMemory> frame = mRetriever->getFrameAtIndex(index, colorFormat, metaOnly);
    if (frame == NULL) {
        ALOGE("failed to extract frame at index %d", index);
    }
    return frame;
}

sp<IMemory> MetadataRetrieverClient::extractAlbumArt()
{
    ALOGV("extractAlbumArt");
    Mutex::Autolock lock(mLock);
    mAlbumArt.clear();
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }
    MediaAlbumArt *albumArt = mRetriever->extractAlbumArt();
    if (albumArt == NULL) {
        ALOGE("failed to extract an album art");
        return NULL;
    }
    size_t size = sizeof(MediaAlbumArt) + albumArt->size();
    sp<MemoryHeapBase> heap = new MemoryHeapBase(size, 0, "MetadataRetrieverClient");
    if (heap == NULL) {
        ALOGE("failed to create MemoryDealer object");
        delete albumArt;
        return NULL;
    }
    mAlbumArt = new MemoryBase(heap, 0, size);
    if (mAlbumArt == NULL) {
        ALOGE("not enough memory for MediaAlbumArt size=%zu", size);
        delete albumArt;
        return NULL;
    }
    MediaAlbumArt::init((MediaAlbumArt *) mAlbumArt->unsecurePointer(),
                        albumArt->size(), albumArt->data());
    delete albumArt;  // We've taken our copy.
    return mAlbumArt;
}

const char* MetadataRetrieverClient::extractMetadata(int keyCode)
{
    ALOGV("extractMetadata");
    Mutex::Autolock lock(mLock);
    if (mRetriever == NULL) {
        ALOGE("retriever is not initialized");
        return NULL;
    }
    return mRetriever->extractMetadata(keyCode);
}

}; // namespace android
