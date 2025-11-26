#ifndef EFFECTS_DSP_H
#define EFFECTS_DSP_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include <simde/x86/avx2.h>
#include <logger.h>

#ifndef SIMD_WIDTH
#ifdef __AVX512F__
#define SIMD_WIDTH 16
#elif defined(__AVX2__)
#define SIMD_WIDTH 8
#elif defined(__AVX__)
#define SIMD_WIDTH 8
#elif defined(__SSE2__)
#define SIMD_WIDTH 4
#elif defined(__ARM_NEON__)
#define SIMD_WIDTH 4
#else
#define SIMD_WIDTH 1
#endif
#endif

#define INF FLT_MAX

#ifndef EPSILON_F
#define EPSILON_F 1e-12f
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define NUM_SCRATCH_BUFFERS 8

static inline float clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi) ? hi : x;
}

static inline float db_to_linear(float db) {
  return powf(10.0f, (db) / 20.0f);
}

static inline float linear_to_db(float lin) {
  if (lin <= 0.0f)
    return -INF;
  return 20.0f * log10f(lin);
}

typedef struct {
  float sampleRate;
  uint32_t numChannels;
  uint32_t blockSize;
  
  float* scratch[NUM_SCRATCH_BUFFERS];
  size_t scratchSize;
} DSPState;

typedef struct {
  DSPState* state;
  float a0;
  float b0;
  float b1;
  float z1;
  int isHighPass;
} OnePole;

typedef enum {
  BQ_LPF,
  BQ_HPF,
  BQ_BPF,
  BQ_NOTCH,
  BQ_PEAK,
  BQ_LOWSHELF,
  BQ_HIGHSHELF,
} BiquadType;

typedef struct {
  DSPState* state;
  float a1, a2;
  float b0, b1, b2;
  float z1, z2;
} Biquad;

typedef struct {
  float* buffer;
  size_t bufferLength;
  size_t index;
  float g;
} AllPassDelay;

typedef struct {
  DSPState* state;
  float* buffer;
  size_t size;
  size_t writeIndex;
} DelayLine;

typedef enum {
  LFO_SINE,
  LFO_TRI,
  LFO_SAW,
  LFO_SQUARE,
  LFO_NOISE
} LFOType;

typedef struct {
  DSPState* state;
  float phase;
  float phase_inc;
  float freq;
  float amp;
  float dc;
  LFOType type;
} LFO;

typedef struct {
  DSPState* state;
  float env;
  float attackCoeff;
  float releaseCoeff;
  int isRMS;
} EnvelopeDetector;

typedef enum {
  CLIP_HARD,
  CLIP_SOFT_TANH,
  CLIP_ARCTAN,
  CLIP_SIGMOID,
  CLIP_CUBIC_SOFT
} ClipperType;

typedef enum {
  TUBE_TRIODE,
  TUBE_PENTODE
} TubeType;

typedef struct {
  float mu;    // Amplification factor
  float x;     // Exponent
  float kg1;   // Plate current fitting factor
  float kg2;   // Screen grid current fitting factor (for screen current only)
  float kp;    // Plate voltage parameter
  float kvb;   // Knee voltage parameter
} TubeParams;


// implement as needed
void dsp_state_init(DSPState* state, float sampleRate, uint32_t numChannels, uint32_t blockSize);
void dsp_state_cleanup(DSPState* state);
void dsp_state_grow_scratches(DSPState* state, size_t newSize);


void onepole_init(OnePole* f, DSPState* state, float cutoffHz, int isHighPass);
void onepole_process(OnePole* f, const float* in, float* out, size_t numSamples);
void onepole_set_cutoff(OnePole* f, float cutoffHz);

void biquad_init(Biquad* bq, DSPState* state, BiquadType type, float freqHz, float Q, float gainDb);
void biquad_process(Biquad* bq, const float* in, float* out, size_t numSamples);
void biquad_process_inplace(Biquad* bq, float* buffer, size_t numSamples);
void biquad_set_params(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb);

