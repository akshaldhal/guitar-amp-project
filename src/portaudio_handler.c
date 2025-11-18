#include <portaudio_handler.h>

PaError initialize_portaudio(void) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "PortAudio initialized successfully.");
  }
  return err;
}

PaError terminate_portaudio(void) {
  PaError err = Pa_Terminate();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to terminate PortAudio: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "PortAudio terminated successfully.");
  }
  return err;
}

int get_number_of_devices(void) {
  int numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    log_message(LOG_LEVEL_ERROR, "ERROR: Pa_GetDeviceCount returned 0x%x", numDevices);
    return -1;
  }
  return numDevices;
}

const PaDeviceInfo* get_device_info(int deviceIndex) {
  const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo((PaDeviceIndex)deviceIndex);
  if (deviceInfo == NULL) {
    log_message(LOG_LEVEL_WARN, "No device info found for device index %d", deviceIndex);
  }
  return deviceInfo;
}

PaError open_audio_stream(PaStream** stream, const AudioStreamConfig* config) {
  PaError err;
  PaStreamParameters inputParams;
  PaStreamParameters outputParams;

  inputParams.device = config->inputDevice;
  inputParams.channelCount = config->inputChannels;
  inputParams.sampleFormat = paFloat32;
  inputParams.suggestedLatency = Pa_GetDeviceInfo(config->inputDevice)->defaultLowInputLatency;
  inputParams.hostApiSpecificStreamInfo = NULL;

  outputParams.device = config->outputDevice;
  outputParams.channelCount = config->outputChannels;
  outputParams.sampleFormat = paFloat32;
  outputParams.suggestedLatency = Pa_GetDeviceInfo(config->outputDevice)->defaultLowOutputLatency;
  outputParams.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(
    stream,
    &inputParams,
    &outputParams,
    config->sampleRate,
    config->framesPerBuffer,
    paNoFlag,
    config->streamCallback,
    config->userData
  );

  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to open audio stream: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "Audio stream opened successfully.");
  }

  return err;
}

PaError start_audio_stream(PaStream* stream) {
  PaError err = Pa_StartStream(stream);
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to start audio stream: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "Audio stream started successfully.");
  }
  return err;
}

PaError stop_audio_stream(PaStream* stream) {
  PaError err = Pa_StopStream(stream);
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to stop audio stream: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "Audio stream stopped successfully.");
  }
  return err;
}

PaError close_audio_stream(PaStream* stream) {
  PaError err = Pa_CloseStream(stream);
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to close audio stream: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "Audio stream closed successfully.");
  }
  return err;
}

SoundEffectChain* create_sound_effect_chain(void) {
  SoundEffectChain* chain = malloc(sizeof(SoundEffectChain));
  if (chain == NULL) {
    log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for SoundEffectChain");
    return NULL;
  }
  chain->head = NULL;
  chain->modifierCount = 0;
  chain->flattenedArray = NULL;
  log_message(LOG_LEVEL_DEBUG, "Created new sound effect chain");
  return chain;
}

SoundModifier* create_simple_modifier(float gain, float bass, float mid, float treble) {
  SoundModifier* modifier = malloc(sizeof(SoundModifier));
  if (modifier == NULL) {
    log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for SoundModifier");
    return NULL;
  }
  
  modifier->type = MODIFIER_SIMPLE;
  modifier->next = NULL;
  modifier->data.simple.gain = gain;
  modifier->data.simple.bass = bass;
  modifier->data.simple.mid = mid;
  modifier->data.simple.treble = treble;
  
  log_message(LOG_LEVEL_DEBUG, "Created simple modifier (gain: %.2f)", gain);
  return modifier;
}

SoundModifier* create_advanced_modifier(float threshold, float ratio, float attackTime, 
                                        float releaseTime, float gain) {
  SoundModifier* modifier = malloc(sizeof(SoundModifier));
  if (modifier == NULL) {
    log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for SoundModifier");
    return NULL;
  }
  
  modifier->type = MODIFIER_ADVANCED;
  modifier->next = NULL;
  modifier->data.advanced.threshold = threshold;
  modifier->data.advanced.ratio = ratio;
  modifier->data.advanced.attackTime = attackTime;
  modifier->data.advanced.releaseTime = releaseTime;
  modifier->data.advanced.gain = gain;
  
  log_message(LOG_LEVEL_DEBUG, "Created advanced modifier (threshold: %.2f, ratio: %.2f)", 
              threshold, ratio);
  return modifier;
}

int add_modifier_to_chain(SoundEffectChain* chain, SoundModifier* modifier) {
  if (chain == NULL || modifier == NULL) {
    log_message(LOG_LEVEL_ERROR, "Cannot add modifier: NULL chain or modifier");
    return -1;
  }

  if (modifier->next != NULL) {
    log_message(LOG_LEVEL_WARN, "Modifier already linked; This is not allowed, resetting next pointer");
    modifier->next = NULL;
  }

  if (chain->head == NULL) {
    chain->head = modifier;
  } else {
    SoundModifier* current = chain->head;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = modifier;
  }
  
  chain->modifierCount++;
  log_message(LOG_LEVEL_DEBUG, "Added modifier to chain (total: %d)", chain->modifierCount);
  return 0;
}

