/* Tempest 2000 (T2K mode) for Game Boy Color.
 * Platform layer: states, palettes, sprite streaming, lit lane, HUD. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/hardware.h>
#include <stdint.h>
#include "game.h"
#include "sound.h"
#include "assets.h"
#include "fmv.h"

#define SPR_CLAW      0
#define SPR_SHOT      4
#define SPR_SPIKETIP 10
#define SPR_SPIKEDOT 12
#define SPR_DROID    14
#define SPR_LASER    16
#define SPR_ESHOT    22
#define SPR_EXPL     28
#define SPR_EN       48
#define MAX_VIS      16

static uint8_t nspr;
static uint16_t shown_img[MAX_EN], shown_base[MAX_EN];
static uint8_t shown_sz[MAX_EN];
static uint16_t claw_img, claw_want;
static uint8_t up_rr, sprites_ok;
static uint8_t hud[20];
static uint8_t lit_shown, lit_target;
static uint8_t gfx_b; static uint16_t gfx_a;
static uint8_t fade;
static uint8_t pal_set;
static uint8_t rot;

static void restore_code_bank(void) { SWITCH_ROM_MBC5(1); }

/* RGB555 palettes: 8 BG, 8 OBJ. */
static const palette_color_t spr_pal[32] = {
    /* 0 claw    */ RGB(0,0,0), RGB(20,8,0), RGB(31,16,0), RGB(31,31,10),
    /* 1 flipper */ RGB(0,0,0), RGB(16,0,0), RGB(31,4,0),  RGB(31,24,0),
    /* 2 mutant  */ RGB(0,0,0), RGB(10,0,16), RGB(24,0,24), RGB(31,8,8),
    /* 3 tanker  */ RGB(0,0,0), RGB(12,0,16), RGB(24,4,28), RGB(31,31,31),
    /* 4 pulsar  */ RGB(0,0,0), RGB(0,8,16), RGB(0,24,28), RGB(31,31,31),
    /* 5 spiker  */ RGB(0,0,0), RGB(0,12,0), RGB(0,28,8),  RGB(24,31,16),
    /* 6 demon   */ RGB(0,0,0), RGB(16,0,8), RGB(31,0,16), RGB(31,24,0),
    /* 7 shot    */ RGB(0,0,0), RGB(31,24,0), RGB(31,31,31), RGB(0,31,31)
};

static const uint8_t hue[8][3][3] = {
    {{8,0,12},{18,4,24},{31,18,31}},
    {{0,0,12},{0,8,24},{12,24,31}},
    {{0,8,4},{0,20,12},{16,31,24}},
    {{12,8,0},{24,16,0},{31,28,10}},
    {{0,8,12},{0,20,24},{10,31,31}},
    {{12,0,8},{24,4,16},{31,16,24}},
    {{8,0,0},{20,4,4},{31,16,12}},
    {{8,8,0},{20,20,0},{31,31,12}}
};

static palette_color_t bg_pal[32];

static void apply_web_palette(uint8_t set, uint8_t f)
{
    uint8_t i;
    /* pal 0 HUD */
    bg_pal[0] = RGB(0,0,0); bg_pal[1] = RGB(8,8,10); bg_pal[2] = RGB(20,20,24); bg_pal[3] = RGB(31,31,31);
    /* pal 1 web */
    bg_pal[4] = RGB(0,0,0);
    bg_pal[5] = RGB((hue[set][0][0]*f)>>3, (hue[set][0][1]*f)>>3, (hue[set][0][2]*f)>>3);
    bg_pal[6] = RGB((hue[set][1][0]*f)>>3, (hue[set][1][1]*f)>>3, (hue[set][1][2]*f)>>3);
    bg_pal[7] = RGB((hue[set][2][0]*f)>>3, (hue[set][2][1]*f)>>3, (hue[set][2][2]*f)>>3);
    /* pal 2 lit lane */
    bg_pal[8] = RGB(0,0,0);
    bg_pal[9] = RGB((16*f)>>3, (12*f)>>3, 0);
    bg_pal[10]= RGB((31*f)>>3, (28*f)>>3, (8*f)>>3);
    bg_pal[11]= RGB((31*f)>>3, (31*f)>>3, (24*f)>>3);
    /* pal 3 title flash */
    bg_pal[12]= RGB(0,0,0);
    bg_pal[13]= RGB(0, 8, 16);
    bg_pal[14]= RGB(8, 20, 31);
    bg_pal[15]= RGB(31, 31, 31);
    for (i = 16; i < 32; i++) bg_pal[i] = RGB(0,0,0);
    set_bkg_palette(0, 8, bg_pal);
}

