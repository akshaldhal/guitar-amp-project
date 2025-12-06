// src/portaudio_handler.c
#include "portaudio_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Local static callback (PortAudio-compatible)
static int portaudio_handler_callback(
    const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    PortAudioHandler* pa = (PortAudioHandler*)userData;
    if (!pa) return paContinue;

    const float* inBuf = (const float*)input;
    float* outBuf = (float*)output;

    if (!outBuf) return paContinue;

    // dsp.scratch is expected to be an array of two float* buffers assigned by dsp_state_init
    float* monoIn  = pa->dsp.scratch[0];
    float* monoOut = pa->dsp.scratch[1];

    // Guard: ensure scratch buffers are present (check elements, not array pointer)
    if (!monoIn || !monoOut) {
        // can't process — output silence
        int outCh = pa->hostOutputChannels > 0 ? pa->hostOutputChannels : 1;
        memset(outBuf, 0, frameCount * sizeof(float) * outCh);
        return paContinue;
    }

    // -------- INPUT: convert to monoIn ----------
    if (!input || !inBuf || pa->hostInputChannels <= 0) {
        memset(monoIn, 0, frameCount * sizeof(float));
    } else {
        int ch = pa->hostInputChannels;
        if (ch >= 2) {
            for (unsigned long i = 0; i < frameCount; ++i) {
                float l = inBuf[i * ch + 0];
                float r = inBuf[i * ch + 1];
                monoIn[i] = 0.5f * (l + r);
            }
        } else if (ch == 1) {
            memcpy(monoIn, inBuf, frameCount * sizeof(float));
        } else {
            memset(monoIn, 0, frameCount * sizeof(float));
        }
    }

    // -------- PROCESS CHAIN ----------
    effect_chain_process(&pa->chain, monoIn, monoOut, frameCount);

    // -------- OUTPUT: duplicate mono to host channels ----------
    int outCh = pa->hostOutputChannels;
    if (outCh >= 2) {
        for (unsigned long i = 0; i < frameCount; ++i) {
            float v = monoOut[i];
            // write to first two channels, silence others
            outBuf[i * outCh + 0] = v;
            outBuf[i * outCh + 1] = v;
            for (int c = 2; c < outCh; ++c) outBuf[i * outCh + c] = 0.0f;
        }
    } else if (outCh == 1) {
        memcpy(outBuf, monoOut, frameCount * sizeof(float));
    }

    (void)timeInfo;
    (void)statusFlags;
    return paContinue;
}

// -----------------------------------------------------------------------------
// Initialization & cleanup
// -----------------------------------------------------------------------------

bool portaudio_handler_init(PortAudioHandler* pa, double sampleRate, uint32_t blockSize)
{
    if (!pa) return false;
    memset(pa, 0, sizeof(*pa));

    pa->sampleRate = sampleRate;
    pa->blockSize = blockSize;
    pa->initialized = false;
    pa->stream = NULL;
    pa->inputDevice = paNoDevice;
    pa->outputDevice = paNoDevice;
    pa->hostInputChannels = 0;
    pa->hostOutputChannels = 0;

    // dsp_state_init in your codebase returns void (set up scratch buffers, etc.)
    dsp_state_init(&pa->dsp, (float)sampleRate, 1u, blockSize);

    // init effect chain (allocates internal data)
    effect_chain_init(&pa->chain, &pa->dsp, blockSize);

    pa->initialized = true;
    return true;
}

void portaudio_handler_cleanup(PortAudioHandler* pa)
{
    if (!pa) return;

    // Close stream if open
    portaudio_handler_close(pa);

    // cleanup effects and dsp
    effect_chain_cleanup(&pa->chain);
    dsp_state_cleanup(&pa->dsp);

    memset(pa, 0, sizeof(*pa));
}

// -----------------------------------------------------------------------------
// Stream open/start/stop/close
// -----------------------------------------------------------------------------

