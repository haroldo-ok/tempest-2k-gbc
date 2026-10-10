/* GBC sound. Sequencer runs from VBlank so tempo stays steady even when
 * a frame overruns (FMV, sprite uploads).
 *
 * Channels never share a job:
 *   CH1 pulse  — music bass
 *   CH2 pulse  — sound effects only
 *   CH3 wave   — music lead
 *   CH4 noise  — music drums, stolen only for explode/die/zap
 */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "sound.h"

/* C2 .. D#7  (64 notes). gb freq = 2048 - 131072/hz */
static const uint16_t nfreq[64] = {
    44,157,263,363,457,547,631,710,786,856,923,986,
    1046,1102,1155,1205,1253,1297,1339,1379,1417,1452,1486,1517,
    1547,1575,1602,1627,1650,1673,1694,1714,1732,1750,1767,1783,
    1798,1812,1825,1837,1849,1860,1871,1881,1890,1899,1907,1915,
    1923,1930,1936,1943,1949,1954,1959,1964,1969,1974,1978,1982,
    1985,1989,1992,1995
};

static uint8_t song, row, wait, tempo, paused;
static uint8_t sfx_t, sfx_id;
static uint8_t sfx_pri;

/* Songs: tempo, length, loop, then interleaved bass, lead, drum per row.
   Note 0 = off, 1-63 = nfreq index, 255 = hold.
   Drum: 0 none, 1 kick, 2 snare, 3 hat, 4 open. */
#define N_TITLE 32
#define N_LVLA  32
#define N_LVLB  32
#define N_LVLC  32
#define N_OVER  16
#define N_WARP  16

/* packed as bass, lead, drum */
static const uint8_t pat_title[] = {
    9,40,1,  0,255,0,  21,36,3, 0,255,0,  9,43,1,  0,255,0,  21,40,3, 0,38,0,
    5,41,2,  0,255,0,  17,38,3, 0,255,0,  5,45,1,  0,255,0,  17,43,3, 0,40,4,
    12,42,1, 0,255,0,  24,40,3, 0,255,0,  12,47,1, 0,255,0,  24,45,3, 0,43,0,
    7,38,2,  0,255,0,  19,36,3, 0,255,0,  7,40,1,  0,42,0,   19,38,3, 0,36,4,
    9,45,1,  0,43,0,   21,40,3, 0,38,0,   9,48,1,  0,47,0,   21,43,3, 0,40,0,
    5,46,2,  0,255,0,  17,43,3, 0,40,0,   5,41,1,  0,43,0,   17,46,3, 0,255,4,
    7,47,1,  0,46,0,   19,43,3, 0,38,0,   7,36,1,  0,38,0,   19,43,3, 0,46,0,
    4,48,2,  0,255,0,  16,255,3,0,255,0,  4,40,1,  0,255,0,  16,36,2, 0,32,4
};
static const uint8_t pat_lvla[] = {
    14,38,1, 0,255,0,  14,33,3, 26,255,0, 14,41,1, 0,40,3,  14,38,1, 26,36,3,
    10,38,1, 0,255,0,  10,255,3,22,41,0,  10,255,1, 0,38,3,  10,34,0, 0,255,4,
    12,40,1, 0,255,0,  12,36,3, 24,43,0, 12,255,1, 0,40,3,  12,36,1, 24,38,3,
    9,40,1,  0,255,0,  9,255,3,  21,33,0, 9,36,1,   0,40,3,  9,45,0,  0,255,4
};
static const uint8_t pat_lvlb[] = {
    16,35,1, 0,40,0,  11,43,3, 0,255,0,  16,42,1, 0,40,3,  11,38,0, 0,35,0,
    12,36,1, 0,255,0,  19,40,3, 0,43,0,  12,255,1, 0,40,3,  19,36,0, 0,255,4,
    14,38,1, 0,42,0,  21,45,3, 0,255,0,  14,43,1, 0,42,3,  21,40,0, 0,38,0,
    11,42,1, 0,255,0,  18,255,3,0,38,0,  11,35,1,  0,255,3, 18,30,2, 0,255,4
};
static const uint8_t pat_lvlc[] = {
    19,31,1, 0,34,0,  14,38,3, 0,43,0,  19,41,1, 0,38,3,  14,34,0, 0,38,0,
    15,39,1, 0,255,0, 19,43,3, 0,255,0, 15,46,1, 0,255,3, 19,43,0, 0,39,4,
    17,41,1, 0,255,0, 12,36,3, 0,33,0,  17,36,1, 0,41,3,  12,45,0, 0,41,0,
    18,42,1, 0,255,0, 0,255,3,  0,45,0,  18,42,1, 0,38,2,  14,255,4,0,255,0
};
static const uint8_t pat_over[] = {
    9,40,1, 0,255,0, 0,255,0, 0,36,0,  0,255,0, 0,255,0, 0,33,0, 0,255,0,
    0,28,0, 0,255,0, 0,255,0, 0,255,0,  5,32,0, 0,255,0, 0,255,0, 0,33,2
};
static const uint8_t pat_warp[] = {
    9,45,1, 21,47,0, 9,48,3, 21,52,0,  9,45,1, 16,47,0, 9,48,3, 21,45,4,
    12,43,1,24,42,0, 12,40,3,19,38,0,  9,36,1,  0,33,0,  9,40,2, 0,45,4
};