static uint8_t glyph(char c)
{
    if (c >= '0' && c <= '9') return 1 + (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'Z') return 11 + (uint8_t)(c - 'A');
    if (c == '-') return 37; if (c == '!') return 38; if (c == '.') return 39; if (c == '*') return 40;
    return 0;
}

static void hud_text(uint8_t x, const char *s)
{
    while (*s && x < 20) hud[x++] = glyph(*s++);
}

static void hud_num(uint8_t x, uint16_t v, uint8_t digits)
{
    x = x + digits;
    while (digits--) { hud[--x] = 1 + (uint8_t)(v % 10); v /= 10; }
}

static void build_hud(void)
{
    uint8_t i;
    for (i = 0; i < 20; i++) hud[i] = 0;
    if (gstate == GS_TITLE) {
        hud_text(0, "HI"); hud_num(2, (uint16_t)(hiscore / 10), 5); hud[7] = 1;
        hud_text(9, "GBC");
        return;
    }
    if (gstate == GS_SELECT) {
        hud_text(0, "LEVEL"); hud_num(6, select_level, 2);
        hud_text(10, "A-GO");
        return;
    }
    for (i = 0; i < 6; i++) hud[i] = 1 + sdig[i];
    hud[6] = glyph('L'); hud_num(7, level, 2);
    hud[9] = glyph('*'); hud_num(10, lives > 9 ? 9 : lives, 1);
    hud[11] = glyph('Z'); hud_num(12, zaps > 9 ? 9 : zaps, 1);
    if (msg_t) {
        const char *s = msg; uint8_t n = 0;
        while (s[n]) n++;
        if (n > 7) hud_text(13, s); else hud_text(20 - n, s);
    } else if (gstate == GS_PAUSE) hud_text(14, "PAUSE");
    else if (gstate == GS_GAMEOVER) hud_text(13, "OVER");
}

static void hud_flush(void)
{
    static const uint8_t ha[20] = {0};
    if (!hud_dirty) return;
    hud_dirty = 0;
    build_hud();
    set_bkg_tiles(0, 0, 20, 1, hud);
    set_bkg_attributes(0, 0, 20, 1, ha);
}

static void clear_playfield(void)
{
    uint8_t blank[20];
    uint8_t y;
    for (y = 0; y < 20; y++) blank[y] = 0;
    for (y = 1; y < 18; y++) {
        set_bkg_tiles(0, y, 20, 1, blank);
        set_bkg_attributes(0, y, 20, 1, blank);
    }
}

static void load_font(void)
{
    MAPB(FONT_BANK);
    set_bkg_data(1, FONT_COUNT, (const uint8_t *)FONT_ADDR);
    restore_code_bank();
}

static void load_static_sprites(void)
{
    MAPB(SAFE_BANK);
    set_sprite_data(SPR_SHOT, SAFE_SIZE / 16, (const uint8_t *)SAFE_ADDR);
    MAPB(OTHER_BANK);
    set_sprite_data(SPR_LASER, OTHER_SIZE / 16, (const uint8_t *)OTHER_ADDR);
    restore_code_bank();
}

static void upload_image(uint16_t id, uint8_t tile)
{
    MAPB((uint8_t)(IMG_BANK0 + (id >> 8)));
    set_sprite_data(tile, 4, (const uint8_t *)(0x4000u + ((uint16_t)(id & 255) << 6)));
    restore_code_bank();
}

