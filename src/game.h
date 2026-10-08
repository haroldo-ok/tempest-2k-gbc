#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include <gb/gb.h>

#define B_LEFT  0x01
#define B_RIGHT 0x02
#define B_UP    0x04
#define B_DOWN  0x08
#define B_FIRE  0x10
#define B_ZAP   0x20
#define B_JUMP  0x40
#define B_START 0x80

enum { T_NONE = 0, T_FLIPPER, T_MUTANT, T_TANKER, T_TANKER_P, T_TANKER_F, T_SPIKER, T_FUSE,
       T_PULSAR, T_DEMON, T_MIRROR, T_POWERUP };
enum { PU_LASER = 0, PU_JUMP, PU_DROID, PU_ZAPPO, PU_OUTTA, PU_WARP, PU_COUNT };

#define MAX_EN    16
#define MAX_SHOT  8
#define MAX_ESHOT 6
#define MAX_EXPL  4
#define DEPTH     4096

typedef struct {
    uint8_t type, lane, phase, timer;
    int16_t z, dz;
    uint8_t lp, aux, dir, hp;
    uint16_t want;
} enemy_t;

typedef struct { uint8_t alive, lane, laser; int16_t z; } shot_t;
typedef struct { uint8_t alive, lane; int16_t z, dz; } eshot_t;
typedef struct { uint8_t t, x, y; } expl_t;

enum { GS_TITLE = 0, GS_SELECT, GS_WARP, GS_PLAY, GS_DIED, GS_ZOOM, GS_GAMEOVER, GS_PAUSE };

extern uint8_t gstate; extern uint16_t gstate_t;
extern uint8_t level, uweb, lanes, closed, lives, zaps, player_lane;
extern uint16_t player_pos; extern int16_t player_z;
extern uint32_t score, hiscore;
extern uint8_t best_level, select_level;
extern enemy_t en[MAX_EN];
extern shot_t shots[MAX_SHOT];
extern eshot_t eshots[MAX_ESHOT];
extern expl_t expl[MAX_EXPL];
extern uint16_t spikes[18];
extern uint8_t laser, jump_on, droid, jumping, zap_active, zap_t, pu_count, warp_tokens;
extern uint8_t enemies_alive, frame8, shake;
extern const char *msg; extern uint8_t msg_t;
extern uint8_t web_bank; extern uint16_t web_tbl;
extern uint8_t hud_dirty;
extern int16_t zoom_cam;
extern uint8_t sdig[6];
extern uint8_t pool[8];
extern int16_t player_dz;

#define MAPB(b) SWITCH_ROM_MBC5(b)
#define WT ((const uint8_t *)web_tbl)

void game_init(void) BANKED;
void score_reset(void) BANKED;
void score_add_start(uint16_t s) BANKED;
void game_message(const char *m, uint8_t t) BANKED;
void level_setup(uint8_t lvl) BANKED;
void play_tick(uint8_t btn, uint8_t press) BANKED;
void died_tick(void) BANKED;
void zoom_tick(uint8_t btn, uint8_t press) BANKED;
uint8_t level_cleared(void) BANKED;
void kill_all_for_zoom(void) BANKED;
uint8_t wrapl(uint8_t lane, uint8_t dir) BANKED;
extern const uint8_t *laneptr[18];
extern uint8_t web_ram[0x1600];

/* web tables live in WRAM after level_setup; safe to call from any bank */
static const uint8_t *lane_at(uint8_t lane, int16_t z)
{
    int16_t zi = (z + 1024) >> 6;
    if (zi < 0) zi = 0; else if (zi > 80) zi = 80;
    return laneptr[lane] + (uint8_t)zi * 3;
}

#endif
