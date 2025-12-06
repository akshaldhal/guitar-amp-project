// src/main.c — exhaustive interactive main for live amp sim
// Build with same flags you use for the rest of the project.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <portaudio.h>

#include "portaudio_handler.h"
#include "effects_interface.h"

/* ---------- Small utilities ---------- */

static inline void tui_clear(void) {
    printf("\033[2J\033[H");
}

static inline void tui_header(const char* text) {
    printf("\n\033[1;44m  %s  \033[0m\n\n", text);
}

static inline void pause_enter(void) {
    printf("\nPress ENTER to continue...");
    fflush(stdout);
    int c = getchar();
    (void)c;
}

/* ---------- Input helpers ---------- */

static bool read_line(char* buf, size_t size) {
    if (!fgets(buf, (int)size, stdin)) return false;
    size_t len = strlen(buf);
    if (len && buf[len-1] == '\n') buf[len-1] = '\0';
    return true;
}

static bool parse_int(const char* s, long* out) {
    if (!s) return false;
    char* end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s) return false;
    if (errno == ERANGE) return false;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    *out = v;
    return true;
}

static bool parse_double(const char* s, double* out) {
    if (!s) return false;
    char* end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s) return false;
    if (errno == ERANGE) return false;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    *out = v;
    return true;
}

/* ---------- Device menu ---------- */

static int menu_choose_device(const char* title, bool requireInput, bool requireOutput)
{
    tui_clear();
    tui_header(title);

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        fprintf(stderr, "Pa_GetDeviceCount error: %s\n", Pa_GetErrorText(numDevices));
        return -1;
    }

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo((PaDeviceIndex)i);
        if (!info) continue;
        if (requireInput && info->maxInputChannels < 1) continue;
        if (requireOutput && info->maxOutputChannels < 1) continue;
        const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(info->hostApi);
        printf("[%3d] %s (host: %s)  In:%d Out:%d\n",
            i,
            info->name,
            hostInfo ? hostInfo->name : "unknown",
            info->maxInputChannels,
            info->maxOutputChannels
        );
    }

    printf("\nSelect device index (or -1 to cancel): ");
    fflush(stdout);

    char line[128];
    if (!read_line(line, sizeof(line))) return -1;
    long idx;
    if (!parse_int(line, &idx)) {
        fprintf(stderr, "Invalid selection\n");
        return -1;
    }
    if (idx < -1 || idx >= numDevices) {
        fprintf(stderr, "Index out of range\n");
        return -1;
    }
    if (idx == -1) return -1;
    return (int)idx;
}

/* ---------- Globals & signal handling ---------- */

static volatile sig_atomic_t g_keep_running = 1;

static void sigint_handler(int signo) {
    (void)signo;
    g_keep_running = 0;
}

/* ---------- Helpers for UI & chain management ---------- */

static Effect* get_or_add_effect(PortAudioHandler* pa, EffectType type) {
    Effect* fx = portaudio_handler_find_effect(pa, type);
    if (fx) return fx;
    return portaudio_handler_add_effect(pa, type);
}

static void list_chain(PortAudioHandler* pa) {
    tui_clear();
    tui_header("Effect Chain");
    int n = portaudio_handler_effect_count(pa);
    if (n == 0) {
        printf("(empty)\n");
    } else {
        for (int i = 0; i < n; ++i) {
            Effect* e = portaudio_handler_get_effect_at(pa, i);
            if (!e) continue;
            const char* name = effect_type_name(e->type);
            printf("[%2d] %-20s  enabled:%c  bypass:%c\n",
                   i,
                   name ? name : "unknown",
                   e->enabled ? 'Y' : 'n',
                   e->bypass ? 'Y' : 'n');
        }
    }
    pause_enter();
}

/* Prompt helpers */
static bool prompt_double(const char* prompt, double* out) {
    char buf[128];
    printf("%s", prompt);
    fflush(stdout);
    if (!read_line(buf, sizeof(buf))) return false;
    return parse_double(buf, out);
}

static bool prompt_int(const char* prompt, long* out) {
    char buf[128];
    printf("%s", prompt);
    fflush(stdout);
    if (!read_line(buf, sizeof(buf))) return false;
    return parse_int(buf, out);
}

/* ---------- Save/Load simple chain (types only) ---------- */
/* Because there are no getters for full parameter state in your effects API,
   this saves only the sequence of effect types and which ones are enabled/bypassed.
   Restoring parameters is not possible without adding getter/serialize APIs. */