bool portaudio_handler_open_stream(PortAudioHandler* pa, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice)
{
    if (!pa || !pa->initialized) return false;

    const PaDeviceInfo* inInfo  = (inputDevice == paNoDevice) ? NULL : Pa_GetDeviceInfo(inputDevice);
    const PaDeviceInfo* outInfo = (outputDevice == paNoDevice) ? NULL : Pa_GetDeviceInfo(outputDevice);

    if (inputDevice != paNoDevice && !inInfo) {
        fprintf(stderr, "portaudio_handler: invalid input device\n");
        return false;
    }
    if (outputDevice != paNoDevice && !outInfo) {
        fprintf(stderr, "portaudio_handler: invalid output device\n");
        return false;
    }

    PaStreamParameters inParams = {0};
    PaStreamParameters outParams = {0};

    int desiredHostCh = 2;
    int hostInCh = 0;
    int hostOutCh = 0;

    if (inInfo) hostInCh = (inInfo->maxInputChannels >= desiredHostCh) ? desiredHostCh : inInfo->maxInputChannels;
    if (outInfo) hostOutCh = (outInfo->maxOutputChannels >= desiredHostCh) ? desiredHostCh : outInfo->maxOutputChannels;

    if (hostInCh < 1) hostInCh = 0;
    if (hostOutCh < 1) hostOutCh = 0;

    pa->hostInputChannels = hostInCh;
    pa->hostOutputChannels = hostOutCh;
    pa->inputDevice = (inInfo) ? inputDevice : paNoDevice;
    pa->outputDevice = (outInfo) ? outputDevice : paNoDevice;

    if (hostInCh == 0 && hostOutCh == 0) {
        fprintf(stderr, "portaudio_handler: neither input nor output supported for chosen devices\n");
        return false;
    }

    if (hostInCh > 0) {
        inParams.device = inputDevice;
        inParams.channelCount = hostInCh;
        inParams.sampleFormat = paFloat32;
        inParams.suggestedLatency = 0.001;
        inParams.hostApiSpecificStreamInfo = NULL;
    }

    if (hostOutCh > 0) {
        outParams.device = outputDevice;
        outParams.channelCount = hostOutCh;
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency = 0.001;
        outParams.hostApiSpecificStreamInfo = NULL;
    }

    // close previous stream if present
    if (pa->stream) {
        Pa_CloseStream(pa->stream);
        pa->stream = NULL;
    }

    PaError err = Pa_OpenStream(
        &pa->stream,
        (hostInCh > 0) ? &inParams : NULL,
        (hostOutCh > 0) ? &outParams : NULL,
        pa->sampleRate,
        pa->blockSize,
        paNoFlag,
        portaudio_handler_callback,
        pa
    );

    if (err != paNoError) {
        fprintf(stderr, "portaudio_handler: Pa_OpenStream error: %s\n", Pa_GetErrorText(err));
        pa->stream = NULL;
        return false;
    }

    return true;
}

