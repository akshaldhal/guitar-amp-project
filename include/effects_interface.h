#ifndef EFFECTS_HANDLER_H
#define EFFECTS_HANDLER_H

#include <effects_dsp.h>
#include <math.h>
#include <logger.h>

// for handling all sorts of audio effects and related functions, assumes that audio buffers will be passed in as float arrays

typedef enum {
  AMP_CHANNEL_CLEAN,         // Clean channels: Very low gain, high headroom, wide bandwidth
  AMP_CHANNEL_FAT_CLEAN,     // Clean channels: More low mids, slightly earlier breakup

  AMP_CHANNEL_CRUNCH,        // Edge of breakup / light crunch: Classic crunch, warm mids, soft clipping
  AMP_CHANNEL_PLEXI,         // Edge of breakup / light crunch: British crunch, bright top, dynamic response

  AMP_CHANNEL_LEAD,          // Lead channels: Saturated mids, smooth sustain, tight bass
  AMP_CHANNEL_HOT_ROD_LEAD,  // Lead channels: Hot-rodded Marshall / Soldano style lead

  AMP_CHANNEL_HIGH_GAIN,     // High gain: Modern high gain, tight lows, scooped mids
  AMP_CHANNEL_METAL,         // High gain: Heavy saturation, extended lows, aggressive mids
  AMP_CHANNEL_DJENT,         // High gain: Ultra-tight, low-end filtered, percussive gating

  AMP_CHANNEL_BASS_CLEAN,    // Bass amps: Wide bandwidth, very high headroom
  AMP_CHANNEL_BASS_DRIVE     // Bass amps: Tube-like saturation, thick low mids
} AmpChannelType;


typedef enum {
  TUBE_TYPE_12AX7,   // Preamp tubes: High-gain preamp, strong saturation, bright mids
  TUBE_TYPE_7025,    // Preamp tubes: Low-noise 12AX7 variant, cleaner, smoother
  TUBE_TYPE_12AT7,   // Preamp tubes: Lower gain, more headroom, softer clipping

  TUBE_TYPE_EL84,    // Power tubes (British): Vox-style chime, early breakup, bright upper mids
  TUBE_TYPE_EL34,    // Power tubes (British): Marshall-style crunch, mid-forward, aggressive

  TUBE_TYPE_6V6,     // Power tubes (American): Small-amp vintage compression, warm, early breakup
  TUBE_TYPE_6L6,     // Power tubes (American): Big clean headroom, punchy low end, smooth highs

  TUBE_TYPE_KT88,    // High-power tubes: Modern metal/bass, huge headroom, tight low end
  TUBE_TYPE_6550     // High-power tubes: Bass/fat clean, strong lows, clean until very loud
} TubeType;


typedef enum {
  MIC_TYPE_DYNAMIC_SM57,     // Dynamic mic: Standard guitar cab mic: mid bump, tight lows
  MIC_TYPE_DYNAMIC_MD421,    // Dynamic mic: Fuller low mids, smoother top end

  MIC_TYPE_CONDENSER_U87,    // Condenser mic: Studio condenser, wide bandwidth, balanced
  MIC_TYPE_CONDENSER_C414,   // Condenser mic: Bright condenser, crisp highs

  MIC_TYPE_RIBBON_R121,      // Ribbon mic: Dark, smooth, classic cab IR pairing
  MIC_TYPE_RIBBON_R122       // Ribbon mic: Slightly brighter active ribbon
} MicType;

typedef enum {
  MIC_POSITION_ON_AXIS,      // Mic pointed directly at center of speaker cone
  MIC_POSITION_OFF_AXIS_45,  // Mic angled 45 degrees off center
  MIC_POSITION_OFF_AXIS_90   // Mic angled 90 degrees off center
} MicPosition;

typedef enum {
  WAVEFORM_SINE,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_SQUARE,
  WAVEFORM_RANDOM
} WaveformType;


