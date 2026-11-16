/* SPDX-License-Identifier: BSD-3-Clause */
#include <SDL.h>
#include <cstring>
#include <vector>
#include "pad_native.h"
#include "event.h"
#include "screen.h"
#include "image.h"
#include "xu4.h"
static SDL_Joystick *stick;
static uint16_t buttons,hat,axis;
static Xu4PadState state;
static std::vector<uint32_t> saved;
static const uint16_t map[]={XP_A,XP_B,XP_X,XP_Y,XP_L,XP_R,XP_SELECT,XP_START};
void xu4PadInit(){
    if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)==0){
        if(SDL_NumJoysticks()>0)stick=SDL_JoystickOpen(0);
        SDL_JoystickEventState(SDL_ENABLE);
    }
}
void xu4PadShutdown(){if(stick)SDL_JoystickClose(stick);stick=NULL;saved.clear();buttons=hat=axis=0;state=Xu4PadState();}
void xu4PadEvent(const SDL_Event &e){
    if(e.type==SDL_JOYBUTTONDOWN||e.type==SDL_JOYBUTTONUP){
        if(e.jbutton.which!=0||e.jbutton.button>=sizeof(map)/sizeof(map[0]))return;
        if(e.type==SDL_JOYBUTTONDOWN)buttons|=map[e.jbutton.button];else buttons&=~map[e.jbutton.button];
    }else if(e.type==SDL_JOYHATMOTION&&e.jhat.which==0&&e.jhat.hat==0){
        hat=0;if(e.jhat.value&SDL_HAT_UP)hat|=XP_UP;if(e.jhat.value&SDL_HAT_DOWN)hat|=XP_DOWN;
        if(e.jhat.value&SDL_HAT_LEFT)hat|=XP_LEFT;if(e.jhat.value&SDL_HAT_RIGHT)hat|=XP_RIGHT;
    }else if(e.type==SDL_JOYAXISMOTION&&e.jaxis.which==0&&e.jaxis.axis<2){
        uint16_t negative=e.jaxis.axis?XP_UP:XP_LEFT,positive=e.jaxis.axis?XP_DOWN:XP_RIGHT;
        axis&=~(negative|positive);
        if(e.jaxis.value<-12000)axis|=negative;if(e.jaxis.value>12000)axis|=positive;
    }else if(e.type==SDL_ACTIVEEVENT && !e.active.gain){buttons=hat=axis=0;}
}
int xu4PadKey(){return state.update(buttons|hat|axis,SDL_GetTicks());}
void xu4PadOverlayBegin(){
    if(!state.keyboard||!xu4.screenImage)return;
    Image *im=xu4.screenImage;
    size_t count=(size_t)im->w*im->h;saved.assign(im->pixels,im->pixels+count);
    int scale=im->w/320;
    im->fillRect(0,120*scale,320*scale,80*scale,0,0,0);
    screenTextAt(0,15,"SELECT CLOSE  A TYPE  Y CASE  START ENTER");
    for(unsigned i=0;i<40;i++){
        char label[5];
        if(i<36){char key="abcdefghijklmnopqrstuvwxyz0123456789"[i];if(state.uppercase&&key>='a'&&key<='z')key-=32;
            snprintf(label,sizeof label," %c ",key);}
        else{const char *special[]={"SPC","ENT","DEL","ESC"};strcpy(label,special[i-36]);}
        if(state.selected==i){screenTextAt((i%8)*5,17+i/8,">%s",label);}
        else screenTextAt((i%8)*5,17+i/8," %s",label);
    }
}
void xu4PadOverlayEnd(){
    if(saved.empty()||!xu4.screenImage)return;
    size_t count=(size_t)xu4.screenImage->w*xu4.screenImage->h;
    if(count==saved.size())memcpy(xu4.screenImage->pixels,&saved[0],count*sizeof(uint32_t));
    saved.clear();
}
