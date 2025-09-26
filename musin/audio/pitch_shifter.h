#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <algorithm>
#include <cstdint>

#include "port/section_macros.h"

#include "block.h"
#include "dspinst.h"
#include "musin/hal/debug_utils.h" // For underrun counter
#include "sample_reader.h"

extern "C" {
#include "hardware/interp.h"
}

// Interpolator strategies
struct CubicInterpolator {
  static constexpr int16_t __time_critical_func(interpolate)(const int16_t y0,
                                                             const int16_t y1,
                                                             const int16_t y2,
                                                             const int16_t y3,
                                                             const float mu) {
    const float mu2 = mu * mu;
    const float mu3 = mu2 * mu;

    // Catmull-Rom cubic interpolation coefficients
    const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float a2 = -0.5f * y0 + 0.5f * y2;
    const float a3 = y1;

    float result = a0 * mu3 + a1 * mu2 + a2 * mu + a3;

    // Clamp to int16_t range
    result = std::clamp(result, -32768.0f, 32767.0f);

    return static_cast<int16_t>(result);
  }
};

struct CubicInterpolatorOptimized {
  static constexpr int16_t __time_critical_func(interpolate)(const int16_t y0,
                                                             const int16_t y1,
                                                             const int16_t y2,
                                                             const int16_t y3,
                                                             const float mu) {
    // Optimized Catmull-Rom using Horner's method to reduce multiplications.
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    float result = ((c3 * mu + c2) * mu + c1) * mu + c0;

    // Clamp to int16_t range
    result = std::clamp(result, -32768.0f, 32767.0f);

    return static_cast<int16_t>(result);
  }
};

struct CubicInterpolatorInt {
  static constexpr int16_t __time_critical_func(interpolate)(const int16_t y0,
                                                             const int16_t y1,
                                                             const int16_t y2,
                                                             const int16_t y3,
                                                             const float mu) {
    // Fixed-point cubic interpolation. Uses 8 fractional bits for mu.
    const int32_t N = 8;
    const int32_t mu_fp = static_cast<int32_t>(mu * (1 << N)); // 0 to 255

    // Pre-calculate coefficients as 32-bit integers
    const int32_t c0 = y1;
    const int32_t c1 = (y2 - y0) / 2;
    const int32_t c2 = y0 - (5 * y1) / 2 + 2 * y2 - y3 / 2;
    const int32_t c3 = (y3 - y0) / 2 + (3 * (y1 - y2)) / 2;

    // Evaluate using Horner's method with fixed-point arithmetic
    int32_t result = (mu_fp * c3) >> N;
    result = (mu_fp * (c2 + result)) >> N;
    result = (mu_fp * (c1 + result)) >> N;
    result += c0;

    return saturate16(result);
  }
};

struct QuadraticInterpolator {
  static constexpr int16_t
  __time_critical_func(interpolate)(const int16_t y0, const int16_t y1,
                                    const int16_t y2, const int16_t /*y3*/,
                                    const float mu) {
    // Quadratic interpolation using y0, y1, y2. y3 is not used.
    const float mu2 = mu * mu;

    // Coefficients for P(mu) = a*mu^2 + b*mu + c
    const float a = 0.5f * (y2 + y0) - y1;
    const float b = 0.5f * (y2 - y0);
    const float c = y1;

    float result = a * mu2 + b * mu + c;

    // Clamp to int16_t range
    result = std::clamp(result, -32768.0f, 32767.0f);

    return static_cast<int16_t>(result);
  }
};

struct QuadraticInterpolatorInt {
  static constexpr int16_t
  __time_critical_func(interpolate)(const int16_t y0, const int16_t y1,
                                    const int16_t y2, const int16_t /*y3*/,
                                    const float mu) {
    // Fixed-point quadratic interpolation. Uses 7 fractional bits for mu
    // to keep intermediate products within 32-bit integers.
    const int32_t N = 7;
    const int32_t mu_fp = static_cast<int32_t>(mu * (1 << N)); // 0 to 127

    const int32_t a =
        static_cast<int32_t>(y0) + y2 - (2 * static_cast<int32_t>(y1));
    const int32_t b = static_cast<int32_t>(y2) - y0;

    const int32_t mu_fp_sq = mu_fp * mu_fp;
    const int32_t term1 = mu_fp_sq * a;
    const int32_t term2 = (mu_fp << N) * b;

    const int32_t interpolated_part = (term1 + term2) >> (2 * N + 1);
    const int32_t result = static_cast<int32_t>(y1) + interpolated_part;

    return saturate16(result);
  }
};

struct NearestNeighborInterpolator {
  static constexpr int16_t
  __time_critical_func(interpolate)(const int16_t /*y0*/, const int16_t y1,
                                    const int16_t y2, const int16_t /*y3*/,
                                    const float mu) {
    // y0 and y3 are not used for nearest neighbor interpolation focusing on y1
    // and y2. mu is the fractional position between y1 and y2.
    if (mu < 0.5f) {
      return y1;
    } else {
      return y2;
    }
  }
};

