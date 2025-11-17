#ifndef PORTAUDIO_HANDLER_H
#define PORTAUDIO_HANDLER_H

#include <portaudio.h>

typedef union SoundModifier SoundModifier;

/* Simple sound modifier with basic EQ controls */
typedef struct SimpleSoundModifier {
  float gain;
  float bass;
  float mid;
  float treble;
  SoundModifier* next;
} SimpleSoundModifier;

/* Advanced sound modifier for gates, compressors, and dynamic effects */
typedef struct AdvancedSoundModifier {
  float threshold;    /* Threshold level in dB */
  float ratio;      /* Compression ratio */
  float attackTime;   /* Attack time in milliseconds */
  float releaseTime;  /* Release time in milliseconds */
  float gain;       /* Make-up gain in dB */
  SoundModifier* next;
} AdvancedSoundModifier;

union SoundModifier {
  AdvancedSoundModifier advanced;
  SimpleSoundModifier simple;
};

typedef struct SoundEffectChain {
  SoundModifier* head;
  int modifierCount;
} SoundEffectChain;

/* Audio buffer structure for processing */
typedef struct AudioBuffer {
  float* data;
  unsigned long frameCount;
  int channelCount;
} AudioBuffer;

/* Configuration for audio stream setup */
typedef struct AudioStreamConfig {
  PaDeviceIndex inputDevice;
  PaDeviceIndex outputDevice;
  double sampleRate;
  unsigned long framesPerBuffer;
  int inputChannels;
  int outputChannels;
  PaStreamCallback* streamCallback;
  void* userData;
} AudioStreamConfig;

/**
 * Initialize the PortAudio library
 * @return PaError code indicating success or failure
 */
PaError initialize_portaudio(void);

/**
 * Terminate the PortAudio library and clean up resources
 * @return PaError code indicating success or failure
 */
PaError terminate_portaudio(void);

/**
 * Get the total number of available audio devices
 * @return Number of devices, or negative error code on failure
 */
int get_number_of_devices(void);

/**
 * Get information about a specific audio device
 * @param deviceIndex Index of the device to query
 * @return Pointer to device info structure, or NULL on error
 */
const PaDeviceInfo* get_device_info(int deviceIndex);

/**
 * Open an audio stream with the specified configuration
 * @param stream Pointer to stream handle to be initialized
 * @param config Configuration parameters for the stream
 * @return PaError code indicating success or failure
 */
PaError open_audio_stream(PaStream** stream, const AudioStreamConfig* config);

/**
 * Start processing audio on an opened stream
 * @param stream Stream to start
 * @return PaError code indicating success or failure
 */
PaError start_audio_stream(PaStream* stream);

/**
 * Stop processing audio on a running stream
 * @param stream Stream to stop
 * @return PaError code indicating success or failure
 */
PaError stop_audio_stream(PaStream* stream);

/**
 * Close an audio stream and release resources
 * @param stream Stream to close
 * @return PaError code indicating success or failure
 */
PaError close_audio_stream(PaStream* stream);

/**
 * Initialize a sound effect chain
 * @param chain Pointer to chain structure to initialize
 */
void init_sound_effect_chain(SoundEffectChain* chain);

/**
 * Add a modifier to the effect chain
 * @param chain Target chain
 * @param modifier Modifier to add
 * @return 0 on success, -1 on failure
 */
int add_modifier_to_chain(SoundEffectChain* chain, SoundModifier* modifier);

/**
 * Remove a modifier from the effect chain
 * @param chain Target chain
 * @param modifier Modifier to remove
 * @return 0 on success, -1 if not found
 */
int remove_modifier_from_chain(SoundEffectChain* chain, SoundModifier* modifier);

/**
 * Clear all modifiers from the effect chain
 * @param chain Chain to clear
 */
void clear_sound_effect_chain(SoundEffectChain* chain);

/**
 * Apply the effect chain to an audio buffer
 * @param chain Effect chain to apply
 * @param buffer Audio buffer to process
 */
void apply_effect_chain(const SoundEffectChain* chain, AudioBuffer* buffer);

#endif