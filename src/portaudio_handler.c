#include <logger.h>
#include <portaudio_handler.h>

PaError initialize_portaudio() {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "PortAudio initialized successfully.");
  }
  return err;
}

PaError terminate_portaudio() {
  PaError err = Pa_Terminate();
  if (err != paNoError) {
    log_message(LOG_LEVEL_ERROR, "Failed to terminate PortAudio: %s", Pa_GetErrorText(err));
  } else {
    log_message(LOG_LEVEL_INFO, "PortAudio terminated successfully.");
  }
  return err;
}

int get_number_of_devices() {
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