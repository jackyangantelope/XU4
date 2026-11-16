/* SPDX-License-Identifier: BSD-3-Clause
 * Platform-independent pad -> Ultima key mapping, including text entry.
 */
#include "pad_native.h"
#include "event.h"
static const char keys[]="abcdefghijklmnopqrstuvwxyz0123456789";
int Xu4PadState::selectedKey() const {
    if(selected<36){int k=keys[selected];return uppercase&&k>='a'&&k<='z'?k-'a'+'A':k;}
    const int special[]={U4_SPACE,U4_ENTER,U4_BACKSPACE,U4_ESC};
    return special[selected-36];
}
int Xu4PadState::update(uint16_t buttons,uint32_t ms) {
    uint16_t edges=buttons&~previous;
    uint16_t dirs=buttons&(XP_UP|XP_DOWN|XP_LEFT|XP_RIGHT);
    uint16_t nav=edges&(XP_UP|XP_DOWN|XP_LEFT|XP_RIGHT);
    if(nav)repeat_at=ms+350;
    else if(dirs && (int32_t)(ms-repeat_at)>=0){nav=dirs;repeat_at=ms+120;}
    previous=buttons;
    if(edges&XP_SELECT){keyboard=!keyboard;return 0;}
    if(keyboard){
        if(nav&XP_LEFT)selected=selected%8?selected-1:selected+7;
        if(nav&XP_RIGHT)selected=selected%8==7?selected-7:selected+1;
        if(nav&XP_UP)selected=selected<8?selected+32:selected-8;
        if(nav&XP_DOWN)selected=selected>=32?selected-32:selected+8;
        if(edges&XP_Y){uppercase=!uppercase;return 0;}
        if(edges&XP_START){keyboard=false;return U4_ENTER;}
        if(edges&XP_B)return U4_BACKSPACE;
        if(edges&XP_X)return U4_SPACE;
        if(edges&XP_A){int key=selectedKey();if(key==U4_ESC||key==U4_ENTER)keyboard=false;return key;}
        return 0;
    }
    if(edges&XP_A)return U4_ENTER;
    if(edges&XP_B)return 'a';
    if(edges&XP_X)return 'e';
    if(edges&XP_Y)return 't';
    if(edges&XP_START)return U4_ESC;
    if(edges&XP_L)return U4_SPACE;
    if(edges&XP_R)return 'z';
    if(nav&XP_UP)return U4_UP;
    if(nav&XP_DOWN)return U4_DOWN;
    if(nav&XP_LEFT)return U4_LEFT;
    if(nav&XP_RIGHT)return U4_RIGHT;
    return 0;
}