void allpass_delay_init(AllPassDelay* ap, float* bufferMemory, size_t bufferLength,float feedback);
void allpass_delay_process(AllPassDelay* ap, const float* in, float* out, size_t numSamples);

void delayline_init(DelayLine* dl, DSPState* state, float* bufferMemory, size_t size);
void delayline_write(DelayLine* dl, const float* samples, size_t numSamples);
void delayline_read_linear(DelayLine* dl, float* out, size_t numSamples, float delaySamples);
void delayline_read_cubic(DelayLine* dl, float* out, size_t numSamples, float delaySamples);;

static inline float lerp_scalar(float a, float b, float t);
void lerp(const float* a, const float* b, const float* t, float* out, size_t numSamples);
static inline float cubic_interp_scalar(float ym1, float y0, float y1, float y2, float t);
void cubic_interp(const float* ym1, const float* y0, const float* y1, const float* y2, const float* t, float* out, size_t numSamples);
void crossfade(const float* a, const float* b, const float* t, float* out, size_t numSamples);

void lfo_init(LFO* lfo, DSPState* state, LFOType type, float freqHz, float amp, float dc);
void lfo_process(LFO* lfo, float* out, size_t numSamples);
void lfo_set_freq(LFO* lfo, float freqHz);

void env_init(EnvelopeDetector* ed, DSPState* state, float attackMs, float releaseMs, int isRMS);
void env_process(EnvelopeDetector* ed, const float* in, float* out, size_t numSamples);

float ms_to_coeff(float ms, float sampleRate);

void hard_clip(const float* in, float threshold, float* out, size_t numSamples);
void tanh_clip(const float* in, float drive, float* out, size_t numSamples);
void arctan_clip(const float* in, float drive, float* out, size_t numSamples);

void build_waveshaper_table(float* lookupTable, size_t tableSize, ClipperType type, float drive);
void waveshaper_lookup(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);
void waveshaper_lookup_linear(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);
void waveshaper_lookup_cubic(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);

void build_triode_table(float* table, size_t tableSize, const TubeParams* p, float gridMin, float gridMax, float Ep);
void build_pentode_table(float* table, size_t tableSize, const TubeParams* p, float gridMin, float gridMax, float Eg2, float Ep);
void build_tube_table_from_koren(float* table, size_t tableSize, TubeType type, const TubeParams* p, float vMin, float vMax, float Ep, float Eg2);

void build_hann_window(float* w, size_t n);

void white_noise(float* out, size_t n);

void apply_window_inplace(float* buffer, const float* window, size_t n);

float hz_to_omega(float hz, float sampleRate);



typedef struct {
  DSPState* state;
  Biquad inputHighpass;
  Biquad toneStack[3];
  float* tubeTable;
  size_t tubeTableSize;
  float tubeGain;
  float sagAmount;
  float sagTimeConstant;
  float supplyVoltage;
  float supplyFilter;
} TubePreamp;

typedef struct {
  DSPState* state;
  EnvelopeDetector detector;
  float ratio;
  float threshold;
  float makeup;
  float kneeWidth;
  float previousGain;
} CompressorState;


void apply_gain_smoothing(float* currentGain, const float* targetGain, float* state, float attackCoeff, float releaseCoeff, size_t numSamples);

void compute_gain_reduction_db(const float* inputDb, const float* thresholdDb, float ratio, float* out, size_t numSamples);

void tubepreamp_init(TubePreamp* preamp, DSPState* state, float* wsTable, size_t wsTableSize);
void tubepreamp_process(TubePreamp* preamp, const float* in, float* out, size_t numSamples);
void tubepreamp_set_gain(TubePreamp* preamp, float gainDb);
void tubepreamp_set_bass(TubePreamp* preamp, float gainDb);
void tubepreamp_set_mid(TubePreamp* preamp, float gainDb);
void tubepreamp_set_treble(TubePreamp* preamp, float gainDb);

void compressor_init(CompressorState* comp, DSPState* state, float attackMs, float releaseMs);
void compressor_process(CompressorState* comp, const float* in, float* out, size_t numSamples);
void compressor_set_params(CompressorState* comp, float threshold, float ratio, float makeup);



#endif