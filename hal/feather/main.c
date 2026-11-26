#include "beeper.h"
#include "feather_hal.h"
#include "feather_board.h"

static beeper_t beeper;
static hal_t hal;

int main(void) {
  feather_hal_init(&hal);
  beeper_init(&beeper);
  for (;;) {
    board_pump();
    beeper_tick(&beeper, &hal);
  }
}
