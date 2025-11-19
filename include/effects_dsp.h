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
  if (lin <= 0.0f) return -INFINITY;
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
} OnePole;

void onepole_init(OnePole* f, float cutoffHz, float sampleRate, int isHighPass);
void onepole_process(OnePole* f, const float* in, float* out, size_t numSamples);

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

// Math function
void biquad_init(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate);
void biquad_process(Biquad* bq, const float* in, float* out, size_t numSamples);

typedef struct {
  float a;
  float z1;
} AllPass1;

// Math function
void allpass1_init(AllPass1* ap, float delaySamples, float feedback);
void allpass1_process(AllPass1* ap, const float* in, float* out, size_t numSamples);

typedef struct {
  float *buffer;
  size_t size;
  size_t writeIndex;
  float sampleRate;
} DelayLine;

// Math function
void delayline_init(DelayLine* dl, float* bufferMemory, size_t size, float sampleRate);
void delayline_write(DelayLine* dl, const float* samples, size_t numSamples);
void delayline_read_linear(DelayLine* dl, float* out, size_t numSamples, float delaySamples);
void delayline_read_cubic(DelayLine* dl, float* out, size_t numSamples, float delaySamples);

// Math function
void lerp(const float* a, const float* b, const float* t, float* out, size_t numSamples);
// Math function
void cubic_interp(const float* ym1, const float* y0, const float* y1, const float* y2, const float* t, float* out, size_t numSamples);

typedef enum {
  LFO_SINE,
  LFO_TRI,
  LFO_SAW,
  LFO_SQUARE,
  LFO_NOISE
} LFOType;

typedef struct {
  float phase;
  float freq;
  float sampleRate;
  float amp;
  float dc;
  LFOType type;
} LFO;

// Math function
void lfo_init(LFO* lfo, LFOType type, float freqHz, float amp, float dc, float sampleRate);
void lfo_process(LFO* lfo, float* out, size_t numSamples);

typedef struct {
  float env;
  float attackCoeff;
  float releaseCoeff;
  float sampleRate;
  int isRMS;
} EnvelopeDetector;

// Math function
void env_init(EnvelopeDetector* ed, float attackMs, float releaseMs, float sampleRate, int isRMS);
void env_process(EnvelopeDetector* ed, const float* in, float* out, size_t numSamples);

// Math function
void compute_gain_reduction_db(const float* inputDb, const float* thresholdDb, float ratio, float* out, size_t numSamples);

// Math function
void apply_gain_smoothing(float* currentGain, const float* targetGain, float attackCoeff, float releaseCoeff, size_t numSamples);

// Math function
float ms_to_coeff(float ms, float sampleRate);

typedef enum {
  CLIP_HARD,
  CLIP_SOFT_TANH,
  CLIP_ARCTAN,
  CLIP_SIGMOID,
  CLIP_CUSTOM_TABLE
} ClipperType;

// Model from real amps
void hard_clip(const float* in, float threshold, float* out, size_t numSamples);
// Model from real amps
void tanh_clip(const float* in, float drive, float* out, size_t numSamples);
// Model from real amps
void arctan_clip(const float* in, float drive, float* out, size_t numSamples);

// Model from real amps
void build_waveshaper_table(float *lookupTable, size_t tableSize, ClipperType type, float drive);

// Math function
void waveshaper_lookup(const float* in, float* out, const float* lookupTable, size_t tableSize, size_t numSamples);

// Math function
void oversample2x(const float *in, float *out, size_t inLen);
// Math function
void downsample2x(const float *in, float *out, size_t inLen);

typedef struct {
  float *combBuffers;
  size_t *combSizes;
  size_t numCombs;
  float *combFeedbacks;
  float *apBuffer;
  size_t *apSizes;
  size_t numAP;
  float wet;
  float dry;
  float sampleRate;
} SimpleReverb;

// Model from real amps
void reverb_init(SimpleReverb* r, float sampleRate, float wet, float dry);
void reverb_process(SimpleReverb* r, const float* in, float* out, size_t numSamples);

typedef struct {
  DelayLine dl;
  float feedback;
} CombFilter;

// Model from real amps
void comb_init(CombFilter* cf, float* bufferMemory, size_t bufferSize, float feedback, float sampleRate);
void comb_process(CombFilter* cf, const float* in, float* out, size_t numSamples);

typedef struct {
  float **partitions;
  float **fftBuffers;
  size_t partitionSize;
  size_t numPartitions;
  size_t currentPartition;
  size_t irLength;
  float *irBuffer;
  size_t irBufferSize;
  float *overlapBuffer;
  size_t overlapSize;
  float sampleRate;
  int irSampleRate;
  float dryMix;
  float wetMix;
  int isActive;
} Convolver;

// Math function
int convolver_init(Convolver* conv, float sampleRate, size_t partitionSize, size_t maxIrLength);
// Math function
int convolver_set_ir(Convolver* conv, const float* irData, size_t irLength, int irSampleRate);
// Math function
void convolver_process(Convolver* conv, const float* in, float* out, size_t numSamples);
// Math function
size_t convolver_get_latency_samples(const Convolver* conv);
// Math function
void convolver_set_mix(Convolver* conv, float dryDb, float wetDb);
// Math function
void convolver_reset(Convolver* conv);
// Math function
void convolver_destroy(Convolver* conv);

// Math function
int fft_init(int size);
// Math function
int fft_forward(const float* timeBuf, float* freqBuf, int size);
// Math function
int fft_inverse(const float* freqBuf, float* timeBuf, int size);

typedef struct {
  DelayLine dl;
  float *window;
  size_t windowSize;
  float hop;
  float sampleRate;
} GranularPS;

// Model from real amps
void granular_init(GranularPS* ps, float* bufferMemory, size_t bufferSize, float* windowMemory, size_t windowSize, float hop, float sampleRate);
void granular_process(GranularPS* ps, const float* in, float* out, size_t numSamples, float pitchRatio);

// Math function
void build_hann_window(float* w, size_t n);

// Math function
void white_noise(float* out, size_t n);

// Math function
void apply_window_inplace(float* buffer, const float* window, size_t n);

// Math function
float hz_to_omega(float hz, float sampleRate);

static inline float ms_to_coef(float ms, float sampleRate) {
  if (ms <= 0.0f) return 0.0f;
  return expf(-1.0f / ((ms * 0.001f) * sampleRate));
}

#endif