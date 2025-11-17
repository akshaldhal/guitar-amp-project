#ifndef PORTAUDIO_HANDLER_H
#define PORTAUDIO_HANDLER_H
#include <portaudio.h>

PaError initialize_portaudio();
PaError terminate_portaudio();
int get_number_of_devices();
const PaDeviceInfo* get_device_info(int deviceIndex);

#endif