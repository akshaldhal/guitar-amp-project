#ifndef EFFECTS_INTERFACE_H
#define EFFECTS_INTERFACE_H

#include "effects_dsp.h"
#include <stddef.h>
#include <stdbool.h>

// ==============================================================
// Forward declarations (required before structs reference each other)
// ==============================================================

// typedef struct DSPState DSPState;
typedef struct Effect Effect;

typedef void (*EffectProcessFunc)(Effect* fx, const float* in, float* out, size_t n);
typedef void (*EffectCleanupFunc)(Effect* fx);

// ==============================================================
// EFFECT TYPES
// ==============================================================

typedef enum {
    FX_NONE = 0,
    FX_NOISEGATE,
    FX_COMPRESSOR,
    FX_OVERDRIVE,
    FX_DISTORTION,
    FX_FUZZ,
    FX_BOOST,
    FX_TUBESCREAMER,
    FX_CHORUS,
    FX_FLANGER,
    FX_PHASER,
    FX_TREMOLO,
    FX_VIBRATO,
    FX_DELAY,
    FX_REVERB,
    FX_WAH,
    FX_EQ_3BAND,
    FX_EQ_PARAMETRIC,
    FX_PREAMP,
    FX_POWERAMP,
    FX_CABINET
} EffectType;

// ==============================================================
// EFFECT STRUCTURES
// ==============================================================

struct Effect {
    EffectType type;
    bool enabled;
    bool bypass;
    DSPState* dsp;
    void* data;
    EffectProcessFunc process;
    EffectCleanupFunc cleanup;
    Effect* next;
};

typedef struct {
    DSPState* dsp;
    Effect* head;
    Effect* tail;
    float* chainBuffer;
    size_t bufferSize;
} EffectChain;

// ==============================================================
// TUBE PRESETS
// ==============================================================

typedef struct {
    const char* name;
    TubeType type;
    TubeParams params;
    float platV;
    float screenV;
} TubeDef;

#define NUM_TUBE_PRESETS 6
#define TUBE_TABLE_SIZE 8192

static const TubeDef tube_presets[NUM_TUBE_PRESETS] = {
    {"6DJ8",  TUBE_TRIODE,  {28.0f, 1.3f, 330.0f, 4500.0f, 320.0f, 300.0f}, 330.0f, 0.0f},
    {"6L6CG", TUBE_PENTODE, {8.7f, 1.35f, 1460.0f, 4500.0f, 48.0f, 12.0f},   330.0f, 330.0f},
    {"12AX7", TUBE_TRIODE,  {100.0f, 1.4f, 1060.0f, 4200.0f, 600.0f, 300.0f},330.0f, 0.0f},
    {"12AU7", TUBE_TRIODE,  {21.5f, 1.3f, 1180.0f, 4800.0f, 84.0f, 300.0f}, 330.0f, 0.0f},
    {"6550",  TUBE_PENTODE, {7.9f, 1.35f, 890.0f, 4800.0f, 60.0f, 24.0f},    500.0f, 500.0f},
    {"KT88",  TUBE_PENTODE, {8.8f, 1.35f, 730.0f, 4200.0f, 32.0f, 16.0f},    500.0f, 500.0f}
};

// ==============================================================
// EFFECT DATA STRUCTS
// ==============================================================
typedef struct {
    EnvelopeDetector env;
    float threshold;
    float holdSamples;
    float holdCounter;
    float attenuation;
} NoiseGateData;

typedef struct {
    EnvelopeDetector env;
    float threshold;
    float ratio;
    float makeup;
    float kneeWidth;
    float prevGain;
} CompressorData;

typedef struct {
    OnePole hpf;
    Biquad tone;
    float drive;
    float outputGain;
    int wsTableIdx;
} OverdriveData;

typedef struct {
    OnePole hpf;
    Biquad toneStack[3];
    float drive;
    float outputGain;
    int wsTableIdx;
} DistortionData;

typedef struct {
    OnePole hpf;
    float drive;
    float outputGain;
    int wsTableIdx;
} FuzzData;

typedef struct {
    float gain;
} BoostData;

typedef struct {
    OnePole hpf;
    Biquad midBoost;
    float drive;
    float tone;
    float outputGain;
    int wsTableIdx;
} TubeScreamerData;

typedef struct {
    DelayLine delayLines[2];
    LFO lfo;
    float depth;
    float mix;
    float* delayBufs[2];
} ChorusData;

typedef struct {
    DelayLine delayLines[2];
    LFO lfo;
    float depth;
    float feedback;
    float mix;
    float* delayBufs[2];
} FlangerData;

typedef struct {
    AllPassDelay allpass[4];
    LFO lfo;
    float depth;
    float feedback;
    float mix;
    float* apBufs[4];
} PhaserData;

typedef struct {
    LFO lfo;
    float depth;
} TremoloData;

typedef struct {
    DelayLine delayLine;
    LFO lfo;
    float depth;
    float* delayBuf;
} VibratoData;

typedef struct {
    DelayLine delayLine;
    Biquad dampFilter;
    float delayTime;
    float feedback;
    float mix;
    float* delayBuf;
} DelayData;

