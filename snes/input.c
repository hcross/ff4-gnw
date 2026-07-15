
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "input.h"
#include "snes.h"
#include "statehandler.h"

Input* input_init(Snes* snes) {
  #ifdef FF4_PORT_STATIC_SNES
  /* TWO instances -- snes_init creates one Input per controller port. A
   * single static here aliases pad 2 onto pad 1 (auto-joypad clobber,
   * found by FBCRC bisect 2026-07-14); rotate across the exactly-two
   * init calls of the singleton contract. */
  static Input _ff4_input_storage[2];
  static int _ff4_input_next = 0;
  Input* input = &_ff4_input_storage[_ff4_input_next++ & 1];
#else
  Input* input = malloc(sizeof(Input));
#endif
  input->snes = snes;
  // TODO: handle (where?)
  input->type = 1;
  input->currentState = 0;
  // TODO: handle I/O line (and latching of PPU)
  return input;
}

void input_free(Input* input) {
#ifndef FF4_PORT_STATIC_SNES
  free(input);
#else
  (void)input;  /* static storage (see input_init) -- freeing it aborts */
#endif
}

void input_reset(Input* input) {
  input->latchLine = false;
  input->latchedState = 0;
}

void input_handleState(Input* input, StateHandler* sh) {
  // TODO: handle types (switch type on state load?)
  sh_handleBytes(sh, &input->type, NULL);
  sh_handleBools(sh, &input->latchLine, NULL);
  sh_handleWords(sh, &input->currentState, &input->latchedState, NULL);
}

void input_latch(Input* input, bool value) {
  input->latchLine = value;
  if(input->latchLine) input->latchedState = input->currentState;
}

uint8_t input_read(Input* input) {
  if(input->latchLine) input->latchedState = input->currentState;
  uint8_t ret = input->latchedState & 1;
  input->latchedState >>= 1;
  input->latchedState |= 0x8000;
  return ret;
}