static void load_map_rec(uint8_t bank, uint16_t addr, uint8_t with_lit)
{
    const uint8_t *p;
    uint8_t ntiles;
    MAPB(bank);
    p = (const uint8_t *)addr;
    ntiles = p[0];
    if (ntiles) set_bkg_data(WEB_TILE0, ntiles, p + 3);
    set_bkg_tiles(0, 1, 20, 17, p + 3 + (uint16_t)ntiles * 16);
    set_bkg_attributes(0, 1, 20, 17, p + 3 + (uint16_t)ntiles * 16 + 340);
    if (with_lit) { gfx_b = bank; gfx_a = addr; }
    restore_code_bank();
}

static const uint8_t *lit_lane_rec(uint8_t lane)
{
    const uint8_t *p;
    uint8_t ntiles, i, n;
    MAPB(gfx_b);
    p = (const uint8_t *)gfx_a;
    ntiles = p[0];
    p = p + 3 + (uint16_t)ntiles * 16 + 680; /* nlanes */
    p++; /* skip nlanes */
    for (i = 0; i < lane; i++) {
        n = *p++;
        p += (uint16_t)n * 2;
    }
    return p;
}

static uint8_t read_attr(uint8_t x, uint8_t y)
{
    uint8_t a;
    VBK_REG = VBK_ATTRIBUTES;
    a = get_bkg_tile_xy(x, y);
    VBK_REG = VBK_TILES;
    return a;
}

/* Only toggle palette. Keep flip + VRAM-bank bits from the FMV layout. */
static void lit_set_pal(uint8_t lane, uint8_t pal)
{
    const uint8_t *r;
    uint8_t n, x, y, at;
    if (lane == 0xFF) return;
    r = lit_lane_rec(lane);
    n = *r++;
    while (n--) {
        x = *r++;
        y = *r++;
        at = read_attr(x, y);
        set_bkg_attribute_xy(x, y, (uint8_t)((at & 0xF8) | pal));
    }
    restore_code_bank();
}

static void lit_restore(uint8_t lane) { lit_set_pal(lane, 1); }
static void lit_apply(uint8_t lane) { lit_set_pal(lane, 2); }

static void lit_job(void)
{
    if (lit_target == lit_shown) return;
    if (lit_shown != 0xFF) lit_restore(lit_shown);
    if (lit_target != 0xFF) lit_apply(lit_target);
    lit_shown = lit_target;
}

static void lit_off_now(void)
{
    if (lit_shown != 0xFF) lit_restore(lit_shown);
    lit_shown = 0xFF; lit_target = 0xFF;
}

static uint8_t pal_of(uint8_t t)
{
    switch (t) {
    case T_FLIPPER: return 1;
    case T_MUTANT: return 2;
    case T_TANKER: case T_TANKER_P: case T_TANKER_F: return 3;
    case T_PULSAR: case T_FUSE: return 4;
    case T_SPIKER: return 5;
    case T_DEMON: return 6;
    case T_MIRROR: return 7;
    case T_POWERUP: return 7;
    default: return 0;
    }
}

static void spr(uint8_t x, uint8_t y, uint8_t tile, uint8_t pal, uint8_t wide)
{
    uint8_t sy;
    if (nspr >= 40) return;
    if (y < 4 || y > 140 || x < 2 || x > 157) return;
    sy = (uint8_t)(y + 8);
    if (wide) {
        set_sprite_tile(nspr, tile);
        set_sprite_prop(nspr, pal);
        move_sprite(nspr, x, sy);
        nspr++;
        if (nspr >= 40) return;
        set_sprite_tile(nspr, (uint8_t)(tile + 2));
        set_sprite_prop(nspr, pal);
        move_sprite(nspr, (uint8_t)(x + 8), sy);
        nspr++;
    } else {
        set_sprite_tile(nspr, tile);
        set_sprite_prop(nspr, pal);
        move_sprite(nspr, (uint8_t)(x + 4), sy);
        nspr++;
    }
}

