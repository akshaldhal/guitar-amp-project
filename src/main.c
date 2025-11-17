#include <logger.h>
#include <portaudio_handler.h>

int main() {
  PaError err = initialize_portaudio();
  if (err != paNoError) {
    return -1;
  }

  int numDevices = get_number_of_devices();
  if (numDevices < 0) {
    terminate_portaudio();
    return -1;
  }

  for (int i = 0; i < numDevices; i++) {
    const PaDeviceInfo* deviceInfo = get_device_info(i);
    if (deviceInfo != NULL) {
      log_message(LOG_LEVEL_INFO, "Device %d: %s", i, deviceInfo->name);
      log_message(LOG_LEVEL_INFO, "  Max Input Channels: %d", deviceInfo->maxInputChannels);
      log_message(LOG_LEVEL_INFO, "  Max Output Channels: %d", deviceInfo->maxOutputChannels);
      log_message(LOG_LEVEL_INFO, "  Default Sample Rate: %.2f", deviceInfo->defaultSampleRate);
      log_message(LOG_LEVEL_INFO, "");
    }
  }

  err = terminate_portaudio();
  if (err != paNoError) {
    return -1;
  }

  return 0;
}