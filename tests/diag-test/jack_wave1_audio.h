#pragma once
#include <stdint.h>
#include <stddef.h>
void jack_wave1_audio_submit(const int16_t*,size_t);
void jack_wave1_audio_report();
bool jack_wave1_audio_begin(unsigned);
