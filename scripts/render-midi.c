/* SPDX-License-Identifier: BSD-3-Clause
 * Render the original score with the reference host's installed GM soundfont.
 * Output: raw 44100 Hz, stereo signed-16 little-endian. No composed sounds. */
#include <fluidsynth.h>
#include <stdio.h>
int main(int argc,char **argv){
    if(argc!=4)return 2;
    fluid_settings_t *s=new_fluid_settings();
    fluid_settings_setnum(s,"synth.sample-rate",44100.0);
    fluid_settings_setstr(s,"player.timing-source","sample");
    fluid_settings_setstr(s,"audio.file.name",argv[3]);
    fluid_settings_setstr(s,"audio.file.type","raw");
    fluid_settings_setstr(s,"audio.file.format","s16");
    fluid_settings_setstr(s,"audio.file.endian","little");
    fluid_synth_t *synth=new_fluid_synth(s);
    if(!synth||fluid_synth_sfload(synth,argv[1],1)<0)return 3;
    fluid_player_t *p=new_fluid_player(synth);
    if(fluid_player_add(p,argv[2])!=FLUID_OK)return 4;
    fluid_file_renderer_t *r=new_fluid_file_renderer(synth);if(!r)return 5;
    fluid_player_play(p);unsigned blocks=0;
    while(fluid_player_get_status(p)==FLUID_PLAYER_PLAYING){
        if(fluid_file_renderer_process_block(r)!=FLUID_OK||++blocks>44100u*180u/64u)return 6;
    }
    delete_fluid_file_renderer(r);delete_fluid_player(p);delete_fluid_synth(synth);delete_fluid_settings(s);
    printf("Original MIDI rendered: FluidSynth %s, 44100 Hz stereo S16LE, blocks=%u\n",fluid_version_str(),blocks);
    return 0;
}
