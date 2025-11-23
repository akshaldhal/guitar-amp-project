#include <effects_dsp.h>

// implementations assumes 1 channel input and output, can be fixed upto 8 using simd, add loggers in init
// and ONLY DEBUG loggers in process

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
    float limit = 1.0f;
    float temp = fminf(limit, fmaxf(-limit, x_driven));
    float result = (temp - (temp * temp * temp) / 3.0f) * 1.5f;
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

void onepole_init(OnePole* f, DSPState* state, float cutoffHz, int isHighPass) {
  f->state = state;
  f->isHighPass = isHighPass;
  float x = expf(-2.0f * M_PI * cutoffHz / state->sampleRate);
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

void onepole_set_cutoff(OnePole* f, float cutoffHz) {
  float x = expf(-2.0f * M_PI * cutoffHz / f->state->sampleRate);
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

void biquad_init(Biquad* bq, DSPState* state, BiquadType type, float freqHz, float Q, float gainDb) {
  bq->state = state;
  biquad_set_params(bq, type, freqHz, Q, gainDb);
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

void biquad_set_params(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb) {
  float A = powf(10.0f, gainDb / 40.0f);
  float omega = hz_to_omega(freqHz, bq->state->sampleRate);
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

    float coeff = releaseCoeff + (is_rising)*(coeff_diff);

    curr += diff * coeff;
    currentGain[i] = curr;
  }
  *state = curr;
}

void lfo_init(LFO* lfo, DSPState* state, LFOType type, float freqHz, float amp, float dc) {
  lfo->state = state;
  lfo->amp = amp;
  lfo->dc = dc;
  lfo->freq = freqHz;
  lfo->phase = 0.0f;
  lfo->phase_inc = freqHz/state->sampleRate;
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
  float sampleRate = lfo->state->sampleRate;
  lfo->phase_inc = freqHz / sampleRate;
}

void env_init(EnvelopeDetector* ed, DSPState* state, float attackMs, float releaseMs, int isRMS) {
  ed->attackCoeff = ms_to_coeff(attackMs, state->sampleRate);
  ed->releaseCoeff = ms_to_coeff(releaseMs, state->sampleRate);
  ed->isRMS = isRMS;
  ed->state = state;
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

void allpass_delay_init(AllPassDelay* ap, float* bufferMemory, size_t bufferLength,float feedback) {
  ap->buffer = bufferMemory;
  ap->bufferLength = bufferLength;
  ap->index = 0;
  ap->g = clampf(feedback, -0.9999f, 0.9999f);
}

void allpass_delay_process(AllPassDelay* ap, const float* in, float* out, size_t numSamples) {
  float* buffer = ap->buffer;
  size_t D      = ap->bufferLength;
  size_t idx    = ap->index;
  float g       = ap->g;
  for (size_t n = 0; n < numSamples; n++) {

    float x = in[n];
    float buf = buffer[idx];
    float y = -g * x + buf;
    buffer[idx] = x + g * y;
    out[n] = y;
    idx++;
    if (idx >= D) idx = 0;
  }
  ap->index = idx;
}

void delayline_init(DelayLine* dl, DSPState* state, float* bufferMemory, size_t size) {
  dl->state = state;
  dl->buffer = bufferMemory;
  dl->size = size;
  dl->writeIndex = 0;
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



// These compute stuff right now, use pre comuted tables later (IF YOU WANT):
// Also these are kinda AI generated formulas/comments, need a deeper review
// Move to effects rather than dsp core

void dsp_state_init(DSPState* state, float sampleRate, uint32_t numChannels, uint32_t blockSize) {
  if (numChannels > 1) {
    log_message(LOG_LEVEL_ERROR, "Only one channel supported at the moment");
    return;
  }
  state->sampleRate = sampleRate;
  state->blockSize = blockSize;
  state->numChannels = numChannels;
  state->scratchSize = blockSize * sampleRate * 4;
  for (int i = 0; i < NUM_SCRATCH_BUFFERS; i++) {
    state->scratch[i] = (float*)malloc(state->scratchSize * sizeof(float));
    if (!state->scratch[i]) {
      log_message(LOG_LEVEL_ERROR, "Failed to allocate DSP scratch buffer %d", i);
    }
  }
}

void dsp_state_cleanup(DSPState* state) {
  for (int i = 0; i < NUM_SCRATCH_BUFFERS; i++) {
    if (state->scratch[i]) {
      free(state->scratch[i]);
      state->scratch[i] = NULL;
    }
  }
  state->scratchSize = 0;
}

void dsp_state_grow_scratches(DSPState* state, size_t newSize) {
  log_message(LOG_LEVEL_DEBUG, "Buffer regrow called");
  if (newSize > state->scratchSize) {
    state->scratchSize = newSize;
    for (int i = 0; i < NUM_SCRATCH_BUFFERS; i++) {
      state->scratch[i] = (float*)realloc(state->scratch[i], state->scratchSize * sizeof(float));
      if (!state->scratch[i]) {
        log_message(LOG_LEVEL_ERROR, "Failed to reallocate DSP scratch buffer %d", i);
      }
    }
  }
}

/*============================================================================
  TUBE TABLE BUILDERS
============================================================================*/

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

/*============================================================================
  TUBE PREAMP - Uses dedicated scratch buffers
============================================================================*/

void tubepreamp_init(TubePreamp* preamp, DSPState* state, float* wsTable, size_t wsTableSize) {
  preamp->state = state;
  biquad_init(&preamp->inputHighpass, state, BQ_HPF, 20.0f, 0.707f, 0.0f);
  
  preamp->waveshapeTable = wsTable;
  preamp->waveshapeTableSize = wsTableSize;
  preamp->tubeGain = 1.0f;
  
  biquad_init(&preamp->toneStack[0], state, BQ_LOWSHELF, 80.0f, 0.707f, 0.0f);
  biquad_init(&preamp->toneStack[1], state, BQ_PEAK, 500.0f, 1.0f, 0.0f);
  biquad_init(&preamp->toneStack[2], state, BQ_HIGHSHELF, 8000.0f, 0.707f, 0.0f);
  
  preamp->sagAmount = 0.1f;
  preamp->sagTimeConstant = 0.05f;
  preamp->supplyVoltage = 1.0f;
  preamp->supplyFilter = 1.0f;
}

void tubepreamp_process(TubePreamp* preamp, const float* in, float* out, size_t numSamples) {
  if (numSamples > preamp->state->scratchSize) {
    dsp_state_grow_scratches(preamp->state, numSamples * 2);
  }
  
  float* temp = preamp->state->scratch[0];
  
  biquad_process(&preamp->inputHighpass, in, temp, numSamples);

  for (size_t i = 0; i < numSamples; i++) {
    temp[i] *= preamp->tubeGain;
  }
  
  float sag_coeff = ms_to_coeff(preamp->sagTimeConstant * 1000.0f, preamp->state->sampleRate);
  for (size_t i = 0; i < numSamples; i++) {
    float input_level = fabsf(temp[i]);
    float sag_amount = input_level * preamp->sagAmount;
    preamp->supplyFilter += (sag_amount - preamp->supplyFilter) * sag_coeff;
    preamp->supplyVoltage = 1.0f - clampf(preamp->supplyFilter, 0.0f, 0.3f);
    temp[i] *= preamp->supplyVoltage;
  }
  
  if (preamp->waveshapeTable && preamp->waveshapeTableSize > 0) {
    waveshaper_lookup_cubic(temp, temp, preamp->waveshapeTable, 
                            preamp->waveshapeTableSize, numSamples);
  }
  
  biquad_process(&preamp->toneStack[0], temp, temp, numSamples);
  biquad_process(&preamp->toneStack[1], temp, temp, numSamples);
  biquad_process(&preamp->toneStack[2], temp, temp, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = clampf(temp[i], -1.0f, 1.0f);
  }
}

void tubepreamp_set_gain(TubePreamp* preamp, float gainDb) {
  preamp->tubeGain = db_to_linear(clampf(gainDb, -12.0f, 48.0f));
}

/*============================================================================
  COMPRESSOR - Uses dedicated scratch buffers
============================================================================*/

void compressor_init(CompressorState* comp, DSPState* state, float attackMs, float releaseMs) {
  comp->state = state;
  env_init(&comp->detector, state, attackMs, releaseMs, 1);
  comp->ratio = 4.0f;
  comp->threshold = -20.0f;
  comp->makeup = 0.0f;
  comp->kneeWidth = 0.0f;
  comp->previousGain = 1.0f;
}

void compressor_process(CompressorState* comp, const float* in, float* out, size_t numSamples) {
  if (numSamples > comp->state->scratchSize) {
    dsp_state_grow_scratches(comp->state, numSamples * 2);
  }
  
  float* inputDb = comp->state->scratch[0];
  float* gainReduction = comp->state->scratch[1];
  float* smoothedGain = comp->state->scratch[2];
  float* thresholdDb = comp->state->scratch[3];
  
  // Convert input to dB
  for (size_t i = 0; i < numSamples; i++) {
    float level = fabsf(in[i]) + EPSILON_F;
    inputDb[i] = linear_to_db(level);
  }
  
  // Fill threshold array
  for (size_t i = 0; i < numSamples; i++) {
    thresholdDb[i] = comp->threshold;
  }
  
  // Compute gain reduction with soft knee if enabled
  if (comp->kneeWidth > EPSILON_F) {
    float knee_low = comp->threshold - comp->kneeWidth * 0.5f;
    float knee_high = comp->threshold + comp->kneeWidth * 0.5f;
    
    for (size_t i = 0; i < numSamples; i++) {
      float input = inputDb[i];
      
      if (input < knee_low) {
        gainReduction[i] = 0.0f;
      } else if (input > knee_high) {
        float excess = input - comp->threshold;
        gainReduction[i] = excess * (1.0f - 1.0f / comp->ratio);
      } else {
        float knee_t = (input - knee_low) / comp->kneeWidth;
        float knee_t_sq = knee_t * knee_t;
        float soft_ratio = 1.0f + (comp->ratio - 1.0f) * knee_t_sq;
        float excess = input - comp->threshold + comp->kneeWidth * 0.5f;
        gainReduction[i] = excess * (1.0f - 1.0f / soft_ratio);
      }
    }
  } else {
    compute_gain_reduction_db(inputDb, thresholdDb, comp->ratio, gainReduction, numSamples);
  }
  
  // Smooth gain reduction
  float attackCoeff = ms_to_coeff(10.0f, comp->state->sampleRate);
  float releaseCoeff = ms_to_coeff(100.0f, comp->state->sampleRate);
  apply_gain_smoothing(smoothedGain, gainReduction, &comp->previousGain, 
                       attackCoeff, releaseCoeff, numSamples);
  
  // Convert gain reduction back to linear and apply makeup gain
  float makeupLinear = db_to_linear(comp->makeup);
  for (size_t i = 0; i < numSamples; i++) {
    float gainLin = db_to_linear(-smoothedGain[i]) * makeupLinear;
    out[i] = in[i] * gainLin;
  }
}

void compressor_set_params(CompressorState* comp, float threshold, float ratio, float makeup) {
  comp->threshold = clampf(threshold, -60.0f, 0.0f);
  comp->ratio = clampf(ratio, 1.0f, 20.0f);
  comp->makeup = clampf(makeup, -24.0f, 24.0f);
}