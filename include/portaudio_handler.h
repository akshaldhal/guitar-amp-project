#ifndef PORTAUDIO_HANDLER_H
#define PORTAUDIO_HANDLER_H

#include <portaudio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "effects_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PortAudioHandler {
    PaStream* stream;

    /* DSP */
    DSPState state;
    AmpChain chain;

    /* Config */
    double sampleRate;
    uint32_t blockSize;

    /* Device/channel info used for opening stream (host channels) */
    int hostInputChannels;
    int hostOutputChannels;
    PaDeviceIndex inputDevice;
    PaDeviceIndex outputDevice;

    /* Waveshaper memory for amp chain */
    float* waveshaperTable;
    size_t waveshaperTableSize;

    bool initialized;
} PortAudioHandler;

/* Initialize the handler (allocates waveshaper table and initializes DSPState/AmpChain)
 * sampleRate: sample rate in Hz
 * blockSize: frames per callback
 */
bool portaudio_handler_init(PortAudioHandler* pa, double sampleRate, uint32_t blockSize);

/* Cleanup handler (closes stream if open, frees resources) */
void portaudio_handler_cleanup(PortAudioHandler* pa);

/* Open a stream using the selected host devices.
 * This function will:
 *  - open input/output using stereo (2 channels) when possible
 *  - set pa->hostInputChannels/hostOutputChannels to chosen channel counts
 */
bool portaudio_handler_open_stream(PortAudioHandler* pa, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice);

/* Start/stop/close stream */
bool portaudio_handler_start(PortAudioHandler* pa);
bool portaudio_handler_stop(PortAudioHandler* pa);
bool portaudio_handler_close(PortAudioHandler* pa);

/* Low-level callback (public for PortAudio) */
int portaudio_handler_callback(
    const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData
);

#ifdef __cplusplus
}
#endif

#endif /* PORTAUDIO_HANDLER_H */
