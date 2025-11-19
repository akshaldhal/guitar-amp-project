#ifndef EFFECTS_DSP_H
#define EFFECTS_DSP_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

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

typedef struct {
  float a0;    /* filter coefficient */
  float z1;    /* state */
} OnePole;

void onepole_init(OnePole* f, float cutoffHz, float sampleRate, int isHighPass);
float onepole_process(OnePole* f, float in);

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
  float a0, a1, a2;   /* numerator (stored normalized) */
  float b1, b2;     /* denominator (note b0 normalized to 1) */
  float z1, z2;     /* states */
} Biquad;

/* Initialize biquad with frequency (Hz), Q, gainDb (for shelves/peaks), and type */
void biquad_init(Biquad* bq, BiquadType type, float freqHz, float Q, float gainDb, float sampleRate);
float biquad_process(Biquad* bq, float in);

typedef struct {
  float a;  /* feedback coefficient */
  float z1;   /* state */
} AllPass1;

void allpass1_init(AllPass1* ap, float delaySamples, float feedback); /* delaySamples may be used for advanced forms; simple variant uses feedback */
float allpass1_process(AllPass1* ap, float in);

typedef struct {
  float *buffer;   /* circular buffer pointer (owned by caller or allocator) */
  size_t size;     /* buffer size in samples (power-of-two recommended) */
  size_t writeIndex;
  float sampleRate;
} DelayLine;

/* Initialize expects an allocated buffer (size samples). Implementations should not allocate per-sample. */
void delayline_init(DelayLine* dl, float* bufferMemory, size_t size, float sampleRate);
void delayline_write(DelayLine* dl, float sample);
float delayline_read_linear(DelayLine* dl, float delaySamples);   /* linear interpolation */
float delayline_read_cubic(DelayLine* dl, float delaySamples);    /* cubic interpolation */

/* Helper interpolation functions */
float lerp(float a, float b, float t);
float cubic_interp(float ym1, float y0, float y1, float y2, float t);

typedef enum {
  LFO_SINE,
  LFO_TRI,
  LFO_SAW,
  LFO_SQUARE,
  LFO_NOISE
} LFOType;

typedef struct {
  float phase;    /* 0..1 */
  float freq;     /* Hz */
  float sampleRate;
  float amp;      /* amplitude 0..1 */
  float dc;       /* DC offset */
  LFOType type;
} LFO;

void lfo_init(LFO* lfo, LFOType type, float freqHz, float amp, float dc, float sampleRate);
float lfo_process(LFO* lfo);   /* returns -amp..+amp plus dc */

typedef struct {
  float env;       /* current envelope (linear) */
  float attackCoeff;   /* per-sample coefficient */
  float releaseCoeff;  /* per-sample coefficient */
  float sampleRate;
  int isRMS;       /* if set, follows RMS envelope (squared input) else peak (abs) */
} EnvelopeDetector;

/* attackMs / releaseMs in milliseconds */
void env_init(EnvelopeDetector* ed, float attackMs, float releaseMs, float sampleRate, int isRMS);
float env_process(EnvelopeDetector* ed, float in);

/* Compute gain reduction in linear domain given input level (dB), threshold (dB), ratio (e.g., 4.0 for 4:1) */
float compute_gain_reduction_db(float inputDb, float thresholdDb, float ratio);

/* Apply gain smoothing (attack/release) to target gain reduction (in linear) */
float apply_gain_smoothing(float currentGain, float targetGain, float attackCoeff, float releaseCoeff);

/* Utility: convert ms -> per-sample coeff (exp curve) */
float ms_to_coeff(float ms, float sampleRate);

typedef enum {
  CLIP_HARD,
  CLIP_SOFT_TANH,
  CLIP_ARCTAN,
  CLIP_SIGMOID,     /* arbitrary soft shape */
  CLIP_CUSTOM_TABLE
} ClipperType;

/* Simple per-sample clippers */
float hard_clip(float x, float threshold);  /* threshold linear amplitude */
float tanh_clip(float x, float drive);    /* uses tanhf(drive * x) approximator, normalized */
float arctan_clip(float x, float drive);

/* Waveshaper table: fill an array lookupTable of size n with curve values for x in [-1..1] */
void build_waveshaper_table(float *lookupTable, size_t n, ClipperType type, float drive);

/* Table lookup (linear interp) */
float waveshaper_lookup(float *lookupTable, size_t n, float x);

/* Prepare buffers for oversampling. These functions are helpers — implementations can be expensive. */
void oversample2x(const float *in, float *out, size_t n);    /* upsample by 2 (out size = 2*n) */
void downsample2x(const float *in, float *out, size_t n);    /* downsample by 2 (in size = 2*n -> out size = n) */

typedef struct {
  float *combBuffers;   /* preallocated memory (caller-managed) */
  size_t *combSizes;
  size_t numCombs;
  float *combFeedbacks;
  float *apBuffer;    /* all-pass buffers */
  size_t *apSizes;
  size_t numAP;
  float wet;        /* wet mix 0..1 */
  float dry;        /* dry mix 0..1 */
  float sampleRate;
} SimpleReverb;

/* init with arrays preallocated by caller (avoids dynamic alloc in realtime) */
void reverb_init(SimpleReverb* r, float sampleRate, float wet, float dry);
float reverb_process_sample(SimpleReverb* r, float in);

typedef struct {
  DelayLine dl;
  float feedback;
} CombFilter;

void comb_init(CombFilter* cf, float* bufferMemory, size_t size, float feedback, float sampleRate);
float comb_process(CombFilter* cf, float in);

/* Convolve single input block with IR using naive conv (use FFT for long IRs in .c implementation).
 * in: input buffer, inLen
 * ir: impulse response, irLen
 * out: output buffer, outLen must be >= inLen + irLen - 1
 */
void convolve_naive(const float* in, size_t inLen, const float* ir, size_t irLen, float* out);

/* --- FFT wrapper interface (optional) ---
 * Provide wrappers to FFT/IFFT (user can implement using kissfft/fftpack/fftw etc.)
 * Note: if you do not plan FFT, you can leave these unimplemented and use convolve_naive for short IRs.
 */
int fft_init(int size); /* returns 0 on success */
int fft_forward(const float* timeBuf, float* freqBuf, int size);
int fft_inverse(const float* freqBuf, float* timeBuf, int size);

/* Basic granular pitch-shift helper: expects preallocated ring buffer & window */
typedef struct {
  DelayLine dl;
  float *window;    /* Hann window array (len = windowSize) */
  size_t windowSize;
  float hop;      /* in samples */
  float sampleRate;
} GranularPS;

void granular_init(GranularPS* ps, float* bufferMemory, size_t bufferSize, float* windowMemory, size_t windowSize, float hop, float sampleRate);
float granular_process_sample(GranularPS* ps, float in, float pitchRatio); /* returns processed sample */

/* Utility: build hann window */
void build_hann_window(float* w, size_t n);

float white_noise();  /* returns -1..+1 (simple LCG internal state in .c file) */

void apply_window_inplace(float* buffer, const float* window, size_t n);

float hz_to_omega(float hz, float sampleRate);

/* Compute per-sample coefficient for exponential smoothing from ms value:
 * coeff = exp(-1/(ms*sampleRate/1000)) */
static inline float ms_to_coef(float ms, float sampleRate) {
  if (ms <= 0.0f) return 0.0f;
  return expf(-1.0f / ((ms * 0.001f) * sampleRate));
}

#endif