/**
 * Apply noise gate effect to audio buffer
 * @param threshold Threshold level in dB
 * @param attackTime Attack time in milliseconds
 * @param releaseTime Release time in milliseconds
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_noise_gate(float threshold, float attackTime, float releaseTime, float* buffer, int bufferSize);

/**
 * Apply overdrive effect to audio buffer
 * @param gain Gain level
 * @param tone Tone control
 * @param level Output level
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_overdrive(float gain, float tone, float level, float* buffer, int bufferSize);

/**
 * Apply distortion effect to audio buffer
 * @param gain Gain level
 * @param tone Tone control
 * @param level Output level
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_distortion(float gain, float tone, float level, float* buffer, int bufferSize);

/**
 * Apply fuzz effect to audio buffer
 * @param gain Gain level
 * @param tone Tone control
 * @param bias Bias control
 * @param gate Gate threshold
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_fuzz(float gain, float tone, float bias, float gate, float* buffer, int bufferSize);

/**
 * Apply 3-band EQ effect to audio buffer
 * @param bass Gain for bass band in dB
 * @param mid Gain for mid band in dB
 * @param treble Gain for treble band in dB
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_3band_eq(float bass, float mid, float treble, float* buffer, int bufferSize);

/**
 * Apply High/Low-pass filter to audio buffer
 * @param cutoffFreq Cutoff frequency in Hz
 * @param resonance Resonance/Q factor
 * @param isHighPass 1 for high-pass, 0 for low-pass
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_high_low_pass_filter(float cutoffFreq, float resonance, int isHighPass, float* buffer, int bufferSize);

/**
 * Apply compressor effect to audio buffer
 * @param threshold Threshold level in dB
 * @param ratio Compression ratio
 * @param attackTime Attack time in milliseconds
 * @param releaseTime Release time in milliseconds
 * @param makeupGain Make-up gain in dB
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_compressor(float threshold, float ratio, float attackTime, float releaseTime, float makeupGain, float* buffer, int bufferSize);

/**
 * Apply reverb effect to audio buffer
 * @param roomSize Size of the virtual room
 * @param damping Damping factor
 * @param preDelay Pre-delay time in milliseconds
 * @param mix Wet/Dry mix percentage
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_reverb(float roomSize, float damping, float preDelay, float mix, float* buffer, int bufferSize);

/**
 * Apply delay effect to audio buffer
 * @param time Delay time in milliseconds
 * @param feedback Feedback amount percentage
 * @param mix Wet/Dry mix percentage
 * @param lowpassCutoff Low-pass filter cutoff frequency in Hz
 * @param wowFlutter Amount of wow/flutter effect
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_delay(float time, float feedback, float mix, float lowpassCutoff, float wowFlutter, float* buffer, int bufferSize);

/**
 * Apply preamp simulation effect to audio buffer
 * @param gain Gain level
 * @param bass Bass control
 * @param mid Mid control
 * @param treble Treble control
 * @param presence Presence control
 * @param channelType Type of amplifier channel
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_preamp_simulation(float gain, float bass, float mid, float treble, float presence, AmpChannelType channelType, float* buffer, int bufferSize);

/**
 * Apply power amp simulation effect to audio buffer
 * @param masterVolume Master volume level
 * @param sag Sag amount
 * @param presence Presence control
 * @param depth Depth control
 * @param tubeType Type of tubes used
 * @param bias Bias level
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_power_amp_simulation(float masterVolume, float sag, float presence, float depth, TubeType tubeType, float bias, float* buffer, int bufferSize);

/**
 * Apply cabinet simulation effect to audio buffer
 * @param micType Type of microphone used
 * @param micPosition Position of the microphone
 * @param distance Distance from the speaker
 * @param roomAmount Amount of room ambience
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_cabinet_simulation(MicType micType, MicPosition micPosition, float distance, float roomAmount, float* buffer, int bufferSize);

/**
 * Apply chorus effect to audio buffer
 * @param rate Rate of modulation in Hz
 * @param depth Depth of modulation
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_chorus(float rate, float depth, float mix, float* buffer, int bufferSize);

/**
 * Apply flanger effect to audio buffer
 * @param rate Rate of modulation in Hz
 * @param depth Depth of modulation
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param feedback Feedback amount percentage, represented as 0.0 to 1.0
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_flanger(float rate, float depth, float mix, float feedback, float* buffer, int bufferSize);

/**
 * Apply phaser effect to audio buffer
 * @param rate Rate of modulation in Hz
 * @param depth Depth of modulation
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param stages Number of all-pass filter stages
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_phaser(float rate, float depth, float mix, int stages, float* buffer, int bufferSize);

/**
 * Apply tremolo effect to audio buffer
 * @param rate Rate of modulation in Hz
 * @param depth Depth of modulation
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param waveform Waveform type
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_tremolo(float rate, float depth, float mix, WaveformType waveform, float* buffer, int bufferSize);

/**
 * Apply pitch shifter
 * @param interval Pitch shift interval in semitones
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param formant Formant preservation amount ranging from 0.0 (none) to 1.0 (full)
 * @param quality Quality setting (0 = low, 1 = high)
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_pitch_shifter(float interval, float mix, float formant, int quality, float* buffer, int bufferSize);

/**
 * Apply looper effect to audio buffer
 * @param loopLength Length of the loop in milliseconds
 * @param feedback Feedback amount percentage, represented as 0.0 to 1.0
 * @param overdubLevel Overdub level percentage, represented as 0.0 to 1.0
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_looper(float loopLength, float feedback, float overdubLevel, float* buffer, int bufferSize);

/**
 * Apply clipper effect to audio buffer
 * @param threshold Clipping threshold level in dB
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_clipper(float threshold, float* buffer, int bufferSize);

/**
 * Apply limiter effect to audio buffer
 * @param threshold Limiting threshold level in dB
 * @param ratio Ratio of compression, typically very high (e.g., 10:1 or greater), input as a float (e.g., 10.0 for 10:1)
 * @param releaseTime Release time in milliseconds
 * @param attackTime Attack time in milliseconds
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_limiter(float threshold, float ratio, float attackTime, float releaseTime, float* buffer, int bufferSize);

/**
 * Apply spectral enhancer effect to audio buffer using FFT
 * @param amount Amount of enhancement
 * @param harmonics Harmonics control
 * @param tilt Tilt control
 * @param mix Wet/Dry mix percentage, represented as 0.0 to 1.0 with 1.0 being fully wet
 * @param buffer Audio buffer to process
 * @param bufferSize Size of the audio buffer
 */
void apply_spectral_enhancer(float amount, float harmonics, float tilt, float mix, float* buffer, int bufferSize);

#endif