static bool save_chain_to_file(PortAudioHandler* pa, const char* path) {
    if (!pa || !path) return false;
    FILE* f = fopen(path, "w");
    if (!f) return false;
    int n = portaudio_handler_effect_count(pa);
    for (int i = 0; i < n; ++i) {
        Effect* e = portaudio_handler_get_effect_at(pa, i);
        if (!e) continue;
        fprintf(f, "%d %d %d\n", (int)e->type, e->enabled ? 1 : 0, e->bypass ? 1 : 0);
        /* note: parameters are not saved due to lack of getters */
    }
    fclose(f);
    return true;
}

static bool load_chain_from_file(PortAudioHandler* pa, const char* path) {
    if (!pa || !path) return false;
    FILE* f = fopen(path, "r");
    if (!f) return false;

    /* clear existing chain first */
    portaudio_handler_clear_chain(pa);

    while (!feof(f)) {
        int t, enabled, bypass;
        int r = fscanf(f, "%d %d %d", &t, &enabled, &bypass);
        if (r != 3) break;
        if (t <= 0 || t >= FX_CABINET + 1) {
            /* skip invalid types */
            continue;
        }
        Effect* e = portaudio_handler_add_effect(pa, (EffectType)t);
        if (!e) continue;
        effect_enable(e, enabled ? true : false);
        effect_bypass(e, bypass ? true : false);
        /* parameters are not restored */
    }

    fclose(f);
    return true;
}

/* ---------- Parameter prompting for individual effects ---------- */
/* For each effect we present meaningful prompts and call the pa_fx_* wrappers
   (or the raw fx_*_set functions if wrappers aren't available). */

static void set_noisegate_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double threshDb, attackMs, releaseMs, holdMs;
    if (!prompt_double("Threshold (dB): ", &threshDb)) return;
    if (!prompt_double("Attack (ms): ", &attackMs)) return;
    if (!prompt_double("Release (ms): ", &releaseMs)) return;
    if (!prompt_double("Hold (ms): ", &holdMs)) return;
    /* prefer portaudio wrapper if present */
    pa_fx_noisegate_set(pa, fx, (float)threshDb, (float)attackMs, (float)releaseMs, (float)holdMs);
}

static void set_compressor_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double threshDb, ratio, makeupDb, kneeDb, attackMs, releaseMs;
    if (!prompt_double("Threshold (dB): ", &threshDb)) return;
    if (!prompt_double("Ratio (e.g. 4.0): ", &ratio)) return;
    if (!prompt_double("Makeup (dB): ", &makeupDb)) return;
    if (!prompt_double("Knee (dB): ", &kneeDb)) return;
    if (!prompt_double("Attack (ms): ", &attackMs)) return;
    if (!prompt_double("Release (ms): ", &releaseMs)) return;
    pa_fx_compressor_set(pa, fx, (float)threshDb, (float)ratio, (float)makeupDb, (float)kneeDb, (float)attackMs, (float)releaseMs);
}

static void set_overdrive_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double driveDb, toneHz, outputDb;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Tone (Hz): ", &toneHz)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    pa_fx_overdrive_set(pa, fx, (float)driveDb, (float)toneHz, (float)outputDb);
}

static void set_distortion_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double driveDb, bassDb, midDb, trebleDb, outputDb;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Bass (dB): ", &bassDb)) return;
    if (!prompt_double("Mid (dB): ", &midDb)) return;
    if (!prompt_double("Treble (dB): ", &trebleDb)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    pa_fx_distortion_set(pa, fx, (float)driveDb, (float)bassDb, (float)midDb, (float)trebleDb, (float)outputDb);
}

static void set_fuzz_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double driveDb, outputDb;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    pa_fx_fuzz_set(pa, fx, (float)driveDb, (float)outputDb);
}

static void set_boost_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double gainDb;
    if (!prompt_double("Gain (dB): ", &gainDb)) return;
    pa_fx_boost_set(pa, fx, (float)gainDb);
}

static void set_tubescreamer_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double driveDb, tone, outputDb;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Tone (0.0-1.0): ", &tone)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    pa_fx_tubescreamer_set(pa, fx, (float)driveDb, (float)tone, (float)outputDb);
}

