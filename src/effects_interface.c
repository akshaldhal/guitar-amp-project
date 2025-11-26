#include "effects_interface.h"
#include <math.h>
#include <string.h>

/*============================================================================
  NOISE GATE
============================================================================*/

void noisegate_init(NoiseGate* ng, DSPState* state, float thresholdDb, float attackMs, float releaseMs, float holdMs) {
  ng->state = state;
  ng->threshold = db_to_linear(thresholdDb);
  ng->holdSamples = (holdMs / 1000.0f) * state->sampleRate;
  ng->holdCounter = 0.0f;
  ng->attenuation = 0.0f;
  ng->enabled = true;
  
  env_init(&ng->detector, state, attackMs, releaseMs, 0);
}

void noisegate_process(NoiseGate* ng, const float* in, float* out, size_t numSamples) {
  if (!ng->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* envOut = (float*)malloc(numSamples * sizeof(float));
  env_process(&ng->detector, in, envOut, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    float envVal = envOut[i];
    
    if (envVal > ng->threshold) {
      ng->holdCounter = ng->holdSamples;
      ng->attenuation = 1.0f;
    } else if (ng->holdCounter > 0.0f) {
      ng->holdCounter--;
      ng->attenuation = 1.0f;
    } else {
      ng->attenuation *= 0.99f;
    }
    
    out[i] = in[i] * ng->attenuation;
  }
  
  free(envOut);
}

void noisegate_set_threshold(NoiseGate* ng, float thresholdDb) {
  ng->threshold = db_to_linear(thresholdDb);
}

void noisegate_set_enabled(NoiseGate* ng, bool enabled) {
  ng->enabled = enabled;
}

/*============================================================================
  COMPRESSOR
============================================================================*/

void compressor_fx_init(Compressor* comp, DSPState* state, float attackMs, float releaseMs) {
  comp->state = state;
  comp->threshold = db_to_linear(-20.0f);
  comp->ratio = 4.0f;
  comp->makeup = 1.0f;
  comp->kneeWidth = 0.0f;
  comp->previousGain = 1.0f;
  comp->enabled = true;
  
  env_init(&comp->detector, state, attackMs, releaseMs, 1);
}

void compressor_fx_process(Compressor* comp, const float* in, float* out, size_t numSamples) {
  if (!comp->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  if (numSamples > comp->state->scratchSize) {
    dsp_state_grow_scratches(comp->state, numSamples * 2);
  }
  
  float* envOut = comp->state->scratch[5];
  env_process(&comp->detector, in, envOut, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    float envDb = linear_to_db(envOut[i]);
    float threshDb = linear_to_db(comp->threshold);
    
    float gainDb = 0.0f;
    if (envDb > threshDb) {
      float overDb = envDb - threshDb;
      gainDb = -overDb * (1.0f - 1.0f / comp->ratio);
    }
    
    float gainLin = db_to_linear(gainDb) * comp->makeup;
    comp->previousGain = gainLin;
    
    out[i] = in[i] * gainLin;
  }
}

void compressor_fx_set_params(Compressor* comp, float thresholdDb, float ratio, float makeupDb, float kneeDb) {
  comp->threshold = db_to_linear(thresholdDb);
  comp->ratio = fmaxf(1.0f, ratio);
  comp->makeup = db_to_linear(makeupDb);
  comp->kneeWidth = kneeDb;
}

void compressor_fx_set_enabled(Compressor* comp, bool enabled) {
  comp->enabled = enabled;
}

/*============================================================================
  LIMITER
============================================================================*/

void limiter_init(Limiter* lim, DSPState* state, float releaseMs) {
  lim->state = state;
  lim->threshold = db_to_linear(0.0f);
  lim->ratio = 100.0f;
  lim->makeup = 1.0f;
  lim->enabled = true;
  
  biquad_init(&lim->inputFilter, state, BQ_LPF, 8000.0f, 0.707f, 0.0f);
  env_init(&lim->detector, state, 1.0f, releaseMs, 1);
}

void limiter_process(Limiter* lim, const float* in, float* out, size_t numSamples) {
  if (!lim->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* filtered = (float*)malloc(numSamples * sizeof(float));
  float* envOut = (float*)malloc(numSamples * sizeof(float));
  
  biquad_process(&lim->inputFilter, in, filtered, numSamples);
  env_process(&lim->detector, filtered, envOut, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    float envDb = linear_to_db(envOut[i]);
    float threshDb = linear_to_db(lim->threshold);
    
    float gainDb = 0.0f;
    if (envDb > threshDb) {
      gainDb = -(envDb - threshDb);
    }
    
    float gainLin = db_to_linear(gainDb);
    out[i] = in[i] * gainLin;
  }
  
  free(filtered);
  free(envOut);
}

void limiter_set_threshold(Limiter* lim, float thresholdDb) {
  lim->threshold = db_to_linear(thresholdDb);
}

void limiter_set_enabled(Limiter* lim, bool enabled) {
  lim->enabled = enabled;
}

/*============================================================================
  OVERDRIVE
============================================================================*/

void overdrive_init(Overdrive* od, DSPState* state, float* wsTable, size_t wsTableSize) {
  od->state = state;
  od->drive = 1.0f;
  od->waveshapeTable = wsTable;
  od->waveshapeTableSize = wsTableSize;
  od->outputGain = 1.0f;
  od->enabled = true;
  
  biquad_init(&od->inputFilter, state, BQ_HPF, 10.0f, 0.707f, 0.0f);
  biquad_init(&od->toneFilter, state, BQ_LPF, 5000.0f, 0.707f, 0.0f);
}

void overdrive_process(Overdrive* od, const float* in, float* out, size_t numSamples) {
  if (!od->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* filtered = (float*)malloc(numSamples * sizeof(float));
  float* driven = (float*)malloc(numSamples * sizeof(float));
  
  biquad_process(&od->inputFilter, in, filtered, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    driven[i] = filtered[i] * od->drive;
  }
  
  float* waveshaped = (float*)malloc(numSamples * sizeof(float));
  waveshaper_lookup(driven, waveshaped, od->waveshapeTable, od->waveshapeTableSize, numSamples);
  
  float* toned = (float*)malloc(numSamples * sizeof(float));
  biquad_process(&od->toneFilter, waveshaped, toned, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = toned[i] * od->outputGain;
  }
  
  free(filtered);
  free(driven);
  free(waveshaped);
  free(toned);
}

void overdrive_set_params(Overdrive* od, float driveDb, float toneFreqHz, float outputDb) {
  od->drive = db_to_linear(driveDb);
  od->outputGain = db_to_linear(outputDb);
  biquad_set_params(&od->toneFilter, BQ_LPF, toneFreqHz, 0.707f, 0.0f);
}

void overdrive_set_enabled(Overdrive* od, bool enabled) {
  od->enabled = enabled;
}

/*============================================================================
  DISTORTION
============================================================================*/

void distortion_init(Distortion* dist, DSPState* state, float* wsTable, size_t wsTableSize) {
  dist->state = state;
  dist->drive = 1.0f;
  dist->waveshapeTable = wsTable;
  dist->waveshapeTableSize = wsTableSize;
  dist->outputGain = 1.0f;
  dist->enabled = true;
  
  biquad_init(&dist->highpassFilter, state, BQ_HPF, 20.0f, 0.707f, 0.0f);
  biquad_init(&dist->toneStack[0], state, BQ_LOWSHELF, 200.0f, 0.707f, 0.0f);
  biquad_init(&dist->toneStack[1], state, BQ_PEAK, 1000.0f, 0.707f, 0.0f);
  biquad_init(&dist->toneStack[2], state, BQ_HIGHSHELF, 5000.0f, 0.707f, 0.0f);
}

void distortion_process(Distortion* dist, const float* in, float* out, size_t numSamples) {
  if (!dist->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* filtered = (float*)malloc(numSamples * sizeof(float));
  float* driven = (float*)malloc(numSamples * sizeof(float));
  float* waveshaped = (float*)malloc(numSamples * sizeof(float));
  float* toned = (float*)malloc(numSamples * sizeof(float));
  float* temp = (float*)malloc(numSamples * sizeof(float));
  
  biquad_process(&dist->highpassFilter, in, filtered, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    driven[i] = filtered[i] * dist->drive;
  }
  
  waveshaper_lookup(driven, waveshaped, dist->waveshapeTable, dist->waveshapeTableSize, numSamples);
  
  biquad_process(&dist->toneStack[0], waveshaped, temp, numSamples);
  biquad_process(&dist->toneStack[1], temp, toned, numSamples);
  biquad_process(&dist->toneStack[2], toned, temp, numSamples);
  
  for (size_t i = 0; i < numSamples; i++) {
    out[i] = temp[i] * dist->outputGain;
  }
  
  free(filtered);
  free(driven);
  free(waveshaped);
  free(toned);
  free(temp);
}

void distortion_set_params(Distortion* dist, float driveDb, float toneFreqHz, float outputDb) {
  dist->drive = db_to_linear(driveDb);
  dist->outputGain = db_to_linear(outputDb);
  biquad_set_params(&dist->toneStack[1], BQ_PEAK, toneFreqHz, 0.707f, 0.0f);
}

void distortion_set_enabled(Distortion* dist, bool enabled) {
  dist->enabled = enabled;
}

/*============================================================================
  PREAMP SIMULATOR
============================================================================*/

void preamp_init(PreampSimulator* preamp, DSPState* state, float* wsTable, size_t wsTableSize) {
  preamp->state = state;
  preamp->gain = 1.0f;
  preamp->sagAmount = 0.0f;
  preamp->enabled = true;
  
  tubepreamp_init(&preamp->preamp, state, wsTable, wsTableSize);
  
  biquad_init(&preamp->toneStack[0], state, BQ_LOWSHELF, 100.0f, 0.707f, 0.0f);
  biquad_init(&preamp->toneStack[1], state, BQ_PEAK, 800.0f, 0.707f, 0.0f);
  biquad_init(&preamp->toneStack[2], state, BQ_HIGHSHELF, 3000.0f, 0.707f, 0.0f);
}

void preamp_process(PreampSimulator* preamp, const float* in, float* out, size_t numSamples) {
  if (!preamp->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* gained = (float*)malloc(numSamples * sizeof(float));
  float* preampOut = (float*)malloc(numSamples * sizeof(float));
  float* temp = (float*)malloc(numSamples * sizeof(float));
  float* temp2 = (float*)malloc(numSamples * sizeof(float));
  
  for (size_t i = 0; i < numSamples; i++) {
    gained[i] = in[i] * preamp->gain;
  }
  
  tubepreamp_process(&preamp->preamp, gained, preampOut, numSamples);
  
  biquad_process(&preamp->toneStack[0], preampOut, temp, numSamples);
  biquad_process(&preamp->toneStack[1], temp, temp2, numSamples);
  biquad_process(&preamp->toneStack[2], temp2, out, numSamples);
  
  free(gained);
  free(preampOut);
  free(temp);
  free(temp2);
}

void preamp_set_gain(PreampSimulator* preamp, float gainDb) {
  preamp->gain = db_to_linear(gainDb);
}

void preamp_set_tone_stack(PreampSimulator* preamp, float lowDb, float midDb, float highDb) {
  biquad_set_params(&preamp->toneStack[0], BQ_LOWSHELF, 100.0f, 0.707f, lowDb);
  biquad_set_params(&preamp->toneStack[1], BQ_PEAK, 800.0f, 0.707f, midDb);
  biquad_set_params(&preamp->toneStack[2], BQ_HIGHSHELF, 3000.0f, 0.707f, highDb);
}

void preamp_set_sag(PreampSimulator* preamp, float sagAmount) {
  preamp->sagAmount = clampf(sagAmount, 0.0f, 1.0f);
  preamp->preamp.sagAmount = preamp->sagAmount;
}

void preamp_set_enabled(PreampSimulator* preamp, bool enabled) {
  preamp->enabled = enabled;
}

/*============================================================================
  POWER AMP SIMULATOR
============================================================================*/

void poweramp_init(PowerAmpSimulator* poweramp, DSPState* state, float* wsTable, size_t wsTableSize) {
  poweramp->state = state;
  poweramp->waveshapeTable = wsTable;
  poweramp->waveshapeTableSize = wsTableSize;
  poweramp->sagAmount = 0.0f;
  poweramp->sagTimeConstant = 0.1f;
  poweramp->supplyVoltage = 400.0f;
  poweramp->supplyFilterState = poweramp->supplyVoltage;
  poweramp->outputGain = 1.0f;
  poweramp->enabled = true;
}

void poweramp_process(PowerAmpSimulator* poweramp, const float* in, float* out, size_t numSamples) {
  if (!poweramp->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* waveshaped = (float*)malloc(numSamples * sizeof(float));
  waveshaper_lookup(in, waveshaped, poweramp->waveshapeTable, poweramp->waveshapeTableSize, numSamples);
  
  float sag_coeff = poweramp->sagTimeConstant * poweramp->state->sampleRate / 1000.0f;
  sag_coeff = clampf(sag_coeff, 0.0f, 1.0f);
  
  for (size_t i = 0; i < numSamples; i++) {
    float sagDrop = fabsf(waveshaped[i]) * poweramp->sagAmount;
    poweramp->supplyFilterState += (poweramp->supplyVoltage - sagDrop - poweramp->supplyFilterState) * sag_coeff;
    
    float sagNormalized = poweramp->supplyVoltage / fmaxf(poweramp->supplyFilterState, 1.0f);
    out[i] = waveshaped[i] * sagNormalized * poweramp->outputGain;
  }
  
  free(waveshaped);
}

void poweramp_set_params(PowerAmpSimulator* poweramp, float sagAmount, float outputDb) {
  poweramp->sagAmount = clampf(sagAmount, 0.0f, 1.0f);
  poweramp->outputGain = db_to_linear(outputDb);
}

void poweramp_set_enabled(PowerAmpSimulator* poweramp, bool enabled) {
  poweramp->enabled = enabled;
}

/*============================================================================
  CABINET SIMULATOR
============================================================================*/

void cabinet_init(CabinetSimulator* cab, DSPState* state, int cabinetType) {
  cab->state = state;
  cab->cabinetType = cabinetType;
  cab->enabled = true;
  
  biquad_init(&cab->resonanceFilter, state, BQ_PEAK, 150.0f, 0.5f, 6.0f);
  biquad_init(&cab->presenceFilter, state, BQ_PEAK, 3000.0f, 0.707f, 3.0f);
  biquad_init(&cab->dampingFilter, state, BQ_LPF, 4000.0f, 0.707f, 0.0f);
}

void cabinet_process(CabinetSimulator* cab, const float* in, float* out, size_t numSamples) {
  if (!cab->enabled) {
    memcpy(out, in, numSamples * sizeof(float));
    return;
  }
  
  float* temp = (float*)malloc(numSamples * sizeof(float));
  float* temp2 = (float*)malloc(numSamples * sizeof(float));
  
  biquad_process(&cab->resonanceFilter, in, temp, numSamples);
  biquad_process(&cab->presenceFilter, temp, temp2, numSamples);
  biquad_process(&cab->dampingFilter, temp2, out, numSamples);
  
  free(temp);
  free(temp2);
}

void cabinet_set_type(CabinetSimulator* cab, int cabinetType) {
  cab->cabinetType = cabinetType;
  
  switch (cabinetType) {
    case 0:
      biquad_set_params(&cab->resonanceFilter, BQ_PEAK, 120.0f, 0.5f, 8.0f);
      biquad_set_params(&cab->presenceFilter, BQ_PEAK, 3500.0f, 0.707f, 4.0f);
      biquad_set_params(&cab->dampingFilter, BQ_LPF, 3500.0f, 0.707f, 0.0f);
      break;
    case 1:
      biquad_set_params(&cab->resonanceFilter, BQ_PEAK, 100.0f, 0.5f, 5.0f);
      biquad_set_params(&cab->presenceFilter, BQ_PEAK, 4000.0f, 0.707f, 2.0f);
      biquad_set_params(&cab->dampingFilter, BQ_LPF, 4500.0f, 0.707f, 0.0f);
      break;
    case 2:
      biquad_set_params(&cab->resonanceFilter, BQ_PEAK, 200.0f, 0.5f, 10.0f);
      biquad_set_params(&cab->presenceFilter, BQ_PEAK, 2500.0f, 0.707f, 5.0f);
      biquad_set_params(&cab->dampingFilter, BQ_LPF, 3000.0f, 0.707f, 0.0f);
      break;
    default:
      break;
  }
}

void cabinet_set_enabled(CabinetSimulator* cab, bool enabled) {
  cab->enabled = enabled;
}

/*============================================================================
  AMP CHAIN
============================================================================*/

void ampchain_init(AmpChain* chain, DSPState* state, float* wsTable, size_t wsTableSize) {
  chain->state = state;
  chain->bypass = false;

  build_waveshaper_table(wsTable, wsTableSize, CLIP_SOFT_TANH, 1.0f);
  // build tube tables in one of the free scratches for tube amps/poweramps
  
  noisegate_init(&chain->noisegate, state, -40.0f, 1.0f, 100.0f, 50.0f);
  overdrive_init(&chain->overdrive, state, wsTable, wsTableSize);
  distortion_init(&chain->distortion, state, wsTable, wsTableSize);
  compressor_fx_init(&chain->compressor, state, 10.0f, 100.0f);
  preamp_init(&chain->preamp, state, wsTable, wsTableSize);
  poweramp_init(&chain->poweramp, state, wsTable, wsTableSize);
  cabinet_init(&chain->cabinet, state, 0);
  limiter_init(&chain->limiter, state, 50.0f);
}

void ampchain_process(AmpChain* chain, const float* in, float* out, size_t numSamples) {
    if (chain->bypass) {
        memcpy(out, in, numSamples * sizeof(float));
        return;
    }

    // Use a dedicated scratch buffer for the DSP chain
    float* buf = chain->state->scratch[6];  // scratch[6] reserved for processing

    // Ensure we never overflow (callback should never grow in real-time)
    if (numSamples > chain->state->scratchSize) {
        // Clip instead of realloc
        numSamples = chain->state->scratchSize;
    }

    memcpy(buf, in, numSamples * sizeof(float));

    // Process DSP in-place safely
    noisegate_process(&chain->noisegate, buf, buf, numSamples);
    overdrive_process(&chain->overdrive, buf, buf, numSamples);
    distortion_process(&chain->distortion, buf, buf, numSamples);
    compressor_fx_process(&chain->compressor, buf, buf, numSamples);
    preamp_process(&chain->preamp, buf, buf, numSamples);
    poweramp_process(&chain->poweramp, buf, buf, numSamples);
    cabinet_process(&chain->cabinet, buf, buf, numSamples);
    limiter_process(&chain->limiter, buf, buf, numSamples);

    memcpy(out, buf, numSamples * sizeof(float));
}

void ampchain_set_bypass(AmpChain* chain, bool bypass) {
  chain->bypass = bypass;
}

void ampchain_reset_all(AmpChain* chain) {
  noisegate_init(&chain->noisegate, chain->state, -40.0f, 1.0f, 100.0f, 50.0f);
  compressor_fx_init(&chain->compressor, chain->state, 10.0f, 100.0f);
  limiter_init(&chain->limiter, chain->state, 50.0f);
}