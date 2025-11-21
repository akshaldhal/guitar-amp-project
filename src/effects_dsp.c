#include <effects_dsp.h>

// implementations assumes 1 channel input and output, can be fixed upto 8 using simd

// Stateless:

static inline float lerp_scalar(float a, float b, float t) {
  return a + t * (b - a);
}

static inline float cubic_interp_scalar(float ym1, float y0, float y1, float y2, float t) {
  float a = (-0.5f * ym1) + (1.5f * y0) - (1.5f * y1) + (0.5f * y2);
  float b = (1.0f * ym1) - (2.5f * y0) + (2.0f * y1) - (0.5f * y2);
  float c = (-0.5f * ym1) + (0.5f * y1);
  float d = y0;
  return ((a * t + b) * t + c) * t + d;
}

void lerp(const float* a, const float* b, const float* t, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = lerp_scalar(a[n], b[n], t[n]);
  }
}

void cubic_interp(const float* ym1, const float* y0, const float* y1, const float* y2, const float* t, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = cubic_interp_scalar(ym1[n], y0[n], y1[n], y2[n], t[n]);
  }
}

void crossfade(const float* a, const float* b, const float* t, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    out[n] = lerp_scalar(a[n], b[n], t[n]);
  }
}

void hard_clip(const float* in, float threshold, float* out, size_t numSamples) {
  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    out[n] = fminf(threshold, fmaxf(-threshold, input));
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
  float slope = 1.0f - (1.0f / ratio);
  for (size_t n = 0; n < numSamples; n++) {
    float diff = inputDb[n] - thresholdDb[n];
    float aboveThreshold = fmaxf(0.0f, diff);
    out[n] = -slope * aboveThreshold;
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

void waveshaper_lookup(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples) {
  const float half = 0.5f;
  const float scale = (float)(tableSize - 1) * half;

  for (size_t n = 0; n < numSamples; n++) {
    float x = fminf(1.0f, fmaxf(-1.0f, in[n]));
    float idx = (x + 1.0f) * scale;
    size_t i0 = (size_t)idx;
    size_t i1 = i0 + 1;
    if (i1 >= tableSize) i1 = tableSize - 1;
    float frac = idx - (float)i0;
    float y0 = lookupTable[i0];
    float y1 = lookupTable[i1];
    out[n] = lerp_scalar(y0, y1, frac);
  }
}

void waveshaper_lookup_linear(const float* in, float* out, const float* table, size_t N, size_t nSamples) {
  const float scale = (float)(N - 1) * 0.5f;
  for (size_t i = 0; i < nSamples; i++) {
    float x = in[i] * scale + scale;
    x = (x < 0.0f) ? 0.0f : (x > (float)(N - 1) ? (float)(N - 1) : x);
    size_t idx = (size_t)x;
    float frac = x - (float)idx;
    float y0 = table[idx];
    float y1 = table[(idx + 1 < N) ? idx + 1 : idx];
    out[i] = lerp_scalar(y0, y1, frac);
  }
}

void waveshaper_lookup_cubic(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples) {
  if (tableSize < 4 || lookupTable == NULL) {
    return; 
  }
  const float half_scale = (float)(tableSize - 1) * 0.5f;
  const size_t max_idx = tableSize - 1;
  for (size_t n = 0; n < numSamples; n++) {
    float x = in[n];
    if (x > 1.0f) x = 1.0f;
    else if (x < -1.0f) x = -1.0f;
    float idx_float = (x + 1.0f) * half_scale;
    size_t i0 = (size_t)idx_float;
    float frac = idx_float - (float)i0;
    size_t im1 = (i0 > 0) ? i0 - 1 : 0;
    size_t i1 = (i0 < max_idx) ? i0 + 1 : max_idx;
    size_t i2 = (i0 < max_idx - 1) ? i0 + 2 : max_idx;
    float ym1_val = lookupTable[im1];
    float y0_val  = lookupTable[i0];
    float y1_val  = lookupTable[i1];
    float y2_val  = lookupTable[i2];
    out[n] = cubic_interp_scalar(ym1_val, y0_val, y1_val, y2_val, frac);
  }
}

void build_hann_window(float* w, size_t n) {
  for (size_t i = 0; i < n; i++) {
    w[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i)/(n - 1)));
  }
}

float ms_to_coeff(float ms, float sampleRate) {
  ms = fmaxf(ms, 0.001f);
  sampleRate = fmaxf(sampleRate, 1.0f);
  return 1.0f - expf(-1.0f / (0.001f * ms * sampleRate));
}

