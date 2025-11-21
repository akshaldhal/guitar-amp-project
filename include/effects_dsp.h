#ifndef EFFECTS_DSP_H
#define EFFECTS_DSP_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <simde/x86/avx2.h>

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


static inline float clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi) ? hi : x;
}

static inline float db_to_linear(float db) {
  return powf(10.0f, (db) / 20.0f);
}

static inline float linear_to_db(float lin) {
  if (lin <= 0.0f)
    return -INFINITY;
  return 20.0f * log10f(lin);
}

#ifndef EPSILON_F
#define EPSILON_F 1e-12f
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct {
  float a0;
  float b0;
  float b1;
  float z1;
  int isHighPass;
} OnePole;

void onepole_init(OnePole* f, float cutoffHz, float sampleRate, int isHighPass);
void onepole_process(OnePole* f, const float* in, float* out, size_t numSamples);
void onepole_set_cutoff(OnePole* f, float cutoffHz, float sampleRate);

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
  float a1, a2;
  float b0, b1, b2;
  float z1, z2;
} Biquad;

void biquad_init(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate);
void biquad_process(Biquad* bq, const float* in, float* out, size_t numSamples);
void biquad_process_inplace(Biquad* bq, float* buffer, size_t numSamples);
void biquad_set_params(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate);

typedef struct {
  float g;
  float x_prev;
  float y_prev;
} AllPass1;

void allpass1_init(AllPass1* ap, float feedback);
void allpass1_process(AllPass1* ap, const float* in, float* out, size_t numSamples);

typedef struct {
  float* buffer;
  size_t size;
  size_t writeIndex;
  float sampleRate;
} DelayLine;

void delayline_init(DelayLine* dl, float* bufferMemory, size_t size, float sampleRate);
void delayline_write(DelayLine* dl, const float* samples, size_t numSamples);
void delayline_read_linear(DelayLine* dl, float* out, size_t numSamples, float delaySamples);
void delayline_read_cubic(DelayLine* dl, float* out, size_t numSamples, float delaySamples);;

static inline float lerp_scalar(float a, float b, float t);
void lerp(const float* a, const float* b, const float* t, float* out, size_t numSamples);
static inline float cubic_interp_scalar(float ym1, float y0, float y1, float y2, float t);
void cubic_interp(const float* ym1, const float* y0, const float* y1, const float* y2, const float* t, float* out, size_t numSamples);
void crossfade(const float* a, const float* b, const float* t, float* out, size_t numSamples);

typedef enum {
  LFO_SINE,
  LFO_TRI,
  LFO_SAW,
  LFO_SQUARE,
  LFO_NOISE
} LFOType;

typedef struct {
  float phase;
  float phase_inc;
  float freq;
  float sampleRate;
  float amp;
  float dc;
  LFOType type;
} LFO;

void lfo_init(LFO* lfo, LFOType type, float freqHz, float amp, float dc, float sampleRate);
void lfo_process(LFO* lfo, float* out, size_t numSamples);
void lfo_set_freq(LFO* lfo, float freqHz);

typedef struct {
  float env;
  float attackCoeff;
  float releaseCoeff;
  float sampleRate;
  int isRMS;
} EnvelopeDetector;

void env_init(EnvelopeDetector* ed, float attackMs, float releaseMs, float sampleRate, int isRMS);
void env_process(EnvelopeDetector* ed, const float* in, float* out, size_t numSamples);

void compute_gain_reduction_db(const float* inputDb, const float* thresholdDb, float ratio, float* out, size_t numSamples);

void apply_gain_smoothing(float* currentGain, const float* targetGain, float* state, float attackCoeff, float releaseCoeff, size_t numSamples);

float ms_to_coeff(float ms, float sampleRate);

typedef enum {
  CLIP_HARD,
  CLIP_SOFT_TANH,
  CLIP_ARCTAN,
  CLIP_SIGMOID,
  CLIP_CUBIC_SOFT
} ClipperType;

void hard_clip(const float* in, float threshold, float* out, size_t numSamples);
void tanh_clip(const float* in, float drive, float* out, size_t numSamples);
void arctan_clip(const float* in, float drive, float* out, size_t numSamples);

void build_waveshaper_table(float* lookupTable, size_t tableSize, ClipperType type, float drive);
void waveshaper_lookup(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);
void waveshaper_lookup_linear(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);
void waveshaper_lookup_cubic(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);

void oversample2x(const float* in, float* out, size_t inLen);
void oversample2x_fir(const float* in, float* out, size_t inLen, const float* fir, size_t firLen);
void downsample2x(const float* in, float* out, size_t inLen);
void downsample2x_fir(const float* in, float* out, size_t inLen, const float* fir, size_t firLen);

void denormal_fix_inplace(float* buffer, size_t n);

typedef enum {
  TUBE_TRIODE,
  TUBE_PENTODE
} TubeType;

typedef struct {
  float mu;
  float k;
  float a;
  float Kg1;
  float Rp;
  float biasV;
} TubeParams;

void build_triode_table(float* table, size_t tableSize, const TubeParams* params, float vMin, float vMax);
void build_pentode_table(float* table, size_t tableSize, const TubeParams* params, float vMin, float vMax);
void build_tube_table_from_koren(float* table, size_t tableSize, TubeType type, const TubeParams* params, float vMin, float vMax);

void normalize_ir(float* ir, size_t n, float targetRMS);
void build_blackman_window(float* w, size_t n);
void build_hann_window(float* w, size_t n);

void white_noise(float* out, size_t n);

void apply_window_inplace(float* buffer, const float* window, size_t n);

float hz_to_omega(float hz, float sampleRate);

#endif