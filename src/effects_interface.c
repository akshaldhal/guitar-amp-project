#include "effects_interface.h"

void noisegate_init(NoiseGate* ng, float thresholdDb, float attackMs, float releaseMs, float holdMs, float sampleRate) {
  memset(ng, 0, sizeof(NoiseGate));
  
  ng->threshold = thresholdDb;
  ng->holdSamples = (holdMs / 1000.0f) * sampleRate;
  ng->holdCounter = 0.0f;
  ng->attenuation = 0.0f;
  ng->enabled = true;
  env_init(&ng->detector, attackMs, releaseMs, sampleRate, 1);
}

void noisegate_process(NoiseGate* ng, const float* in, float* out, size_t numSamples) {
  if (!ng->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  float* envBuffer = (float*)malloc(numSamples * sizeof(float));
  if (!envBuffer) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  env_process(&ng->detector, in, envBuffer, numSamples);
  for (size_t i = 0; i < numSamples; i++) {
    float levelDb = linear_to_db(envBuffer[i]);
    if (levelDb > ng->threshold) {
      ng->attenuation = 1.0f;
      ng->holdCounter = ng->holdSamples;
    } else {
      if (ng->holdCounter > 0.0f) {
        ng->holdCounter -= 1.0f;
        ng->attenuation = 1.0f;
      } else {
        ng->attenuation = 0.0f;
      }
    }
    out[i] = in[i] * ng->attenuation;
  }
  
  free(envBuffer);
}

void noisegate_set_threshold(NoiseGate* ng, float thresholdDb) {
  ng->threshold = clampf(thresholdDb, -100.0f, 0.0f);
}

void noisegate_set_enabled(NoiseGate* ng, bool enabled) {
  ng->enabled = enabled;
}







void overdrive_init(Overdrive* od, float* wsTable, size_t wsTableSize, float sampleRate) {
  memset(od, 0, sizeof(Overdrive));
  
  od->waveshapeTable = wsTable;
  od->waveshapeTableSize = wsTableSize;
  od->drive = 0.0f;
  od->inputGain = 1.0f;
  od->outputGain = 1.0f;
  od->enabled = true;
  biquad_init(&od->toneFilter, BQ_PEAK, 3500.0f, 0.7f, 0.0f, sampleRate);
}

void overdrive_process(Overdrive* od, const float* in, float* out, size_t numSamples) {
  if (!od->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* gainStage = (float*)malloc(numSamples * sizeof(float));
  float* shaped = (float*)malloc(numSamples * sizeof(float));
  
  if (!gainStage || !shaped) {
    memcpy(out, in, numSamples * sizeof(float));
    if (gainStage) free(gainStage);
    if (shaped) free(shaped);
    return;
  }
  
  for (size_t i = 0; i < numSamples; i++) {
    gainStage[i] = in[i] * od->inputGain;
  }
  
  waveshaper_lookup_cubic(gainStage, shaped, od->waveshapeTable, od->waveshapeTableSize, numSamples);
  
  biquad_process(&od->toneFilter, shaped, shaped, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = shaped[i] * od->outputGain;
  }
  
  free(gainStage);
  free(shaped);
}

void overdrive_set_params(Overdrive* od, float driveDb, float toneFreq, float outputDb) {
  od->inputGain = db_to_linear(clampf(driveDb, -12.0f, 24.0f));
  float clampedFreq = clampf(toneFreq, 1000.0f, 10000.0f);
  biquad_set_params(&od->toneFilter, BQ_PEAK, clampedFreq, 0.7f, 0.0f, 44100.0f);
  od->outputGain = db_to_linear(clampf(outputDb, -12.0f, 12.0f));
}

void overdrive_set_enabled(Overdrive* od, bool enabled) {
  od->enabled = enabled;
}







void distortion_init(Distortion* dist, float* wsTable, size_t wsTableSize, float sampleRate) {
  memset(dist, 0, sizeof(Distortion));
  dist->waveshapeTable = wsTable;
  dist->waveshapeTableSize = wsTableSize;
  dist->drive = 1.0f;
  dist->outputGain = 1.0f;
  dist->enabled = true;
  biquad_init(&dist->lowcut, BQ_HPF, 80.0f, 0.707f, 0.0f, sampleRate);
  biquad_init(&dist->highcut, BQ_LPF, 8000.0f, 0.707f, 0.0f, sampleRate);
}

void distortion_process(Distortion* dist, const float* in, float* out, size_t numSamples) {
  if (!dist->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  float* filtered = (float*)malloc(numSamples * sizeof(float));
  float* shaped = (float*)malloc(numSamples * sizeof(float));
  
  if (!filtered || !shaped) {
    memcpy(out, in, numSamples * sizeof(float));
    if (filtered) free(filtered);
    if (shaped) free(shaped);
    return;
  }
  biquad_process(&dist->lowcut, in, filtered, numSamples);
  for (size_t i = 0; i < numSamples; i++) {
    filtered[i] = filtered[i] * dist->drive;
  }
  waveshaper_lookup_cubic(filtered, shaped, dist->waveshapeTable, dist->waveshapeTableSize, numSamples);
  biquad_process(&dist->highcut, shaped, shaped, numSamples);
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = shaped[i] * dist->outputGain;
  }
  
  free(filtered);
  free(shaped);
}

void distortion_set_params(Distortion* dist, float driveDb, float outputDb) {
  dist->drive = db_to_linear(clampf(driveDb, 0.0f, 48.0f));
  dist->outputGain = db_to_linear(clampf(outputDb, -18.0f, 6.0f));
}

void distortion_set_enabled(Distortion* dist, bool enabled) {
  dist->enabled = enabled;
}







void fuzz_init(Fuzz* fz, float* wsTable, size_t wsTableSize, float sampleRate) {
  memset(fz, 0, sizeof(Fuzz));
  
  fz->waveshapeTable = wsTable;
  fz->waveshapeTableSize = wsTableSize;
  fz->fuzz = 1.0f;
  fz->bias = 0.0f;
  fz->outputGain = 1.0f;
  fz->enabled = true;
}

void fuzz_process(Fuzz* fz, const float* in, float* out, size_t numSamples) {
  if (!fz->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  float* biased = (float*)malloc(numSamples * sizeof(float));
  float* shaped = (float*)malloc(numSamples * sizeof(float));
  
  if (!biased || !shaped) {
    memcpy(out, in, numSamples * sizeof(float));
    if (biased) free(biased);
    if (shaped) free(shaped);
    return;
  }
  for (size_t i = 0; i < numSamples; i++) {
    float biasedSample = in[i] + fz->bias;
    biased[i] = biasedSample * fz->fuzz;
  }
  waveshaper_lookup_cubic(biased, shaped, fz->waveshapeTable, fz->waveshapeTableSize, numSamples);
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = shaped[i] * fz->outputGain;
  }
  
  free(biased);
  free(shaped);
}

void fuzz_set_params(Fuzz* fz, float fuzzAmount, float bias, float outputDb) {
  fz->fuzz = clampf(fuzzAmount, 0.1f, 10.0f);
  fz->bias = clampf(bias, -0.5f, 0.5f);
  fz->outputGain = db_to_linear(clampf(outputDb, -12.0f, 12.0f));
}

void fuzz_set_enabled(Fuzz* fz, bool enabled) {
  fz->enabled = enabled;
}








void clipper_init(Clipper* clip, ClipperType type, float threshold) {
  memset(clip, 0, sizeof(Clipper));
  
  clip->type = type;
  clip->threshold = clampf(threshold, 0.001f, 1.0f);
  clip->drive = 1.0f;
  clip->enabled = true;
}

void clipper_process(Clipper* clip, const float* in, float* out, size_t numSamples) {
  if (!clip->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  switch (clip->type) {
    case CLIP_HARD:
      hard_clip(in, clip->threshold * clip->drive, out, numSamples);
      break;
      
    case CLIP_SOFT_TANH:
      {
        float* driven = (float*)malloc(numSamples * sizeof(float));
        if (driven) {
          for (size_t i = 0; i < numSamples; i++) {
            driven[i] = in[i] * clip->drive;
          }
          tanh_clip(driven, clip->threshold, out, numSamples);
          free(driven);
        } else {
          memcpy(out, in, numSamples * sizeof(float));
        }
      }
      break;
      
    case CLIP_ARCTAN:
      {
        float* driven = (float*)malloc(numSamples * sizeof(float));
        if (driven) {
          for (size_t i = 0; i < numSamples; i++) {
            driven[i] = in[i] * clip->drive;
          }
          arctan_clip(driven, clip->threshold, out, numSamples);
          free(driven);
        } else {
          memcpy(out, in, numSamples * sizeof(float));
        }
      }
      break;
      
    case CLIP_SIGMOID:
      {
        float* driven = (float*)malloc(numSamples * sizeof(float));
        if (driven) {
          for (size_t i = 0; i < numSamples; i++) {
            driven[i] = in[i] * clip->drive;
          }
          for (size_t i = 0; i < numSamples; i++) {
            float x = driven[i] / clip->threshold;
            out[i] = clip->threshold * (2.0f / (1.0f + expf(-x)) - 1.0f);
          }
          free(driven);
        } else {
          memcpy(out, in, numSamples * sizeof(float));
        }
      }
      break;
      
    case CLIP_CUBIC_SOFT:
      {
        float* driven = (float*)malloc(numSamples * sizeof(float));
        if (driven) {
          for (size_t i = 0; i < numSamples; i++) {
            driven[i] = in[i] * clip->drive;
          }
          for (size_t i = 0; i < numSamples; i++) {
            float x = driven[i] / clip->threshold;
            float absX = fabsf(x);
            
            if (absX < 1.0f) {
              out[i] = driven[i];
            } else if (absX < 2.0f) {
              float s = (x < 0.0f) ? -1.0f : 1.0f;
              float t = 2.0f - absX;
              out[i] = s * (2.0f - (t * t) / 3.0f) * clip->threshold;
            } else {
              out[i] = (x < 0.0f) ? -2.0f * clip->threshold : 2.0f * clip->threshold;
            }
          }
          free(driven);
        } else {
          memcpy(out, in, numSamples * sizeof(float));
        }
      }
      break;
    default:
      memcpy(out, in, numSamples * sizeof(float));
      break;
  }
}

void clipper_set_params(Clipper* clip, float threshold, float drive) {
  clip->threshold = clampf(threshold, 0.001f, 1.0f);
  clip->drive = clampf(drive, 0.1f, 10.0f);
}

void clipper_set_enabled(Clipper* clip, bool enabled) {
  clip->enabled = enabled;
}




void threebande_init(ThreeBandEQ* eq, float sampleRate) {
  memset(eq, 0, sizeof(ThreeBandEQ));
  eq->enabled = true;
  biquad_init(&eq->lowShelf, BQ_LOWSHELF, 100.0f, 0.707f, 0.0f, sampleRate);
  biquad_init(&eq->midPeak, BQ_PEAK, 1000.0f, 1.0f, 0.0f, sampleRate);
  biquad_init(&eq->highShelf, BQ_HIGHSHELF, 10000.0f, 0.707f, 0.0f, sampleRate);
}

void threebande_process(ThreeBandEQ* eq, const float* in, float* out, size_t numSamples) {
  if (!eq->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* temp1 = (float*)malloc(numSamples * sizeof(float));
  float* temp2 = (float*)malloc(numSamples * sizeof(float));
  
  if (!temp1 || !temp2) {
    memcpy(out, in, numSamples * sizeof(float));
    if (temp1) free(temp1);
    if (temp2) free(temp2);
    return;
  }
  biquad_process(&eq->lowShelf, in, temp1, numSamples);
  biquad_process(&eq->midPeak, temp1, temp2, numSamples);
  biquad_process(&eq->highShelf, temp2, out, numSamples);
  
  free(temp1);
  free(temp2);
}

void threebande_set_params(ThreeBandEQ* eq, float lowGainDb, float midGainDb, float highGainDb, float midFreq, float midQ) {
  float clampedLowGain = clampf(lowGainDb, -18.0f, 18.0f);
  float clampedMidGain = clampf(midGainDb, -18.0f, 18.0f);
  float clampedHighGain = clampf(highGainDb, -18.0f, 18.0f);
  float clampedMidFreq = clampf(midFreq, 200.0f, 5000.0f);
  float clampedQ = clampf(midQ, 0.5f, 5.0f);
  biquad_set_params(&eq->lowShelf, BQ_LOWSHELF, 100.0f, 0.707f, clampedLowGain, 44100.0f);
  biquad_set_params(&eq->midPeak, BQ_PEAK, clampedMidFreq, clampedQ, clampedMidGain, 44100.0f);
  biquad_set_params(&eq->highShelf, BQ_HIGHSHELF, 10000.0f, 0.707f, clampedHighGain, 44100.0f);
}

void threebande_set_enabled(ThreeBandEQ* eq, bool enabled) {
  eq->enabled = enabled;
}




void highpass_init(HighPassFilter* hpf, float cutoffHz, float sampleRate) {
  memset(hpf, 0, sizeof(HighPassFilter));
  hpf->enabled = true;
  onepole_init(&hpf->highpass, cutoffHz, sampleRate, 1);
}

void highpass_process(HighPassFilter* hpf, const float* in, float* out, size_t numSamples) {
  if (!hpf->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  onepole_process(&hpf->highpass, in, out, numSamples);
}

void highpass_set_cutoff(HighPassFilter* hpf, float cutoffHz) {
  cutoffHz = clampf(cutoffHz, 20.0f, 20000.0f);
  onepole_set_cutoff(&hpf->highpass, cutoffHz, hpf->highpass.a0);
}

void highpass_set_enabled(HighPassFilter* hpf, bool enabled) {
  hpf->enabled = enabled;
}















void ampchain_init(AmpChain* chain, float* memoryPool, size_t memorySize, float sampleRate) {
  memset(chain, 0, sizeof(AmpChain));
  
  chain->sampleRate = sampleRate;
  chain->bypass = false;
  
  size_t poolOffset = 0;
  
  noisegate_init(&chain->noisegate, -40.0f, 10.0f, 100.0f, 50.0f, sampleRate);
  highpass_init(&chain->inputHighpass, 20.0f, sampleRate);
  
  size_t wsTableSize = 2048;
  float* od_wsTable = memoryPool + poolOffset;
  poolOffset += wsTableSize;
  build_waveshaper_table(od_wsTable, wsTableSize, CLIP_SOFT_TANH, 1.0f);
  overdrive_init(&chain->overdrive, od_wsTable, wsTableSize, sampleRate);
  
  float* dist_wsTable = memoryPool + poolOffset;
  poolOffset += wsTableSize;
  build_waveshaper_table(dist_wsTable, wsTableSize, CLIP_SOFT_TANH, 2.0f);
  distortion_init(&chain->distortion, dist_wsTable, wsTableSize, sampleRate);
  
  float* fuzz_wsTable = memoryPool + poolOffset;
  poolOffset += wsTableSize;
  build_waveshaper_table(fuzz_wsTable, wsTableSize, CLIP_ARCTAN, 1.5f);
  fuzz_init(&chain->fuzz, fuzz_wsTable, wsTableSize, sampleRate);
  
  threebande_init(&chain->eq, sampleRate);
  compressor_init_interface(&chain->compressor, 10.0f, 100.0f, sampleRate);
  
  size_t delayBufferSize = (size_t)(sampleRate * 0.5f);  // 500ms delay buffer
  
  float* chorusBuffer = memoryPool + poolOffset;
  poolOffset += delayBufferSize;
  chorus_init(&chain->chorus, chorusBuffer, delayBufferSize, 500.0f, sampleRate);
  
  float* flangerBuffer = memoryPool + poolOffset;
  poolOffset += delayBufferSize;
  flanger_init(&chain->flanger, flangerBuffer, delayBufferSize, sampleRate);
  
  phaser_init(&chain->phaser, sampleRate);
  tremolo_init(&chain->tremolo, sampleRate);
  
  float* delayBuffer = memoryPool + poolOffset;
  poolOffset += delayBufferSize * 2;  // 1 second max delay
  delay_init(&chain->delay, delayBuffer, delayBufferSize * 2, 1000.0f, sampleRate);
  
  float* reverbBuffer = memoryPool + poolOffset;
  poolOffset += delayBufferSize * 4;  // Large buffer for reverb
  reverb_init_interface(&chain->reverb, reverbBuffer, delayBufferSize * 4, sampleRate);
  
  float* preamp_wsTable = memoryPool + poolOffset;
  poolOffset += wsTableSize;
  build_waveshaper_table(preamp_wsTable, wsTableSize, CLIP_SOFT_TANH, 1.0f);
  preamp_init(&chain->preamp, preamp_wsTable, wsTableSize, sampleRate);
  
  float* poweramp_wsTable = memoryPool + poolOffset;
  poolOffset += wsTableSize;
  build_waveshaper_table(poweramp_wsTable, wsTableSize, CLIP_SOFT_TANH, 1.5f);
  poweramp_init(&chain->poweramp, poweramp_wsTable, wsTableSize, sampleRate);
  
  cabinet_init(&chain->cabinet, sampleRate);
  
  limiter_init(&chain->limiter, 0.0f, 5.0f, 50.0f, sampleRate);
}

void ampchain_process(AmpChain* chain, const float* in, float* out, size_t numSamples) {
  if (chain->bypass) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  float* stage = (float*)malloc(numSamples * sizeof(float));
  if (!stage) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  memcpy(stage, in, numSamples * sizeof(float));
  noisegate_process(&chain->noisegate, stage, stage, numSamples);
  highpass_process(&chain->inputHighpass, stage, stage, numSamples);
  overdrive_process(&chain->overdrive, stage, stage, numSamples);
  distortion_process(&chain->distortion, stage, stage, numSamples);
  fuzz_process(&chain->fuzz, stage, stage, numSamples);
  threebande_process(&chain->eq, stage, stage, numSamples);
  compressor_process_interface(&chain->compressor, stage, stage, numSamples);
  float* modulation = (float*)malloc(numSamples * sizeof(float));
  if (modulation) {
    chorus_process(&chain->chorus, stage, modulation, numSamples);
    memcpy(stage, modulation, numSamples * sizeof(float));
    flanger_process(&chain->flanger, stage, modulation, numSamples);
    memcpy(stage, modulation, numSamples * sizeof(float));
    phaser_process(&chain->phaser, stage, modulation, numSamples);
    memcpy(stage, modulation, numSamples * sizeof(float));
    tremolo_process(&chain->tremolo, stage, modulation, numSamples);
    memcpy(stage, modulation, numSamples * sizeof(float));
    
    free(modulation);
  }
  delay_process(&chain->delay, stage, stage, numSamples);
  reverb_process_interface(&chain->reverb, stage, stage, numSamples);
  preamp_process(&chain->preamp, stage, stage, numSamples);
  poweramp_process(&chain->poweramp, stage, stage, numSamples);
  cabinet_process(&chain->cabinet, stage, stage, numSamples);
  limiter_process(&chain->limiter, stage, out, numSamples);
  
  free(stage);
}

void ampchain_set_bypass(AmpChain* chain, bool bypass) {
  chain->bypass = bypass;
}

void ampchain_reset_all(AmpChain* chain) {
  noisegate_init(&chain->noisegate, -40.0f, 10.0f, 100.0f, 50.0f, chain->sampleRate);
  highpass_init(&chain->inputHighpass, 20.0f, chain->sampleRate);
  tremolo_init(&chain->tremolo, chain->sampleRate);
  phaser_init(&chain->phaser, chain->sampleRate);
  env_init(&chain->compressor.state.detector, 10.0f, 100.0f, chain->sampleRate, 0);
  env_init(&chain->limiter.state.detector, 5.0f, 50.0f, chain->sampleRate, 0);
}