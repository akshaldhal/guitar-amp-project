#ifndef PORTAUDIO_HANDLER_H
#define PORTAUDIO_HANDLER_H

#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <logger.h>
#include <string.h>

/* Forward declaration */
typedef struct SoundModifier SoundModifier;

/* Enumeration for modifier types */
typedef enum ModifierType {
  MODIFIER_SIMPLE,
  MODIFIER_ADVANCED
} ModifierType;

/* Simple sound modifier with basic EQ controls */
typedef struct SimpleSoundModifier {
  float gain;
  float bass;
  float mid;
  float treble;
} SimpleSoundModifier;

/* Advanced sound modifier for gates, compressors, and dynamic effects */
typedef struct AdvancedSoundModifier {
  float threshold;    /* Threshold level in dB */
  float ratio;      /* Compression ratio */
  float attackTime;   /* Attack time in milliseconds */
  float releaseTime;  /* Release time in milliseconds */
  float gain;       /* Make-up gain in dB */
} AdvancedSoundModifier;

/* Sound modifier with type tag and linked list support */
struct SoundModifier {
  ModifierType type;
  union {
    SimpleSoundModifier simple;
    AdvancedSoundModifier advanced;
  } data;
  SoundModifier* next;
};

/* Chain of sound effects to be applied in sequence */
typedef struct SoundEffectChain {
  SoundModifier* head;
  int modifierCount;  /* Track number of modifiers in chain */
  SoundModifier** flattenedArray; /* Optional: flattened array for efficient processing */
} SoundEffectChain;

/* Audio buffer structure for processing */
typedef struct AudioBuffer {
  float* data;
  unsigned long frameCount;
  int channelCount;   /* Number of audio channels */
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
 * Create a new sound effect chain (allocates memory)
 * @return Pointer to new chain, or NULL on failure
 */
SoundEffectChain* create_sound_effect_chain(void);

/**
 * Create a simple sound modifier
 * @return Pointer to new modifier, or NULL on failure
 */
SoundModifier* create_simple_modifier(float gain, float bass, float mid, float treble);

/**
 * Create an advanced sound modifier
 * @return Pointer to new modifier, or NULL on failure
 */
SoundModifier* create_advanced_modifier(float threshold, float ratio, float attackTime, 
                    float releaseTime, float gain);

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
 * Destroy the sound effect chain and free memory
 * @param chain Chain to destroy
 */
void destroy_sound_effect_chain(SoundEffectChain* chain);

/**
 * Apply the effect chain to an audio buffer
 * @param chain Effect chain to apply
 * @param buffer Audio buffer to process
 */
void apply_effect_chain(const SoundEffectChain* chain, AudioBuffer* buffer);

#endif