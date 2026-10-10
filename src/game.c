/* Tempest 2000 mode rules, ported from the SMS/32X 8.8-fixed-point core. */
#include "game.h"
#include "sound.h"
#include "assets.h"
#pragma bank 1

uint8_t gstate; uint16_t gstate_t;
uint8_t level, uweb, lanes, closed, lives, zaps, player_lane;
uint16_t player_pos; int16_t player_z, player_dz;
uint32_t score, hiscore, next_bonus;
uint8_t best_level = 1, select_level = 1;
enemy_t en[MAX_EN];
shot_t shots[MAX_SHOT];
eshot_t eshots[MAX_ESHOT];
expl_t expl[MAX_EXPL];
uint16_t spikes[18];
uint8_t laser, jump_on, droid, jumping, zap_active, zap_t, pu_count, warp_tokens;
uint16_t droid_pos;
uint8_t droid_lane, droid_cd;
uint8_t enemies_alive, frame8, shake;
const char *msg; uint8_t msg_t;
uint8_t web_bank; uint16_t web_tbl;
uint8_t hud_dirty;
int16_t zoom_cam;
uint8_t pool[8];
uint8_t spawn_t, fire_cd, kills, shots_alive;
const uint8_t *laneptr[18];
static uint16_t rng = 0xACE1;
uint8_t sdig[6];
uint8_t web_ram[0x1600];

uint8_t rnd(void) { rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (uint8_t)rng; }
uint8_t rndn(uint8_t n) { return (uint8_t)(((uint16_t)rnd() * n) >> 8); }

void game_message(const char *m, uint8_t t) BANKED { msg = m; msg_t = t; hud_dirty = 1; }

void add_dec(uint16_t s)
{
    static const uint16_t pw[5] = { 10000, 1000, 100, 10, 1 };
    uint8_t i, c, k, d[6];
    c = 0;
    d[0] = 0;
    for (i = 0; i < 5; i++) { k = 0; while (s >= pw[i]) { s -= pw[i]; k++; } d[i + 1] = k; }
    for (i = 6; i-- > 0;) { uint8_t v = sdig[i] + d[i] + c; c = v >= 10; sdig[i] = c ? v - 10 : v; }
}
void add_score(uint16_t s)
{
    score += s; add_dec(s);
    if (score >= next_bonus) { next_bonus += 100000UL; lives++; game_message("EXTRA LIFE", 90); voice_play(VOICE_ONEUP); sfx(SFX_ONEUP); }
    hud_dirty = 1;
}

