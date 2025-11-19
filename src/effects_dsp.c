#include <effects_dsp.h>

// implementations assumes 1 channel input and output, can be fixed upto 8 using simd

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

void hard_clip(const float* in, float threshold, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    out[n] = fminf(threshold, fmaxf(-threshold, input));
  }
}