static void set_chorus_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double rateHz, depthMs, mix;
    if (!prompt_double("Rate (Hz): ", &rateHz)) return;
    if (!prompt_double("Depth (ms): ", &depthMs)) return;
    if (!prompt_double("Mix (0-1): ", &mix)) return;
    pa_fx_chorus_set(pa, fx, (float)rateHz, (float)depthMs, (float)mix);
}

static void set_flanger_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double rateHz, depthMs, feedback, mix;
    if (!prompt_double("Rate (Hz): ", &rateHz)) return;
    if (!prompt_double("Depth (ms): ", &depthMs)) return;
    if (!prompt_double("Feedback (0-1): ", &feedback)) return;
    if (!prompt_double("Mix (0-1): ", &mix)) return;
    pa_fx_flanger_set(pa, fx, (float)rateHz, (float)depthMs, (float)feedback, (float)mix);
}

static void set_phaser_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double rateHz, depth, feedback, mix;
    if (!prompt_double("Rate (Hz): ", &rateHz)) return;
    if (!prompt_double("Depth (0-1): ", &depth)) return;
    if (!prompt_double("Feedback (0-1): ", &feedback)) return;
    if (!prompt_double("Mix (0-1): ", &mix)) return;
    pa_fx_phaser_set(pa, fx, (float)rateHz, (float)depth, (float)feedback, (float)mix);
}

static void set_tremolo_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double rateHz, depth;
    if (!prompt_double("Rate (Hz): ", &rateHz)) return;
    if (!prompt_double("Depth (0-1): ", &depth)) return;
    pa_fx_tremolo_set(pa, fx, (float)rateHz, (float)depth);
}

static void set_vibrato_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double rateHz, depthMs;
    if (!prompt_double("Rate (Hz): ", &rateHz)) return;
    if (!prompt_double("Depth (ms): ", &depthMs)) return;
    pa_fx_vibrato_set(pa, fx, (float)rateHz, (float)depthMs);
}

static void set_delay_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double timeSec, feedback, dampHz, mix;
    if (!prompt_double("Time (sec): ", &timeSec)) return;
    if (!prompt_double("Feedback (0-1): ", &feedback)) return;
    if (!prompt_double("Damp (Hz): ", &dampHz)) return;
    if (!prompt_double("Mix (0-1): ", &mix)) return;
    pa_fx_delay_set(pa, fx, (float)timeSec, (float)feedback, (float)dampHz, (float)mix);
}

static void set_reverb_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double decay, dampHz, mix;
    if (!prompt_double("Decay (secs): ", &decay)) return;
    if (!prompt_double("Damp (Hz): ", &dampHz)) return;
    if (!prompt_double("Mix (0-1): ", &mix)) return;
    pa_fx_reverb_set(pa, fx, (float)decay, (float)dampHz, (float)mix);
}

static void set_wah_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double freq, Q, sensitivity;
    if (!prompt_double("Center frequency (Hz): ", &freq)) return;
    if (!prompt_double("Q (e.g. 0.5-10): ", &Q)) return;
    if (!prompt_double("Sensitivity: ", &sensitivity)) return;
    pa_fx_wah_set(pa, fx, (float)freq, (float)Q, (float)sensitivity);
}

static void set_eq3_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double bassDb, midDb, trebleDb;
    if (!prompt_double("Bass (dB): ", &bassDb)) return;
    if (!prompt_double("Mid (dB): ", &midDb)) return;
    if (!prompt_double("Treble (dB): ", &trebleDb)) return;
    pa_fx_eq3band_set(pa, fx, (float)bassDb, (float)midDb, (float)trebleDb);
}

static void set_eqparametric_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    long band;
    double freqHz, Q, gainDb;
    if (!prompt_int("Band index (0-3): ", &band)) return;
    if (!prompt_double("Freq (Hz): ", &freqHz)) return;
    if (!prompt_double("Q: ", &Q)) return;
    if (!prompt_double("Gain (dB): ", &gainDb)) return;
    fx_eqparametric_set_band(fx, (int)band, (float)freqHz, (float)Q, (float)gainDb);
}

