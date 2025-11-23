#ifndef EFFECTS_INTERFACE_H
#define EFFECTS_INTERFACE_H

#include "effects_dsp.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*============================================================================
  DYNAMICS
============================================================================*/

typedef struct {
  DSPState* state;
  EnvelopeDetector detector;
  float threshold;
  float holdSamples;
  float holdCounter;
  float attenuation;
  bool enabled;
} NoiseGate;

void noisegate_init(NoiseGate* ng, DSPState* state, float thresholdDb, float attackMs, float releaseMs, float holdMs);
void noisegate_process(NoiseGate* ng, const float* in, float* out, size_t numSamples);
void noisegate_set_threshold(NoiseGate* ng, float thresholdDb);
void noisegate_set_enabled(NoiseGate* ng, bool enabled);


typedef struct {
  DSPState* state;
  EnvelopeDetector detector;
  float threshold;
  float ratio;
  float makeup;
  float kneeWidth;
  float previousGain;
  bool enabled;
} Compressor;

void compressor_fx_init(Compressor* comp, DSPState* state, float attackMs, float releaseMs);
void compressor_fx_process(Compressor* comp, const float* in, float* out, size_t numSamples);
void compressor_fx_set_params(Compressor* comp, float thresholdDb, float ratio, float makeupDb, float kneeDb);
void compressor_fx_set_enabled(Compressor* comp, bool enabled);


typedef struct {
  DSPState* state;
  Biquad inputFilter;
  EnvelopeDetector detector;
  float threshold;
  float ratio;
  float makeup;
  bool enabled;
} Limiter;

void limiter_init(Limiter* lim, DSPState* state, float releaseMs);
void limiter_process(Limiter* lim, const float* in, float* out, size_t numSamples);
void limiter_set_threshold(Limiter* lim, float thresholdDb);
void limiter_set_enabled(Limiter* lim, bool enabled);

/*============================================================================
  DISTORTION
============================================================================*/

typedef struct {
  DSPState* state;
  Biquad inputFilter;
  float drive;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  Biquad toneFilter;
  float outputGain;
  bool enabled;
} Overdrive;

void overdrive_init(Overdrive* od, DSPState* state, float* wsTable, size_t wsTableSize);
void overdrive_process(Overdrive* od, const float* in, float* out, size_t numSamples);
void overdrive_set_params(Overdrive* od, float driveDb, float toneFreqHz, float outputDb);
void overdrive_set_enabled(Overdrive* od, bool enabled);


typedef struct {
  DSPState* state;
  Biquad highpassFilter;
  float drive;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  Biquad toneStack[3];
  float outputGain;
  bool enabled;
} Distortion;

void distortion_init(Distortion* dist, DSPState* state, float* wsTable, size_t wsTableSize);
void distortion_process(Distortion* dist, const float* in, float* out, size_t numSamples);
void distortion_set_params(Distortion* dist, float driveDb, float toneFreqHz, float outputDb);
void distortion_set_enabled(Distortion* dist, bool enabled);

/*============================================================================
  AMP SIMULATION
============================================================================*/

typedef struct {
  DSPState* state;
  TubePreamp preamp;
  float gain;
  Biquad toneStack[3];
  float sagAmount;
  bool enabled;
} PreampSimulator;

void preamp_init(PreampSimulator* preamp, DSPState* state, float* wsTable, size_t wsTableSize);
void preamp_process(PreampSimulator* preamp, const float* in, float* out, size_t numSamples);
void preamp_set_gain(PreampSimulator* preamp, float gainDb);
void preamp_set_tone_stack(PreampSimulator* preamp, float lowDb, float midDb, float highDb);
void preamp_set_sag(PreampSimulator* preamp, float sagAmount);
void preamp_set_enabled(PreampSimulator* preamp, bool enabled);


typedef struct {
  DSPState* state;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  float sagAmount;
  float sagTimeConstant;
  float supplyVoltage;
  float supplyFilterState;
  float outputGain;
  bool enabled;
} PowerAmpSimulator;

void poweramp_init(PowerAmpSimulator* poweramp, DSPState* state, float* wsTable, size_t wsTableSize);
void poweramp_process(PowerAmpSimulator* poweramp, const float* in, float* out, size_t numSamples);
void poweramp_set_params(PowerAmpSimulator* poweramp, float sagAmount, float outputDb);
void poweramp_set_enabled(PowerAmpSimulator* poweramp, bool enabled);


typedef struct {
  DSPState* state;
  Biquad resonanceFilter;
  Biquad presenceFilter;
  Biquad dampingFilter;
  int cabinetType;
  bool enabled;
} CabinetSimulator;

void cabinet_init(CabinetSimulator* cab, DSPState* state, int cabinetType);
void cabinet_process(CabinetSimulator* cab, const float* in, float* out, size_t numSamples);
void cabinet_set_type(CabinetSimulator* cab, int cabinetType);
void cabinet_set_enabled(CabinetSimulator* cab, bool enabled);

/*============================================================================
  INTEGRATED AMP CHAIN
============================================================================*/

typedef struct {
  DSPState* state;
  
  // Input stage
  NoiseGate noisegate;
  
  // Preamp effects
  Overdrive overdrive;
  Distortion distortion;
  
  // Tone shaping
  Compressor compressor;
  
  // Amp simulation
  PreampSimulator preamp;
  PowerAmpSimulator poweramp;
  CabinetSimulator cabinet;
  
  // Output protection
  Limiter limiter;
  
  // Configuration
  bool bypass;
} AmpChain;

void ampchain_init(AmpChain* chain, DSPState* state, float* wsTable, size_t wsTableSize);
void ampchain_process(AmpChain* chain, const float* in, float* out, size_t numSamples);
void ampchain_set_bypass(AmpChain* chain, bool bypass);
void ampchain_reset_all(AmpChain* chain);

#endif /* EFFECTS_INTERFACE_H */