struct HardwareLinearInterpolator {
  static int16_t __time_critical_func(interpolate)(const int16_t /*y0*/,
                                                   const int16_t y1,
                                                   const int16_t y2,
                                                   const int16_t /*y3*/,
                                                   const float mu) {
    // Ensure the hardware is initialized. This will only run the setup code
    // once.
    initialize_hardware();

    // Convert float fraction (0.0 to 1.0) to 8-bit integer (0 to 255)
    const uint32_t fraction = static_cast<uint32_t>(mu * 255.0f);

    // Load the two samples to interpolate between into BASE0 and BASE1
    interp0->base[0] = y1;
    interp0->base[1] = y2;

    // Load the fraction into ACCUM1. The LSBs are used by the interpolator.
    interp0->accum[1] = fraction;

    // Read the linearly interpolated result from PEEK1
    return static_cast<int16_t>(interp0->peek[1]);
  }

private:
  static void initialize_hardware() {
    // This static local variable ensures the lambda is only executed once.
    static const bool initialized = []() {
      // Configure Lane 0 for blend mode (linear interpolation)
      interp_config cfg = interp_default_config();
      interp_config_set_blend(&cfg, true);
      interp_set_config(interp0, 0, &cfg);

      // Configure Lane 1 for signed values, which is needed for signed audio
      // samples
      cfg = interp_default_config();
      interp_config_set_signed(&cfg, true);
      interp_set_config(interp0, 1, &cfg);

      return true;
    }();
    // The 'initialized' variable is not used, but its initialization
    // triggers the one-time setup code.
    (void)initialized;
  }
};

struct PitchShifter : SampleReader {
  using InterpolateFn = int16_t (*)(const int16_t, const int16_t, const int16_t,
                                    const int16_t, const float);

  PitchShifter(SampleReader &reader)
      : speed(1.0f), sample_reader(reader), m_internal_buffer_read_idx(0),
        m_internal_buffer_valid_samples(0), has_reached_end(false),
        m_interpolate_fn(&QuadraticInterpolator::interpolate) {
    // Initialize interpolation buffer to avoid clicks/pops
    reset();
  }

  // Reader interface
  void reset() override {
    sample_reader.reset();

    // Zero out the interpolation buffer to prevent clicks from stale data.
    for (int i = 0; i < 4; i++) {
      interpolation_samples[i] = 0;
    }

    position = 0.0f;
    buffer_position = 0;
    source_index = 0;
    has_reached_end = false;

    // Reset internal buffer for read_next when resampling
    m_internal_buffer_read_idx = 0;
    m_internal_buffer_valid_samples = 0;

    // Reset the source buffer state
    m_source_buffer_read_idx = 0;
    m_source_buffer_valid_samples = 0;
  }

  // Reader interface
  bool has_data() override {
    if (this->speed > 0.99f && this->speed < 1.01f) {
      return sample_reader.has_data();
    } else {
      // Either we have buffered samples or the source still has data
      // or we're still processing the last few samples in the interpolation
      // buffer
      return (m_internal_buffer_read_idx < m_internal_buffer_valid_samples) ||
             sample_reader.has_data() || !has_reached_end;
    }
  }

  // Reader interface
  bool read_next(int16_t &out) override {
    if (this->speed > 0.99f && this->speed < 1.01f) {
      // Passthrough directly from the source reader
      return sample_reader.read_next(out);
    } else {
      // Resampling path: use internal buffer
      if (m_internal_buffer_read_idx >= m_internal_buffer_valid_samples) {
        // Buffer is exhausted, try to refill it by calling read_resampled
        m_internal_buffer_valid_samples = read_resampled(m_internal_buffer);
        m_internal_buffer_read_idx =
            0; // Reset read index for the new buffer content

        if (m_internal_buffer_valid_samples == 0) {
          // No more samples could be generated by read_resampled
          out = 0; // Provide a default silence value
          return false;
        }
      }
      // Provide the next sample from the internal buffer
      out = m_internal_buffer[m_internal_buffer_read_idx++];
      return true;
    }
  }

  // Reader interface
  uint32_t read_samples(AudioBlock &out) override {
    // Fast path for essentially unmodified speed
    if (this->speed > 0.99f && this->speed < 1.01f) {
      return sample_reader.read_samples(out);
    } else {
      return read_resampled(out);
    }
  }

  constexpr void set_speed(const float new_speed) {
    // Clamp speed to valid range
    if (new_speed < 0.2f) {
      this->speed = 0.2f;
    } else if (new_speed > 2.0f) {
      this->speed = 2.0f;
    } else {
      this->speed = new_speed;
    }

    m_interpolate_fn = &QuadraticInterpolator::interpolate;
  }

  constexpr void set_interpolator(InterpolateFn fn) {
    m_interpolate_fn = fn;
  }

private:
  bool __time_critical_func(get_next_source_sample)(int16_t &out_sample) {
    // If our local source buffer is exhausted, refill it by reading a full
    // block.
    if (m_source_buffer_read_idx >= m_source_buffer_valid_samples) {
      m_source_buffer_valid_samples =
          sample_reader.read_samples(m_source_buffer);
      m_source_buffer_read_idx = 0;

      if (m_source_buffer_valid_samples == 0) {
        // The underlying reader has no more data.
        return false;
      }
    }

    // Serve the next sample from our local buffer.
    out_sample = m_source_buffer[m_source_buffer_read_idx++];
    return true;
  }

