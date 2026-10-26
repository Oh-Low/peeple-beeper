#pragma once

#include <stdint.h>

/* 1-bit pictures. A set bit is ink (1) and a clear bit is paper (0).
   Bit 0 is the top pixel of that 8-row page, same as the 5x7 glyphs.
   The byte index is page * width + x, which display_blit_sprite reads. */

#define ART_LEFT_W 28
#define ART_LEFT_H 30
#define ART_RIGHT_W 30
#define ART_RIGHT_H 30
#define ART_OCTOHAT_W 64
#define ART_OCTOHAT_H 64
#define ART_CAMNON_W 64
#define ART_CAMNON_H 64
#define ART_VIBEY_EYE_W 64
#define ART_VIBEY_EYE_H 64
#define ART_VOIDFARERS_W 64
#define ART_VOIDFARERS_H 64
#define ART_ALLEYCAT_W 64
#define ART_ALLEYCAT_H 64

#define ART_LEFT_LEN (ART_LEFT_W * ((ART_LEFT_H + 7) / 8))
#define ART_RIGHT_LEN (ART_RIGHT_W * ((ART_RIGHT_H + 7) / 8))
#define ART_OCTOHAT_LEN (ART_OCTOHAT_W * ((ART_OCTOHAT_H + 7) / 8))
#define ART_CAMNON_LEN (ART_CAMNON_W * ((ART_CAMNON_H + 7) / 8))
#define ART_VIBEY_EYE_LEN (ART_VIBEY_EYE_W * ((ART_VIBEY_EYE_H + 7) / 8))
#define ART_VOIDFARERS_LEN (ART_VOIDFARERS_W * ((ART_VOIDFARERS_H + 7) / 8))
#define ART_ALLEYCAT_LEN (ART_ALLEYCAT_W * ((ART_ALLEYCAT_H + 7) / 8))

#define ART_BEEP_W 57
#define ART_BEEP_H 53
#define ART_BEEP_LEN (ART_BEEP_W * ((ART_BEEP_H + 7) / 8))
#define ART_SPIDER_W 62
#define ART_SPIDER_H 47
#define ART_SPIDER_LEN (ART_SPIDER_W * ((ART_SPIDER_H + 7) / 8))
#define ART_ARROW_UP_W 17
#define ART_ARROW_UP_H 23
#define ART_ARROW_UP_LEN (ART_ARROW_UP_W * ((ART_ARROW_UP_H + 7) / 8))
#define ART_ARROW_RIGHT_W 21
#define ART_ARROW_RIGHT_H 19
#define ART_ARROW_RIGHT_LEN (ART_ARROW_RIGHT_W * ((ART_ARROW_RIGHT_H + 7) / 8))
#define ART_ARROW_UP_RIGHT_W 17
#define ART_ARROW_UP_RIGHT_H 19
#define ART_ARROW_UP_RIGHT_LEN (ART_ARROW_UP_RIGHT_W * ((ART_ARROW_UP_RIGHT_H + 7) / 8))
#define ART_ARROW_DOWN_RIGHT_W 17
#define ART_ARROW_DOWN_RIGHT_H 19
#define ART_ARROW_DOWN_RIGHT_LEN (ART_ARROW_DOWN_RIGHT_W * ((ART_ARROW_DOWN_RIGHT_H + 7) / 8))
#define ART_BEEPBOP_W 64
#define ART_BEEPBOP_H 64
#define ART_BEEPBOP_LEN (ART_BEEPBOP_W * ((ART_BEEPBOP_H + 7) / 8))
#define ART_DINO_W 64
#define ART_DINO_H 64
#define ART_DINO_LEN (ART_DINO_W * ((ART_DINO_H + 7) / 8))
#define ART_MIDGE_W 64
#define ART_MIDGE_H 64
#define ART_MIDGE_LEN (ART_MIDGE_W * ((ART_MIDGE_H + 7) / 8))
#define ART_OSCAR_W 64
#define ART_OSCAR_H 64
#define ART_OSCAR_LEN (ART_OSCAR_W * ((ART_OSCAR_H + 7) / 8))
#define ART_ROBOT_W 64
#define ART_ROBOT_H 64
#define ART_ROBOT_LEN (ART_ROBOT_W * ((ART_ROBOT_H + 7) / 8))
#define ART_SPOODER_W 64
#define ART_SPOODER_H 64
#define ART_SPOODER_LEN (ART_SPOODER_W * ((ART_SPOODER_H + 7) / 8))
#define ART_TEEF_W 64
#define ART_TEEF_H 64
#define ART_TEEF_LEN (ART_TEEF_W * ((ART_TEEF_H + 7) / 8))
extern const uint8_t art_left[ART_LEFT_LEN];
extern const uint8_t art_right[ART_RIGHT_LEN];
extern const uint8_t art_octohat[ART_OCTOHAT_LEN];
extern const uint8_t art_camnon[ART_CAMNON_LEN];
extern const uint8_t art_vibey_eye[ART_VIBEY_EYE_LEN];
extern const uint8_t art_voidfarers[ART_VOIDFARERS_LEN];
extern const uint8_t art_alleycat[ART_ALLEYCAT_LEN];
extern const uint8_t art_beep[ART_BEEP_LEN];
extern const uint8_t art_spider[ART_SPIDER_LEN];
extern const uint8_t art_arrow_up[ART_ARROW_UP_LEN];
extern const uint8_t art_arrow_right[ART_ARROW_RIGHT_LEN];
extern const uint8_t art_arrow_up_right[ART_ARROW_UP_RIGHT_LEN];
extern const uint8_t art_arrow_down_right[ART_ARROW_DOWN_RIGHT_LEN];
extern const uint8_t art_beepbop[ART_BEEPBOP_LEN];
extern const uint8_t art_dino[ART_DINO_LEN];
extern const uint8_t art_midge[ART_MIDGE_LEN];
extern const uint8_t art_oscar[ART_OSCAR_LEN];
extern const uint8_t art_robot[ART_ROBOT_LEN];
extern const uint8_t art_spooder[ART_SPOODER_LEN];
extern const uint8_t art_teef[ART_TEEF_LEN];