// States ahead, beaware:

void onepole_init(OnePole* f, float cutoffHz, float sampleRate, int isHighPass) {
  f->isHighPass = isHighPass;
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

void onepole_set_cutoff(OnePole* f, float cutoffHz, float sampleRate) {
  float x = expf(-2.0f * M_PI * cutoffHz / sampleRate);
  int isHighPass = f->isHighPass;
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
}

void biquad_init(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate) {
  biquad_set_params(bq, type, freqHz, Q, gainDb, sampleRate);
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

void biquad_process_inplace(Biquad* bq, float* buffer, size_t numSamples) {
  biquad_process(bq, buffer, buffer, numSamples);
}

void biquad_set_params(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate){
  float A = powf(10.0f, gainDb / 40.0f);
  float omega = hz_to_omega(freqHz, sampleRate);
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
      if (phase >= 1.0f) phase -= 1.0f;
    }
  }

  lfo->phase = phase;
}

void lfo_set_freq(LFO* lfo, float freqHz) {
  lfo->freq = freqHz;
  float sampleRate = lfo->sampleRate;
  lfo->phase_inc = freqHz / sampleRate;
}

void env_init(EnvelopeDetector* ed, float attackMs, float releaseMs, float sampleRate, int isRMS) {
  ed->attackCoeff = ms_to_coeff(attackMs, sampleRate);
  ed->releaseCoeff = ms_to_coeff(releaseMs, sampleRate);
  ed->isRMS = isRMS;
  ed->sampleRate = sampleRate;
  ed->env = 0.0f;
}

void env_process(EnvelopeDetector* ed, const float* in, float* out, size_t numSamples) {
  float env = ed->env;
  const float attack = ed->attackCoeff;
  const float release = ed->releaseCoeff;
  const int isRMS = ed->isRMS;
  const float coeff_diff = attack - release;

  for (size_t i = 0; i < numSamples; i++)
  {
    float input = in[i];
    float target;
    if (isRMS) {
      target = input * input;
    } else {
      target = fabsf(input);
    }
    float diff = target - env;
    float is_rising = (float)(diff > 0.0f); 
    float coeff = release + (is_rising * coeff_diff);
    env += diff * coeff;
    float output = env;
    if (isRMS) {
      output = sqrtf(output);
    }

    out[i] = output;
  }
  ed->env = env;
}

void allpass1_init(AllPass1* ap, float feedback) {
  ap->g = clampf(feedback, -0.9999f, 0.9999f); 
  ap->x_prev = 0.0f;
  ap->y_prev = 0.0f;
}

void allpass1_process(AllPass1* ap, const float* in, float* out, size_t numSamples) {
  const float g = ap->g; 
  float x_prev = ap->x_prev;
  float y_prev = ap->y_prev;

  for (size_t n = 0; n < numSamples; n++) {
    float input = in[n];
    float output = (-g * input) + x_prev + (g * y_prev);
    x_prev = input;
    y_prev = output;
    
    out[n] = output;
  }
  ap->x_prev = x_prev;
  ap->y_prev = y_prev;
}

void delayline_init(DelayLine* dl, float* bufferMemory, size_t size, float sampleRate) {
  dl->buffer = bufferMemory;
  dl->size = size;

  dl->writeIndex = 0;

  dl->sampleRate = sampleRate;

  if (dl->buffer != NULL && dl->size > 0) {
    memset(dl->buffer, 0, dl->size * sizeof(float));
  }
}

void delayline_write(DelayLine* dl, const float* samples, size_t numSamples) {
  float* buffer = dl->buffer;
  const size_t size = dl->size;
  size_t writeIndex = dl->writeIndex;
  if (size == 0 || buffer == NULL) {
    return;
  }
  for (size_t n = 0; n < numSamples; n++) {
    buffer[writeIndex] = samples[n];
    writeIndex = (writeIndex + 1) % size;
  }
  dl->writeIndex = writeIndex;
}

