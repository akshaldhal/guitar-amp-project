#ifndef EFFECTS_INTERFACE_H
#define EFFECTS_INTERFACE_H

#include "effects_dsp.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  EnvelopeDetector detector;
  float threshold;
  float holdSamples;
  float holdCounter;
  float attenuation;
  bool enabled;
} NoiseGate;

void noisegate_init(NoiseGate* ng, float thresholdDb, float attackMs, float releaseMs, float holdMs, float sampleRate);
void noisegate_process(NoiseGate* ng, const float* in, float* out, size_t numSamples);
void noisegate_set_threshold(NoiseGate* ng, float thresholdDb);
void noisegate_set_enabled(NoiseGate* ng, bool enabled);


typedef struct {
  float drive;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  Biquad toneFilter;
  float outputGain;
  float inputGain;
  bool enabled;
} Overdrive;

void overdrive_init(Overdrive* od, float* wsTable, size_t wsTableSize, float sampleRate);
void overdrive_process(Overdrive* od, const float* in, float* out, size_t numSamples);
void overdrive_set_params(Overdrive* od, float driveDb, float toneFreq, float outputDb);
void overdrive_set_enabled(Overdrive* od, bool enabled);


typedef struct {
  float drive;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  Biquad lowcut;
  Biquad highcut;
  float outputGain;
  bool enabled;
} Distortion;

void distortion_init(Distortion* dist, float* wsTable, size_t wsTableSize, float sampleRate);
void distortion_process(Distortion* dist, const float* in, float* out, size_t numSamples);
void distortion_set_params(Distortion* dist, float driveDb, float outputDb);
void distortion_set_enabled(Distortion* dist, bool enabled);


typedef struct {
  float fuzz;
  float bias;
  float* waveshapeTable;
  size_t waveshapeTableSize;
  float outputGain;
  bool enabled;
} Fuzz;

void fuzz_init(Fuzz* fz, float* wsTable, size_t wsTableSize, float sampleRate);
void fuzz_process(Fuzz* fz, const float* in, float* out, size_t numSamples);
void fuzz_set_params(Fuzz* fz, float fuzzAmount, float bias, float outputDb);
void fuzz_set_enabled(Fuzz* fz, bool enabled);


typedef struct {
  ClipperType type;
  float threshold;
  float drive;
  bool enabled;
} Clipper;

void clipper_init(Clipper* clip, ClipperType type, float threshold);
void clipper_process(Clipper* clip, const float* in, float* out, size_t numSamples);
void clipper_set_params(Clipper* clip, float threshold, float drive);
void clipper_set_enabled(Clipper* clip, bool enabled);


typedef struct {
  Biquad lowShelf;
  Biquad midPeak;
  Biquad highShelf;
  bool enabled;
} ThreeBandEQ;

void threebande_init(ThreeBandEQ* eq, float sampleRate);
void threebande_process(ThreeBandEQ* eq, const float* in, float* out, size_t numSamples);
void threebande_set_params(ThreeBandEQ* eq, float lowGainDb, float midGainDb, float highGainDb, float midFreq, float midQ);
void threebande_set_enabled(ThreeBandEQ* eq, bool enabled);


typedef struct {
  OnePole highpass;
  float sampleRate;
  bool enabled;
} HighPassFilter;

void highpass_init(HighPassFilter* hpf, float cutoffHz, float sampleRate);
void highpass_process(HighPassFilter* hpf, const float* in, float* out, size_t numSamples);
void highpass_set_cutoff(HighPassFilter* hpf, float cutoffHz);
void highpass_set_enabled(HighPassFilter* hpf, bool enabled);


typedef struct {
  OnePole lowpass;
  bool enabled;
} LowPassFilter;

void lowpass_init(LowPassFilter* lpf, float cutoffHz, float sampleRate);
void lowpass_process(LowPassFilter* lpf, const float* in, float* out, size_t numSamples);
void lowpass_set_cutoff(LowPassFilter* lpf, float cutoffHz);
void lowpass_set_enabled(LowPassFilter* lpf, bool enabled);


typedef struct {
  CompressorState state;
  float threshold;
  float ratio;
  float makeup;
  float kneeDb;
  bool enabled;
} Compressor;

void compressor_init_interface(Compressor* comp, float attackMs, float releaseMs, float sampleRate);
void compressor_process_interface(Compressor* comp, const float* in, float* out, size_t numSamples);
void compressor_set_params_interface(Compressor* comp, float thresholdDb, float ratio, float makeupDb, float kneeDb);
void compressor_set_enabled(Compressor* comp, bool enabled);


typedef struct {
  CompressorState state;
  float threshold;
  bool enabled;
} Limiter;

void limiter_init(Limiter* lim, float thresholdDb, float attackMs, float releaseMs, float sampleRate);
void limiter_process(Limiter* lim, const float* in, float* out, size_t numSamples);
void limiter_set_threshold(Limiter* lim, float thresholdDb);
void limiter_set_enabled(Limiter* lim, bool enabled);


typedef struct {
  DelayLine delayLine;
  float delaySamples;
  float feedback;
  float dryWet;
  bool enabled;
} Delay;

void delay_init(Delay* dly, float* bufferMemory, size_t bufferSize, float maxDelayMs, float sampleRate);
void delay_process(Delay* dly, const float* in, float* out, size_t numSamples);
void delay_set_params(Delay* dly, float delayMs, float feedback, float dryWetMix);
void delay_set_enabled(Delay* dly, bool enabled);


