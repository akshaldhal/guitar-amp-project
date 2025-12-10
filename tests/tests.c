#include <logger.h>
#include <portaudio.h>
#include <effects_dsp.h>
#include <stdlib.h>

void test_log_message() {
  char * message = "Test log message";
  log_message(LOG_LEVEL_INFO, "%s", message);
  log_message(LOG_LEVEL_ERROR, "An error occurred: %d", -1);
  log_message(LOG_LEVEL_DEBUG, "Debugging value: %f", 3.14);
  log_message(LOG_LEVEL_WARN, "This is a warning");
  log_message(LOG_LEVEL_TRACE, "Trace message for detailed debugging");
}

void port_audio_stream_test() {
  // THIS USES PORTAUDIO DIRECTLY, IT SHOULD BE REPLACED WITH TESTS FOR THE PORTAUDIO HANDLER WRAPPER!!!
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "ERROR: Pa_Initialize returned 0x%x", err);
    return;
  }
  PaDeviceIndex numberOfDevices = Pa_GetDeviceCount();
  if (numberOfDevices < 0) {
    log_message(LOG_LEVEL_ERROR, "ERROR: Pa_GetDeviceCount returned 0x%x", numberOfDevices);
    return;
  }
  log_message(LOG_LEVEL_INFO, "Number of audio devices: %d", numberOfDevices);
  log_message(LOG_LEVEL_INFO, "Using device with most input and device with most ouput channels:");
  
  PaDeviceIndex maxInputDevice = -1;
  int maxInputChannels = 0;
  PaDeviceIndex maxOutputDevice = -1;
  int maxOutputChannels = 0;
  for (PaDeviceIndex i = 0; i < numberOfDevices; i++) {
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
    if (deviceInfo->maxInputChannels > maxInputChannels) {
      maxInputChannels = deviceInfo->maxInputChannels;
      maxInputDevice = i;
    }
    if (deviceInfo->maxOutputChannels > maxOutputChannels) {
      maxOutputChannels = deviceInfo->maxOutputChannels;
      maxOutputDevice = i;
    }
  }
  log_message(LOG_LEVEL_INFO, "Max Input Device: %d with %d channels", maxInputDevice, maxInputChannels);
  log_message(LOG_LEVEL_INFO, "Max Output Device: %d with %d channels", maxOutputDevice, maxOutputChannels);

  // capture playback and print captured samples, then stream captured playback to output
  if (maxInputDevice < 0 || maxOutputDevice < 0) {
    log_message(LOG_LEVEL_ERROR, "No valid input/output device found.");
    return;
  }

  PaStreamParameters inputParams;
  PaStreamParameters outputParams;
  PaStream* inputStream = NULL;
  PaStream* outputStream = NULL;

  inputParams.device                    = maxInputDevice;
  inputParams.channelCount              = maxInputChannels;
  inputParams.sampleFormat              = paFloat32;
  inputParams.suggestedLatency          = Pa_GetDeviceInfo(maxInputDevice)->defaultLowInputLatency;
  inputParams.hostApiSpecificStreamInfo = NULL;

  outputParams.device                    = maxOutputDevice;
  outputParams.channelCount              = maxOutputChannels;
  outputParams.sampleFormat              = paFloat32;
  outputParams.suggestedLatency          = Pa_GetDeviceInfo(maxOutputDevice)->defaultLowOutputLatency;
  outputParams.hostApiSpecificStreamInfo = NULL;

  const double sampleRate      = 44100.0;
  const unsigned long frames   = 512;
  const int channelsToCapture  = (maxInputChannels < maxOutputChannels) ? maxInputChannels : maxOutputChannels;

  float *buffer = (float*)calloc(frames * channelsToCapture, sizeof(float));
  if (!buffer) {
    log_message(LOG_LEVEL_ERROR, "Failed to allocate buffer.");
    return;
  }

  log_message(LOG_LEVEL_INFO,
    "Opening input stream (%d channels) and output stream (%d channels)...",
    channelsToCapture, maxOutputChannels);

  // Open input stream
  err = Pa_OpenStream(
    &inputStream,
    &inputParams,
    NULL,
    sampleRate,
    frames,
    paClipOff,
    NULL,
    NULL
  );

  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to open input stream: %s", Pa_GetErrorText(err));
    free(buffer);
    return;
  }

  // Open output stream
  err = Pa_OpenStream(
    &outputStream,
    NULL,
    &outputParams,
    sampleRate,
    frames,
    paClipOff,
    NULL,
    NULL
  );

  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to open output stream: %s", Pa_GetErrorText(err));
    Pa_CloseStream(inputStream);
    free(buffer);
    return;
  }

  // Start both streams
  Pa_StartStream(inputStream);
  Pa_StartStream(outputStream);

  log_message(LOG_LEVEL_INFO, "Capturing %lu frames ...", frames);

  err = Pa_ReadStream(inputStream, buffer, frames);
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to read from input stream: %s", Pa_GetErrorText(err));
  } else {
      // Print first 10 samples
      log_message(LOG_LEVEL_INFO, "First few captured samples:");
      for (unsigned long i = 0; i < 10 && i < frames * channelsToCapture; i++) {
        log_message(LOG_LEVEL_DEBUG, "Sample[%d] = %f", i, buffer[i]);
      }

      log_message(LOG_LEVEL_INFO, "Playing captured audio...");
      err = Pa_WriteStream(outputStream, buffer, frames);
      if (err != paNoError) {
        log_message(LOG_LEVEL_ERROR, "Failed to write to output stream: %s", Pa_GetErrorText(err));
      }
  }

  Pa_StopStream(inputStream);
  Pa_StopStream(outputStream);

  Pa_CloseStream(inputStream);
  Pa_CloseStream(outputStream);

  free(buffer);

  log_message(LOG_LEVEL_INFO, "Audio capture/playback test complete.");

  


  err = Pa_Terminate();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "ERROR: Pa_Terminate returned 0x%x", err);
  }
}

int get_simd_width() {
  return SIMD_WIDTH;
}

int main() {
  test_log_message();
  // port_audio_stream_test();
  // printf("SIMD width: %d\n", get_simd_width());
  return 0;
}