void delayline_read_linear(DelayLine* dl, float* out, size_t numSamples, float delaySamples) {
  const float* buffer = dl->buffer;
  const size_t size = dl->size;
  const size_t writeIndex = dl->writeIndex;
  if (size == 0 || buffer == NULL || numSamples == 0) {
    return;
  }
  delaySamples = clampf(delaySamples, 0.0f, (float)size - 1.0f);
  float readFIndex = (float)writeIndex - delaySamples;
  while (readFIndex < 0.0f) {
    readFIndex += (float)size;
  }

  for (size_t n = 0; n < numSamples; n++) {
      size_t idx_int = (size_t)readFIndex;
      float frac = readFIndex - (float)idx_int;
      size_t idx_i = idx_int % size; 
      float y0 = buffer[idx_i];
      size_t idx_i_plus_1 = (idx_i + 1) % size;
      float y1 = buffer[idx_i_plus_1];
      out[n] = lerp_scalar(y0, y1, frac);
      readFIndex += 1.0f;
      if (readFIndex >= (float)size) {
        readFIndex -= (float)size;
      }
  }
}

void delayline_read_cubic(DelayLine* dl, float* out, size_t numSamples, float delaySamples) {
  const float* buffer = dl->buffer;
  const size_t size = dl->size;
  const float fsize = (float)size;
  if (size == 0 || buffer == NULL || numSamples == 0) return;
  delaySamples = clampf(delaySamples, 0.0f, fsize - 3.0f);
  float readFIndex = (float)dl->writeIndex - delaySamples;
  while (readFIndex < 0.0f) {
    readFIndex += fsize;
  }

  for (size_t n = 0; n < numSamples; n++) {
    size_t i0 = (size_t)readFIndex;
    float t = readFIndex - (float)i0;
    size_t im1 = (i0 == 0) ? size - 1 : i0 - 1;
    size_t i1 = i0 + 1;
    if (i1 >= size) i1 -= size;
    size_t i2 = i1 + 1;
    if (i2 >= size) i2 -= size;
    float ym1 = buffer[im1];
    float y0  = buffer[i0];
    float y1  = buffer[i1];
    float y2  = buffer[i2];
    out[n] = cubic_interp_scalar(ym1, y0, y1, y2, t);
    readFIndex += 1.0f;
    if (readFIndex >= fsize) {
      readFIndex -= fsize;
    }
  }
}


// Idk if these are SIMD friendly, I didn't pay attention too much, check again, they're probably not:

void resampler_init(ResamplerState* rs, float* historyBuffer, size_t firLen) {
  if (firLen < 2) {
    log_message(LOG_LEVEL_ERROR, "firLen cannot be less than 2, not initializing ResamplerState");
  }
  rs->history = historyBuffer;
  rs->historySize = firLen;
  if (rs->history != NULL && rs->historySize > 0) {
    memset(rs->history, 0, rs->historySize * sizeof(float));
  }
}

void design_resampler_fir(float* fir, size_t numTaps, float gain) {
  if (fir == NULL || numTaps == 0) return;
  const float cutoffNorm = 0.24f; 
  const float center = (float)(numTaps - 1) * 0.5f;
  const float two_pi_cutoff = 2.0f * M_PI * cutoffNorm;
  float sum = 0.0f;
  for (size_t i = 0; i < numTaps; i++) {
    float n = (float)i - center;
    float sinc;
    if (fabsf(n) < 1e-5f) {
      sinc = 1.0f; 
    } else {
      float arg = two_pi_cutoff * n; 
      sinc = sinf(arg) / (M_PI * n);
    }
    float window = blackman_window_scalar((float)i, numTaps);
    fir[i] = sinc * window;
    sum += fir[i];
  }
  if (fabsf(sum) > 1e-9f) {
    float scale = gain / sum;
    for (size_t i = 0; i < numTaps; i++) {
      fir[i] *= scale;
    }
  }
}

void oversample2x_linear(const float* in, float* out, size_t n, float* state) {
  size_t j = 0;
  for (size_t i = 0; i < n - 1; i++) {
    float x0 = in[i];
    float x1 = in[i + 1];
    out[j++] = x0;
    out[j++] = 0.5f * (x0 + x1);
  }
  float last = in[n - 1];
  out[j++] = last;
  out[j++] = last; 
  *state = last;
}

void oversample2x_fir(const float* in, float* out, size_t n, const float* fir, ResamplerState* state) {
  size_t firLen = state->historySize;
  size_t halfLen = firLen / 2;
  float* history = state->history;
  for (size_t i = 0; i < n; i++) {
    float input = in[i];
    memmove(&history[0], &history[1], (halfLen - 1) * sizeof(float));
    history[halfLen - 1] = input;
    float accEven = 0.0f;
    float accOdd  = 0.0f;
    for (size_t k = 0; k < halfLen; k++) {
      float histVal = history[halfLen - 1 - k];
      accEven += histVal * fir[2 * k];
      accOdd  += histVal * fir[2 * k + 1];
    }
    out[2 * i] = accEven;
    out[2 * i + 1] = accOdd;
  }
}