static uint16_t enemy_image(enemy_t *e, uint8_t ang, uint8_t sz, uint16_t *base)
{
    uint16_t b, k;
    switch (e->type) {
    case T_FLIPPER: b = IMG_FLIPPER; k = ang; break;
    case T_MUTANT:  b = IMG_MUTANT; k = ang; break;
    case T_TANKER:  b = IMG_TANKER; k = ang; break;
    case T_TANKER_P: b = IMG_TANKERP; k = ang; break;
    case T_TANKER_F: b = IMG_TANKERF; k = ang; break;
    case T_SPIKER:  b = IMG_SPIKER; k = (frame8 >> 3) & 3; break;
    case T_FUSE:    b = IMG_FUSE; k = (frame8 >> 2) & 1; break;
    case T_PULSAR:  b = IMG_PULSAR; k = (uint16_t)ang * 2 + e->aux; break;
    case T_DEMON:   b = IMG_DEMON; k = ang; break;
    case T_MIRROR:  b = IMG_MIRROR; k = ang; break;
    default:        b = IMG_POWERUP; k = (frame8 >> 3) & 1; break;
    }
    *base = b;
    return b + k * 5 + sz;
}

static void draw_claw(int16_t zrel)
{
    const uint8_t *p;
    uint8_t x, y, sz, ang;
    (void)zrel;
    ang = WT[4 + player_lane];
    if (player_z < 0) { p = lane_at(player_lane, player_z); x = p[0]; y = p[1]; sz = p[2]; }
    else {
        uint8_t k = (uint8_t)(((player_pos - ((uint16_t)player_lane << 8) + 128) >> 5) & 7);
        const uint8_t *c = WT + 0x20 + (player_lane * 8 + k) * 2;
        x = c[0]; y = c[1]; sz = lane_at(player_lane, 0)[2];
    }
    claw_want = IMG_CLAW + ang * 5 + sz;
    if (claw_img == claw_want || claw_img != 0xFFFF)
        spr(x, y, SPR_CLAW, 0, sz != 0);
    if (droid && (gstate == GS_PLAY || gstate == GS_PAUSE)) {
        uint8_t dk = (uint8_t)(((droid_pos - ((uint16_t)droid_lane << 8) + 128) >> 5) & 7);
        const uint8_t *dc = WT + 0x20 + (droid_lane * 8 + dk) * 2;
        spr(dc[0], dc[1], SPR_DROID, 4, 0);
    }
}

static void draw_shots(int16_t zrel)
{
    uint8_t i;
    for (i = 0; i < MAX_SHOT; i++) {
        shot_t *s = &shots[i]; const uint8_t *p; uint8_t sz;
        if (!s->alive) continue;
        if (s->z - zrel < -1024) continue;
        p = lane_at(s->lane, (int16_t)(s->z - zrel)); sz = p[2] >> 1; if (sz > 2) sz = 2;
        spr(p[0], p[1], (uint8_t)((s->laser ? SPR_LASER : SPR_SHOT) + sz * 2), 7, 0);
    }
}

static void draw_spikes(int16_t zrel)
{
    uint8_t l;
    for (l = 0; l < lanes; l++) {
        uint16_t h = spikes[l]; int16_t zt;
        if (!h) continue;
        zt = DEPTH - (int16_t)h - zrel;
        if (zt >= -768) { const uint8_t *p = lane_at(l, zt); spr(p[0], p[1], SPR_SPIKETIP, 5, 0); }
        if (h > 1024) {
            int16_t zm = DEPTH - (int16_t)(h >> 1) - zrel;
            if (zm >= -768) { const uint8_t *p = lane_at(l, zm); spr(p[0], p[1], SPR_SPIKEDOT, 5, 0); }
        }
    }
}