bool portaudio_handler_start(PortAudioHandler* pa)
{
    if (!pa || !pa->stream) return false;
    PaError err = Pa_StartStream(pa->stream);
    if (err != paNoError) {
        fprintf(stderr, "portaudio_handler: Pa_StartStream error: %s\n", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

bool portaudio_handler_stop(PortAudioHandler* pa)
{
    if (!pa || !pa->stream) return false;
    PaError err = Pa_StopStream(pa->stream);
    if (err != paNoError) {
        fprintf(stderr, "portaudio_handler: Pa_StopStream error: %s\n", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

bool portaudio_handler_close(PortAudioHandler* pa)
{
    if (!pa) return false;
    if (!pa->stream) return true;

    PaError err = Pa_CloseStream(pa->stream);
    if (err != paNoError) {
        fprintf(stderr, "portaudio_handler: Pa_CloseStream error: %s\n", Pa_GetErrorText(err));
        return false;
    }
    pa->stream = NULL;
    return true;
}

// -----------------------------------------------------------------------------
// Chain management (thin wrappers)
// -----------------------------------------------------------------------------

Effect* portaudio_handler_add_effect(PortAudioHandler* pa, EffectType type) {
    if (!pa) return NULL;
    return effect_chain_add(&pa->chain, type);
}

void portaudio_handler_remove_effect(PortAudioHandler* pa, Effect* fx) {
    if (!pa || !fx) return;
    effect_chain_remove(&pa->chain, fx);
}

void portaudio_handler_clear_chain(PortAudioHandler* pa) {
    if (!pa) return;
    effect_chain_clear(&pa->chain);
}

Effect* portaudio_handler_find_effect(PortAudioHandler* pa, EffectType type) {
    if (!pa) return NULL;
    return effect_chain_find(&pa->chain, type);
}

void portaudio_handler_move_effect(PortAudioHandler* pa, Effect* fx, int position) {
    if (!pa || !fx) return;
    effect_chain_move(&pa->chain, fx, position);
}

int portaudio_handler_effect_count(PortAudioHandler* pa) {
    if (!pa) return 0;
    return effect_chain_count(&pa->chain);
}

Effect* portaudio_handler_get_effect_at(PortAudioHandler* pa, int index) {
    if (!pa) return NULL;
    return effect_chain_get_at(&pa->chain, index);
}

void portaudio_handler_enable_all(PortAudioHandler* pa, bool enabled) {
    if (!pa) return;
    effect_chain_enable_all(&pa->chain, enabled);
}

void portaudio_handler_bypass_all(PortAudioHandler* pa, bool bypass) {
    if (!pa) return;
    effect_chain_bypass_all(&pa->chain, bypass);
}

// -----------------------------------------------------------------------------
// Effect enable/bypass helpers
// -----------------------------------------------------------------------------

void portaudio_handler_effect_enable(PortAudioHandler* pa, Effect* fx, bool enabled) {
    if (!pa || !fx) return;
    effect_enable(fx, enabled);
}

void portaudio_handler_effect_bypass(PortAudioHandler* pa, Effect* fx, bool bypass) {
    if (!pa || !fx) return;
    effect_bypass(fx, bypass);
}

// -----------------------------------------------------------------------------
// Preset chains
// -----------------------------------------------------------------------------

void portaudio_handler_load_preset(PortAudioHandler* pa, const char* presetName) {
    if (!pa || !presetName) return;

    if (strcmp(presetName, "clean") == 0) {
        fx_chain_preset_clean(&pa->chain);
    } else if (strcmp(presetName, "crunch") == 0) {
        fx_chain_preset_crunch(&pa->chain);
    } else if (strcmp(presetName, "lead") == 0) {
        fx_chain_preset_lead(&pa->chain);
    } else if (strcmp(presetName, "metal") == 0) {
        fx_chain_preset_metal(&pa->chain);
    } else if (strcmp(presetName, "fuzz") == 0) {
        fx_chain_preset_fuzz(&pa->chain);
    } else if (strcmp(presetName, "ambient") == 0) {
        fx_chain_preset_ambient(&pa->chain);
    } else if (strcmp(presetName, "blues") == 0) {
        fx_chain_preset_blues(&pa->chain);
    } else if (strcmp(presetName, "shoegaze") == 0) {
        fx_chain_preset_shoegaze(&pa->chain);
    } else if (strcmp(presetName, "funk") == 0) {
        fx_chain_preset_funk(&pa->chain);
    }
}

// -----------------------------------------------------------------------------
// Per-effect parameter wrappers (names match the header I provided earlier)
// -----------------------------------------------------------------------------

void pa_fx_noisegate_set(PortAudioHandler* pa, Effect* fx, float threshDb, float attackMs, float releaseMs, float holdMs) {
    (void)pa;
    if (!fx) return;
    fx_noisegate_set(fx, threshDb, attackMs, releaseMs, holdMs);
}

void pa_fx_compressor_set(PortAudioHandler* pa, Effect* fx, float threshDb, float ratio, float makeupDb,
                          float kneeDb, float attackMs, float releaseMs) {
    (void)pa;
    if (!fx) return;
    fx_compressor_set(fx, threshDb, ratio, makeupDb, kneeDb, attackMs, releaseMs);
}

void pa_fx_overdrive_set(PortAudioHandler* pa, Effect* fx, float driveDb, float toneHz, float outputDb) {
    (void)pa;
    if (!fx) return;
    fx_overdrive_set(fx, driveDb, toneHz, outputDb);
}

void pa_fx_distortion_set(PortAudioHandler* pa, Effect* fx, float driveDb, float bassDb, float midDb,
                          float trebleDb, float outputDb) {
    (void)pa;
    if (!fx) return;
    fx_distortion_set(fx, driveDb, bassDb, midDb, trebleDb, outputDb);
}

void pa_fx_fuzz_set(PortAudioHandler* pa, Effect* fx, float driveDb, float outputDb) {
    (void)pa;
    if (!fx) return;
    fx_fuzz_set(fx, driveDb, outputDb);
}

void pa_fx_boost_set(PortAudioHandler* pa, Effect* fx, float gainDb) {
    (void)pa;
    if (!fx) return;
    fx_boost_set(fx, gainDb);
}

void pa_fx_tubescreamer_set(PortAudioHandler* pa, Effect* fx, float driveDb, float tone, float outputDb) {
    (void)pa;
    if (!fx) return;
    fx_tubescreamer_set(fx, driveDb, tone, outputDb);
}

void pa_fx_chorus_set(PortAudioHandler* pa, Effect* fx, float rateHz, float depthMs, float mix) {
    (void)pa;
    if (!fx) return;
    fx_chorus_set(fx, rateHz, depthMs, mix);
}

void pa_fx_flanger_set(PortAudioHandler* pa, Effect* fx, float rateHz, float depthMs, float feedback, float mix) {
    (void)pa;
    if (!fx) return;
    fx_flanger_set(fx, rateHz, depthMs, feedback, mix);
}

void pa_fx_phaser_set(PortAudioHandler* pa, Effect* fx, float rateHz, float depth, float feedback, float mix) {
    (void)pa;
    if (!fx) return;
    fx_phaser_set(fx, rateHz, depth, feedback, mix);
}

void pa_fx_tremolo_set(PortAudioHandler* pa, Effect* fx, float rateHz, float depth) {
    (void)pa;
    if (!fx) return;
    fx_tremolo_set(fx, rateHz, depth);
}

void pa_fx_vibrato_set(PortAudioHandler* pa, Effect* fx, float rateHz, float depthMs) {
    (void)pa;
    if (!fx) return;
    fx_vibrato_set(fx, rateHz, depthMs);
}

void pa_fx_delay_set(PortAudioHandler* pa, Effect* fx, float timeSec, float feedback, float dampHz, float mix) {
    (void)pa;
    if (!fx) return;
    fx_delay_set(fx, timeSec, feedback, dampHz, mix);
}

void pa_fx_reverb_set(PortAudioHandler* pa, Effect* fx, float decay, float dampHz, float mix) {
    (void)pa;
    if (!fx) return;
    fx_reverb_set(fx, decay, dampHz, mix);
}

void pa_fx_wah_set(PortAudioHandler* pa, Effect* fx, float freq, float Q, float sensitivity) {
    (void)pa;
    if (!fx) return;
    fx_wah_set(fx, freq, Q, sensitivity);
}

void pa_fx_eq3band_set(PortAudioHandler* pa, Effect* fx, float bassDb, float midDb, float trebleDb) {
    (void)pa;
    if (!fx) return;
    fx_eq3band_set(fx, bassDb, midDb, trebleDb);
}

void pa_fx_eqparametric_set_band(PortAudioHandler* pa, Effect* fx, int band, float freqHz, float Q, float gainDb) {
    (void)pa;
    if (!fx) return;
    fx_eqparametric_set_band(fx, band, freqHz, Q, gainDb);
}

void pa_fx_preamp_set(PortAudioHandler* pa, Effect* fx, float inputDb, float driveDb, float bassDb,
                      float midDb, float trebleDb, float outputDb, float sag, int tubeIdx) {
    (void)pa;
    if (!fx) return;
    fx_preamp_set(fx, inputDb, driveDb, bassDb, midDb, trebleDb, outputDb, sag, tubeIdx);
}

void pa_fx_poweramp_set(PortAudioHandler* pa, Effect* fx, float driveDb, float outputDb, float sag,
                        float sagTimeMs, int tubeIdx) {
    (void)pa;
    if (!fx) return;
    fx_poweramp_set(fx, driveDb, outputDb, sag, sagTimeMs, tubeIdx);
}

void pa_fx_cabinet_set(PortAudioHandler* pa, Effect* fx, int cabinetType) {
    (void)pa;
    if (!fx) return;
    fx_cabinet_set(fx, cabinetType);
}

// -----------------------------------------------------------------------------
// Additional accessors (tube presets)
// -----------------------------------------------------------------------------

int portaudio_handler_get_tube_preset_count(void) {
    return NUM_TUBE_PRESETS;
}

const TubeDef* portaudio_handler_get_tube_preset(int idx) {
    if (idx < 0 || idx >= NUM_TUBE_PRESETS) return NULL;
    return &tube_presets[idx];
}

// -----------------------------------------------------------------------------
// Misc helpers
// -----------------------------------------------------------------------------

double portaudio_handler_get_sample_rate(PortAudioHandler* pa) {
    if (!pa) return 0.0;
    return pa->sampleRate;
}

uint32_t portaudio_handler_get_block_size(PortAudioHandler* pa) {
    if (!pa) return 0;
    return pa->blockSize;
}

int portaudio_handler_get_host_input_channels(PortAudioHandler* pa) {
    if (!pa) return 0;
    return pa->hostInputChannels;
}

int portaudio_handler_get_host_output_channels(PortAudioHandler* pa) {
    if (!pa) return 0;
    return pa->hostOutputChannels;
}