uint8_t wrap(int8_t l)
{
    if (closed) { if (l < 0) l += lanes; else if (l >= (int8_t)lanes) l -= lanes; return (uint8_t)l; }
    if (l < 0) return 0;
    if (l >= (int8_t)lanes) return lanes - 1;
    return (uint8_t)l;
}
int8_t toward_player(uint8_t from)
{
    int8_t d = (int8_t)player_lane - (int8_t)from;
    if (closed) { int8_t h = lanes >> 1; if (d > h) d -= lanes; else if (d < -h) d += lanes; }
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

void game_init(void) BANKED { hiscore = 20000; }
void score_reset(void) BANKED { uint8_t i; score = 0; next_bonus = 20000; for (i = 0; i < 6; i++) sdig[i] = 0; }
void score_add_start(uint16_t s) BANKED { add_score(s); }

void copy_web_table(uint8_t bank, uint16_t addr) NONBANKED
{
    uint8_t saved = CURRENT_BANK;
    const uint8_t *s;
    uint8_t *d = web_ram;
    uint16_t n = 0x1600;
    SWITCH_ROM_MBC5(bank);
    s = (const uint8_t *)addr;
    while (n--) *d++ = *s++;
    SWITCH_ROM_MBC5(saved);
}

void level_setup(uint8_t lvl) BANKED
{
    uint8_t i, l = lvl;
    uint8_t wi, b;
    uint16_t a;
    wi = (uint8_t)((lvl - 1) % NUM_LEVEL_WEBS);
    level = lvl;
    uweb = level_web[wi];
    b = uweb_bank[uweb]; a = uweb_addr[uweb];
    copy_web_table(b, a);
    web_bank = 1; web_tbl = (uint16_t)web_ram;
    lanes = WT[0]; closed = WT[1];
    for (i = 0; i < lanes; i++) laneptr[i] = WT + 0x4C0 + (uint16_t)i * 243;
    player_lane = WT[2]; player_pos = (uint16_t)player_lane << 8;
    pool[0] = 8 + l * 2;            if (pool[0] > 40) pool[0] = 40;
    pool[1] = l >= 3 ? 2 + l / 3 : 0;  if (pool[1] > 10) pool[1] = 10;
    pool[2] = l >= 4 ? 1 + l / 4 : 0;  if (pool[2] > 6) pool[2] = 6;
    pool[3] = l >= 11 ? 1 + l / 8 : 0; if (pool[3] > 6) pool[3] = 6;
    pool[4] = l >= 17 ? 2 + l / 10 : 0; if (pool[4] > 8) pool[4] = 8;
    pool[5] = l >= 33 ? 1 + l / 20 : 0;
    pool[6] = l >= 25 ? 1 + l / 15 : 0; if (pool[6] > 4) pool[6] = 4;
    for (i = 0; i < MAX_EN; i++) en[i].type = T_NONE;
    for (i = 0; i < MAX_SHOT; i++) shots[i].alive = 0;
    for (i = 0; i < MAX_ESHOT; i++) eshots[i].alive = 0;
    for (i = 0; i < MAX_EXPL; i++) expl[i].t = 0;
    for (i = 0; i < 18; i++) spikes[i] = 0;
    zaps = 2; zap_active = 0; jump_on = 0; laser = 0; droid = 0; pu_count = 0;
    player_z = 0; jumping = 0; shots_alive = 0; enemies_alive = 0;
    spawn_t = 40; fire_cd = 0; kills = 0;
    droid_pos = player_pos; droid_lane = player_lane; droid_cd = 0;
    if (lvl > best_level && (lvl & 1)) best_level = lvl;
    hud_dirty = 1;
}

static enemy_t *alloc_en(void)
{
    uint8_t i;
    for (i = 0; i < MAX_EN; i++) if (en[i].type == T_NONE) {
        enemy_t *e = &en[i];
        e->phase = 0; e->timer = 0; e->lp = 0; e->aux = 0; e->dir = 0; e->hp = 1;
        return e;
    }
    return 0;
}

void spawn(uint8_t type, int8_t lane, int16_t z)
{
    enemy_t *e = alloc_en();
    uint8_t spd = 4 + level; if (spd > 36) spd = 36;
    if (!e) return;
    e->type = type; e->lane = wrap(lane); e->z = z;
    switch (type) {
    case T_FLIPPER: e->dz = -(int16_t)((spd * 41) >> 4); break;
    case T_MUTANT:  e->dz = -(int16_t)((spd * 65) >> 4); break;
    case T_TANKER: case T_TANKER_P: case T_TANKER_F: e->dz = -(int16_t)((spd * 25) >> 4); break;
    case T_SPIKER:  e->dz = -(int16_t)((spd * 49) >> 4); break;
    case T_FUSE:    e->dz = -(int16_t)((spd * 49) >> 4); e->timer = 30 + rndn(60); break;
    case T_PULSAR:  e->dz = -(int16_t)((spd * 37) >> 4); break;
    case T_DEMON:   e->dz = -(int16_t)((spd * 29) >> 4); e->hp = 2; break;
    case T_MIRROR:  e->dz = -(int16_t)((spd * 33) >> 4); break;
    case T_POWERUP: e->dz = -26; e->aux = pu_count < PU_COUNT ? pu_count : PU_WARP; break;
    }
    if (type != T_POWERUP) enemies_alive++;
}

void spawn_wave(void)
{
    int8_t l = rndn(lanes);
    uint8_t budget = 1 + (level + 2) / 4; if (budget > 4) budget = 4;
    while (budget--) {
        uint8_t roll = rndn(100);
        if (pool[4] && roll < 12)      { pool[4]--; spawn(T_PULSAR, l, DEPTH); }
        else if (pool[3] && roll < 22) { pool[3]--; spawn(T_FUSE, l, DEPTH); }
        else if (pool[2] && roll < 34) { pool[2]--; spawn(T_SPIKER, l, DEPTH); }
        else if (pool[1] && roll < 50) { uint8_t t = rndn(3); pool[1]--;
            spawn(t == 0 && level >= 20 ? T_TANKER_P : t == 1 && level >= 14 ? T_TANKER_F : T_TANKER, l, DEPTH); }
        else if (pool[6] && roll < 58) { pool[6]--; spawn(T_DEMON, l, DEPTH); }
        else if (pool[5] && roll < 62) { pool[5]--; spawn(T_MIRROR, l, DEPTH); }
        else if (pool[0])              { pool[0]--; spawn(level > 40 && rndn(4) == 0 ? T_MUTANT : T_FLIPPER, l, DEPTH + ((int16_t)rndn(3) << 8)); }
        else break;
        l = wrap(l + (int8_t)rndn(5) - 2);
    }
}

void enemy_shot(uint8_t lane, int16_t z)
{
    uint8_t i;
    for (i = 0; i < MAX_ESHOT; i++) if (!eshots[i].alive) {
        eshots[i].alive = 1; eshots[i].lane = lane; eshots[i].z = z; eshots[i].dz = -(31 + level); return;
    }
}

void explode_at(uint8_t lane, int16_t z)
{
    uint8_t i; const uint8_t *p = lane_at(lane, z);
    for (i = 0; i < MAX_EXPL; i++) if (!expl[i].t) { expl[i].t = 16; expl[i].x = p[0]; expl[i].y = p[1]; return; }
}

uint16_t score_for(uint8_t t)
{
    switch (t) {
    case T_FLIPPER: return 150; case T_MUTANT: return 200;
    case T_TANKER: case T_TANKER_P: case T_TANKER_F: return 100;
    case T_SPIKER: return 50; case T_FUSE: return 250; case T_PULSAR: return 200;
    case T_DEMON: return 300; case T_MIRROR: return 100;
    }
    return 0;
}

void kill_enemy(enemy_t *e, uint8_t by_player)
{
    uint8_t t = e->type;
    explode_at(e->lane, e->z);
    e->type = T_NONE; enemies_alive--;
    if (t == T_TANKER || t == T_TANKER_P || t == T_TANKER_F) {
        uint8_t c = t == T_TANKER_P ? T_PULSAR : t == T_TANKER_F ? T_FUSE : T_FLIPPER;
        spawn(c, (int8_t)e->lane - 1, e->z); spawn(c, (int8_t)e->lane + 1, e->z);
        sfx(SFX_SPLIT);
    }
    if (by_player) {
        add_score(score_for(t) << (e->z > (12 << 8) ? 1 : 0));
        if (++kills >= 5 + pu_count * 2) { kills = 0; spawn(T_POWERUP, e->lane, DEPTH); }
    }
    sfx(SFX_EXPLODE);
}

void player_die(void)
{
    if (gstate != GS_PLAY) return;
    explode_at(player_lane, 0);
    sfx(SFX_DIE);
    gstate = GS_DIED; gstate_t = 0; shake = 20;
    lives--; hud_dirty = 1;
}

void superzap(void)
{
    if (!zaps || zap_active) return;
    zaps--; zap_active = 1; zap_t = 0; hud_dirty = 1;
    game_message(zaps == 1 ? "SUPERZAPPER" : "PARTIAL ZAP", 60);
    sfx(SFX_ZAP);
}
void zap_tick(void)
{
    uint8_t i, full;
    if (!zap_active) return;
    zap_t++;
    full = (zaps == 1);
    if ((zap_t & 3) == 0) {
        for (i = 0; i < MAX_ESHOT; i++) eshots[i].alive = 0;
        for (i = 0; i < MAX_EN; i++) {
            enemy_t *e = &en[i];
            if (e->type == T_NONE || e->type == T_POWERUP) continue;
            kill_enemy(e, 1);
            if (!full) { zap_active = 0; return; }
            if (zap_t < 40) break;
        }
    }
    if (zap_t > 50) zap_active = 0;
}

void run_flipper(enemy_t *e)
{
    if (e->z > 0) {
        e->z += e->dz; if (e->z < 0) e->z = 0;
        if (e->type == T_MUTANT && rndn(60) == 0) e->lane = wrap(e->lane + (rnd() & 1 ? 1 : -1));
        if (level > 3 && rnd() == 0 && (rnd() & 1)) enemy_shot(e->lane, e->z);
        return;
    }
    if (e->timer == 0) {
        int8_t d = toward_player(e->lane);
        if (d == 0) { if (player_z == 0) player_die(); return; }
        e->dir = (uint8_t)d;
        e->timer = 16 - level / 8; if (e->timer < 6 || e->timer > 16) e->timer = 6;
        e->aux = e->timer; e->lp = 0;
        sfx(SFX_FLIP);
    }
    e->timer--;
    e->lp = (uint8_t)(((uint8_t)(e->aux - e->timer) << 3) / e->aux);
    if (e->timer == 0) {
        e->lane = wrap((int8_t)e->lane + (int8_t)e->dir); e->lp = 0;
        if (e->lane == player_lane && player_z == 0) player_die();
    }
}

void run_spiker(enemy_t *e)
{
    int16_t h;
    e->z += e->dz;
    h = DEPTH - e->z;
    if (e->dz < 0 && h > 0 && (uint16_t)h > spikes[e->lane]) spikes[e->lane] = h;
    if (e->dz < 0 && e->z < 512 + ((int16_t)(rnd() & 1) << 8)) e->dz = -e->dz;
    if (e->z >= DEPTH) { e->type = T_NONE; enemies_alive--; if (rnd() & 1) pool[2]++; }
}

void run_fuse(enemy_t *e)
{
    if (e->phase == 0) {
        e->z += e->dz;
        if (--e->timer == 0) { e->phase = 1; e->timer = 20; e->dir = (rnd() & 1) ? 1 : 255; }
        if (e->z <= 0) { e->z = 0; e->phase = 2; e->timer = 60; }
    } else if (e->phase == 1) {
        e->lp = 20 - e->timer;
        if (--e->timer == 0) { e->lane = wrap((int8_t)e->lane + (int8_t)e->dir); e->lp = 0; e->phase = 0; e->timer = 30 + rndn(50); }
    } else {
        if (--e->timer == 0) {
            int8_t d = toward_player(e->lane); if (!d) d = 1;
            e->lane = wrap((int8_t)e->lane + d); e->timer = 40;
            if (e->lane == player_lane && player_z == 0) player_die();
        }
        if (rndn(200) == 0) { e->phase = 0; if (e->dz < 0) e->dz = -e->dz; e->timer = 200; }
    }
    if (e->z >= DEPTH && e->dz > 0) e->dz = -e->dz;
}

void run_pulsar(enemy_t *e)
{
    e->z += e->dz; if (e->z < 0) e->z = 0;
    e->aux = ((frame8 >> 5) & 3) == 0;
    if (e->aux && e->lane == player_lane && player_z == 0 && e->z < 384) player_die();
    if (e->z == 0 && (frame8 & 63) == 0) e->lane = wrap(e->lane + ((rnd() & 1) ? 1 : -1));
}

void run_bouncer(enemy_t *e, int16_t lo)
{
    e->z += e->dz;
    if (e->z <= lo && e->dz < 0) e->dz = -e->dz;
    if (e->z >= DEPTH && e->dz > 0) e->dz = -e->dz;
}

void collect(uint8_t k)
{
    static const char *const names[PU_COUNT] = { "PARTICLE LASER", "JUMP ENABLED", "AI DROID", "ZAPPO 2000", "OUTTA HERE!", "WARP TOKEN" };
    uint8_t i;
    game_message(names[k], 70); sfx(SFX_POWERUP); add_score(500);
    /* Jaguar: pickup whoosh + text; spoken lines only on a few of them.
     * Zappo plays Excellent (sprzap); queuing a droid plays Yes; other
     * pickups get the menu-confirm "sexy yes" so they still talk. */
    {
        static const uint8_t pv[PU_COUNT] = {
            VOICE_SEXY1, VOICE_SEXY2, VOICE_YES, VOICE_EXCELLENT, VOICE_SEXY2, VOICE_YES
        };
        voice_play(k < PU_COUNT ? pv[k] : VOICE_SEXY2);
    }
    switch (k) {
    case PU_LASER: laser = 1; break;
    case PU_JUMP: jump_on = 1; break;
    case PU_DROID:
        droid = 1;
        droid_pos = player_pos;
        droid_lane = player_lane;
        droid_cd = 0;
        break;
    case PU_ZAPPO: zaps++; break;
    case PU_OUTTA:
        for (i = 0; i < MAX_EN; i++) en[i].type = T_NONE;
        for (i = 0; i < 8; i++) pool[i] = 0;
        enemies_alive = 0; break;
    case PU_WARP:
        if (++warp_tokens >= 3) { warp_tokens = 0; add_score(10000); game_message("WARP BONUS", 90); }
        break;
    }
    pu_count++;
}

void run_powerup(enemy_t *e)
{
    e->z += e->dz;
    if (e->z <= 0) {
        e->type = T_NONE;
        if (e->lane == player_lane || (droid && e->lane == droid_lane)) collect(e->aux);
    }
}

void update_enemies(void)
{
    uint8_t i; enemy_t *e = en;
    for (i = MAX_EN; i; i--, e++) {
        if (!e->type) continue;
        switch (e->type) {
        case T_FLIPPER: case T_MUTANT: run_flipper(e); break;
        case T_TANKER: case T_TANKER_P: case T_TANKER_F:
            e->z += e->dz; if (e->z <= 0) { e->z = 0; kill_enemy(e, 0); } break;
        case T_SPIKER: run_spiker(e); break;
        case T_FUSE: run_fuse(e); break;
        case T_PULSAR: run_pulsar(e); break;
        case T_DEMON: run_bouncer(e, 256); if (rndn(120) == 0) enemy_shot(e->lane, e->z);
            if (rndn(200) == 0) e->lane = wrap(e->lane + ((rnd() & 1) ? 1 : -1)); break;
        case T_MIRROR: run_bouncer(e, 128); break;
        case T_POWERUP: run_powerup(e); break;
        }
    }
}

void fire(uint8_t lane, int16_t z, uint8_t las)
{
    uint8_t i;
    for (i = 0; i < MAX_SHOT; i++) if (!shots[i].alive) {
        shots[i].alive = 1; shots[i].lane = lane; shots[i].z = z; shots[i].laser = las; shots_alive++;
        return;
    }
}

void run_shots(int16_t depth_limit)
{
    uint8_t i, j;
    for (i = 0; i < MAX_SHOT; i++) {
        shot_t *s = &shots[i];
        if (!s->alive) continue;
        /* Hit-test at the shot's current z first so rim (z≈0) enemies
         * are not skipped when the shot then jumps +141. */
        if (gstate == GS_PLAY) {
            for (j = 0; j < MAX_ESHOT; j++) {
                eshot_t *q = &eshots[j];
                if (q->alive && q->lane == s->lane && (uint16_t)(q->z - s->z + 179) < 358) {
                    q->alive = 0; add_score(10); if (!s->laser) { s->alive = 0; shots_alive--; } break;
                }
            }
            if (!s->alive) continue;
            for (j = 0; j < MAX_EN; j++) {
                enemy_t *e = &en[j];
                uint8_t same;
                if (e->type == T_NONE || e->type == T_POWERUP) continue;
                same = e->lane == s->lane;
                if (!same && (e->type == T_FLIPPER || e->type == T_MUTANT) && e->z == 0 && e->lp > 4 &&
                    wrap((int8_t)e->lane + (int8_t)e->dir) == s->lane) same = 1;
                if (!same) continue;
                if (e->type == T_FUSE && e->phase != 1 && !s->laser) continue;
                if ((uint16_t)(e->z - s->z + 179) < 358) {
                    if (e->type == T_MIRROR) {
                        s->alive = 0; shots_alive--; enemy_shot(e->lane, e->z); break;
                    }
                    if (--e->hp == 0) kill_enemy(e, 1);
                    if (!s->laser || e->type == T_DEMON) { s->alive = 0; shots_alive--; }
                    break;
                }
            }
            if (!s->alive) continue;
        }
        s->z += 141;
        if (s->z > depth_limit) { s->alive = 0; shots_alive--; continue; }
        if (spikes[s->lane] && s->z >= (int16_t)(DEPTH - spikes[s->lane])) {
            spikes[s->lane] = spikes[s->lane] > 128 ? spikes[s->lane] - 128 : 0;
            add_score(1); sfx(SFX_SPIKE);
            s->alive = 0; shots_alive--; continue;
        }
    }
}

void run_eshots(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ESHOT; i++) {
        eshot_t *q = &eshots[i];
        if (!q->alive) continue;
        q->z += q->dz;
        if (q->z <= 0) {
            q->alive = 0;
            if (q->lane == player_lane && player_z == 0 && !zap_active) player_die();
        }
    }
}