static void set_preamp_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double inputDb, driveDb, bassDb, midDb, trebleDb, outputDb, sag;
    long tubeIdx;
    if (!prompt_double("Input (dB): ", &inputDb)) return;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Bass (dB): ", &bassDb)) return;
    if (!prompt_double("Mid (dB): ", &midDb)) return;
    if (!prompt_double("Treble (dB): ", &trebleDb)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    if (!prompt_double("Sag amount: ", &sag)) return;
    if (!prompt_int("Tube index (0..N): ", &tubeIdx)) return;
    pa_fx_preamp_set(pa, fx, (float)inputDb, (float)driveDb, (float)bassDb, (float)midDb, (float)trebleDb, (float)outputDb, (float)sag, (int)tubeIdx);
}

static void set_poweramp_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    double driveDb, outputDb, sag, sagTimeMs;
    long tubeIdx;
    if (!prompt_double("Drive (dB): ", &driveDb)) return;
    if (!prompt_double("Output (dB): ", &outputDb)) return;
    if (!prompt_double("Sag amount: ", &sag)) return;
    if (!prompt_double("Sag time (ms): ", &sagTimeMs)) return;
    if (!prompt_int("Tube index (0..N): ", &tubeIdx)) return;
    pa_fx_poweramp_set(pa, fx, (float)driveDb, (float)outputDb, (float)sag, (float)sagTimeMs, (int)tubeIdx);
}

static void set_cabinet_params(PortAudioHandler* pa, Effect* fx) {
    if (!fx) return;
    long cab;
    if (!prompt_int("Cabinet type index (int): ", &cab)) return;
    pa_fx_cabinet_set(pa, fx, (int)cab);
}

/* Dispatcher: ask user for type-specific params */
static void prompt_set_params_for_effect(PortAudioHandler* pa, Effect* fx) {
    if (!pa || !fx) return;
    switch (fx->type) {
        case FX_NONE: break;
        case FX_NOISEGATE: set_noisegate_params(pa, fx); break;
        case FX_COMPRESSOR: set_compressor_params(pa, fx); break;
        case FX_OVERDRIVE: set_overdrive_params(pa, fx); break;
        case FX_DISTORTION: set_distortion_params(pa, fx); break;
        case FX_FUZZ: set_fuzz_params(pa, fx); break;
        case FX_BOOST: set_boost_params(pa, fx); break;
        case FX_TUBESCREAMER: set_tubescreamer_params(pa, fx); break;
        case FX_CHORUS: set_chorus_params(pa, fx); break;
        case FX_FLANGER: set_flanger_params(pa, fx); break;
        case FX_PHASER: set_phaser_params(pa, fx); break;
        case FX_TREMOLO: set_tremolo_params(pa, fx); break;
        case FX_VIBRATO: set_vibrato_params(pa, fx); break;
        case FX_DELAY: set_delay_params(pa, fx); break;
        case FX_REVERB: set_reverb_params(pa, fx); break;
        case FX_WAH: set_wah_params(pa, fx); break;
        case FX_EQ_3BAND: set_eq3_params(pa, fx); break;
        case FX_EQ_PARAMETRIC: set_eqparametric_params(pa, fx); break;
        case FX_PREAMP: set_preamp_params(pa, fx); break;
        case FX_POWERAMP: set_poweramp_params(pa, fx); break;
        case FX_CABINET: set_cabinet_params(pa, fx); break;
        default:
            printf("No parameter UI for this effect type yet.\n");
            break;
    }
}

/* ---------- Main runtime menu (exhaustive) ---------- */

