#include "portaudio_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declare ampchain_process signature from effects_interface.h just in case */
extern void ampchain_process(AmpChain* chain, const float* in, float* out, size_t numSamples);

int portaudio_handler_callback(
    const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    PortAudioHandler* pa = (PortAudioHandler*)userData;
    const float* inBuf = (const float*)input;
    float* outBuf = (float*)output;

    if (!outBuf) return paContinue;

    // Use dedicated scratch buffers
    float* monoIn  = pa->state.scratch[0];
    float* monoOut = pa->state.scratch[1];

    // Convert input to mono safely
    if (!input || !inBuf) {
        memset(monoIn, 0, sizeof(float) * frameCount);
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
            memset(monoIn, 0, sizeof(float) * frameCount);
        }
    }

    // Process DSP chain
    ampchain_process(&pa->chain, monoIn, monoOut, frameCount);

    // Convert mono -> host output channels safely
    int outCh = pa->hostOutputChannels;
    if (outCh >= 2) {
        for (unsigned long i = 0; i < frameCount; ++i) {
            float v = monoOut[i];
            for (int c = 0; c < outCh; ++c)
                outBuf[i * outCh + c] = (c < 2) ? v : 0.0f;
        }
    } else if (outCh == 1) {
        memcpy(outBuf, monoOut, frameCount * sizeof(float));
    }

    return paContinue;
}

/* ------------------------------------------------------------------ */

bool portaudio_handler_init(PortAudioHandler* pa, double sampleRate, uint32_t blockSize)
{
    if (!pa) return false;
    memset(pa, 0, sizeof(*pa));

    pa->sampleRate = sampleRate;
    pa->blockSize = blockSize;
    pa->initialized = false;

    /* waveshaper allocation (simple default) */
    pa->waveshaperTableSize = 4096;
    pa->waveshaperTable = (float*)calloc(pa->waveshaperTableSize, sizeof(float));
    if (!pa->waveshaperTable) {
        fprintf(stderr, "portaudio_handler: failed to allocate waveshaper table\n");
        return false;
    }

    /* initialize DSPState as MONO (the processing chain is mono) */
    dsp_state_init(&pa->state, (float)sampleRate, 1u, blockSize);

    /* init amp chain (uses provided waveshaper table) */
    ampchain_init(&pa->chain, &pa->state, pa->waveshaperTable, pa->waveshaperTableSize);

    pa->initialized = true;
    return true;
}

void portaudio_handler_cleanup(PortAudioHandler* pa)
{
    if (!pa) return;

    portaudio_handler_close(pa);

    /* cleanup DSP state and free waveshaper table */
    dsp_state_cleanup(&pa->state);

    if (pa->waveshaperTable) {
        free(pa->waveshaperTable);
        pa->waveshaperTable = NULL;
    }

    memset(pa, 0, sizeof(*pa));
}

/* Open stream and set host channel counts to stereo (2) if possible.
 * Uses Pa_OpenStream; returns true on success.
 */
bool portaudio_handler_open_stream(PortAudioHandler* pa, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice)
{
    if (!pa || !pa->initialized) return false;

    const PaDeviceInfo* inInfo = Pa_GetDeviceInfo(inputDevice);
    const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(outputDevice);
    if (!inInfo || !outInfo) {
        fprintf(stderr, "portaudio_handler: invalid device info\n");
        return false;
    }

    PaStreamParameters inParams = {0};
    PaStreamParameters outParams = {0};

    /* Try to open with stereo (2 channels). If device doesn't have 2 channels,
       fall back to its max channel count (>=1). */
    int desiredHostCh = 2;

    int hostInCh = (inInfo->maxInputChannels >= desiredHostCh) ? desiredHostCh : inInfo->maxInputChannels;
    int hostOutCh = (outInfo->maxOutputChannels >= desiredHostCh) ? desiredHostCh : outInfo->maxOutputChannels;

    if (hostInCh < 1) hostInCh = 0;
    if (hostOutCh < 1) hostOutCh = 0;

    pa->hostInputChannels = hostInCh;
    pa->hostOutputChannels = hostOutCh;
    pa->inputDevice = inputDevice;
    pa->outputDevice = outputDevice;

    if (hostInCh == 0 && hostOutCh == 0) {
        fprintf(stderr, "portaudio_handler: neither input nor output supported for chosen devices\n");
        return false;
    }

    if (hostInCh > 0) {
        inParams.device = inputDevice;
        inParams.channelCount = hostInCh;
        inParams.sampleFormat = paFloat32;
        inParams.suggestedLatency = inInfo->defaultLowInputLatency;
        inParams.hostApiSpecificStreamInfo = NULL;
    } else {
        /* no input - open stream as output-only */
        inParams.device = paNoDevice;
    }

    if (hostOutCh > 0) {
        outParams.device = outputDevice;
        outParams.channelCount = hostOutCh;
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency = outInfo->defaultLowOutputLatency;
        outParams.hostApiSpecificStreamInfo = NULL;
    } else {
        outParams.device = paNoDevice;
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