typedef struct {
  SimpleReverb reverb;
  float roomSize;
  float damping;
  float width;
  float dryWet;
  bool enabled;
} Reverb;

void reverb_init_interface(Reverb* rev, float* bufferMemory, size_t bufferSize, float sampleRate);
void reverb_process_interface(Reverb* rev, const float* in, float* out, size_t numSamples);
void reverb_set_params_interface(Reverb* rev, float roomSize, float damping, float width, float dryWetMix);
void reverb_set_enabled(Reverb* rev, bool enabled);


typedef struct {
  DelayLine delayLine;
  LFO lfo;
  float baseDelaySamples;
  float depthSamples;
  float dryWet;
  float feedback;
  bool enabled;
} Chorus;

void chorus_init(Chorus* ch, float* bufferMemory, size_t bufferSize, 
                 float maxDelayMs, float sampleRate);
void chorus_process(Chorus* ch, const float* in, float* out, size_t numSamples);
void chorus_set_params(Chorus* ch, float rateHz, float depthMs, float dryWetMix);
void chorus_set_enabled(Chorus* ch, bool enabled);


typedef struct {
  DelayLine delayLine;
  LFO lfo;
  float baseDelaySamples;
  float depthSamples;
  float feedback;
  float dryWet;
  bool enabled;
} Flanger;

void flanger_init(Flanger* fl, float* bufferMemory, size_t bufferSize, float sampleRate);
void flanger_process(Flanger* fl, const float* in, float* out, size_t numSamples);
void flanger_set_params(Flanger* fl, float rateHz, float depthMs, float feedback, 
                        float dryWetMix);
void flanger_set_enabled(Flanger* fl, bool enabled);


typedef struct {
  AllPass1 allpassStages[4];
  LFO lfo;
  float feedback;
  float dryWet;
  bool enabled;
} Phaser;

void phaser_init(Phaser* ph, float sampleRate);
void phaser_process(Phaser* ph, const float* in, float* out, size_t numSamples);
void phaser_set_params(Phaser* ph, float rateHz, float feedback, float dryWetMix);
void phaser_set_enabled(Phaser* ph, bool enabled);


typedef struct {
  LFO lfo;
  float depth;
  bool enabled;
} Tremolo;

void tremolo_init(Tremolo* tr, float sampleRate);
void tremolo_process(Tremolo* tr, const float* in, float* out, size_t numSamples);
void tremolo_set_params(Tremolo* tr, float rateHz, float depth);
void tremolo_set_enabled(Tremolo* tr, bool enabled);


typedef struct {
  TubePreamp preamp;
  float gain;
  float lowGain;
  float midGain;
  float highGain;
  float sagAmount;
  bool enabled;
} PreampSimulator;

void preamp_init(PreampSimulator* preamp, float* wsTable, size_t wsTableSize, float sampleRate);
void preamp_process(PreampSimulator* preamp, const float* in, float* out, size_t numSamples);
void preamp_set_gain(PreampSimulator* preamp, float gainDb);
void preamp_set_tone_stack(PreampSimulator* preamp, float lowDb, float midDb, float highDb);
void preamp_set_sag(PreampSimulator* preamp, float sagAmount);
void preamp_set_enabled(PreampSimulator* preamp, bool enabled);


typedef struct {
  float* waveshapeTable;
  size_t waveshapeTableSize;
  float sagAmount;
  float sagTimeConstant;
  float supplyVoltage;
  float supplyFilter;
  float outputGain;
  bool enabled;
} PowerAmpSimulator;

void poweramp_init(PowerAmpSimulator* poweramp, float* wsTable, size_t wsTableSize, float sampleRate);
void poweramp_process(PowerAmpSimulator* poweramp, const float* in, float* out, size_t numSamples);
void poweramp_set_params(PowerAmpSimulator* poweramp, float sagAmount, float outputDb);
void poweramp_set_enabled(PowerAmpSimulator* poweramp, bool enabled);


typedef struct {
  Biquad resonance;
  Biquad presence;
  Biquad damping;
  bool enabled;
} CabinetSimulator;

void cabinet_init(CabinetSimulator* cab, float sampleRate);
void cabinet_process(CabinetSimulator* cab, const float* in, float* out, size_t numSamples);
void cabinet_set_type(CabinetSimulator* cab, int cabinetType);
void cabinet_set_enabled(CabinetSimulator* cab, bool enabled);


typedef struct {
  // Input stage
  NoiseGate noisegate;
  HighPassFilter inputHighpass;
  
  // Preamp effects
  Overdrive overdrive;
  Distortion distortion;
  Fuzz fuzz;
  
  // Tone shaping
  ThreeBandEQ eq;
  Compressor compressor;
  
  // Modulation effects
  Chorus chorus;
  Flanger flanger;
  Phaser phaser;
  Tremolo tremolo;
  
  // Time-based effects
  Delay delay;
  Reverb reverb;
  
  // Amp simulation
  PreampSimulator preamp;
  PowerAmpSimulator poweramp;
  CabinetSimulator cabinet;
  
  // Output protection
  Limiter limiter;
  
  // Configuration
  float sampleRate;
  bool bypass;
} AmpChain;

void ampchain_init(AmpChain* chain, float* memoryPool, size_t memorySize, float sampleRate);
void ampchain_process(AmpChain* chain, const float* in, float* out, size_t numSamples);
void ampchain_set_bypass(AmpChain* chain, bool bypass);
void ampchain_reset_all(AmpChain* chain);

#endif /* EFFECTS_INTERFACE_H */