void downsample2x(const float* in, float* out, size_t n) {
  size_t outLen = n / 2;
  for (size_t i = 0; i < outLen; i++) {
    out[i] = in[2 * i];
  }
}

void downsample2x_fir(const float* in, float* out, size_t n, const float* fir, ResamplerState* state) {
  if (n < 2) return;
  float* history = state->history;
  size_t firLen = state->historySize;
  size_t outLen = n / 2;
  for (size_t i = 0; i < outLen; i++) {
    memmove(&history[0], &history[2], (firLen - 2) * sizeof(float));
    history[firLen - 2] = in[2 * i];
    history[firLen - 1] = in[2 * i + 1];
    float acc = 0.0f;
    for (size_t k = 0; k < firLen; k++) {
      acc += history[firLen - 1 - k] * fir[k];
    }
    out[i] = acc;
  }
}

void denormal_fix_inplace(float* buffer, size_t n) {
  const float DENORMAL_THRESHOLD = 1.0e-24f;
  for (size_t i = 0; i < n; i++) {
    buffer[i] *= (fabsf(buffer[i]) >= DENORMAL_THRESHOLD);
  }
}

float blackman_window_scalar(float w, size_t n) {
  float N = (float)n;
  const float alpha = 0.16f;
  const float a0 = (1.0f - alpha) / 2.0f;
  const float a1 = 0.5f;
  const float a2 = alpha / 2.0f;
  return a0 - a1 * cosf((2.0f * M_PI * w) / (N - 1)) + a2 * cosf((4.0f * M_PI * w) / (N - 1));
}

void build_blackman_window(float* w, size_t n) {
  for (size_t i = 0; i < n; i++)
  {
    w[i] = blackman_window_scalar((float)i, n);
  }
}

void normalize_ir(float* ir, size_t n, float targetRMS) {
  if (ir == NULL || n == 0 || targetRMS < 1e-9f) return;
  float sum_sq = 0.0f;
  for (size_t i = 0; i < n; i++) {
    sum_sq += ir[i] * ir[i];
  }
  float currentRMS = sqrtf(sum_sq / (float)n);
  if (currentRMS > 1e-9f) {
    float scale = targetRMS / currentRMS;
    for (size_t i = 0; i < n; i++) {
      ir[i] *= scale;
    }
  }
}

void build_triode_table(float* table, size_t tableSize, const TubeParams* params, float vMin, float vMax) {
  if (table == NULL || tableSize == 0 || params == NULL) return;
  for (size_t i = 0; i < tableSize; i++) {
    float v = vMin + (vMax - vMin) * (float)i / (float)(tableSize - 1);
    float vgs = v - params->biasV;
    float vgs_sq = vgs * vgs;
    float denominator = params->Rp + params->k * (params->mu + 1.0f) * (vgs + sqrtf(vgs_sq + params->a));
    if (fabsf(denominator) > 1e-9f) {
      table[i] = ((params->mu + 1.0f) * vgs) / denominator;
    } else {
      table[i] = 0.0f;
    }
    table[i] = fmaxf(table[i], 0.0f);
  }
}

void build_pentode_table(float* table, size_t tableSize, const TubeParams* params, float vMin, float vMax) {
  if (table == NULL || tableSize == 0 || params == NULL) return;
  for (size_t i = 0; i < tableSize; i++) {
    float v = vMin + (vMax - vMin) * (float)i / (float)(tableSize - 1);
    float vgs = v - params->biasV;
    float vgs_sq = vgs * vgs;
    float denominator = params->Rp + params->k * (params->mu + 1.0f) * (vgs + sqrtf(vgs_sq + params->a));
    float g1_factor = 1.0f + params->Kg1 * vgs;
    if (fabsf(denominator) > 1e-9f) {
      table[i] = (((params->mu + 1.0f) * vgs) / denominator) * g1_factor;
    } else {
      table[i] = 0.0f;
    }
    table[i] = fmaxf(table[i], 0.0f);
  }
}

void build_tube_table_from_koren(float* table, size_t tableSize, TubeType type, const TubeParams* params, float vMin, float vMax) {
  if (table == NULL || tableSize == 0 || params == NULL) return;
  switch (type) {
    case TUBE_TRIODE:
      build_triode_table(table, tableSize, params, vMin, vMax);
      break;
    case TUBE_PENTODE:
      build_pentode_table(table, tableSize, params, vMin, vMax);
      break;
    default:
      break;
  }
}