static void draw_enemy(enemy_t *e, uint8_t i)
{
    const uint8_t *p;
    uint8_t x, y, sz, ang, t;
    uint16_t base, id;
    uint8_t slot = i;
    if (slot >= MAX_VIS) return;
    ang = WT[4 + e->lane];
    p = lane_at(e->lane, e->z); x = p[0]; y = p[1]; sz = p[2];
    t = e->type;
    if ((t == T_FLIPPER || t == T_MUTANT) && e->z == 0 && e->lp) {
        uint8_t k = e->lp - 1; const uint8_t *f;
        if (k > 7) k = 7;
        f = WT + 0x160 + (uint16_t)((uint8_t)((e->lane << 1) + (e->dir == 1)) * 8 + k) * 3;
        x = f[0]; y = f[1]; ang = f[2];
    } else if (t == T_FUSE && e->phase == 1) {
        const uint8_t *q = lane_at(wrapl(e->lane, e->dir), e->z);
        x += (int8_t)((((int16_t)q[0] - x) * e->lp * 13) >> 8);
        y += (int8_t)((((int16_t)q[1] - y) * e->lp * 13) >> 8);
    }
    id = enemy_image(e, ang, sz, &base);
    e->want = id;
    if (shown_img[i] == id) spr(x, y, (uint8_t)(SPR_EN + (slot << 2)), pal_of(t), sz != 0);
    else if (shown_base[i] == base) spr(x, y, (uint8_t)(SPR_EN + (slot << 2)), pal_of(t), shown_sz[i] != 0);
}

static void draw_sprites(void)
{
    uint8_t i;
    nspr = 0;
    if (gstate == GS_PLAY || gstate == GS_PAUSE || (gstate == GS_DIED && gstate_t < 2)) draw_claw(0);
    if (gstate == GS_ZOOM) { draw_claw(zoom_cam); draw_shots(zoom_cam); draw_spikes(zoom_cam); goto hide; }
    draw_shots(0);
    if (sprites_ok) {
        for (i = 0; i < MAX_ESHOT; i++) if (eshots[i].alive) {
            const uint8_t *p = lane_at(eshots[i].lane, eshots[i].z);
            uint8_t sz = p[2] >> 1; if (sz > 2) sz = 2;
            spr(p[0], p[1], (uint8_t)(SPR_ESHOT + sz * 2), 1, 0);
        }
        rot += 3; if (rot >= MAX_EN) rot -= MAX_EN;
        for (i = rot; i < MAX_EN; i++) if (en[i].type) draw_enemy(&en[i], i);
        for (i = 0; i < rot; i++) if (en[i].type) draw_enemy(&en[i], i);
        draw_spikes(0);
        for (i = 0; i < MAX_EXPL; i++) if (expl[i].t)
            spr(expl[i].x, expl[i].y, (uint8_t)(SPR_EXPL + ((16 - expl[i].t) >> 2) * 4), 7, 1);
    }
hide:
    while (nspr < 40) { hide_sprite(nspr); nspr++; }
}

static void image_jobs(void)
{
    uint8_t n = 0, k, i;
    if (claw_want != claw_img) { upload_image(claw_want, SPR_CLAW); claw_img = claw_want; n++; }
    if (!sprites_ok) return;
    for (k = 0; k < MAX_EN && n < 2; k++) {
        i = up_rr; if (++up_rr >= MAX_EN) up_rr = 0;
        if (en[i].type != T_NONE && en[i].want != shown_img[i]) {
            uint16_t id = en[i].want;
            uint16_t b = 0;
            upload_image(id, (uint8_t)(SPR_EN + (i << 2)));
            shown_img[i] = id;
            enemy_image(&en[i], 0, 0, &b);
            shown_base[i] = b;
            shown_sz[i] = (uint8_t)((id - b) % 5);
            n++;
        }
    }
}

static void palette_fx(void)
{
    static const palette_color_t cyc[6] = {
        RGB(31,8,0), RGB(31,31,0), RGB(0,31,16), RGB(0,16,31), RGB(24,0,31), RGB(31,0,8)
    };
    set_sprite_palette_entry(7, 3, cyc[(frame8 >> 2) % 6]);
    if (zap_active) {
        palette_color_t c = (zap_t & 2) ? RGB(31,31,31) : RGB(31,31,0);
        set_bkg_palette_entry(1, 1, c);
        set_bkg_palette_entry(1, 2, c);
        set_bkg_palette_entry(1, 3, c);
    }
}

