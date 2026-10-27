#include "beeper.h"
#include "beeper_hal.h"
#include "board.h"

static beeper_t beeper;
static hal_t hal;

int main(void) {
  beeper_hal_init(&hal);
  beeper_init(&beeper);
  for (;;) {
    board_pump();
    beeper_tick(&beeper, &hal);
  }
}