static void runtime_menu(PortAudioHandler* pa) {
    if (!pa) return;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    while (g_keep_running) {
        tui_clear();
        tui_header("LIVE AMP SIM - Main Menu");

        printf("Audio stream: %s\n", (pa->stream ? "OPEN" : "CLOSED"));
        printf("Input device: %d   Output device: %d\n", (int)pa->inputDevice, (int)pa->outputDevice);
        printf("Sample rate: %.1f   Block size: %u\n\n", pa->sampleRate, pa->blockSize);

        printf("Chain (%d effects):\n", portaudio_handler_effect_count(pa));
        int cnt = portaudio_handler_effect_count(pa);
        for (int i = 0; i < cnt; ++i) {
            Effect* e = portaudio_handler_get_effect_at(pa, i);
            printf(" [%2d] %s  enabled:%c bypass:%c\n", i,
                effect_type_name(e->type), e->enabled ? 'Y' : 'n', e->bypass ? 'Y' : 'n');
        }
        printf("\nCommands:\n");
        printf("  1) Add effect\n");
        printf("  2) Remove effect (by index)\n");
        printf("  3) Move effect (from->to)\n");
        printf("  4) Enable/Disable effect (by index)\n");
        printf("  5) Bypass/Unbypass effect (by index)\n");
        printf("  6) Edit effect parameters (by index)\n");
        printf("  7) Load preset (clean/crunch/lead/metal/fuzz/ambient/blues/shoegaze/funk)\n");
        printf("  8) Save chain to file (types+flags only)\n");
        printf("  9) Load chain from file (types+flags only)\n");
        printf("  a) Start audio\n");
        printf("  s) Stop audio\n");
        printf("  l) List available effect types\n");
        printf("  t) Show tube presets\n");
        printf("  q) Quit\n");

        printf("\nChoice: ");
        fflush(stdout);

        char buf[128];
        if (!read_line(buf, sizeof(buf))) break;
        char choice = buf[0];

        if (choice == '1') {
            /* Add effect — show effect type list */
            tui_clear();
            tui_header("Add Effect - choose type");
            for (int t = FX_NONE+1; t <= FX_CABINET; ++t) {
                printf("[%2d] %s\n", t, effect_type_name((EffectType)t));
            }
            printf("\nType number (or -1 to cancel): ");
            if (!read_line(buf, sizeof(buf))) continue;
            long vt;
            if (!parse_int(buf, &vt)) continue;
            if (vt < 0) continue;
            if (vt <= FX_NONE || vt > FX_CABINET) {
                printf("Invalid effect type\n");
                pause_enter();
                continue;
            }
            Effect* e = portaudio_handler_add_effect(pa, (EffectType)vt);
            if (!e) {
                printf("Failed to add effect\n");
            } else {
                printf("Added %s at end of chain\n", effect_type_name(e->type));
            }
            pause_enter();
        }
        else if (choice == '2') {
            printf("Index to remove: ");
            if (!read_line(buf, sizeof(buf))) continue;
            long idx;
            if (!parse_int(buf, &idx)) continue;
            Effect* e = portaudio_handler_get_effect_at(pa, (int)idx);
            if (!e) {
                printf("No effect at that index\n");
                pause_enter();
                continue;
            }
            portaudio_handler_remove_effect(pa, e);
            printf("Removed.\n");
            pause_enter();
        }
        else if (choice == '3') {
            printf("Move from index: ");
            if (!read_line(buf, sizeof(buf))) continue;
            long from;
            if (!parse_int(buf, &from)) continue;
            printf("Move to position (0..end): ");
            if (!read_line(buf, sizeof(buf))) continue;
            long to;
            if (!parse_int(buf, &to)) continue;
            Effect* e = portaudio_handler_get_effect_at(pa, (int)from);
            if (!e) {
                printf("No effect at from-index\n");
                pause_enter();
                continue;
            }
            portaudio_handler_move_effect(pa, e, (int)to);
            printf("Moved.\n");
            pause_enter();
        }
        else if (choice == '4') {
            printf("Index to toggle enable: ");
            if (!read_line(buf, sizeof(buf))) continue;
            long idx;
            if (!parse_int(buf, &idx)) continue;
            Effect* e = portaudio_handler_get_effect_at(pa, (int)idx);
            if (!e) {
                printf("No effect at that index\n");
                pause_enter();
                continue;
            }
            bool now = !effect_is_enabled(e);
            effect_enable(e, now);
            printf("Effect %s\n", now ? "enabled" : "disabled");
            pause_enter();
        }
        else if (choice == '5') {
            printf("Index to toggle bypass: ");
            if (!read_line(buf, sizeof(buf))) continue;
            long idx;
            if (!parse_int(buf, &idx)) continue;
            Effect* e = portaudio_handler_get_effect_at(pa, (int)idx);
            if (!e) {
                printf("No effect at that index\n");
                pause_enter();
                continue;
            }
            bool now = !effect_is_bypassed(e);
            effect_bypass(e, now);
            printf("Effect %s\n", now ? "bypassed" : "unbypassed");
            pause_enter();
        }
        else if (choice == '6') {
            printf("Index to edit params: ");
            if (!read_line(buf, sizeof(buf))) continue;
            long idx;
            if (!parse_int(buf, &idx)) continue;
            Effect* e = portaudio_handler_get_effect_at(pa, (int)idx);
            if (!e) {
                printf("No effect at that index\n");
                pause_enter();
                continue;
            }
            prompt_set_params_for_effect(pa, e);
            pause_enter();
        }
        else if (choice == '7') {
            printf("Preset name (clean/crunch/lead/metal/fuzz/ambient/blues/shoegaze/funk): ");
            if (!read_line(buf, sizeof(buf))) continue;
            portaudio_handler_load_preset(pa, buf);
            printf("Preset loaded (chain replaced accordingly)\n");
            pause_enter();
        }
        else if (choice == '8') {
            printf("Save path: ");
            if (!read_line(buf, sizeof(buf))) continue;
            if (save_chain_to_file(pa, buf)) printf("Saved.\n");
            else printf("Save failed.\n");
            pause_enter();
        }
        else if (choice == '9') {
            printf("Load path: ");
            if (!read_line(buf, sizeof(buf))) continue;
            if (load_chain_from_file(pa, buf)) printf("Loaded (types+flags). Parameters NOT restored.\n");
            else printf("Load failed.\n");
            pause_enter();
        }
        else if (choice == 'a' || choice == 'A') {
            if (pa->stream) {
                printf("Stream already open\n");
            } else {
                if (!portaudio_handler_open_stream(pa, pa->inputDevice, pa->outputDevice)) {
                    printf("Open stream failed\n");
                } else if (!portaudio_handler_start(pa)) {
                    printf("Start stream failed\n");
                } else {
                    printf("Audio started\n");
                }
            }
            pause_enter();
        }
        else if (choice == 's' || choice == 'S') {
            if (!pa->stream) {
                printf("No stream to stop\n");
            } else {
                if (!portaudio_handler_stop(pa)) {
                    printf("Stop failed\n");
                } else {
                    printf("Audio stopped\n");
                }
            }
            pause_enter();
        }
        else if (choice == 'l' || choice == 'L') {
            tui_clear();
            tui_header("Available Effect Types");
            for (int t = FX_NONE+1; t <= FX_CABINET; ++t) {
                printf("[%2d] %s\n", t, effect_type_name((EffectType)t));
            }
            pause_enter();
        }
        else if (choice == 't' || choice == 'T') {
            tui_clear();
            tui_header("Tube Presets");
            int n = portaudio_handler_get_tube_preset_count();
            for (int i = 0; i < n; ++i) {
                const TubeDef* td = portaudio_handler_get_tube_preset(i);
                if (!td) continue;
                printf("[%2d] %s  type:%d platV:%.1f screenV:%.1f\n",
                       i, td->name, td->type, td->platV, td->screenV);
            }
            pause_enter();
        }
        else if (choice == 'q' || choice == 'Q') {
            break;
        }
        else {
            /* ignore unknown */
        }
    }
}

