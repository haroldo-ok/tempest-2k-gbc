/* Pre-rendered FMV player (warp-in, zoom-out, title loop).
 * Stream: flags, hold, aux, nup, nup*{slot, tid_lo, tid_hi}, u16 nruns,
 * nruns*{cell, len, len*{tile, attr}}.  0xFF at a step start = next bank.
 * flags bit0 = visible frame, bit1 = last step. */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "fmv.h"
#include "assets.h"

uint8_t fmv_state, fmv_aux;

static uint8_t fmv_bank, loop_bank;
static uint8_t flags, hold, wait, nup;
static const uint8_t *fmv_p, *loop_p;

static void next_bank(void)
{
    fmv_bank++;
    SWITCH_ROM_MBC5(fmv_bank);
    fmv_p = (const uint8_t *)0x4000;
}

static void map_stream(void)
{
    SWITCH_ROM_MBC5(fmv_bank);
    if (*fmv_p == 0xFF) next_bank();
}

void fmv_start(uint8_t bank, uint16_t addr) NONBANKED
{
    fmv_bank = bank;
    fmv_p = (const uint8_t *)addr;
    loop_bank = 0;
    wait = 0;
    fmv_aux = 0;
    fmv_state = FMV_RUN;
}

void fmv_set_loop(uint8_t bank, uint16_t addr) NONBANKED
{
    loop_bank = bank;
    loop_p = (const uint8_t *)addr;
}

void fmv_stop(void) NONBANKED { fmv_state = FMV_IDLE; }

void fmv_tick(void) NONBANKED
{
    uint8_t saved, i, len, x, y;
    uint16_t nruns, tid;
    uint8_t slot, db;
    uint8_t tiles[20], attrs[20];

    if (fmv_state != FMV_RUN) return;
    if (wait) { wait--; return; }

    saved = CURRENT_BANK;
    map_stream();

    flags = fmv_p[0];
    hold = fmv_p[1];
    fmv_aux = fmv_p[2];
    nup = fmv_p[3];
    fmv_p += 4;

    for (i = 0; i < nup; i++) {
        slot = fmv_p[0];
        db = fmv_p[1]; /* VRAM bank 0/1 */
        tid = fmv_p[2] | ((uint16_t)fmv_p[3] << 8);
        fmv_p += 4;
        SWITCH_ROM_MBC5((uint8_t)(DICT_BANK0 + (tid >> 10)));
        VBK_REG = db;
        set_bkg_data(slot, 1, (const uint8_t *)(0x4000u + ((uint16_t)(tid & 1023) << 4)));
        VBK_REG = 0;
        SWITCH_ROM_MBC5(fmv_bank);
    }

    nruns = fmv_p[0] | ((uint16_t)fmv_p[1] << 8);
    fmv_p += 2;
    while (nruns--) {
        x = fmv_p[0];
        y = fmv_p[1];
        len = fmv_p[2];
        fmv_p += 3;
        for (i = 0; i < len; i++) {
            tiles[i] = fmv_p[0];
            attrs[i] = fmv_p[1];
            fmv_p += 2;
        }
        if (len) {
            set_bkg_tiles(x, y, len, 1, tiles);
            set_bkg_attributes(x, y, len, 1, attrs);
        }
    }

    if (flags & 1) wait = hold;
    if (flags & 2) {
        if (loop_bank) {
            fmv_bank = loop_bank;
            fmv_p = loop_p;
        } else {
            fmv_state = FMV_DONE;
        }
    }
    SWITCH_ROM_MBC5(saved);
}
