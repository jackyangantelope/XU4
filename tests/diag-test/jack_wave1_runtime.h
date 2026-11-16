#pragma once
#include <stdint.h>
#define JACK_WAVE1_PATH_MAX 256
struct jack_wave1_key_event_t {uint8_t keycode,modifier;};
void jack_wave1_usb_service();
bool jack_wave1_usb_pop_key(jack_wave1_key_event_t*);
uint8_t jack_wave1_usb_current_modifiers();
uint16_t jack_wave1_poll_physical_buttons();
uint16_t *jack_wave1_framebuffer();
bool jack_wave1_init(const char*);
bool jack_wave1_mount_sd();
void jack_wave1_show_error(const char*,const char*);