static void enter_title(void)
{
    uint8_t i;
    HIDE_SPRITES;
    fmv_stop();
    gstate = GS_TITLE; gstate_t = 0;
    pal_set = 0;
    apply_web_palette(0, 8);
    clear_playfield();
    sprites_ok = 0;
    fmv_start(TITLE_FMV_BANK, TITLE_FMV_ADDR);
    fmv_set_loop(TITLE_LOOP_BANK, TITLE_LOOP_ADDR);
    music_play(SONG_TITLE);
    hud_dirty = 1; hud_flush();
    for (i = 0; i < 40; i++) hide_sprite(i);
    SHOW_SPRITES;
    SHOW_BKG;
}

static void show_select(void)
{
    DISPLAY_OFF;
    fmv_stop();
    level_setup(select_level);
    pal_set = (uint8_t)((select_level - 1) & 7);
    apply_web_palette(pal_set, 8);
    clear_playfield();
    load_map_rec(gfx_bank[uweb], gfx_addr[uweb], 1);
    lit_shown = lit_target = 0xFF;
    hud_dirty = 1; hud_flush();
    DISPLAY_ON;
}

static void enter_warp(void)
{
    uint8_t i;
    DISPLAY_OFF;
    fmv_stop();
    for (i = 0; i < 40; i++) hide_sprite(i);
    level_setup(level);
    pal_set = (uint8_t)((level - 1) & 7);
    apply_web_palette(pal_set, 8);
    clear_playfield();
    gfx_b = gfx_bank[uweb];
    gfx_a = gfx_addr[uweb];
    lit_shown = lit_target = 0xFF;
    sprites_ok = 0;
    fade = 8;
    gstate = GS_WARP; gstate_t = 0;
    hud_dirty = 1; hud_flush();
    fmv_start(warp_bank[uweb], warp_addr[uweb]);
    DISPLAY_ON;
    {
        static const uint8_t wt[4] = { SONG_LEVELA, SONG_LEVELB, SONG_LEVELC, SONG_LEVELD };
        music_play(wt[((level - 1) >> 4) & 3]);
    }
    sfx(SFX_WARP);
}

static void enter_play(void)
{
    uint8_t i;
    load_static_sprites();
    for (i = 0; i < MAX_EN; i++) { shown_img[i] = 0xFFFF; shown_base[i] = 0xFFFF; }
    claw_img = 0xFFFF;
    sprites_ok = 1;
    gstate = GS_PLAY; gstate_t = 0;
    game_message("TEMPEST 2000", 60);
}

