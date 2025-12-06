#ifndef PORTAUDIO_HANDLER_H
#define PORTAUDIO_HANDLER_H

#include <portaudio.h>
#include <stdbool.h>
#include <stdint.h>

#include "effects_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PORTAUDIO HANDLER STRUCT
// ============================================================================

typedef struct {
    PaStream* stream;
    PaDeviceIndex inputDevice;
    PaDeviceIndex outputDevice;
    int hostInputChannels;
    int hostOutputChannels;

    double sampleRate;
    uint32_t blockSize;

    DSPState dsp;
    EffectChain chain;

    bool initialized;
} PortAudioHandler;

// ============================================================================
// INITIALIZATION / STREAM LIFECYCLE
// ============================================================================

bool portaudio_handler_init(PortAudioHandler* pa, double sampleRate, uint32_t blockSize);
bool portaudio_handler_open_stream(PortAudioHandler* pa, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice);
bool portaudio_handler_start(PortAudioHandler* pa);
bool portaudio_handler_stop(PortAudioHandler* pa);
bool portaudio_handler_close(PortAudioHandler* pa);
void portaudio_handler_cleanup(PortAudioHandler* pa);

// ============================================================================
// EFFECT CHAIN MANAGEMENT
// ============================================================================

Effect* portaudio_handler_add_effect(PortAudioHandler* pa, EffectType type);
void   portaudio_handler_remove_effect(PortAudioHandler* pa, Effect* fx);
void   portaudio_handler_clear_chain(PortAudioHandler* pa);

Effect* portaudio_handler_find_effect(PortAudioHandler* pa, EffectType type);
void   portaudio_handler_move_effect(PortAudioHandler* pa, Effect* fx, int position);

int     portaudio_handler_effect_count(PortAudioHandler* pa);
Effect* portaudio_handler_get_effect_at(PortAudioHandler* pa, int index);

void portaudio_handler_enable_all(PortAudioHandler* pa, bool enabled);
void portaudio_handler_bypass_all(PortAudioHandler* pa, bool bypass);

// ============================================================================
// PRESET CONTROL
// ============================================================================

void portaudio_handler_load_preset(PortAudioHandler* pa, const char* presetName);

// ============================================================================
// INDIVIDUAL EFFECT PARAMETER WRAPPERS
// (PortAudio-level API: main() should NEVER call effects directly)
// ============================================================================

// --- Noise Gate ---
void pa_fx_noisegate_set(PortAudioHandler* pa, Effect* fx,
                         float threshDb, float attackMs, float releaseMs, float holdMs);

// --- Compressor ---
void pa_fx_compressor_set(PortAudioHandler* pa, Effect* fx,
                           float threshDb, float ratio, float makeupDb,
                           float kneeDb, float attackMs, float releaseMs);

// --- Overdrive ---
void pa_fx_overdrive_set(PortAudioHandler* pa, Effect* fx,
                         float driveDb, float toneHz, float outputDb);

// --- Distortion ---
void pa_fx_distortion_set(PortAudioHandler* pa, Effect* fx,
                          float driveDb, float bassDb, float midDb,
                          float trebleDb, float outputDb);

// --- Fuzz ---
void pa_fx_fuzz_set(PortAudioHandler* pa, Effect* fx,
                    float driveDb, float outputDb);

// --- Boost ---
void pa_fx_boost_set(PortAudioHandler* pa, Effect* fx,
                     float gainDb);

// --- Tube Screamer ---
void pa_fx_tubescreamer_set(PortAudioHandler* pa, Effect* fx,
                            float driveDb, float tone, float outputDb);

// --- Chorus ---
void pa_fx_chorus_set(PortAudioHandler* pa, Effect* fx,
                      float rateHz, float depthMs, float mix);

// --- Flanger ---
void pa_fx_flanger_set(PortAudioHandler* pa, Effect* fx,
                       float rateHz, float depthMs, float feedback, float mix);

// --- Phaser ---
void pa_fx_phaser_set(PortAudioHandler* pa, Effect* fx,
                      float rateHz, float depth, float feedback, float mix);

// --- Tremolo ---
void pa_fx_tremolo_set(PortAudioHandler* pa, Effect* fx,
                       float rateHz, float depth);

// --- Vibrato ---
void pa_fx_vibrato_set(PortAudioHandler* pa, Effect* fx,
                       float rateHz, float depthMs);

// --- Delay ---
void pa_fx_delay_set(PortAudioHandler* pa, Effect* fx,
                     float timeSec, float feedback, float dampHz, float mix);

// --- Reverb ---
void pa_fx_reverb_set(PortAudioHandler* pa, Effect* fx,
                      float decay, float dampHz, float mix);

// --- Wah ---
void pa_fx_wah_set(PortAudioHandler* pa, Effect* fx,
                   float freq, float Q, float sensitivity);

// --- 3-band EQ ---
void pa_fx_eq3band_set(PortAudioHandler* pa, Effect* fx,
                       float bassDb, float midDb, float trebleDb);

// --- Parametric EQ ---
void pa_fx_eqparametric_set_band(PortAudioHandler* pa, Effect* fx,
                                 int band, float freqHz, float Q, float gainDb);

// --- Preamp ---
void pa_fx_preamp_set(PortAudioHandler* pa, Effect* fx,
                      float inputDb, float driveDb,
                      float bassDb, float midDb, float trebleDb,
                      float outputDb, float sag, int tubeIdx);

// --- Poweramp ---
void pa_fx_poweramp_set(PortAudioHandler* pa, Effect* fx,
                        float driveDb, float outputDb,
                        float sag, float sagTimeMs, int tubeIdx);

// --- Cabinet ---
void pa_fx_cabinet_set(PortAudioHandler* pa, Effect* fx, int cabinetType);


int portaudio_handler_get_tube_preset_count(void);
const TubeDef* portaudio_handler_get_tube_preset(int idx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* PORTAUDIO_HANDLER_H */
