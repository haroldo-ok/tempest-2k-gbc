#ifndef FMV_H
#define FMV_H
#include <stdint.h>
#include <gb/gb.h>

enum { FMV_IDLE = 0, FMV_RUN, FMV_DONE };

extern uint8_t fmv_state, fmv_aux;

void fmv_start(uint8_t bank, uint16_t addr) NONBANKED;
void fmv_set_loop(uint8_t bank, uint16_t addr) NONBANKED;
void fmv_stop(void) NONBANKED;
void fmv_tick(void) NONBANKED;

#endif