typedef struct {
    DelayLine delays[8];
    Biquad damping[8];
    float decay;
    float mix;
    float* delayBufs[8];
} ReverbData;

typedef struct {
    Biquad wahFilter;
    EnvelopeDetector env;
    float freq;
    float Q;
    float sensitivity;
} WahData;

typedef struct {
    Biquad bass;
    Biquad mid;
    Biquad treble;
} EQ3BandData;

typedef struct {
    Biquad filters[4];
    float freqs[4];
    float Qs[4];
    float gains[4];
} EQParametricData;

typedef struct {
    Biquad inputHPF;
    Biquad toneStack[3];
    float inputGain;
    float drive;
    float outputGain;
    float sagAmount;
    float sagState;
    int tubeTableIdx;
} PreampData;

typedef struct {
    float drive;
    float outputGain;
    float sagAmount;
    float sagTime;
    float sagState;
    float supplyV;
    int tubeTableIdx;
} PowerampData;

typedef struct {
    Biquad lowResonance;
    Biquad midPresence;
    Biquad highDamping;
    int cabinetType;
} CabinetData;

// ==============================================================
// CHAIN MANAGEMENT
// ==============================================================

void effect_chain_init(EffectChain* chain, DSPState* dsp, size_t maxBlockSize);
void effect_chain_cleanup(EffectChain* chain);
void effect_chain_process(EffectChain* chain, const float* in, float* out, size_t n);
Effect* effect_chain_add(EffectChain* chain, EffectType type);
void effect_chain_remove(EffectChain* chain, Effect* fx);
void effect_chain_clear(EffectChain* chain);
Effect* effect_chain_find(EffectChain* chain, EffectType type);
void effect_chain_move(EffectChain* chain, Effect* fx, int newPosition);
int effect_chain_count(EffectChain* chain);
Effect* effect_chain_get_at(EffectChain* chain, int index);
void effect_chain_enable_all(EffectChain* chain, bool enabled);
void effect_chain_bypass_all(EffectChain* chain, bool bypass);

// ==============================================================
// EFFECT CONTROL
// ==============================================================

void effect_enable(Effect* fx, bool enabled);
void effect_bypass(Effect* fx, bool bypass);
bool effect_is_enabled(Effect* fx);
bool effect_is_bypassed(Effect* fx);
const char* effect_type_name(EffectType type);

// ==============================================================
// EFFECT PARAMETER SETTERS
// ==============================================================

void fx_noisegate_set(Effect* fx, float threshDb, float attackMs, float releaseMs, float holdMs);
void fx_compressor_set(Effect* fx, float threshDb, float ratio, float makeupDb,
                       float kneeDb, float attackMs, float releaseMs);
void fx_overdrive_set(Effect* fx, float driveDb, float toneHz, float outputDb);
void fx_distortion_set(Effect* fx, float driveDb, float bassDb, float midDb,
                       float trebleDb, float outputDb);
void fx_fuzz_set(Effect* fx, float driveDb, float outputDb);
void fx_boost_set(Effect* fx, float gainDb);
void fx_tubescreamer_set(Effect* fx, float driveDb, float tone, float outputDb);
void fx_chorus_set(Effect* fx, float rateHz, float depthMs, float mix);
void fx_flanger_set(Effect* fx, float rateHz, float depthMs, float feedback, float mix);
void fx_phaser_set(Effect* fx, float rateHz, float depth, float feedback, float mix);
void fx_tremolo_set(Effect* fx, float rateHz, float depth);
void fx_vibrato_set(Effect* fx, float rateHz, float depthMs);
void fx_delay_set(Effect* fx, float timeSec, float feedback, float dampHz, float mix);
void fx_reverb_set(Effect* fx, float decay, float dampHz, float mix);
void fx_wah_set(Effect* fx, float freq, float Q, float sensitivity);
void fx_eq3band_set(Effect* fx, float bassDb, float midDb, float trebleDb);
void fx_eqparametric_set_band(Effect* fx, int band, float freqHz, float Q, float gainDb);
void fx_preamp_set(Effect* fx, float inputDb, float driveDb, float bassDb,
                   float midDb, float trebleDb, float outputDb, float sag, int tubeIdx);
void fx_poweramp_set(Effect* fx, float driveDb, float outputDb, float sag,
                     float sagTimeMs, int tubeIdx);
void fx_cabinet_set(Effect* fx, int cabinetType);

// ==============================================================
// PRESET CHAINS
// ==============================================================

void fx_chain_preset_clean(EffectChain* chain);
void fx_chain_preset_crunch(EffectChain* chain);
void fx_chain_preset_lead(EffectChain* chain);
void fx_chain_preset_metal(EffectChain* chain);
void fx_chain_preset_fuzz(EffectChain* chain);
void fx_chain_preset_ambient(EffectChain* chain);
void fx_chain_preset_blues(EffectChain* chain);
void fx_chain_preset_shoegaze(EffectChain* chain);
void fx_chain_preset_funk(EffectChain* chain);


// int portaudio_handler_get_tube_preset_count(void);
// const TubeDef* portaudio_handler_get_tube_preset(int index);


#endif /* EFFECTS_INTERFACE_H */
