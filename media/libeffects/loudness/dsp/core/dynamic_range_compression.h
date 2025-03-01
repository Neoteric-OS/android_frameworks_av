/*
 * Copyright (C) 2013 The Android Open Source Project
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
#ifndef LE_FX_ENGINE_DSP_CORE_DYNAMIC_RANGE_COMPRESSION_H_
#define LE_FX_ENGINE_DSP_CORE_DYNAMIC_RANGE_COMPRESSION_H_

//#define LOG_NDEBUG 0

#include "common/core/types.h"
#include "common/core/math.h"
#include "dsp/core/basic.h"
#include "dsp/core/interpolation.h"

#include <audio_utils/intrinsic_utils.h>
#include <android/log.h>

namespace le_fx {

// An adaptive dynamic range compression algorithm. The gain adaptation is made
// at the logarithmic domain and it is based on a Branching-Smooth compensated
// digital peak detector with different time constants for attack and release.
class AdaptiveDynamicRangeCompression {
 public:
    AdaptiveDynamicRangeCompression();

    // Initializes the compressor using prior information. It assumes that the
    // input signal is speech from high-quality recordings that is scaled and then
    // fed to the compressor. The compressor is tuned according to the target gain
    // that is expected to be applied.
    //
    // Target gain receives values between 0.0 and 10.0. The knee threshold is
    // reduced as the target gain increases in order to fit the increased range of
    // values.
    //
    // Values between 1.0 and 2.0 will only mildly affect your signal. Higher
    // values will reduce the dynamic range of the signal to the benefit of
    // increased loudness.
    //
    // If nothing is known regarding the input, a `target_gain` of 1.0f is a
    // relatively safe choice for many signals.
    bool Initialize(float target_gain, float sampling_rate);

  // in-place compression.
  void Compress(size_t channelCount,
          float inputAmp, float inverseScale, float* buffer, size_t frameCount);

  // Sets knee threshold (in decibel).
  void set_knee_threshold(float decibel);

  // Sets knee threshold via the target gain using an experimentally derived
  // relationship.
  void set_knee_threshold_via_target_gain(float target_gain);

 private:

  // Templated Compress routine.
  template <typename V>
  void Compress(float inputAmp, float inverseScale, V* buffer, size_t frameCount) {
    for (size_t i = 0; i < frameCount; ++i) {
        auto v = android::audio_utils::intrinsics::vmul(buffer[i], inputAmp);
        const float max_abs_x = android::audio_utils::intrinsics::vmaxv(
                android::audio_utils::intrinsics::vabs(v));
        const float max_abs_x_dB = math::fast_log(std::max(max_abs_x, kMinLogAbsValue));
        // Subtract Threshold from log-encoded input to get the amount of overshoot
        const float overshoot = max_abs_x_dB - knee_threshold_;
        // Hard half-wave rectifier
        const float rect = std::max(overshoot, 0.0f);
        // Multiply rectified overshoot with slope
        const float cv = rect * slope_;
        const float prev_state = state_;
        if (cv <= state_) {
            state_ = alpha_attack_ * state_ + (1.0f - alpha_attack_) * cv;
        } else {
            state_ = alpha_release_ * state_ + (1.0f - alpha_release_) * cv;
        }
        compressor_gain_ *= expf(state_ - prev_state);
        const auto x = android::audio_utils::intrinsics::vmul(v, compressor_gain_);
        v = android::audio_utils::intrinsics::vclamp(x, -kFixedPointLimit, kFixedPointLimit);
        buffer[i] = android::audio_utils::intrinsics::vmul(inverseScale, v);
    }
  }

  // The minimum accepted absolute input value and it's natural logarithm. This
  // is to prevent numerical issues when the input is close to zero
  static const float kMinAbsValue;
  static const float kMinLogAbsValue;
  // Fixed-point arithmetic limits
  static const float kFixedPointLimit;
  static const float kInverseFixedPointLimit;
  // The default knee threshold in decibel. The knee threshold defines when the
  // compressor is actually starting to compress the value of the input samples
  static const float kDefaultKneeThresholdInDecibel;
  // The compression ratio is the reciprocal of the slope of the line segment
  // above the threshold (in the log-domain). The ratio controls the
  // effectiveness of the compression.
  static const float kCompressionRatio;
  // The attack time of the envelope detector
  static const float kTauAttack;
  // The release time of the envelope detector
  static const float kTauRelease;

  float sampling_rate_;
  // the internal state of the envelope detector
  float state_;
  // the latest gain factor that was applied to the input signal
  float compressor_gain_;
  // attack constant for exponential dumping
  float alpha_attack_;
  // release constant for exponential dumping
  float alpha_release_;
  float slope_;
  // The knee threshold
  float knee_threshold_;
  float knee_threshold_in_decibel_;
  // This interpolator provides the function that relates target gain to knee
  // threshold.
  sigmod::InterpolatorLinear<float> target_gain_to_knee_threshold_;

  LE_FX_DISALLOW_COPY_AND_ASSIGN(AdaptiveDynamicRangeCompression);
};

}  // namespace le_fx

#include "dsp/core/dynamic_range_compression-inl.h"

#endif  // LE_FX_ENGINE_DSP_CORE_DYNAMIC_RANGE_COMPRESSION_H_
