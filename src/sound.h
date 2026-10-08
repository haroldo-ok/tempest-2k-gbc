#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>

#define SFX_SHOT 0
#define SFX_LASER 1
#define SFX_EXPLODE 2
#define SFX_FLIP 3
#define SFX_DIE 4
#define SFX_ZAP 5
#define SFX_POWERUP 6
#define SFX_CLEAR 7
#define SFX_SPIKE 8
#define SFX_JUMP 9
#define SFX_MENU 10
#define SFX_WARP 11
#define SFX_ONEUP 12
#define SFX_SPLIT 13
#define SFX_COUNT 14

#define SONG_TITLE 0
#define SONG_LEVELA 1
#define SONG_LEVELB 2
#define SONG_LEVELC 3
#define SONG_GAMEOVER 4
#define SONG_WARP 5
#define SONG_OFF 255

void sound_init(void) BANKED;
void sfx(uint8_t id) BANKED;
void music_play(uint8_t song) BANKED;
void music_stop(void) BANKED;
void sound_pause(uint8_t on) BANKED;
void sound_tick(void) BANKED;

#endif
