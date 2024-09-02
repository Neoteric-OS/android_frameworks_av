/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_NEON_CAL_H
#define ANDROID_AUDIO_NEON_CAL_H

#include <system/audio.h>

#include <utils/Log.h>
#if defined(__aarch64__) || defined(__ARM_NEON__)
#ifndef USE_NEON
#define USE_NEON (true)
#endif
#else
#define USE_NEON (false)
#endif
#if USE_NEON
#include <arm_neon.h>
#endif
namespace android {

/*
 * mtypeIds is used to record the data type passed into neonCal,
 * assigning it an initial value of AudioMixerBase::FLOAT_FLOAT_FLOAT_MTYPE_IDS:333,
 * representing the three Float types, which are also the most commonly used types.
 */
static int mtypeIds = 333;

//All typies can be calculated by MixMulNeon, but using it need to clarify return type.
template <typename TO, typename TI, typename TV>
TO MixMulNeon(TI value, TV volume);

template <>
inline int32x4_t MixMulNeon<int32x4_t, int16x4_t, int16x4_t>(int16x4_t value, int16x4_t volume) {
    return vmull_s16(volume, value);
}

template <>
inline int32x4_t MixMulNeon<int32x4_t, int32x4_t, int16x4_t>(int32x4_t value, int16x4_t volume) {
    int32x4_t volume32 = vmovl_s16(volume);
    return vmulq_s32(volume32, vshrq_n_s32(value, 12));
}

template <>
inline int32x4_t MixMulNeon<int32x4_t, int16x4_t, int32x4_t>(int16x4_t value, int32x4_t volume) {
    int32x4_t value32 = vmovl_s16(value);
    return vmulq_s32(value32, vshrq_n_s32(volume, 16));
}

template <>
inline int32x4_t MixMulNeon<int32x4_t, int32x4_t, int32x4_t>(int32x4_t value, int32x4_t volume) {
    return vmulq_s32(volume, value);
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, float32x4_t, int16x4_t>(float32x4_t value, int16x4_t volume) {
    float32x4_t volume_float = vcvtq_f32_s32(vmovl_s16(volume));
    static const float32x4_t norm = vdupq_n_f32(1.0f / (1 << 12));
    return vmulq_f32(value, vmulq_f32(volume_float, norm));
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, float32x4_t, int32x4_t>(float32x4_t value, int32x4_t volume) {
    float32x4_t volume_float = vcvtq_f32_s32(volume);
    static const float32x4_t norm = vdupq_n_f32(1.0f / (1 << 28));
    return vmulq_f32(value, vmulq_f32(volume_float, norm));
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, float32x4_t, int16x4_t>(float32x4_t value, int16x4_t volume) {
    float32x4_t volume_float = vcvtq_f32_s32(vmovl_s16(volume));
    float32x4_t result_float = vmulq_f32(value, volume_float);
    int16x4_t result = vqmovn_s32(vcvtq_s32_f32(result_float));
    return result;
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, float32x4_t, int32x4_t>(float32x4_t value, int32x4_t volume) {
    float32x4_t volume_float = vcvtq_f32_s32(volume);
    float32x4_t result_float = vmulq_f32(value, volume_float);
    int16x4_t result = vqmovn_s32(vcvtq_s32_f32(result_float));
    return result;
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, int16x4_t, int16x4_t>(int16x4_t value, int16x4_t volume) {
    static const float norm = 1.0f / (1 << (15 + 12));
    float32x4_t value_f32 = vcvtq_f32_s32(vmovl_s16(value));
    float32x4_t volume_f32 = vcvtq_f32_s32(vmovl_s16(volume));
    float32x4_t result = vmulq_f32(value_f32, volume_f32) * norm;
    return result;
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, int16x4_t, int32x4_t>(int16x4_t value, int32x4_t volume) {
    float32x4_t value_float = vcvtq_f32_s32(vmovl_s16(value));
    float32x4_t volume_float = vcvtq_f32_s32(volume);
    static const float norm = 1.0f / (1ULL << (15 + 28));
    float32x4_t result = vmulq_f32(value_float, volume_float) * norm;
    return result;
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, int16x4_t, int16x4_t>(int16x4_t value, int16x4_t volume) {
    int32x4_t result = vshrq_n_s32(vmulq_s32(vmovl_s16(value), vmovl_s16(volume)), 12);
    return vqmovn_s32(result);
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, int32x4_t, int16x4_t>(int32x4_t value, int16x4_t volume) {
    int32x4_t result = vmulq_s32(value, vmovl_s16(volume));
    return vqshrn_n_s32(result, 12);
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, int16x4_t, int32x4_t>(int16x4_t value, int32x4_t volume) {
    int32x4_t result = vmulq_s32(volume, vmovl_s16(value));
    return vqshrn_n_s32(result, 12);
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, int32x4_t, int32x4_t>(int32x4_t value, int32x4_t volume) {
    return vqshrn_n_s32(vmulq_s32(volume, value), 12);
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, float32x4_t, float32x4_t>(float32x4_t value, float32x4_t volume) {
    return vmulq_f32(volume, value);
}

template <>
inline float32x4_t MixMulNeon<float32x4_t, int16x4_t, float32x4_t>(int16x4_t value, float32x4_t volume) {
    float32x4_t value_float = vcvtq_f32_s32(vmovl_s16(value));
    return vmulq_f32(volume, value_float);
}

template <>
inline int32x4_t MixMulNeon<int32x4_t, int32x4_t, float32x4_t>(int32x4_t value, float32x4_t volume) {
    float32x4_t value_float = vcvtq_f32_s32(value);
    return vcvtq_s32_f32(vmulq_f32(value_float, volume));
}

template <>
inline int32x4_t MixMulNeon<int32x4_t, int16x4_t, float32x4_t>(int16x4_t value, float32x4_t volume) {
    float32x4_t value_float = vcvtq_f32_s32(vmovl_s16(value));
    return vcvtq_s32_f32(vmulq_f32(value_float, volume) * (1 << 12));
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, int16x4_t, float32x4_t>(int16x4_t value, float32x4_t volume) {
    float32x4_t value_float = vcvtq_f32_s32(vmovl_s16(value));
    return vqmovn_s32(vcvtq_s32_f32(vmulq_f32(value_float, volume)));
}

template <>
inline int16x4_t MixMulNeon<int16x4_t, float32x4_t, float32x4_t>(float32x4_t value, float32x4_t volume) {
    return vqmovn_s32(vcvtq_s32_f32(vmulq_f32(value, volume)));
}

template <typename TO, typename TI, typename TV>
void mulAddNeon(TO*& out, TI value, TV volume);

template <>
inline void mulAddNeon<int32_t, int32x4_t, int32x4_t>(int32_t*& out, int32x4_t value, int32x4_t volume){
    vst1q_s32(out, vmlaq_s32(vld1q_s32(out), value, volume));
}

template <>
inline void mulAddNeon<float, float32x4_t, float32x4_t>(float*& out, float32x4_t value, float32x4_t volume){
    vst1q_f32(out, vmlaq_f32(vld1q_f32(out), value, volume));
}

template <>
inline void mulAddNeon<int16_t, int16x4_t, int16x4_t>(int16_t*& out, int16x4_t value, int16x4_t volume){
    vst1_s16(out, vadd_s16((int16x4_t)vld1_s16(out), int16x4_t(MixMulNeon<int16x4_t, int16x4_t, int16x4_t>(value, volume))));
}


enum class TypeChecks {
    MZERO,
    MINT32,
    MINT16,
    MFLOAT
};

//Calculate the new value of typeIds based on the input type
template <typename TO, typename TI, typename TV>
void checkTypeIds(){
    int typeIds = static_cast<int>(TypeChecks::MZERO);

    if (std::is_same<TO, int32_t>::value) {
        typeIds = static_cast<int>(TypeChecks::MINT32) * 100;
    } else if (std::is_same<TO, int16_t>::value) {
        typeIds = static_cast<int>(TypeChecks::MINT16) * 100;
    } else {
        typeIds = static_cast<int>(TypeChecks::MFLOAT) * 100;
    }

    if (std::is_same<TI, int32_t>::value) {
        typeIds += static_cast<int>(TypeChecks::MINT32) * 10;
    } else if (std::is_same<TI, int16_t>::value) {
        typeIds += static_cast<int>(TypeChecks::MINT16) * 10;
    } else {
        typeIds += static_cast<int>(TypeChecks::MFLOAT) * 10;
    }

    if (std::is_same<TV, int32_t>::value) {
        typeIds += static_cast<int>(TypeChecks::MINT32);
    } else if (std::is_same<TV, int16_t>::value) {
        typeIds += static_cast<int>(TypeChecks::MINT16);
    } else {
        typeIds += static_cast<int>(TypeChecks::MFLOAT);
    }
    mtypeIds = typeIds;
}

/*
* The main function of neon computing mainly performs parallel multiplication and addition operations,
* and the results are directly put to out
*/
template <typename TO, typename TI, typename TV>
void mixCalNeon(TO*& out, const TI*& in, TV vol, bool isAdd) {
    constexpr unsigned NEON_BATCH_SIZE = 4;

    /*
    * check mtypeIds, Assign appropriate types
    * Assign appropriate types, if isAdd=true, perform addition operation; otherwise,
    * perform multiplication and addition operation
    * Many neon functions are used inside, please refer to the official website for details
    */
    switch(mtypeIds){
        case 333:
            if(isAdd){
                    mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                        vld1q_f32((float*&)in),
                        vdupq_n_f32(vol));
            }
            else{
                    vst1q_f32((float*&)out,
                        float32x4_t(MixMulNeon<float32x4_t, float32x4_t, float32x4_t>(
                            vld1q_f32((float*&)in),
                            vdupq_n_f32(vol))));
            }
            break;
        case 122:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vmovl_s16(vld1_s16((int16_t*&)in)),
                    vmovl_s16(vdup_n_s16(vol)));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int16x4_t, int16x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 112:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vld1q_s32((int32_t*&)in),
                    vmovl_s16(vdup_n_s16(vol)));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int32x4_t, int16x4_t>(
                        vld1q_s32((int32_t*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 121:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vmovl_s16(vld1_s16((int16_t*&)in)),
                    vdupq_n_s32(vol));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int16x4_t, int32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 111:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vld1q_s32((int32_t*&)in),
                    vdupq_n_s32(vol));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int32x4_t, int32x4_t>(
                        vld1q_s32((int32_t*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 332:
            if(isAdd){
                mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                    vld1q_f32((float*&)in),
                    vcvtq_f32_s32(vmovl_s16(vdup_n_s16(vol))));
            }
            else{
                vst1q_f32((float*&)out,
                    float32x4_t(MixMulNeon<float32x4_t, float32x4_t, int16x4_t>(
                        vld1q_f32((float*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 331:
            if(isAdd){
                mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                    vld1q_f32((float*&)in),
                    vcvtq_f32_s32(vdupq_n_s32(vol)));
            }
            else{
                vst1q_f32((float*&)out,
                    float32x4_t(MixMulNeon<float32x4_t, float32x4_t, int32x4_t>(
                        vld1q_f32((float*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 232:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vqshrn_n_s32(vcvtq_s32_f32(vld1q_f32((float*&)in)), 12),
                    vdup_n_s16(vol));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, float32x4_t, int16x4_t>(
                        vld1q_f32((float*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 231:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vqshrn_n_s32(vcvtq_s32_f32(vld1q_f32((float*&)in)), 12),
                    vqshrn_n_s32(vdupq_n_s32(vol), 12));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, float32x4_t, int32x4_t>(
                        vld1q_f32((float*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 322:
            if(isAdd){
                mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                    vcvtq_f32_s32(vmovl_s16(vld1_s16((int16_t*&)in))),
                    vcvtq_f32_s32(vmovl_s16(vdup_n_s16(vol))));
            }
            else{
                vst1q_f32((float*&)out,
                    float32x4_t(MixMulNeon<float32x4_t, int16x4_t, int16x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 321:
            if(isAdd){
                mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                    vcvtq_f32_s32(vmovl_s16(vld1_s16((int16_t*&)in))),
                    vcvtq_f32_s32(vdupq_n_s32(vol)));
            }
            else{
                vst1q_f32((float*&)out,
                    float32x4_t(MixMulNeon<float32x4_t, int16x4_t, int32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 222:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vld1_s16((int16_t*&)in),
                    vdup_n_s16(vol));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, int16x4_t, int16x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 212:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vqshrn_n_s32(vld1q_s32((int32_t*&)in), 12),
                    vdup_n_s16(vol));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, int32x4_t, int16x4_t>(
                        vld1q_s32((int32_t*&)in),
                        vdup_n_s16(vol))));
            }
            break;
        case 221:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vld1_s16((int16_t*&)in),
                    vqshrn_n_s32(vdupq_n_s32(vol), 12));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, int16x4_t, int32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 211:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vqshrn_n_s32(vld1q_s32((int32_t*&)in), 12),
                    vqshrn_n_s32(vdupq_n_s32(vol), 12));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, int32x4_t, int32x4_t>(
                        vld1q_s32((int32_t*&)in),
                        vdupq_n_s32(vol))));
            }
            break;
        case 323:
            if(isAdd){
                mulAddNeon<float, float32x4_t, float32x4_t>((float*&)out,
                    vcvtq_f32_s32(vmovl_s16(vld1_s16((int16_t*&)in))),
                    vdupq_n_f32(vol));
            }
            else{
                vst1q_f32((float*&)out,
                    float32x4_t(MixMulNeon<float32x4_t, int16x4_t, float32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_f32(vol))));
            }
            break;
        case 113:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vld1q_s32((int32_t*&)in),
                    vcvtq_s32_f32(vdupq_n_f32(vol)));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int32x4_t, float32x4_t>(
                        vld1q_s32((int32_t*&)in),
                        vdupq_n_f32(vol))));
            }
            break;
        case 123:
            if(isAdd){
                mulAddNeon<int32_t, int32x4_t, int32x4_t>((int32_t*&)out,
                    vmovl_s16(vld1_s16((int16_t*&)in)),
                    vcvtq_s32_f32(vdupq_n_f32(vol)));
            }
            else{
                vst1q_s32((int32_t*&)out,
                    int32x4_t(MixMulNeon<int32x4_t, int16x4_t, float32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_f32(vol))));
            }
            break;
        case 223:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vld1_s16((int16_t*&)in),
                    vqshrn_n_s32(vcvtq_s32_f32(vdupq_n_f32(vol)), 12));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, int16x4_t, float32x4_t>(
                        vld1_s16((int16_t*&)in),
                        vdupq_n_f32(vol))));
            }
            break;
        case 233:
            if(isAdd){
                mulAddNeon<int16_t, int16x4_t, int16x4_t>((int16_t*&)out,
                    vqshrn_n_s32(vcvtq_s32_f32(vld1q_f32((float*&)in)), 12),
                    vqshrn_n_s32(vcvtq_s32_f32(vdupq_n_f32(vol)), 12));
            }
            else{
                vst1_s16((int16_t*&)out,
                    int16x4_t(MixMulNeon<int16x4_t, float32x4_t, float32x4_t>(
                        vld1q_f32((float*&)in),
                        vdupq_n_f32(vol))));
            }
            break;
        default:
            break;
    }

    //in and out need to be add
    in += NEON_BATCH_SIZE;
    out += NEON_BATCH_SIZE;
}
//END AudioFlinger_performance_neon
};
#endif /* ANDROID_AUDIO_NEON_CAL_H */
