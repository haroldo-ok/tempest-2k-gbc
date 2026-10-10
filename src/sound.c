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
#include "tunes.h"

/* C2 .. D#7  (64 notes). gb freq = 2048 - 131072/hz */
static const uint16_t nfreq[64] = {
    44,157,263,363,457,547,631,710,786,856,923,986,
    1046,1102,1155,1205,1253,1297,1339,1379,1417,1452,1486,1517,
    1547,1575,1602,1627,1650,1673,1694,1714,1732,1750,1767,1783,
    1798,1812,1825,1837,1849,1860,1871,1881,1890,1899,1907,1915,
    1923,1930,1936,1943,1949,1954,1959,1964,1969,1974,1978,1982,
    1985,1989,1992,1995
};

static uint8_t song, wait, paused;
static uint16_t row;
static uint8_t s_bank, s_tempo;
static uint16_t s_addr, s_rows;
static uint8_t sfx_t, sfx_id;
static uint8_t sfx_pri;

/* Rounded bass wave for CH3. */
static const uint8_t wave_sq[16] = {
    0x89,0xBC,0xDE,0xFF, 0xFF,0xED,0xCB,0x98,
    0x76,0x43,0x21,0x00, 0x00,0x12,0x34,0x67
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

/* CH1 pulse: melody (cuts through noise). 0 = mute, 255 = hold. */
static void lead_pulse(uint8_t note)
{
    uint16_t f;
    if (note == 0) { NR12_REG = 0; return; }
    if (note == 255 || note > 63) return;
    f = nfreq[note];
    NR10_REG = 0;
    NR11_REG = 0x80;            /* 50% duty */
    NR12_REG = 0xD0;            /* sustain, vol 13 */
    NR13_REG = (uint8_t)f;
    NR14_REG = 0x80 | (uint8_t)(f >> 8);
}

/* CH3 wave: bass, quieter than the pulse lead. */
static void bass_wave(uint8_t note)
{
    uint16_t f;
    if (note == 0) { NR32_REG = 0; return; }
    if (note == 255 || note > 63) return;
    f = nfreq[note];
    NR32_REG = 0x40; /* 50% */
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
    if (!d || d == 3) return; /* hats off — they bury the melody */
    NR41_REG = 0x3F;
    if (d == 1) { NR42_REG = 0x52; NR43_REG = 0x70; } /* kick, quiet */
    else { NR42_REG = 0x62; NR43_REG = 0x24; }        /* snare, quiet */
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
    if (s >= NSONGS) { music_stop(); return; }
    if (s == song) return; /* same track: keep playing across warp / next web */
    CRITICAL {
        song = s;
        row = 0;
        wait = 0;
        s_bank = tune_bank[s];
        s_addr = tune_addr[s];
        s_rows = tune_rows[s];
        s_tempo = tune_tempo[s];
    }
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

static void fetch_row(uint8_t *out)
{
    uint8_t b = s_bank, saved, i;
    uint32_t o = (uint32_t)s_addr + (uint32_t)row * 3;
    while (o >= 0x8000u) { o -= 0x4000u; b++; }
    saved = CURRENT_BANK;
    for (i = 0; i < 3; i++) {
        if (o >= 0x8000u) { o -= 0x4000u; b++; }
        SWITCH_ROM_MBC5(b);
        out[i] = *(const uint8_t *)(uint16_t)o;
        o++;
    }
    SWITCH_ROM_MBC5(saved);
}

/* VBlank ISR body (installed via add_VBL). Do not tag CRITICAL/INTERRUPT. */
static void sound_vbl(void) NONBANKED
{
    uint8_t p[3];
    if (paused) return;
    sfx_run();
    if (song == SONG_OFF) return;
    if (wait) { wait--; return; }
    wait = s_tempo;
    fetch_row(p);
    bass_wave(p[0]);
    lead_pulse(p[1]);
    if (!sfx_uses_noise()) drum(p[2]);
    row++;
    if (row >= s_rows) row = 0;
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