void main(void)
{
    uint8_t j, btn, press, prev;
    uint8_t blank[16];
    uint8_t i;

    if (_cpu == CGB_TYPE) cpu_fast();

    for (i = 0; i < 16; i++) blank[i] = 0;
    set_bkg_data(0, 1, blank);

    SPRITES_8x16;
    set_sprite_palette(0, 8, spr_pal);
    load_font();
    load_static_sprites();
    game_init();
    sound_init();
    DISPLAY_ON;
    SHOW_BKG;
    SHOW_SPRITES;

    claw_img = 0xFFFF;
    claw_want = 0;
    lit_shown = 0xFF;
    prev = 0;
    enter_title();

    for (;;) {
        j = joypad();
        btn = 0;
        if (j & J_LEFT) btn |= B_LEFT;
        if (j & J_RIGHT) btn |= B_RIGHT;
        if (j & J_UP) btn |= B_UP | B_JUMP;
        if (j & J_DOWN) btn |= B_DOWN;
        if (j & J_A) btn |= B_FIRE;
        if (j & J_B) btn |= B_ZAP;
        if (j & J_START) btn |= B_START;
        press = (uint8_t)(btn & ~prev); prev = btn;
        gstate_t++;

        switch (gstate) {
        case GS_TITLE:
            if ((frame8 & 7) == 0) {
                static const palette_color_t pulse[4] = {
                    RGB(31,18,31), RGB(31,31,31), RGB(18,8,31), RGB(31,24,31)
                };
                set_bkg_palette_entry(1, 3, pulse[(frame8 >> 3) & 3]);
            }
            if (press & (B_FIRE | B_START)) {
                sfx(SFX_MENU);
                gstate = GS_SELECT; gstate_t = 0;
                if (select_level > best_level) select_level = 1;
                show_select();
            }
            frame8++;
            break;
        case GS_SELECT:
            if ((press & B_RIGHT) && select_level + 2 <= best_level) { select_level += 2; sfx(SFX_MENU); show_select(); }
            if ((press & B_LEFT) && select_level > 2) { select_level -= 2; sfx(SFX_MENU); show_select(); }
            if (press & (B_FIRE | B_START)) {
                score_reset(); lives = 3; warp_tokens = 0;
                if (select_level > 1) score_add_start((uint16_t)(select_level - 1) * 1000);
                level = select_level; enter_warp();
            }
            if (press & B_ZAP) enter_title();
            break;
        case GS_WARP:
            if (fmv_state == FMV_DONE) enter_play();
            break;
        case GS_PLAY:
            play_tick(btn, press);
            lit_target = player_lane;
            if (gstate == GS_PLAY && level_cleared() && gstate_t > 60) {
                gstate = GS_ZOOM; gstate_t = 0;
                game_message(level & 1 ? "EXCELLENT!" : "YES YES YES!", 90);
                sfx(SFX_CLEAR);
                kill_all_for_zoom();
                zap_active = 0;
                apply_web_palette(pal_set, 8);
                lit_off_now();
                sprites_ok = 1;
                zoom_cam = 0;
                fmv_start(zoom_bank[uweb], zoom_addr[uweb]);
            }
            if (press & B_START) { gstate = GS_PAUSE; hud_dirty = 1; sound_pause(1); }
            break;
        case GS_PAUSE:
            if (press & (B_START | B_FIRE)) { gstate = GS_PLAY; hud_dirty = 1; sound_pause(0); }
            break;
        case GS_DIED:
            died_tick();
            if (gstate_t > 100) {
                if (lives == 0) {
                    gstate = GS_GAMEOVER; gstate_t = 0; hud_dirty = 1;
                    if (score > hiscore) hiscore = score;
                    music_play(SONG_GAMEOVER);
                } else {
                    player_respawn();
                    gstate = GS_PLAY; gstate_t = 0; hud_dirty = 1;
                }
            }
            break;
        case GS_ZOOM:
            zoom_cam = (int16_t)fmv_aux << 6;
            zoom_tick(btn, press);
            if (fmv_state == FMV_DONE) {
                if (lives == 0) {
                    gstate = GS_GAMEOVER; gstate_t = 0; hud_dirty = 1;
                    if (score > hiscore) hiscore = score;
                    music_play(SONG_GAMEOVER);
                } else {
                    score_add_start((uint16_t)(1000 * (level < 10 ? level : 10)));
                    level = level >= 99 ? 1 : (uint8_t)(level + 1);
                    enter_warp();
                }
            }
            break;
        case GS_GAMEOVER:
            if (gstate_t > 200 || (gstate_t > 60 && (press & (B_FIRE | B_START)))) enter_title();
            break;
        }

        draw_sprites();
        wait_vbl_done();
        if (gstate == GS_TITLE || gstate == GS_WARP || gstate == GS_ZOOM) fmv_tick();
        if (shake) {
            SCX_REG = (uint8_t)((frame8 & 2) ? 1 : 0xFF);
            SCY_REG = (uint8_t)((frame8 & 4) ? 1 : 0);
            shake--;
            if (!shake) { SCX_REG = 0; SCY_REG = 0; }
        }
        if (gstate == GS_PLAY || gstate == GS_DIED || gstate == GS_PAUSE || gstate == GS_GAMEOVER) {
            palette_fx();
            lit_job();
        }
        hud_flush();
        image_jobs();
    }
}