typedef struct { const uint8_t *p; uint8_t n, tempo, loop; } song_t;
static const song_t songs[6] = {
    { pat_title, N_TITLE, 7, 0 },
    { pat_lvla,  N_LVLA,  5, 0 },
    { pat_lvlb,  N_LVLB,  5, 0 },
    { pat_lvlc,  N_LVLC,  5, 0 },
    { pat_over,  N_OVER,  9, 255 },
    { pat_warp,  N_WARP,  4, 0 },
};

/* 50% square in CH3 wave RAM — close to the old pulse lead. */
static const uint8_t wave_sq[16] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

static void wave_init(void)
{
    uint8_t i;
    NR30_REG = 0;
    for (i = 0; i < 16; i++) ((volatile uint8_t *)0xFF30)[i] = wave_sq[i];
    NR30_REG = 0x80;
    NR31_REG = 0xFF;
    NR32_REG = 0;
}

/* CH1: music bass. note 0 = mute, 255 = hold. */
static void bass(uint8_t note, uint8_t vol)
{
    uint16_t f;
    if (note == 0) { NR12_REG = 0; return; }
    if (note == 255 || note > 63) return;
    f = nfreq[note];
    NR10_REG = 0;
    NR11_REG = 0xBF; /* 75% duty */
    NR12_REG = (vol << 4) | 0x02;
    NR13_REG = (uint8_t)f;
    NR14_REG = 0x80 | (uint8_t)(f >> 8);
}

/* CH3: music lead. */
static void lead(uint8_t note)
{
    uint16_t f;
    if (note == 0) { NR32_REG = 0; return; }
    if (note == 255 || note > 63) return;
    f = nfreq[note];
    NR32_REG = 0x20; /* 100% */
    NR33_REG = (uint8_t)f;
    NR34_REG = 0x80 | (uint8_t)(f >> 8);
}

/* CH2: SFX only. */
static void sfx_pulse(uint8_t note, uint8_t vol, uint8_t duty)
{
    uint16_t f;
    if (note == 0 || note > 63) { NR22_REG = 0; return; }
    f = nfreq[note];
    NR21_REG = (duty << 6) | 0x3F;
    NR22_REG = (vol << 4) | 0x02;
    NR23_REG = (uint8_t)f;
    NR24_REG = 0x80 | (uint8_t)(f >> 8);
}

static void drum(uint8_t d)
{
    if (!d) return;
    NR41_REG = 0x3F;
    if (d == 1) { NR42_REG = 0xA1; NR43_REG = 0x74; }      /* kick */
    else if (d == 2) { NR42_REG = 0xC1; NR43_REG = 0x24; } /* snare */
    else if (d == 3) { NR42_REG = 0x61; NR43_REG = 0x11; } /* hat */
    else { NR42_REG = 0x81; NR43_REG = 0x13; }             /* open */
    NR44_REG = 0x80;
}

