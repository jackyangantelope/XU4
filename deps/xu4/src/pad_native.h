/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef XU4_PAD_NATIVE_H
#define XU4_PAD_NATIVE_H
#include <stdint.h>
/* Same normalized bit order as the RP2350 NES/SNES input service. */
enum Xu4PadButton { XP_A=1,XP_B=2,XP_SELECT=4,XP_START=8,XP_UP=16,XP_DOWN=32,
                   XP_LEFT=64,XP_RIGHT=128,XP_X=256,XP_Y=512,XP_L=1024,XP_R=2048 };
struct Xu4PadState {
    uint16_t previous;
    uint32_t repeat_at;
    unsigned selected;
    bool keyboard, uppercase;
    Xu4PadState():previous(0),repeat_at(0),selected(0),keyboard(false),uppercase(false){}
    int update(uint16_t buttons,uint32_t ms);
    int selectedKey() const;
};
void xu4PadInit();
void xu4PadShutdown();
void xu4PadOverlayBegin();
void xu4PadOverlayEnd();
#endif
