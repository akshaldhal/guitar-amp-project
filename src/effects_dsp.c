#include <effects_dsp.h>

// implementations assumes 1 channel input and output, can be fixed upto 8 using simd

// Stateless, can use SIMD out of box:

void hard_clip(const float* in, float threshold, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    out[n] = fminf(threshold, fmaxf(-threshold, input));
  }
}

void lerp(const float* a, const float* b, const float* t, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = a[n] + t[n] * (b[n] - a[n]);
  }
}

void tanh_clip(const float* in, float drive, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = tanhf(in[n] * drive);
  }
}

void arctan_clip(const float* in, float drive, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = (2.0f / M_PI) * atanf(in[n] * drive);
  }
}

void white_noise(float* out, size_t n) {
  for (size_t i = 0; i < n; i++) {
    out[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
  }
}

float hz_to_omega(float hz, float sampleRate) {
  return 2.0f * M_PI * hz / sampleRate;
}

void apply_window_inplace(float* buffer, const float* window, size_t n) {
  for (size_t i = 0; i < n; i++) {
    buffer[i] *= window[i];
  }
}

void compute_gain_reduction_db(const float* inputDb, const float* thresholdDb, float ratio, float* out, size_t numSamples) {
  float slope = (1.0f / ratio) - 1.0f;
  for (size_t n = 0; n < numSamples; n++) {
    float diff = inputDb[n] - thresholdDb[n];
    float aboveThreshold = fmaxf(0.0f, diff);
    out[n] = slope * aboveThreshold;
  }
}

void build_waveshaper_table(float *lookupTable, size_t tableSize, ClipperType type, float drive) {
  if (tableSize < 2) {
    return;
  }
  for (size_t i = 0; i < tableSize; i++) {
    float x = ((float)i / (float)(tableSize - 1)) * 2.0f - 1.0f;
    float x_driven = x * drive;
    switch (type) {
      case CLIP_HARD:
        lookupTable[i] = fminf(1.0f, fmaxf(-1.0f, x_driven));
        break;
      case CLIP_SOFT_TANH:
        lookupTable[i] = tanhf(x_driven);
        break;
      case CLIP_ARCTAN:
        lookupTable[i] = (2.0f / M_PI) * atanf(x_driven);
        break;
      case CLIP_SIGMOID:
        lookupTable[i] = (2.0f / (1.0f + expf(-x_driven))) - 1.0f;
        break;
      case CLIP_CUBIC_SOFT:
        float limit = 1.0f;
        float temp = fminf(limit, fmaxf(-limit, x_driven));
        float result = (temp - (temp * temp * temp) / 3.0f) * 1.5f;
        lookupTable[i] = fminf(1.0f, fmaxf(-1.0f, result));
        break;
      default:
        lookupTable[i] = x;
        break;
    }
  }
}

void build_hann_window(float* w, size_t n) {
  for (size_t i = 0; i < n; i++) {
    w[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i)/(n - 1)));
  }
}

float ms_to_coeff(float ms, float sampleRate) {
  ms = maxf(ms, 0.001f);
  sampleRate = maxf(sampleRate, 1.0f);
  float alpha = expf(-1.0f / (0.001f * ms * sampleRate));
  return 1.0f - alpha;
}

// Scalar states, can use SIMD for parallel channels i think, think about state management here:

void onepole_init(OnePole* f, float cutoffHz, float sampleRate, int isHighPass) {
  float x = expf(-2.0f * M_PI * cutoffHz / sampleRate);
  if (isHighPass) {
    float scale = (1.0f + x) / 2.0f;
    f->b0 = scale;
    f->b1 = -scale;
    f->a0 = -x;
  } else {
    f->b0 = 1.0f - x;
    f->b1 = 0.0f;
    f->a0 = -x;
  }
  f->z1 = 0.0f;
}

void onepole_process(OnePole* f, const float* in, float* out, size_t numSamples) {
  const float b0 = f->b0;
  const float b1 = f->b1;
  const float a0 = f->a0;
  float z1 = f->z1;
  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    float output = input * b0 + z1;
    z1 = input * b1 - output * a0;
    out[n] = output;
  }
  f->z1 = z1;
  if (fabsf(f->z1) < 1.0e-15f) {
    f->z1 = 0.0f;
  }
}