  uint32_t __time_critical_func(read_resampled)(AudioBlock &out) {
    int16_t sample = 0;
    uint32_t samples_generated = 0;
    float current_position = position;

    // Process each output sample
    for (uint32_t out_sample_index = 0; out_sample_index < out.size();
         ++out_sample_index) {
      // Get the integer and fractional parts of the current position
      int new_buffer_position = static_cast<int>(current_position);
      float mu = current_position - static_cast<float>(new_buffer_position);

      // Ensure we have enough samples in the buffer for interpolation.
      // For Catmull-Rom, we need samples at n-1, n, n+1, and n+2 to interpolate
      // at position n. So we need to have read up to sample n+2.
      bool has_more_data = true;
      while (source_index <= static_cast<uint32_t>(new_buffer_position + 2) &&
             has_more_data) {
        bool is_first_sample_ever = (source_index == 0);
        has_more_data = get_next_source_sample(sample);

        if (is_first_sample_ever && has_more_data) {
          // This is the first sample. Prime the entire interpolation buffer
          // with it to provide a stable history for interpolation at the very
          // beginning.
          for (int i = 0; i < 4; i++) {
            interpolation_samples[i] = sample;
          }
        } else {
          if (!has_more_data) {
            // Reached the end of input data
            has_reached_end = true;
            // Don't immediately exit - we can still use the samples in the
            // buffer Just pad with zeros if needed
            sample = 0;
          }
          shift_interpolation_samples(sample);
        }
        source_index++;
      }

      // Calculate interpolated value even if we've reached the end of the
      // source data This allows us to use the remaining samples in the
      // interpolation buffer

      // The interpolation buffer acts as a shift register. The while loop above
      // ensures it holds the four samples needed for interpolation around the
      // current position.
      // y0, y1, y2, y3 correspond to samples at indices n-1, n, n+1, and n+2,
      // where n is the integer part of the current sample position.
      const int16_t y0 = interpolation_samples[0];
      const int16_t y1 = interpolation_samples[1];
      const int16_t y2 = interpolation_samples[2];
      const int16_t y3 = interpolation_samples[3];

      // Calculate interpolated value
      const int16_t interpolated_value = m_interpolate_fn(y0, y1, y2, y3, mu);

      // Write the interpolated sample to output
      out[out_sample_index] = interpolated_value;
      samples_generated++;

      // Advance position based on playback speed
      current_position += this->speed;

      // If we've moved past the available data and we've already reached the
      // end, start fading out the sound to avoid abrupt stopping
      if (has_reached_end &&
          static_cast<uint32_t>(new_buffer_position) > source_index) {
        // Only continue for a few more samples to provide a smooth tail-off
        int samples_beyond_end =
            new_buffer_position -
            static_cast<int>(
                source_index);        // Keep consistent types for subtraction
        if (samples_beyond_end > 8) { // Arbitrary cutoff for tail fade-out
          // Fill the rest of the output buffer with silence
          for (uint32_t i = out_sample_index + 1; i < out.size(); ++i) {
            out[i] = 0;
          }
          samples_generated = out_sample_index + 1;
          break; // Exit the main sample generation loop
        }
      }
    }

    if (samples_generated < out.size() && sample_reader.has_data()) {
      // If we didn't fill the block but the underlying reader still has data,
      // it's a pitch shifter specific underrun (couldn't process fast enough
      // or logic error in resampling).
      musin::hal::DebugUtils::g_pitch_shifter_underruns++;
    }

    // Save position for next call
    position = current_position;
    buffer_position = static_cast<int>(position);

    return samples_generated;
  }

  void __time_critical_func(shift_interpolation_samples)(const int16_t sample) {
    // Shift samples in the interpolation buffer. This acts as a shift register.
    interpolation_samples[0] = interpolation_samples[1];
    interpolation_samples[1] = interpolation_samples[2];
    interpolation_samples[2] = interpolation_samples[3];
    interpolation_samples[3] = sample;
  }

  // Members are ordered to match the constructor's member initializer list
  // to prevent -Wreorder warnings.

  // Using float instead of double for better performance on Cortex-M33 FPU
  float speed;
  SampleReader &sample_reader;
  uint32_t m_internal_buffer_read_idx;
  uint32_t m_internal_buffer_valid_samples;
  bool has_reached_end; // Tracks if we've reached the end of source data
  InterpolateFn m_interpolate_fn;

  // Other members (initialized in reset() or by default constructor)
  // Align interpolation buffer to word boundary for better memory access
  alignas(4) int16_t interpolation_samples[4];
  uint32_t source_index;
  float position;
  int buffer_position;
  // Internal buffer for read_next when resampling
  AudioBlock m_internal_buffer;

  // Buffer for reading blocks from the source reader
  AudioBlock m_source_buffer;
  uint32_t m_source_buffer_read_idx = 0;
  uint32_t m_source_buffer_valid_samples = 0;
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