int remove_modifier_from_chain(SoundEffectChain* chain, SoundModifier* modifier) {
  if (chain == NULL || modifier == NULL || chain->head == NULL) {
    log_message(LOG_LEVEL_ERROR, "Cannot remove modifier: invalid parameters");
    return -1;
  }

  if (chain->head == modifier) {
    chain->head = modifier->next;
    free(modifier);
    chain->modifierCount--;
    log_message(LOG_LEVEL_DEBUG, "Removed head modifier from chain");
    return 0;
  }

  SoundModifier* current = chain->head;
  while (current->next != NULL && current->next != modifier) {
    current = current->next;
  }

  if (current->next == modifier) {
    current->next = modifier->next;
    free(modifier);
    chain->modifierCount--;
    log_message(LOG_LEVEL_DEBUG, "Removed modifier from chain (remaining: %d)", 
                chain->modifierCount);
    return 0;
  }

  log_message(LOG_LEVEL_WARN, "Modifier not found in chain");
  return -1;
}

void clear_sound_effect_chain(SoundEffectChain* chain) {
  if (chain == NULL) {
    return;
  }

  SoundModifier* current = chain->head;
  while (current != NULL) {
    SoundModifier* next = current->next;
    free(current);
    current = next;
  }

  chain->head = NULL;
  chain->modifierCount = 0;
  log_message(LOG_LEVEL_INFO, "Cleared all modifiers from chain");
}

void destroy_sound_effect_chain(SoundEffectChain* chain) {
  if (chain == NULL) {
    return;
  }
  clear_sound_effect_chain(chain);
  free(chain);
  log_message(LOG_LEVEL_DEBUG, "Destroyed sound effect chain");
}

static inline float db_to_linear(float db) {
  return powf(10.0f, db / 20.0f);
}

static void apply_simple_modifier(const SimpleSoundModifier* mod, AudioBuffer* buffer) {
  if (mod == NULL || buffer == NULL || buffer->data == NULL) {
    return;
  }

  float gain_linear = db_to_linear(mod->gain);
  
  // Apply gain to all samples
  for (unsigned long i = 0; i < buffer->frameCount * buffer->channelCount; i++) {
    buffer->data[i] *= gain_linear;
  }

  // Note: Bass, mid, treble would require proper EQ filters (biquad filters)
  // This is a simplified version - full implementation would use proper DSP filters
  log_message(LOG_LEVEL_TRACE, "Applied simple modifier (gain: %.2f dB)", mod->gain);
}

static void apply_advanced_modifier(const AdvancedSoundModifier* mod, AudioBuffer* buffer) {
  if (mod == NULL || buffer == NULL || buffer->data == NULL) {
    return;
  }

  float threshold_linear = db_to_linear(mod->threshold);
  float makeup_gain_linear = db_to_linear(mod->gain);
  
  // Simple compressor/gate implementation
  for (unsigned long i = 0; i < buffer->frameCount * buffer->channelCount; i++) {
    float sample = buffer->data[i];
    float sample_abs = fabsf(sample);
    
    if (sample_abs > threshold_linear) {
      // Compression
      float over_threshold = sample_abs - threshold_linear;
      float compressed = threshold_linear + (over_threshold / mod->ratio);
      float gain_reduction = compressed / sample_abs;
      buffer->data[i] = sample * gain_reduction * makeup_gain_linear;
    } else {
      // Gate (simple version - could be improved with attack/release)
      float gate_amount = sample_abs / threshold_linear;
      buffer->data[i] = sample * gate_amount * makeup_gain_linear;
    }
  }

  // Note: Proper implementation would include envelope followers for attack/release
  log_message(LOG_LEVEL_TRACE, "Applied advanced modifier (threshold: %.2f dB)", 
              mod->threshold);
}

void apply_effect_chain(const SoundEffectChain* chain, AudioBuffer* buffer) {
  if (chain == NULL || buffer == NULL || buffer->data == NULL) {
    log_message(LOG_LEVEL_ERROR, "Cannot apply effects: invalid parameters");
    return;
  }

  if (chain->head == NULL) {
    // No effects to apply, pass through
    return;
  }

  SoundModifier* current = chain->head;
  int effects_applied = 0;

  while (current != NULL) {
    switch (current->type) {
      case MODIFIER_SIMPLE:
        apply_simple_modifier(&current->data.simple, buffer);
        effects_applied++;
        break;
      
      case MODIFIER_ADVANCED:
        apply_advanced_modifier(&current->data.advanced, buffer);
        effects_applied++;
        break;
      
      default:
        log_message(LOG_LEVEL_WARN, "Unknown modifier type: %d", current->type);
        break;
    }
    
    current = current->next;
  }

  log_message(LOG_LEVEL_TRACE, "Applied %d effects to %lu frames", 
              effects_applied, buffer->frameCount);
}