void biquad_init(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate) {
  float A = powf(10.0f, gainDb / 40.0f);
  float omega = 2.0f * M_PI * freqHz / sampleRate;
  float sinOmega = sinf(omega);
  float cosOmega = cosf(omega);
  float alpha = sinOmega / (2.0f * Q);
  
  float a0, a1, a2, b0, b1, b2;

  switch (type) {
    case BQ_LPF:
      b0 = (1.0f - cosOmega) / 2.0f;
      b1 = 1.0f - cosOmega;
      b2 = (1.0f - cosOmega) / 2.0f;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosOmega;
      a2 = 1.0f - alpha;
      break;
    case BQ_HPF:
      b0 = (1.0f + cosOmega) / 2.0f;
      b1 = -(1.0f + cosOmega);
      b2 = (1.0f + cosOmega) / 2.0f;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosOmega;
      a2 = 1.0f - alpha;
      break;
    case BQ_BPF:
      b0 = alpha;
      b1 = 0.0f;
      b2 = -alpha;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosOmega;
      a2 = 1.0f - alpha;
      break;
    case BQ_NOTCH:
      b0 = 1.0f;
      b1 = -2.0f * cosOmega;
      b2 = 1.0f;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosOmega;
      a2 = 1.0f - alpha;
      break;
    case BQ_PEAK:
      b0 = 1.0f + (alpha * A);
      b1 = -2.0f * cosOmega;
      b2 = 1.0f - (alpha * A);
      a0 = 1.0f + (alpha / A);
      a1 = -2.0f * cosOmega;
      a2 = 1.0f - (alpha / A);
      break;
    case BQ_LOWSHELF:
      b0 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega + 2.0f * sqrtf(A) * alpha);
      b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosOmega);
      b2 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega - 2.0f * sqrtf(A) * alpha);
      a0 = (A + 1.0f) + (A - 1.0f) * cosOmega + 2.0f * sqrtf(A) * alpha;
      a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosOmega);
      a2 = (A + 1.0f) + (A - 1.0f) * cosOmega - 2.0f * sqrtf(A) * alpha;
      break;
    case BQ_HIGHSHELF:
      b0 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega + 2.0f * sqrtf(A) * alpha);
      b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosOmega);
      b2 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega - 2.0f * sqrtf(A) * alpha);
      a0 = (A + 1.0f) - (A - 1.0f) * cosOmega + 2.0f * sqrtf(A) * alpha;
      a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosOmega);
      a2 = (A + 1.0f) - (A - 1.0f) * cosOmega - 2.0f * sqrtf(A) * alpha;
      break;
    default:
      b0 = 1.0f;
      b1 = 0.0f;
      b2 = 0.0f;
      a0 = 1.0f;
      a1 = 0.0f;
      a2 = 0.0f;
      break;
  }

  float invA0 = 1.0f / a0;
  bq->a1 = a1 * invA0;
  bq->a2 = a2 * invA0;
  bq->b0 = b0 * invA0;
  bq->b1 = b1 * invA0;
  bq->b2 = b2 * invA0;

  bq->z1 = 0.0f;
  bq->z2 = 0.0f;
}

void biquad_process(Biquad* bq, const float* in, float* out, size_t numSamples) {
  const float b0 = bq->b0;
  const float b1 = bq->b1;
  const float b2 = bq->b2;
  const float a1 = bq->a1;
  const float a2 = bq->a2;

  float z1 = bq->z1;
  float z2 = bq->z2;

  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    float output = input * b0 + z1;
    z1 = input * b1 - output * a1 + z2;
    z2 = input * b2 - output * a2;
    out[n] = output;
  }

  if (fabsf(z1) < 1.0e-15f) z1 = 0.0f;
  if (fabsf(z2) < 1.0e-15f) z2 = 0.0f;

  bq->z1 = z1;
  bq->z2 = z2;
}

void apply_gain_smoothing(float* currentGain, const float* targetGain, float* state, float attackCoeff, float releaseCoeff, size_t numSamples) {
  float curr = *state;
  float target = 0.0f;
  // using mask: coeff = release + (is_rising)(attack - release)
  float coeff_diff = attackCoeff - releaseCoeff;

  for (size_t i = 0; i < numSamples; i++) {
    target = targetGain[i];
    float diff = target - curr;
    float is_rising = (float)(diff > 0.0f);

    float coeff = releaseCoeff + (is_rising)*(attackCoeff - releaseCoeff);

    curr += diff * coeff;
    currentGain[i] = curr;
  }
  *state = curr;
}

void lfo_init(LFO* lfo, LFOType type, float freqHz, float amp, float dc, float sampleRate) {
  lfo->amp = amp;
  lfo->dc = dc;
  lfo->freq = freqHz;
  lfo->phase = 0.0f;
  lfo->phase_inc = freqHz/sampleRate;
  lfo->sampleRate = sampleRate;
  lfo->type = type;
}

void lfo_process(LFO* lfo, float* out, size_t numSamples) {
  float phase = lfo->phase;
  float phase_inc = lfo->phase_inc;
  float amp = lfo->amp;
  float dc = lfo->dc;
  LFOType type = lfo->type;

  for (size_t i = 0; i < numSamples; i++)
  {
    float sample;

    switch (type) {
      case (LFO_SINE):
        sample = sinf(phase * 2.0f * M_PI);
        break;
      case (LFO_TRI):
        sample = 1.0f - 4.0f * fabsf(phase - 0.5f);
        break;
      case (LFO_SAW):
        sample = 2.0f * phase - 1.0f;
        break;
      case (LFO_SQUARE):
        sample = (phase < 0.5f) ? 1.0f : -1.0f;
        break;
      case (LFO_NOISE):
        sample = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        break;
      default:
        sample = 0.0f;
        break;
    }

    out[i] = (sample * amp) + dc;

    // updating phase is unimportant in case of noise, if this conditional branching outweighs phase update, then phase update should happen anyway
    if (type != LFO_NOISE) {
      phase += phase_inc;
      phase -= floorf(phase);
    }
  }

  lfo->phase = phase;
}

void env_init(EnvelopeDetector* ed, float attackMs, float releaseMs, float sampleRate, int isRMS) {
  implement me
}
void env_process(EnvelopeDetector* ed, const float* in, float* out, size_t numSamples) {
  implement me
}