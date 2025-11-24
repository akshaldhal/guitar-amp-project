#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <portaudio.h>
#include <string.h>

#include "portaudio_handler.h"

/* ---------- Simple ANSI TUI helpers ---------- */

static inline void tui_clear() {
    printf("\033[2J\033[H");
}

static inline void tui_header(const char* text) {
    printf("\n\033[1;44m  %s  \033[0m\n\n", text);
}

static inline void pause_enter() {
    printf("\nPress ENTER to continue...");
    getchar();
}

/* ---------- Device selection menu ---------- */

int menu_choose_device(const char* title, bool requireInput, bool requireOutput)
{
    tui_clear();
    tui_header(title);

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        fprintf(stderr, "Pa_GetDeviceCount error: %s\n", Pa_GetErrorText(numDevices));
        return -1;
    }

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        /* Filter: show devices that have required direction */
        if (requireInput && info->maxInputChannels < 1) continue;
        if (requireOutput && info->maxOutputChannels < 1) continue;

        printf("[%2d] %s  (In:%d Out:%d)\n",
               i, info->name, info->maxInputChannels, info->maxOutputChannels);
    }

    printf("\nSelect device index (or -1 to cancel): ");
    int idx = -1;
    if (scanf("%d", &idx) != 1) {
        idx = -1;
    }
    getchar(); /* consume newline */
    return idx;
}

/* ---------- Runtime control menu ---------- */

void runtime_menu(PortAudioHandler* pa)
{
    bool running = true;
    while (running) {
        tui_clear();
        tui_header("LIVE AMP SIM — Controls");

        printf("Stream: %s\n", (pa->stream ? "OPEN" : "CLOSED"));
        printf("Input Device: %d   Output Device: %d\n\n", pa->inputDevice, pa->outputDevice);

        printf("1) Toggle bypass (current: %s)\n", pa->chain.bypass ? "ON" : "OFF");
        printf("2) Toggle overdrive (current: %s)\n", pa->chain.overdrive.enabled ? "ON" : "OFF");
        printf("3) Toggle distortion (current: %s)\n", pa->chain.distortion.enabled ? "ON" : "OFF");
        printf("4) Toggle noise gate (current: %s)\n", pa->chain.noisegate.enabled ? "ON" : "OFF");
        printf("5) Set preamp gain (current: %.1f dB)\n", pa->chain.preamp.gain);
        printf("6) Stop audio and exit\n");

        printf("\nChoose: ");
        int c = getchar();
        getchar(); /* eat newline */

        switch (c) {
            case '1':
                pa->chain.bypass = !pa->chain.bypass;
                break;
            case '2':
                pa->chain.overdrive.enabled = !pa->chain.overdrive.enabled;
                break;
            case '3':
                pa->chain.distortion.enabled = !pa->chain.distortion.enabled;
                break;
            case '4':
                pa->chain.noisegate.enabled = !pa->chain.noisegate.enabled;
                break;
            case '5': {
                printf("Enter preamp gain (dB): ");
                float g = 0.0f;
                if (scanf("%f", &g) == 1) {
                    preamp_set_gain(&pa->chain.preamp, g);
                }
                getchar();
                break;
            }
            case '6':
                running = false;
                break;
            default:
                break;
        }
    }
}

/* ---------- Main ---------- */

int main(void)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    int inDev = menu_choose_device("Select INPUT device", true, false);
    if (inDev < 0) {
        printf("No input selected, exiting.\n");
        Pa_Terminate();
        return 0;
    }

    int outDev = menu_choose_device("Select OUTPUT device", false, true);
    if (outDev < 0) {
        printf("No output selected, exiting.\n");
        Pa_Terminate();
        return 0;
    }

    /* Setup handler */
    PortAudioHandler pa;
    if (!portaudio_handler_init(&pa, 48000.0, 256)) {
        fprintf(stderr, "portaudio_handler_init failed\n");
        Pa_Terminate();
        return 1;
    }

    if (!portaudio_handler_open_stream(&pa, (PaDeviceIndex)inDev, (PaDeviceIndex)outDev)) {
        fprintf(stderr, "Stream open error\n");
        portaudio_handler_cleanup(&pa);
        Pa_Terminate();
        return 1;
    }

    if (!portaudio_handler_start(&pa)) {
        fprintf(stderr, "Failed to start audio\n");
        portaudio_handler_cleanup(&pa);
        Pa_Terminate();
        return 1;
    }

    printf("Audio running. Enter runtime menu.\n");
    pause_enter();

    runtime_menu(&pa);

    /* Cleanup */
    portaudio_handler_stop(&pa);
    portaudio_handler_close(&pa);
    portaudio_handler_cleanup(&pa);

    Pa_Terminate();
    printf("Goodbye.\n");
    return 0;
}