/* ---------- main ---------- */

int main(void) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    int inDev = menu_choose_device("Select INPUT device", true, false);
    if (inDev < 0) {
        printf("No input selected, exiting\n");
        Pa_Terminate();
        return 0;
    }

    int outDev = menu_choose_device("Select OUTPUT device", false, true);
    if (outDev < 0) {
        printf("No output selected, exiting\n");
        Pa_Terminate();
        return 0;
    }

    PortAudioHandler pa;
    memset(&pa, 0, sizeof(pa));
    pa.inputDevice = (PaDeviceIndex)inDev;
    pa.outputDevice = (PaDeviceIndex)outDev;

    if (!portaudio_handler_init(&pa, 48000.0, 256u)) {
        fprintf(stderr, "portaudio_handler_init failed\n");
        Pa_Terminate();
        return 1;
    }

    if (!portaudio_handler_open_stream(&pa, (PaDeviceIndex)inDev, (PaDeviceIndex)outDev)) {
        fprintf(stderr, "portaudio_handler_open_stream failed\n");
        portaudio_handler_cleanup(&pa);
        Pa_Terminate();
        return 1;
    }

    if (!portaudio_handler_start(&pa)) {
        fprintf(stderr, "portaudio_handler_start failed\n");
        portaudio_handler_close(&pa);
        portaudio_handler_cleanup(&pa);
        Pa_Terminate();
        return 1;
    }

    printf("Audio started. Press ENTER to open runtime menu.\n");
    pause_enter();

    runtime_menu(&pa);

    /* cleanup */
    if (pa.stream) {
        portaudio_handler_stop(&pa);
        portaudio_handler_close(&pa);
    }

    portaudio_handler_cleanup(&pa);
    Pa_Terminate();
    printf("Goodbye.\n");
    return 0;
}