void run_expl(void)
{
    uint8_t i;
    for (i = 0; i < MAX_EXPL; i++) if (expl[i].t) expl[i].t--;
}

void move_player(uint8_t btn, uint8_t spd)
{
    uint16_t maxp = (uint16_t)lanes << 8;
    if (btn & B_LEFT)  { if (player_pos >= spd) player_pos -= spd; else player_pos = closed ? player_pos + maxp - spd : 0; }
    if (btn & B_RIGHT) { player_pos += spd; if (player_pos >= maxp) player_pos = closed ? player_pos - maxp : maxp - 1; }
    player_lane = (uint8_t)((player_pos + 128) >> 8);
    if (player_lane >= lanes) player_lane = closed ? 0 : lanes - 1;
}

static int8_t toward_lane(uint8_t from, uint8_t to)
{
    int8_t d = (int8_t)to - (int8_t)from;
    if (closed) { int8_t h = lanes >> 1; if (d > h) d -= lanes; else if (d < -h) d += lanes; }
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

/* Independent AI claw: seek the most threatening enemy and fire on its lane. */
static void run_droid(void)
{
    uint8_t i;
    int16_t best = 0x7FFF;
    int8_t tl = -1;
    int8_t dir;
    uint16_t maxp;
    uint8_t spd = 36;

    if (!droid) return;

    for (i = 0; i < MAX_EN; i++) {
        enemy_t *e = &en[i];
        int8_t d, ad;
        int16_t sc;
        if (e->type == T_NONE || e->type == T_POWERUP) continue;
        d = (int8_t)e->lane - (int8_t)droid_lane;
        if (closed) {
            if (d > (int8_t)(lanes >> 1)) d -= lanes;
            else if (d < -(int8_t)(lanes >> 1)) d += lanes;
        }
        ad = d < 0 ? (int8_t)-d : d;
        sc = (e->z >> 5) + ((int16_t)ad << 4);
        if (sc < best) { best = sc; tl = (int8_t)e->lane; }
    }

    maxp = (uint16_t)lanes << 8;
    if (tl >= 0) {
        dir = toward_lane(droid_lane, (uint8_t)tl);
        if (dir < 0) {
            if (droid_pos >= spd) droid_pos -= spd;
            else droid_pos = closed ? droid_pos + maxp - spd : 0;
        } else if (dir > 0) {
            droid_pos += spd;
            if (droid_pos >= maxp) droid_pos = closed ? droid_pos - maxp : maxp - 1;
        }
    }
    droid_lane = (uint8_t)((droid_pos + 128) >> 8);
    if (droid_lane >= lanes) droid_lane = closed ? 0 : lanes - 1;

    if (droid_cd) droid_cd--;
    if (tl >= 0 && !droid_cd) {
        int8_t d, ad;
        d = (int8_t)droid_lane - tl;
        if (closed) {
            if (d > (int8_t)(lanes >> 1)) d -= lanes;
            else if (d < -(int8_t)(lanes >> 1)) d += lanes;
        }
        ad = d < 0 ? (int8_t)-d : d;
        /* Shoot the target lane once we're on it or next to it, from the rim
         * so the shot overlaps z≈0 (playfield edge) this frame. */
        if (ad <= 1) {
            fire((uint8_t)tl, 0, 0);
            droid_cd = 6;
        }
    }
}

void play_tick(uint8_t btn, uint8_t press) BANKED
{
    frame8++;
    move_player(btn, 41);
    if (fire_cd) fire_cd--;
    if ((btn & B_FIRE) && !fire_cd) {
        fire(player_lane, player_z + 77, laser); fire_cd = laser ? 3 : 5;
        sfx(laser ? SFX_LASER : SFX_SHOT);
    }
    if (press & B_ZAP) superzap();
    if ((press & B_JUMP) && jump_on && !jumping) { jumping = 1; player_dz = -115; sfx(SFX_JUMP); }
    if (jumping) { player_z += player_dz; player_dz += 6; if (player_z >= 0) { player_z = 0; jumping = 0; } }
    run_droid();
    update_enemies();
    run_shots(DEPTH + 256);
    run_eshots();
    run_expl();
    zap_tick();
    if (--spawn_t == 0) {
        spawn_wave();
        spawn_t = level < 50 ? 130 - level * 2 : 30; if (spawn_t < 30) spawn_t = 30;
    }
    if (msg_t && --msg_t == 0) hud_dirty = 1;
}

uint8_t level_cleared(void) BANKED
{
    uint8_t i;
    for (i = 0; i < 7; i++) if (pool[i]) return 0;
    return enemies_alive == 0;
}

void died_tick(void) BANKED
{
    uint8_t i;
    frame8++;
    run_expl();
    if (gstate_t == 1) for (i = 0; i < MAX_ESHOT; i++) eshots[i].alive = 0;
    if (msg_t && --msg_t == 0) hud_dirty = 1;
}

void player_respawn(void) BANKED
{
    uint8_t i;
    for (i = 0; i < MAX_EN; i++) en[i].type = T_NONE;
    for (i = 0; i < MAX_SHOT; i++) shots[i].alive = 0;
    for (i = 0; i < MAX_ESHOT; i++) eshots[i].alive = 0;
    enemies_alive = 0;
    zap_active = 0;
    jumping = 0;
    player_z = 0;
    shots_alive = 0;
    fire_cd = 0;
    if (zaps < 1) zaps = 1;
}

void kill_all_for_zoom(void) BANKED
{
    uint8_t i;
    for (i = 0; i < MAX_EN; i++) en[i].type = T_NONE;
    for (i = 0; i < MAX_ESHOT; i++) eshots[i].alive = 0;
    for (i = 0; i < MAX_EXPL; i++) expl[i].t = 0;
    enemies_alive = 0; jumping = 0; player_z = 0;
}

void zoom_tick(uint8_t btn, uint8_t press) BANKED
{
    (void)press;
    frame8++;
    move_player(btn, 26);
    if (fire_cd) fire_cd--;
    if ((btn & B_FIRE) && !fire_cd) { fire(player_lane, zoom_cam + 77, 0); fire_cd = 4; sfx(SFX_SHOT); }
    run_shots(DEPTH + 256);
    if (spikes[player_lane] && zoom_cam >= (int16_t)(DEPTH - spikes[player_lane]) && zoom_cam < DEPTH) {
        spikes[player_lane] = 0; lives--; shake = 30; sfx(SFX_DIE); hud_dirty = 1;
        game_message("SPIKED!", 60);
    }
    if (msg_t && --msg_t == 0) hud_dirty = 1;
}

uint8_t wrapl(uint8_t lane, uint8_t dir) BANKED { return wrap((int8_t)lane + (int8_t)dir); }