static uint8_t sfx_uses_noise(void)
{
    return (uint8_t)(sfx_t && (sfx_id == SFX_EXPLODE || sfx_id == SFX_DIE || sfx_id == SFX_ZAP));
}

static const uint8_t sfx_pri_tbl[SFX_COUNT] = { 2,2,4,1,5,5,3,4,2,3,1,3,4,3 };

void sfx(uint8_t id) NONBANKED
{
    if (id >= SFX_COUNT) return;
    CRITICAL {
        if (!(sfx_t && sfx_pri_tbl[id] < sfx_pri)) {
            sfx_id = id; sfx_t = 12; sfx_pri = sfx_pri_tbl[id];
        }
    }
}

static void sfx_run(void)
{
    uint8_t t;
    if (!sfx_t) return;
    t = 12 - sfx_t;
    sfx_t--;
    switch (sfx_id) {
    case SFX_SHOT:   sfx_pulse(50 - t, 8, 2); break;
    case SFX_LASER:  sfx_pulse(55 - t, 10, 1); break;
    case SFX_EXPLODE: if (t == 0) { NR41_REG=0x3F; NR42_REG=0xF3; NR43_REG=0x57; NR44_REG=0x80; } break;
    case SFX_FLIP:   sfx_pulse(40 + t, 6, 1); break;
    case SFX_DIE:    sfx_pulse(36 - t, 12, 2); if (t == 0) { NR41_REG=0x3F; NR42_REG=0xF4; NR43_REG=0x44; NR44_REG=0x80; } break;
    case SFX_ZAP:    if ((t & 1) == 0) { NR41_REG=0x3F; NR42_REG=0xF1; NR43_REG=0x14 + t; NR44_REG=0x80; } sfx_pulse(48 + (t & 3), 10, 0); break;
    case SFX_POWERUP: sfx_pulse(40 + t * 2, 10, 2); break;
    case SFX_CLEAR:  sfx_pulse(45 + t, 10, 2); break;
    case SFX_SPIKE:  sfx_pulse(30, 6, 1); break;
    case SFX_JUMP:   sfx_pulse(36 + t * 2, 8, 2); break;
    case SFX_MENU:   sfx_pulse(48, 8, 2); break;
    case SFX_WARP:   sfx_pulse(52 + t, 8, 1); break;
    case SFX_ONEUP:  sfx_pulse(48 + (t > 6 ? 4 : 0), 12, 2); break;
    case SFX_SPLIT:  sfx_pulse(42, 7, 1); break;
    }
    if (!sfx_t) { sfx_pri = 0; NR22_REG = 0; }
}

void music_play(uint8_t s) NONBANKED
{
    CRITICAL { song = s; row = 0; wait = 0; }
}
void music_stop(void) NONBANKED
{
    CRITICAL { song = SONG_OFF; }
    NR12_REG = 0; NR32_REG = 0;
}
void sound_pause(uint8_t on) NONBANKED
{
    CRITICAL { paused = on; }
    if (on) { NR12_REG = 0; NR32_REG = 0; NR42_REG = 0; }
}

/* VBlank ISR body (installed via add_VBL). Do not tag CRITICAL/INTERRUPT. */
static void sound_vbl(void) NONBANKED
{
    const song_t *s;
    const uint8_t *p;
    if (paused) return;
    sfx_run();
    if (song == SONG_OFF) return;
    s = &songs[song];
    if (wait) { wait--; return; }
    wait = s->tempo;
    p = s->p + (uint16_t)row * 3;
    bass(p[0], 9);
    lead(p[1]);
    if (!sfx_uses_noise()) drum(p[2]);
    row++;
    if (row >= s->n) {
        if (s->loop == 255) { song = SONG_OFF; return; }
        row = s->loop;
    }
}

void sound_init(void) NONBANKED
{
    NR52_REG = 0x80;
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
    wave_init();
    song = SONG_OFF;
    paused = 0;
    sfx_t = 0;
    CRITICAL { add_VBL(sound_vbl); }
}
