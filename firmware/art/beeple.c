#include "art.h"
#include "beeple.h"

static const beeple_def_t BEEPLES[] = {
    {"Voidfarer", art_voidfarers, ART_VOIDFARERS_W, ART_VOIDFARERS_H},
    {"Octohat", art_octohat, ART_OCTOHAT_W, ART_OCTOHAT_H},
    {"Camnon", art_camnon, ART_CAMNON_W, ART_CAMNON_H},
    {"Vibey Eye", art_vibey_eye, ART_VIBEY_EYE_W, ART_VIBEY_EYE_H},
    {"Alleycat", art_alleycat, ART_ALLEYCAT_W, ART_ALLEYCAT_H},
    {"Beep Bop", art_beepbop, ART_BEEPBOP_W, ART_BEEPBOP_H},
    {"Dino", art_dino, ART_DINO_W, ART_DINO_H},
    {"Midge", art_midge, ART_MIDGE_W, ART_MIDGE_H},
    {"Oscar", art_oscar, ART_OSCAR_W, ART_OSCAR_H},
    {"Robort v2", art_robot, ART_ROBOT_W, ART_ROBOT_H},
    {"Spooder", art_spooder, ART_SPOODER_W, ART_SPOODER_H},
    {"Teef", art_teef, ART_TEEF_W, ART_TEEF_H},
};
_Static_assert(sizeof BEEPLES / sizeof BEEPLES[0] == BEEPLE_N, "beeple table");

const beeple_def_t *beeple_at(int id) {
  if (id < 0 || id >= BEEPLE_N) return 0;
  return &BEEPLES[id];
}
