#include "effects_interface.h"
#include "effects_dsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>



// int portaudio_handler_get_tube_preset_count(void) {
//     return NUM_TUBE_PRESETS;
// }

// const TubeDef* portaudio_handler_get_tube_preset(int index) {
//     if (index < 0 || index >= NUM_TUBE_PRESETS)
//         return NULL;
//     return &tube_presets[index];
// }



// ============================================================================
// EFFECT CREATION & PROCESSING
// ============================================================================

static void noisegate_process(Effect* fx, const float* in, float* out, size_t n) {
    NoiseGateData* ng = (NoiseGateData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* envOut = fx->dsp->scratch[8];
    env_process(&ng->env, in, envOut, n);
    
    for (size_t i = 0; i < n; i++) {
        if (envOut[i] > ng->threshold) {
            ng->holdCounter = ng->holdSamples;
            ng->attenuation = 1.0f;
        } else if (ng->holdCounter > 0.0f) {
            ng->holdCounter -= 1.0f;
        } else {
            ng->attenuation *= 0.99f;
        }
        out[i] = in[i] * ng->attenuation;
    }
}

static void compressor_process(Effect* fx, const float* in, float* out, size_t n) {
    CompressorData* c = (CompressorData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* inputDb = fx->dsp->scratch[8];
    float* gainRedDb = fx->dsp->scratch[9];
    float* smoothDb = fx->dsp->scratch[10];
    
    for (size_t i = 0; i < n; i++) {
        inputDb[i] = linear_to_db(fabsf(in[i]) + EPSILON_F);
    }
    
    float T = c->threshold;
    float knee = c->kneeWidth * 0.5f;
    for (size_t i = 0; i < n; i++) {
        float x = inputDb[i];
        if (knee > EPSILON_F) {
            if (x < T - knee) {
                gainRedDb[i] = 0.0f;
            } else if (x > T + knee) {
                gainRedDb[i] = (x - T) * (1.0f - 1.0f / c->ratio);
            } else {
                float delta = x - (T - knee);
                gainRedDb[i] = (1.0f - 1.0f / c->ratio) * (delta * delta) / (2.0f * knee);
            }
        } else {
            gainRedDb[i] = (x > T) ? (x - T) * (1.0f - 1.0f / c->ratio) : 0.0f;
        }
    }
    
    float a = c->env.attackCoeff;
    float r = c->env.releaseCoeff;
    float prev = c->prevGain;
    for (size_t i = 0; i < n; i++) {
        prev += ((gainRedDb[i] > prev) ? a : r) * (gainRedDb[i] - prev);
        smoothDb[i] = prev;
    }
    c->prevGain = prev;
    
    float makeupLin = db_to_linear(c->makeup);
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * db_to_linear(-smoothDb[i]) * makeupLin;
    }
}

static void overdrive_process(Effect* fx, const float* in, float* out, size_t n) {
    OverdriveData* od = (OverdriveData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    float* wsTable = fx->dsp->scratch[od->wsTableIdx];
    
    onepole_process(&od->hpf, in, buf1, n);
    for (size_t i = 0; i < n; i++) buf1[i] *= od->drive;
    waveshaper_lookup(buf1, buf2, wsTable, 4096, n);
    biquad_process(&od->tone, buf2, buf1, n);
    for (size_t i = 0; i < n; i++) out[i] = buf1[i] * od->outputGain;
}

static void distortion_process(Effect* fx, const float* in, float* out, size_t n) {
    DistortionData* d = (DistortionData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    float* wsTable = fx->dsp->scratch[d->wsTableIdx];
    
    onepole_process(&d->hpf, in, buf1, n);
    for (size_t i = 0; i < n; i++) buf1[i] *= d->drive;
    waveshaper_lookup(buf1, buf2, wsTable, 4096, n);
    biquad_process(&d->toneStack[0], buf2, buf1, n);
    biquad_process(&d->toneStack[1], buf1, buf2, n);
    biquad_process(&d->toneStack[2], buf2, buf1, n);
    for (size_t i = 0; i < n; i++) out[i] = buf1[i] * d->outputGain;
}

static void fuzz_process(Effect* fx, const float* in, float* out, size_t n) {
    FuzzData* f = (FuzzData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    float* wsTable = fx->dsp->scratch[f->wsTableIdx];
    
    onepole_process(&f->hpf, in, buf1, n);
    for (size_t i = 0; i < n; i++) buf1[i] *= f->drive;
    waveshaper_lookup(buf1, buf2, wsTable, 4096, n);
    for (size_t i = 0; i < n; i++) out[i] = buf2[i] * f->outputGain;
}

static void boost_process(Effect* fx, const float* in, float* out, size_t n) {
    BoostData* b = (BoostData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    for (size_t i = 0; i < n; i++) out[i] = in[i] * b->gain;
}

static void tubescreamer_process(Effect* fx, const float* in, float* out, size_t n) {
    TubeScreamerData* ts = (TubeScreamerData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    float* wsTable = fx->dsp->scratch[ts->wsTableIdx];
    
    onepole_process(&ts->hpf, in, buf1, n);
    biquad_process(&ts->midBoost, buf1, buf2, n);
    for (size_t i = 0; i < n; i++) buf2[i] *= ts->drive;
    waveshaper_lookup(buf2, buf1, wsTable, 4096, n);
    for (size_t i = 0; i < n; i++) out[i] = buf1[i] * ts->outputGain;
}

static void chorus_process(Effect* fx, const float* in, float* out, size_t n) {
    ChorusData* ch = (ChorusData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* lfoOut = fx->dsp->scratch[8];
    float* delayed1 = fx->dsp->scratch[9];
    float* delayed2 = fx->dsp->scratch[10];
    
    lfo_process(&ch->lfo, lfoOut, n);
    
    delayline_write(&ch->delayLines[0], in, n);
    delayline_write(&ch->delayLines[1], in, n);
    
    for (size_t i = 0; i < n; i++) {
        float mod = lfoOut[i] * ch->depth;
        float d1 = 0.010f * fx->dsp->sampleRate + mod;
        float d2 = 0.015f * fx->dsp->sampleRate - mod;
        delayline_read_linear(&ch->delayLines[0], &delayed1[i], 1, d1);
        delayline_read_linear(&ch->delayLines[1], &delayed2[i], 1, d2);
    }
    
    for (size_t i = 0; i < n; i++) {
        float wet = (delayed1[i] + delayed2[i]) * 0.5f;
        out[i] = in[i] * (1.0f - ch->mix) + wet * ch->mix;
    }
}

static void flanger_process(Effect* fx, const float* in, float* out, size_t n) {
    FlangerData* fl = (FlangerData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* lfoOut = fx->dsp->scratch[8];
    float* delayed = fx->dsp->scratch[9];
    
    lfo_process(&fl->lfo, lfoOut, n);
    
    for (size_t i = 0; i < n; i++) {
        float mod = lfoOut[i] * fl->depth;
        float delaySamps = 0.001f * fx->dsp->sampleRate + mod;
        
        float feedback = delayed[i] * fl->feedback;
        float inputWithFb = in[i] + feedback;
        
        delayline_write(&fl->delayLines[0], &inputWithFb, 1);
        delayline_read_linear(&fl->delayLines[0], &delayed[i], 1, delaySamps);
        
        out[i] = in[i] * (1.0f - fl->mix) + delayed[i] * fl->mix;
    }
}

static void phaser_process(Effect* fx, const float* in, float* out, size_t n) {
    PhaserData* ph = (PhaserData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* lfoOut = fx->dsp->scratch[8];
    float* buf = fx->dsp->scratch[9];
    
    lfo_process(&ph->lfo, lfoOut, n);
    memcpy(buf, in, n * sizeof(float));
    
    for (int stage = 0; stage < 4; stage++) {
        for (size_t i = 0; i < n; i++) {
            float mod = lfoOut[i] * ph->depth;
            ph->allpass[stage].g = 0.5f + mod * 0.4f;
        }
        allpass_delay_process(&ph->allpass[stage], buf, buf, n);
    }
    
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * (1.0f - ph->mix) + buf[i] * ph->mix;
    }
}

static void tremolo_process(Effect* fx, const float* in, float* out, size_t n) {
    TremoloData* tr = (TremoloData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* lfoOut = fx->dsp->scratch[8];
    lfo_process(&tr->lfo, lfoOut, n);
    
    for (size_t i = 0; i < n; i++) {
        float mod = 1.0f - tr->depth + lfoOut[i] * tr->depth;
        out[i] = in[i] * mod;
    }
}

static void vibrato_process(Effect* fx, const float* in, float* out, size_t n) {
    VibratoData* vib = (VibratoData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* lfoOut = fx->dsp->scratch[8];
    lfo_process(&vib->lfo, lfoOut, n);
    
    delayline_write(&vib->delayLine, in, n);
    
    for (size_t i = 0; i < n; i++) {
        float mod = lfoOut[i] * vib->depth;
        float delaySamps = 0.005f * fx->dsp->sampleRate + mod;
        delayline_read_cubic(&vib->delayLine, &out[i], 1, delaySamps);
    }
}

static void delay_process(Effect* fx, const float* in, float* out, size_t n) {
    DelayData* dl = (DelayData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* delayed = fx->dsp->scratch[8];
    float* damped = fx->dsp->scratch[9];
    
    float delaySamps = dl->delayTime * fx->dsp->sampleRate;
    delayline_read_linear(&dl->delayLine, delayed, n, delaySamps);
    biquad_process(&dl->dampFilter, delayed, damped, n);
    
    for (size_t i = 0; i < n; i++) {
        float feedbackSamp = damped[i] * dl->feedback + in[i];
        delayline_write(&dl->delayLine, &feedbackSamp, 1);
        out[i] = in[i] * (1.0f - dl->mix) + damped[i] * dl->mix;
    }
}

static void reverb_process(Effect* fx, const float* in, float* out, size_t n) {
    ReverbData* rv = (ReverbData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* delayed = fx->dsp->scratch[8];
    memset(delayed, 0, n * sizeof(float));
    
    static const float delayTimes[] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0050f, 0.0017f, 0.0041f, 0.0023f};
    
    for (int i = 0; i < 8; i++) {
        float* temp = fx->dsp->scratch[9 + i % 4];
        float delaySamps = delayTimes[i] * fx->dsp->sampleRate;
        
        delayline_write(&rv->delays[i], in, n);
        delayline_read_linear(&rv->delays[i], temp, n, delaySamps);
        biquad_process(&rv->damping[i], temp, temp, n);
        
        for (size_t j = 0; j < n; j++) {
            delayed[j] += temp[j] * rv->decay;
        }
    }
    
    for (size_t i = 0; i < n; i++) {
        out[i] = in[i] * (1.0f - rv->mix) + delayed[i] * rv->mix;
    }
}

static void wah_process(Effect* fx, const float* in, float* out, size_t n) {
    WahData* wah = (WahData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* envOut = fx->dsp->scratch[8];
    env_process(&wah->env, in, envOut, n);
    
    for (size_t i = 0; i < n; i++) {
        float freq = 400.0f + envOut[i] * wah->sensitivity * 2000.0f;
        biquad_set_params(&wah->wahFilter, BQ_BPF, freq, wah->Q, 0.0f);
        
        float temp;
        biquad_process(&wah->wahFilter, &in[i], &temp, 1);
        out[i] = temp;
    }
}

static void eq3band_process(Effect* fx, const float* in, float* out, size_t n) {
    EQ3BandData* eq = (EQ3BandData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    
    biquad_process(&eq->bass, in, buf1, n);
    biquad_process(&eq->mid, buf1, buf2, n);
    biquad_process(&eq->treble, buf2, out, n);
}

static void eqparametric_process(Effect* fx, const float* in, float* out, size_t n) {
    EQParametricData* eq = (EQParametricData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    
    biquad_process(&eq->filters[0], in, buf1, n);
    biquad_process(&eq->filters[1], buf1, buf2, n);
    biquad_process(&eq->filters[2], buf2, buf1, n);
    biquad_process(&eq->filters[3], buf1, out, n);
}

static void preamp_process(Effect* fx, const float* in, float* out, size_t n) {
    PreampData* pre = (PreampData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    float* tubeTable = fx->dsp->scratch[pre->tubeTableIdx];
    
    for (size_t i = 0; i < n; i++) buf1[i] = in[i] * pre->inputGain;
    biquad_process(&pre->inputHPF, buf1, buf2, n);
    for (size_t i = 0; i < n; i++) buf2[i] *= pre->drive;
    waveshaper_lookup(buf2, buf1, tubeTable, TUBE_TABLE_SIZE, n);
    
    biquad_process(&pre->toneStack[0], buf1, buf2, n);
    biquad_process(&pre->toneStack[1], buf2, buf1, n);
    biquad_process(&pre->toneStack[2], buf1, buf2, n);
    
    for (size_t i = 0; i < n; i++) {
        float sagDrop = fabsf(buf2[i]) * pre->sagAmount;
        pre->sagState += (1.0f - sagDrop - pre->sagState) * 0.01f;
        buf2[i] *= pre->sagState;
        out[i] = buf2[i] * pre->outputGain;
    }
}

static void poweramp_process(Effect* fx, const float* in, float* out, size_t n) {
    PowerampData* pa = (PowerampData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* tubeTable = fx->dsp->scratch[pa->tubeTableIdx];
    
    for (size_t i = 0; i < n; i++) buf1[i] = in[i] * pa->drive;
    waveshaper_lookup(buf1, buf1, tubeTable, TUBE_TABLE_SIZE, n);
    
    float sagCoeff = (pa->sagTime / 1000.0f) * fx->dsp->sampleRate;
    sagCoeff = clampf(sagCoeff, 0.0f, 1.0f);
    
    for (size_t i = 0; i < n; i++) {
        float sagDrop = fabsf(buf1[i]) * pa->sagAmount;
        float targetV = pa->supplyV - sagDrop;
        pa->sagState += (targetV - pa->sagState) * sagCoeff;
        float sagNorm = pa->supplyV / fmaxf(pa->sagState, 1.0f);
        out[i] = buf1[i] * sagNorm * pa->outputGain;
    }
}

static void cabinet_process(Effect* fx, const float* in, float* out, size_t n) {
    CabinetData* cab = (CabinetData*)fx->data;
    if (!fx->enabled || fx->bypass) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = fx->dsp->scratch[8];
    float* buf2 = fx->dsp->scratch[9];
    
    biquad_process(&cab->lowResonance, in, buf1, n);
    biquad_process(&cab->midPresence, buf1, buf2, n);
    biquad_process(&cab->highDamping, buf2, out, n);
}

// ============================================================================
// EFFECT CHAIN MANAGEMENT
// ============================================================================

void effect_chain_init(EffectChain* chain, DSPState* dsp, size_t maxBlockSize) {
    chain->dsp = dsp;
    chain->head = NULL;
    chain->tail = NULL;
    chain->bufferSize = maxBlockSize;
    chain->chainBuffer = (float*)calloc(maxBlockSize, sizeof(float));
    
    for (int i = 20; i < 24; i++) {
        build_waveshaper_table(dsp->scratch[i], 4096, CLIP_SOFT_TANH + (i - 20), 1.0f);
    }
    
    for (int i = 0; i < NUM_TUBE_PRESETS; i++) {
        float* tubeTable = dsp->scratch[24 + i];
        const TubeDef* td = &tube_presets[i];
        build_tube_table_from_koren(tubeTable, TUBE_TABLE_SIZE, td->type, &td->params, 0.0f, td->platV, td->platV, td->screenV);
    }
}

void effect_chain_cleanup(EffectChain* chain) {
    Effect* curr = chain->head;
    while (curr) {
        Effect* next = curr->next;
        if (curr->cleanup) curr->cleanup(curr);
        if (curr->data) free(curr->data);
        free(curr);
        curr = next;
    }
    if (chain->chainBuffer) free(chain->chainBuffer);
    chain->head = NULL;
    chain->tail = NULL;
}

void effect_chain_process(EffectChain* chain, const float* in, float* out, size_t n) {
    if (!chain->head) {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    
    float* buf1 = chain->chainBuffer;
    float* buf2 = chain->dsp->scratch[30];
    
    memcpy(buf1, in, n * sizeof(float));
    
    Effect* curr = chain->head;
    while (curr) {
        curr->process(curr, buf1, buf2, n);
        float* temp = buf1;
        buf1 = buf2;
        buf2 = temp;
        curr = curr->next;
    }
    
    memcpy(out, buf1, n * sizeof(float));
}

Effect* effect_chain_add(EffectChain* chain, EffectType type) {
    Effect* fx = (Effect*)calloc(1, sizeof(Effect));
    fx->type = type;
    fx->enabled = true;
    fx->bypass = false;
    fx->dsp = chain->dsp;
    fx->next = NULL;
    
    switch (type) {
        case FX_NOISEGATE: {
            NoiseGateData* ng = (NoiseGateData*)calloc(1, sizeof(NoiseGateData));
            env_init(&ng->env, chain->dsp, 1.0f, 100.0f, 0);
            ng->threshold = db_to_linear(-40.0f);
            ng->holdSamples = 0.05f * chain->dsp->sampleRate;
            ng->holdCounter = 0.0f;
            ng->attenuation = 0.0f;
            fx->data = ng;
            fx->process = noisegate_process;
            break;
        }
        
        case FX_COMPRESSOR: {
            CompressorData* c = (CompressorData*)calloc(1, sizeof(CompressorData));
            env_init(&c->env, chain->dsp, 10.0f, 100.0f, 1);
            c->threshold = -20.0f;
            c->ratio = 4.0f;
            c->makeup = 0.0f;
            c->kneeWidth = 0.0f;
            c->prevGain = 0.0f;
            fx->data = c;
            fx->process = compressor_process;
            break;
        }
        
        case FX_OVERDRIVE: {
            OverdriveData* od = (OverdriveData*)calloc(1, sizeof(OverdriveData));
            onepole_init(&od->hpf, chain->dsp, 10.0f, 0);
            biquad_init(&od->tone, chain->dsp, BQ_LPF, 5000.0f, 0.707f, 0.0f);
            od->drive = 3.0f;
            od->outputGain = 0.5f;
            od->wsTableIdx = 20;
            fx->data = od;
            fx->process = overdrive_process;
            break;
        }
        
        case FX_DISTORTION: {
            DistortionData* d = (DistortionData*)calloc(1, sizeof(DistortionData));
            onepole_init(&d->hpf, chain->dsp, 20.0f, 1);
            biquad_init(&d->toneStack[0], chain->dsp, BQ_LOWSHELF, 200.0f, 0.707f, 0.0f);
            biquad_init(&d->toneStack[1], chain->dsp, BQ_PEAK, 1000.0f, 0.707f, 0.0f);
            biquad_init(&d->toneStack[2], chain->dsp, BQ_HIGHSHELF, 5000.0f, 0.707f, 0.0f);
            d->drive = 10.0f;
            d->outputGain = 0.3f;
            d->wsTableIdx = 21;
            fx->data = d;
            fx->process = distortion_process;
            break;
        }
        
        case FX_FUZZ: {
            FuzzData* f = (FuzzData*)calloc(1, sizeof(FuzzData));
            onepole_init(&f->hpf, chain->dsp, 50.0f, 1);
            f->drive = 20.0f;
            f->outputGain = 0.2f;
            f->wsTableIdx = 22;
            fx->data = f;
            fx->process = fuzz_process;
            break;
        }
        
        case FX_BOOST: {
            BoostData* b = (BoostData*)calloc(1, sizeof(BoostData));
            b->gain = 2.0f;
            fx->data = b;
            fx->process = boost_process;
            break;
        }
        
        case FX_TUBESCREAMER: {
            TubeScreamerData* ts = (TubeScreamerData*)calloc(1, sizeof(TubeScreamerData));
            onepole_init(&ts->hpf, chain->dsp, 720.0f, 0);
            biquad_init(&ts->midBoost, chain->dsp, BQ_PEAK, 720.0f, 0.5f, 12.0f);
            ts->drive = 5.0f;
            ts->tone = 0.5f;
            ts->outputGain = 0.4f;
            ts->wsTableIdx = 20;
            fx->data = ts;
            fx->process = tubescreamer_process;
            break;
        }
        
        case FX_CHORUS: {
            ChorusData* ch = (ChorusData*)calloc(1, sizeof(ChorusData));
            size_t delaySize = (size_t)(0.05f * chain->dsp->sampleRate);
            ch->delayBufs[0] = (float*)calloc(delaySize, sizeof(float));
            ch->delayBufs[1] = (float*)calloc(delaySize, sizeof(float));
            delayline_init(&ch->delayLines[0], chain->dsp, ch->delayBufs[0], delaySize);
            delayline_init(&ch->delayLines[1], chain->dsp, ch->delayBufs[1], delaySize);
            lfo_init(&ch->lfo, chain->dsp, LFO_SINE, 1.5f, 1.0f, 0.0f);
            ch->depth = 0.002f * chain->dsp->sampleRate;
            ch->mix = 0.5f;
            fx->data = ch;
            fx->process = chorus_process;
            fx->cleanup = NULL;
            break;
        }
        
        case FX_FLANGER: {
            FlangerData* fl = (FlangerData*)calloc(1, sizeof(FlangerData));
            size_t delaySize = (size_t)(0.01f * chain->dsp->sampleRate);
            fl->delayBufs[0] = (float*)calloc(delaySize, sizeof(float));
            fl->delayBufs[1] = (float*)calloc(delaySize, sizeof(float));
            delayline_init(&fl->delayLines[0], chain->dsp, fl->delayBufs[0], delaySize);
            delayline_init(&fl->delayLines[1], chain->dsp, fl->delayBufs[1], delaySize);
            lfo_init(&fl->lfo, chain->dsp, LFO_SINE, 0.5f, 1.0f, 0.0f);
            fl->depth = 0.001f * chain->dsp->sampleRate;
            fl->feedback = 0.5f;
            fl->mix = 0.5f;
            fx->data = fl;
            fx->process = flanger_process;
            break;
        }
        
        case FX_PHASER: {
            PhaserData* ph = (PhaserData*)calloc(1, sizeof(PhaserData));
            for (int i = 0; i < 4; i++) {
                size_t apSize = 256;
                ph->apBufs[i] = (float*)calloc(apSize, sizeof(float));
                allpass_delay_init(&ph->allpass[i], ph->apBufs[i], apSize, 0.7f);
            }
            lfo_init(&ph->lfo, chain->dsp, LFO_SINE, 0.5f, 1.0f, 0.0f);
            ph->depth = 0.5f;
            ph->feedback = 0.7f;
            ph->mix = 0.5f;
            fx->data = ph;
            fx->process = phaser_process;
            break;
        }
        
        case FX_TREMOLO: {
            TremoloData* tr = (TremoloData*)calloc(1, sizeof(TremoloData));
            lfo_init(&tr->lfo, chain->dsp, LFO_SINE, 5.0f, 1.0f, 0.0f);
            tr->depth = 0.5f;
            fx->data = tr;
            fx->process = tremolo_process;
            break;
        }
        
        case FX_VIBRATO: {
            VibratoData* vib = (VibratoData*)calloc(1, sizeof(VibratoData));
            size_t delaySize = (size_t)(0.02f * chain->dsp->sampleRate);
            vib->delayBuf = (float*)calloc(delaySize, sizeof(float));
            delayline_init(&vib->delayLine, chain->dsp, vib->delayBuf, delaySize);
            lfo_init(&vib->lfo, chain->dsp, LFO_SINE, 5.0f, 1.0f, 0.0f);
            vib->depth = 0.003f * chain->dsp->sampleRate;
            fx->data = vib;
            fx->process = vibrato_process;
            break;
        }
        
        case FX_DELAY: {
            DelayData* dl = (DelayData*)calloc(1, sizeof(DelayData));
            size_t delaySize = (size_t)(2.0f * chain->dsp->sampleRate);
            dl->delayBuf = (float*)calloc(delaySize, sizeof(float));
            delayline_init(&dl->delayLine, chain->dsp, dl->delayBuf, delaySize);
            biquad_init(&dl->dampFilter, chain->dsp, BQ_LPF, 4000.0f, 0.707f, 0.0f);
            dl->delayTime = 0.5f;
            dl->feedback = 0.4f;
            dl->mix = 0.3f;
            fx->data = dl;
            fx->process = delay_process;
            break;
        }
        
        case FX_REVERB: {
            ReverbData* rv = (ReverbData*)calloc(1, sizeof(ReverbData));
            static const float delays[] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0050f, 0.0017f, 0.0041f, 0.0023f};
            for (int i = 0; i < 8; i++) {
                size_t delaySize = (size_t)(delays[i] * chain->dsp->sampleRate * 2.0f);
                rv->delayBufs[i] = (float*)calloc(delaySize, sizeof(float));
                delayline_init(&rv->delays[i], chain->dsp, rv->delayBufs[i], delaySize);
                biquad_init(&rv->damping[i], chain->dsp, BQ_LPF, 5000.0f, 0.707f, 0.0f);
            }
            rv->decay = 0.5f;
            rv->mix = 0.3f;
            fx->data = rv;
            fx->process = reverb_process;
            break;
        }
        
        case FX_WAH: {
            WahData* wah = (WahData*)calloc(1, sizeof(WahData));
            biquad_init(&wah->wahFilter, chain->dsp, BQ_BPF, 1000.0f, 10.0f, 0.0f);
            env_init(&wah->env, chain->dsp, 10.0f, 50.0f, 0);
            wah->freq = 1000.0f;
            wah->Q = 10.0f;
            wah->sensitivity = 1.0f;
            fx->data = wah;
            fx->process = wah_process;
            break;
        }
        
        case FX_EQ_3BAND: {
            EQ3BandData* eq = (EQ3BandData*)calloc(1, sizeof(EQ3BandData));
            biquad_init(&eq->bass, chain->dsp, BQ_LOWSHELF, 200.0f, 0.707f, 0.0f);
            biquad_init(&eq->mid, chain->dsp, BQ_PEAK, 1000.0f, 0.707f, 0.0f);
            biquad_init(&eq->treble, chain->dsp, BQ_HIGHSHELF, 5000.0f, 0.707f, 0.0f);
            fx->data = eq;
            fx->process = eq3band_process;
            break;
        }
        
        case FX_EQ_PARAMETRIC: {
            EQParametricData* eq = (EQParametricData*)calloc(1, sizeof(EQParametricData));
            float freqs[] = {100.0f, 500.0f, 2000.0f, 8000.0f};
            for (int i = 0; i < 4; i++) {
                biquad_init(&eq->filters[i], chain->dsp, BQ_PEAK, freqs[i], 1.0f, 0.0f);
                eq->freqs[i] = freqs[i];
                eq->Qs[i] = 1.0f;
                eq->gains[i] = 0.0f;
            }
            fx->data = eq;
            fx->process = eqparametric_process;
            break;
        }
        
        case FX_PREAMP: {
            PreampData* pre = (PreampData*)calloc(1, sizeof(PreampData));
            biquad_init(&pre->inputHPF, chain->dsp, BQ_HPF, 10.0f, 0.707f, 0.0f);
            biquad_init(&pre->toneStack[0], chain->dsp, BQ_LOWSHELF, 100.0f, 0.707f, 0.0f);
            biquad_init(&pre->toneStack[1], chain->dsp, BQ_PEAK, 800.0f, 0.707f, 0.0f);
            biquad_init(&pre->toneStack[2], chain->dsp, BQ_HIGHSHELF, 3000.0f, 0.707f, 0.0f);
            pre->inputGain = 1.0f;
            pre->drive = 3.0f;
            pre->outputGain = 1.0f;
            pre->sagAmount = 0.0f;
            pre->sagState = 1.0f;
            pre->tubeTableIdx = 26;
            fx->data = pre;
            fx->process = preamp_process;
            break;
        }
        
        case FX_POWERAMP: {
            PowerampData* pa = (PowerampData*)calloc(1, sizeof(PowerampData));
            pa->drive = 2.0f;
            pa->outputGain = 1.0f;
            pa->sagAmount = 0.0f;
            pa->sagTime = 10.0f;
            pa->sagState = 400.0f;
            pa->supplyV = 400.0f;
            pa->tubeTableIdx = 25;
            fx->data = pa;
            fx->process = poweramp_process;
            break;
        }
        
        case FX_CABINET: {
            CabinetData* cab = (CabinetData*)calloc(1, sizeof(CabinetData));
            biquad_init(&cab->lowResonance, chain->dsp, BQ_PEAK, 120.0f, 0.5f, 6.0f);
            biquad_init(&cab->midPresence, chain->dsp, BQ_PEAK, 3000.0f, 0.707f, 3.0f);
            biquad_init(&cab->highDamping, chain->dsp, BQ_LPF, 4000.0f, 0.707f, 0.0f);
            cab->cabinetType = 0;
            fx->data = cab;
            fx->process = cabinet_process;
            break;
        }
        
        default:
            free(fx);
            return NULL;
    }
    
    if (!chain->head) {
        chain->head = fx;
        chain->tail = fx;
    } else {
        chain->tail->next = fx;
        chain->tail = fx;
    }
    
    return fx;
}

void effect_chain_remove(EffectChain* chain, Effect* fx) {
    if (!fx || !chain->head) return;
    
    if (chain->head == fx) {
        chain->head = fx->next;
        if (chain->tail == fx) chain->tail = NULL;
    } else {
        Effect* curr = chain->head;
        while (curr->next && curr->next != fx) {
            curr = curr->next;
        }
        if (curr->next == fx) {
            curr->next = fx->next;
            if (chain->tail == fx) chain->tail = curr;
        }
    }
    
    if (fx->cleanup) fx->cleanup(fx);
    if (fx->data) free(fx->data);
    free(fx);
}

void effect_chain_clear(EffectChain* chain) {
    Effect* curr = chain->head;
    while (curr) {
        Effect* next = curr->next;
        if (curr->cleanup) curr->cleanup(curr);
        if (curr->data) free(curr->data);
        free(curr);
        curr = next;
    }
    chain->head = NULL;
    chain->tail = NULL;
}

Effect* effect_chain_find(EffectChain* chain, EffectType type) {
    Effect* curr = chain->head;
    while (curr) {
        if (curr->type == type) return curr;
        curr = curr->next;
    }
    return NULL;
}

void effect_chain_move(EffectChain* chain, Effect* fx, int newPosition) {
    if (!fx || !chain->head) return;
    
    effect_chain_remove(chain, fx);
    
    if (newPosition <= 0 || !chain->head) {
        fx->next = chain->head;
        chain->head = fx;
        if (!chain->tail) chain->tail = fx;
        return;
    }
    
    Effect* curr = chain->head;
    int pos = 0;
    while (curr->next && pos < newPosition - 1) {
        curr = curr->next;
        pos++;
    }
    
    fx->next = curr->next;
    curr->next = fx;
    if (!fx->next) chain->tail = fx;
}

// ============================================================================
// PARAMETER SETTERS
// ============================================================================

void fx_noisegate_set(Effect* fx, float threshDb, float attackMs, float releaseMs, float holdMs) {
    if (fx->type != FX_NOISEGATE) return;
    NoiseGateData* ng = (NoiseGateData*)fx->data;
    ng->threshold = db_to_linear(threshDb);
    ng->env.attackCoeff = ms_to_coeff(attackMs, fx->dsp->sampleRate);
    ng->env.releaseCoeff = ms_to_coeff(releaseMs, fx->dsp->sampleRate);
    ng->holdSamples = (holdMs / 1000.0f) * fx->dsp->sampleRate;
}

void fx_compressor_set(Effect* fx, float threshDb, float ratio, float makeupDb, float kneeDb, float attackMs, float releaseMs) {
    if (fx->type != FX_COMPRESSOR) return;
    CompressorData* c = (CompressorData*)fx->data;
    c->threshold = threshDb;
    c->ratio = ratio;
    c->makeup = makeupDb;
    c->kneeWidth = kneeDb;
    c->env.attackCoeff = ms_to_coeff(attackMs, fx->dsp->sampleRate);
    c->env.releaseCoeff = ms_to_coeff(releaseMs, fx->dsp->sampleRate);
}

void fx_overdrive_set(Effect* fx, float driveDb, float toneHz, float outputDb) {
    if (fx->type != FX_OVERDRIVE) return;
    OverdriveData* od = (OverdriveData*)fx->data;
    od->drive = db_to_linear(driveDb);
    od->outputGain = db_to_linear(outputDb);
    biquad_set_params(&od->tone, BQ_LPF, toneHz, 0.707f, 0.0f);
}

void fx_distortion_set(Effect* fx, float driveDb, float bassDb, float midDb, float trebleDb, float outputDb) {
    if (fx->type != FX_DISTORTION) return;
    DistortionData* d = (DistortionData*)fx->data;
    d->drive = db_to_linear(driveDb);
    d->outputGain = db_to_linear(outputDb);
    biquad_set_params(&d->toneStack[0], BQ_LOWSHELF, 200.0f, 0.707f, bassDb);
    biquad_set_params(&d->toneStack[1], BQ_PEAK, 1000.0f, 0.707f, midDb);
    biquad_set_params(&d->toneStack[2], BQ_HIGHSHELF, 5000.0f, 0.707f, trebleDb);
}

void fx_fuzz_set(Effect* fx, float driveDb, float outputDb) {
    if (fx->type != FX_FUZZ) return;
    FuzzData* f = (FuzzData*)fx->data;
    f->drive = db_to_linear(driveDb);
    f->outputGain = db_to_linear(outputDb);
}

void fx_boost_set(Effect* fx, float gainDb) {
    if (fx->type != FX_BOOST) return;
    BoostData* b = (BoostData*)fx->data;
    b->gain = db_to_linear(gainDb);
}

void fx_tubescreamer_set(Effect* fx, float driveDb, float tone, float outputDb) {
    if (fx->type != FX_TUBESCREAMER) return;
    TubeScreamerData* ts = (TubeScreamerData*)fx->data;
    ts->drive = db_to_linear(driveDb);
    ts->tone = tone;
    ts->outputGain = db_to_linear(outputDb);
}

void fx_chorus_set(Effect* fx, float rateHz, float depthMs, float mix) {
    if (fx->type != FX_CHORUS) return;
    ChorusData* ch = (ChorusData*)fx->data;
    lfo_set_freq(&ch->lfo, rateHz);
    ch->depth = (depthMs / 1000.0f) * fx->dsp->sampleRate;
    ch->mix = clampf(mix, 0.0f, 1.0f);
}

void fx_flanger_set(Effect* fx, float rateHz, float depthMs, float feedback, float mix) {
    if (fx->type != FX_FLANGER) return;
    FlangerData* fl = (FlangerData*)fx->data;
    lfo_set_freq(&fl->lfo, rateHz);
    fl->depth = (depthMs / 1000.0f) * fx->dsp->sampleRate;
    fl->feedback = clampf(feedback, 0.0f, 0.95f);
    fl->mix = clampf(mix, 0.0f, 1.0f);
}

void fx_phaser_set(Effect* fx, float rateHz, float depth, float feedback, float mix) {
    if (fx->type != FX_PHASER) return;
    PhaserData* ph = (PhaserData*)fx->data;
    lfo_set_freq(&ph->lfo, rateHz);
    ph->depth = clampf(depth, 0.0f, 1.0f);
    ph->feedback = clampf(feedback, 0.0f, 0.95f);
    ph->mix = clampf(mix, 0.0f, 1.0f);
}

void fx_tremolo_set(Effect* fx, float rateHz, float depth) {
    if (fx->type != FX_TREMOLO) return;
    TremoloData* tr = (TremoloData*)fx->data;
    lfo_set_freq(&tr->lfo, rateHz);
    tr->depth = clampf(depth, 0.0f, 1.0f);
}

void fx_vibrato_set(Effect* fx, float rateHz, float depthMs) {
    if (fx->type != FX_VIBRATO) return;
    VibratoData* vib = (VibratoData*)fx->data;
    lfo_set_freq(&vib->lfo, rateHz);
    vib->depth = (depthMs / 1000.0f) * fx->dsp->sampleRate;
}

void fx_delay_set(Effect* fx, float timeSec, float feedback, float dampHz, float mix) {
    if (fx->type != FX_DELAY) return;
    DelayData* dl = (DelayData*)fx->data;
    dl->delayTime = clampf(timeSec, 0.001f, 2.0f);
    dl->feedback = clampf(feedback, 0.0f, 0.95f);
    dl->mix = clampf(mix, 0.0f, 1.0f);
    biquad_set_params(&dl->dampFilter, BQ_LPF, dampHz, 0.707f, 0.0f);
}

void fx_reverb_set(Effect* fx, float decay, float dampHz, float mix) {
    if (fx->type != FX_REVERB) return;
    ReverbData* rv = (ReverbData*)fx->data;
    rv->decay = clampf(decay, 0.0f, 0.95f);
    rv->mix = clampf(mix, 0.0f, 1.0f);
    for (int i = 0; i < 8; i++) {
        biquad_set_params(&rv->damping[i], BQ_LPF, dampHz, 0.707f, 0.0f);
    }
}

void fx_wah_set(Effect* fx, float freq, float Q, float sensitivity) {
    if (fx->type != FX_WAH) return;
    WahData* wah = (WahData*)fx->data;
    wah->freq = freq;
    wah->Q = Q;
    wah->sensitivity = sensitivity;
}

void fx_eq3band_set(Effect* fx, float bassDb, float midDb, float trebleDb) {
    if (fx->type != FX_EQ_3BAND) return;
    EQ3BandData* eq = (EQ3BandData*)fx->data;
    biquad_set_params(&eq->bass, BQ_LOWSHELF, 200.0f, 0.707f, bassDb);
    biquad_set_params(&eq->mid, BQ_PEAK, 1000.0f, 0.707f, midDb);
    biquad_set_params(&eq->treble, BQ_HIGHSHELF, 5000.0f, 0.707f, trebleDb);
}

void fx_eqparametric_set_band(Effect* fx, int band, float freqHz, float Q, float gainDb) {
    if (fx->type != FX_EQ_PARAMETRIC || band < 0 || band > 3) return;
    EQParametricData* eq = (EQParametricData*)fx->data;
    eq->freqs[band] = freqHz;
    eq->Qs[band] = Q;
    eq->gains[band] = gainDb;
    biquad_set_params(&eq->filters[band], BQ_PEAK, freqHz, Q, gainDb);
}

void fx_preamp_set(Effect* fx, float inputDb, float driveDb, float bassDb, float midDb, float trebleDb, float outputDb, float sag, int tubeIdx) {
    if (fx->type != FX_PREAMP) return;
    PreampData* pre = (PreampData*)fx->data;
    pre->inputGain = db_to_linear(inputDb);
    pre->drive = db_to_linear(driveDb);
    pre->outputGain = db_to_linear(outputDb);
    pre->sagAmount = clampf(sag, 0.0f, 1.0f);
    if (tubeIdx >= 0 && tubeIdx < NUM_TUBE_PRESETS) {
        pre->tubeTableIdx = 24 + tubeIdx;
    }
    biquad_set_params(&pre->toneStack[0], BQ_LOWSHELF, 100.0f, 0.707f, bassDb);
    biquad_set_params(&pre->toneStack[1], BQ_PEAK, 800.0f, 0.707f, midDb);
    biquad_set_params(&pre->toneStack[2], BQ_HIGHSHELF, 3000.0f, 0.707f, trebleDb);
}

void fx_poweramp_set(Effect* fx, float driveDb, float outputDb, float sag, float sagTimeMs, int tubeIdx) {
    if (fx->type != FX_POWERAMP) return;
    PowerampData* pa = (PowerampData*)fx->data;
    pa->drive = db_to_linear(driveDb);
    pa->outputGain = db_to_linear(outputDb);
    pa->sagAmount = clampf(sag, 0.0f, 1.0f);
    pa->sagTime = clampf(sagTimeMs, 0.1f, 100.0f);
    if (tubeIdx >= 0 && tubeIdx < NUM_TUBE_PRESETS) {
        pa->tubeTableIdx = 24 + tubeIdx;
    }
}

void fx_cabinet_set(Effect* fx, int cabinetType) {
    if (fx->type != FX_CABINET) return;
    CabinetData* cab = (CabinetData*)fx->data;
    cab->cabinetType = cabinetType;
    
    switch (cabinetType) {
        case 0:
            biquad_set_params(&cab->lowResonance, BQ_PEAK, 120.0f, 0.5f, 8.0f);
            biquad_set_params(&cab->midPresence, BQ_PEAK, 3500.0f, 0.707f, 4.0f);
            biquad_set_params(&cab->highDamping, BQ_LPF, 3500.0f, 0.707f, 0.0f);
            break;
        case 1:
            biquad_set_params(&cab->lowResonance, BQ_PEAK, 100.0f, 0.5f, 5.0f);
            biquad_set_params(&cab->midPresence, BQ_PEAK, 4000.0f, 0.707f, 2.0f);
            biquad_set_params(&cab->highDamping, BQ_LPF, 4500.0f, 0.707f, 0.0f);
            break;
        case 2:
            biquad_set_params(&cab->lowResonance, BQ_PEAK, 200.0f, 0.5f, 10.0f);
            biquad_set_params(&cab->midPresence, BQ_PEAK, 2500.0f, 0.707f, 5.0f);
            biquad_set_params(&cab->highDamping, BQ_LPF, 3000.0f, 0.707f, 0.0f);
            break;
        default:
            break;
    }
}

// ============================================================================
// PRESET AMP CHAINS
// ============================================================================

void fx_chain_preset_clean(EffectChain* chain) {
    effect_chain_clear(chain);
    Effect* comp = effect_chain_add(chain, FX_COMPRESSOR);
    fx_compressor_set(comp, -15.0f, 3.0f, 3.0f, 6.0f, 5.0f, 50.0f);
    
    Effect* pre = effect_chain_add(chain, FX_PREAMP);
    fx_preamp_set(pre, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3);
    
    Effect* eq = effect_chain_add(chain, FX_EQ_3BAND);
    fx_eq3band_set(eq, 2.0f, 0.0f, 1.0f);
    
    Effect* chorus = effect_chain_add(chain, FX_CHORUS);
    fx_chorus_set(chorus, 1.0f, 3.0f, 0.3f);
    
    Effect* reverb = effect_chain_add(chain, FX_REVERB);
    fx_reverb_set(reverb, 0.3f, 5000.0f, 0.2f);
}

void fx_chain_preset_crunch(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* gate = effect_chain_add(chain, FX_NOISEGATE);
    fx_noisegate_set(gate, -45.0f, 1.0f, 100.0f, 50.0f);
    
    Effect* od = effect_chain_add(chain, FX_OVERDRIVE);
    fx_overdrive_set(od, 12.0f, 4000.0f, -3.0f);
    
    Effect* pre = effect_chain_add(chain, FX_PREAMP);
    fx_preamp_set(pre, 6.0f, 6.0f, 3.0f, 2.0f, 0.0f, 0.0f, 0.1f, 2);
    
    Effect* power = effect_chain_add(chain, FX_POWERAMP);
    fx_poweramp_set(power, 3.0f, 0.0f, 0.2f, 10.0f, 1);
    
    Effect* cab = effect_chain_add(chain, FX_CABINET);
    fx_cabinet_set(cab, 0);
    
    Effect* delay = effect_chain_add(chain, FX_DELAY);
    fx_delay_set(delay, 0.375f, 0.3f, 3000.0f, 0.25f);
}

void fx_chain_preset_lead(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* gate = effect_chain_add(chain, FX_NOISEGATE);
    fx_noisegate_set(gate, -40.0f, 0.5f, 80.0f, 30.0f);
    
    Effect* ts = effect_chain_add(chain, FX_TUBESCREAMER);
    fx_tubescreamer_set(ts, 9.0f, 0.6f, 0.0f);
    
    Effect* pre = effect_chain_add(chain, FX_PREAMP);
    fx_preamp_set(pre, 9.0f, 12.0f, 0.0f, 6.0f, 3.0f, 0.0f, 0.3f, 2);
    
    Effect* power = effect_chain_add(chain, FX_POWERAMP);
    fx_poweramp_set(power, 6.0f, 0.0f, 0.3f, 15.0f, 1);
    
    Effect* cab = effect_chain_add(chain, FX_CABINET);
    fx_cabinet_set(cab, 1);
    
    Effect* delay = effect_chain_add(chain, FX_DELAY);
    fx_delay_set(delay, 0.5f, 0.4f, 4000.0f, 0.3f);
    
    Effect* reverb = effect_chain_add(chain, FX_REVERB);
    fx_reverb_set(reverb, 0.4f, 6000.0f, 0.2f);
}

void fx_chain_preset_metal(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* gate = effect_chain_add(chain, FX_NOISEGATE);
    fx_noisegate_set(gate, -35.0f, 0.5f, 50.0f, 20.0f);
    
    Effect* dist = effect_chain_add(chain, FX_DISTORTION);
    fx_distortion_set(dist, 18.0f, -3.0f, 0.0f, -2.0f, -6.0f);
    
    Effect* pre = effect_chain_add(chain, FX_PREAMP);
    fx_preamp_set(pre, 12.0f, 9.0f, -3.0f, 3.0f, 0.0f, 0.0f, 0.2f, 2);
    
    Effect* power = effect_chain_add(chain, FX_POWERAMP);
    fx_poweramp_set(power, 9.0f, 0.0f, 0.4f, 20.0f, 4);
    
    Effect* cab = effect_chain_add(chain, FX_CABINET);
    fx_cabinet_set(cab, 2);
    
    Effect* eq = effect_chain_add(chain, FX_EQ_3BAND);
    fx_eq3band_set(eq, 3.0f, -3.0f, 0.0f);
}

void fx_chain_preset_fuzz(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* fuzz = effect_chain_add(chain, FX_FUZZ);
    fx_fuzz_set(fuzz, 24.0f, -9.0f);
    
    Effect* eq = effect_chain_add(chain, FX_EQ_3BAND);
    fx_eq3band_set(eq, 6.0f, 0.0f, -6.0f);
    
    Effect* cab = effect_chain_add(chain, FX_CABINET);
    fx_cabinet_set(cab, 1);
    
    Effect* trem = effect_chain_add(chain, FX_TREMOLO);
    fx_tremolo_set(trem, 4.0f, 0.5f);
}

void fx_chain_preset_ambient(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* comp = effect_chain_add(chain, FX_COMPRESSOR);
    fx_compressor_set(comp, -20.0f, 4.0f, 6.0f, 10.0f, 10.0f, 100.0f);
    
    Effect* chorus = effect_chain_add(chain, FX_CHORUS);
    fx_chorus_set(chorus, 0.5f, 5.0f, 0.5f);
    
    Effect* delay1 = effect_chain_add(chain, FX_DELAY);
    fx_delay_set(delay1, 0.375f, 0.5f, 4000.0f, 0.4f);
    
    Effect* delay2 = effect_chain_add(chain, FX_DELAY);
    fx_delay_set(delay2, 0.5f, 0.4f, 5000.0f, 0.3f);
    
    Effect* reverb = effect_chain_add(chain, FX_REVERB);
    fx_reverb_set(reverb, 0.7f, 6000.0f, 0.5f);
}

void fx_chain_preset_blues(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* od = effect_chain_add(chain, FX_OVERDRIVE);
    fx_overdrive_set(od, 9.0f, 5000.0f, -3.0f);
    
    Effect* pre = effect_chain_add(chain, FX_PREAMP);
    fx_preamp_set(pre, 6.0f, 3.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.2f, 2);
    
    Effect* power = effect_chain_add(chain, FX_POWERAMP);
    fx_poweramp_set(power, 6.0f, 0.0f, 0.3f, 15.0f, 1);
    
    Effect* cab = effect_chain_add(chain, FX_CABINET);
    fx_cabinet_set(cab, 0);
    
    Effect* trem = effect_chain_add(chain, FX_TREMOLO);
    fx_tremolo_set(trem, 5.0f, 0.3f);
    
    Effect* reverb = effect_chain_add(chain, FX_REVERB);
    fx_reverb_set(reverb, 0.4f, 5000.0f, 0.3f);
}

void fx_chain_preset_shoegaze(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* fuzz = effect_chain_add(chain, FX_FUZZ);
    fx_fuzz_set(fuzz, 18.0f, -6.0f);
    
    Effect* chorus = effect_chain_add(chain, FX_CHORUS);
    fx_chorus_set(chorus, 0.3f, 8.0f, 0.6f);
    
    Effect* flanger = effect_chain_add(chain, FX_FLANGER);
    fx_flanger_set(flanger, 0.2f, 3.0f, 0.7f, 0.4f);
    
    Effect* vibrato = effect_chain_add(chain, FX_VIBRATO);
    fx_vibrato_set(vibrato, 6.0f, 2.0f);
    
    Effect* delay = effect_chain_add(chain, FX_DELAY);
    fx_delay_set(delay, 0.5f, 0.6f, 3000.0f, 0.5f);
    
    Effect* reverb = effect_chain_add(chain, FX_REVERB);
    fx_reverb_set(reverb, 0.8f, 7000.0f, 0.6f);
}

void fx_chain_preset_funk(EffectChain* chain) {
    effect_chain_clear(chain);
    
    Effect* comp = effect_chain_add(chain, FX_COMPRESSOR);
    fx_compressor_set(comp, -18.0f, 6.0f, 6.0f, 8.0f, 3.0f, 40.0f);
    
    Effect* wah = effect_chain_add(chain, FX_WAH);
    fx_wah_set(wah, 1000.0f, 10.0f, 1.5f);
    
    Effect* phaser = effect_chain_add(chain, FX_PHASER);
    fx_phaser_set(phaser, 0.5f, 0.7f, 0.6f, 0.5f);
    
    Effect* eq = effect_chain_add(chain, FX_EQ_3BAND);
    fx_eq3band_set(eq, 3.0f, -2.0f, 2.0f);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* effect_type_name(EffectType type) {
    switch (type) {
        case FX_NOISEGATE: return "Noise Gate";
        case FX_COMPRESSOR: return "Compressor";
        case FX_OVERDRIVE: return "Overdrive";
        case FX_DISTORTION: return "Distortion";
        case FX_FUZZ: return "Fuzz";
        case FX_BOOST: return "Boost";
        case FX_TUBESCREAMER: return "Tube Screamer";
        case FX_CHORUS: return "Chorus";
        case FX_FLANGER: return "Flanger";
        case FX_PHASER: return "Phaser";
        case FX_TREMOLO: return "Tremolo";
        case FX_VIBRATO: return "Vibrato";
        case FX_DELAY: return "Delay";
        case FX_REVERB: return "Reverb";
        case FX_WAH: return "Auto-Wah";
        case FX_EQ_3BAND: return "3-Band EQ";
        case FX_EQ_PARAMETRIC: return "Parametric EQ";
        case FX_PREAMP: return "Tube Preamp";
        case FX_POWERAMP: return "Power Amp";
        case FX_CABINET: return "Cabinet Simulator";
        default: return "Unknown";
    }
}

int effect_chain_count(EffectChain* chain) {
    int count = 0;
    Effect* curr = chain->head;
    while (curr) {
        count++;
        curr = curr->next;
    }
    return count;
}

Effect* effect_chain_get_at(EffectChain* chain, int index) {
    if (index < 0) return NULL;
    Effect* curr = chain->head;
    int i = 0;
    while (curr && i < index) {
        curr = curr->next;
        i++;
    }
    return curr;
}

void effect_enable(Effect* fx, bool enabled) {
    if (fx) fx->enabled = enabled;
}

void effect_bypass(Effect* fx, bool bypass) {
    if (fx) fx->bypass = bypass;
}

bool effect_is_enabled(Effect* fx) {
    return fx ? fx->enabled : false;
}

bool effect_is_bypassed(Effect* fx) {
    return fx ? fx->bypass : false;
}

void effect_chain_enable_all(EffectChain* chain, bool enabled) {
    Effect* curr = chain->head;
    while (curr) {
        curr->enabled = enabled;
        curr = curr->next;
    }
}

void effect_chain_bypass_all(EffectChain* chain, bool bypass) {
    Effect* curr = chain->head;
    while (curr) {
        curr->bypass = bypass;
        curr = curr->next;
    }
}

// ============================================================================
// SCRATCH BUFFER ALLOCATION GUIDE
// ============================================================================
// Scratch buffer usage:
// [0-7]   - Reserved for DSP core operations
// [8-19]  - Effect processing buffers (shared, reused per effect)
// [20-23] - Waveshaper tables (4 different clipper types)
// [24-29] - Tube tables (6 tube presets)
// [30-31] - Effect chain double buffering
// ============================================================================