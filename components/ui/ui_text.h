/**
 * @file ui_text.h
 * @brief All text shown on the display, in the language picked in menuconfig
 *        (orbi -> Display language).
 *
 * The pixel font is ASCII only, so German umlauts are written out (ae, oe, ue).
 */

#pragma once

#include "sdkconfig.h"  // IWYU pragma: keep (CONFIG_ORBI_UI_LANGUAGE_*)

#if defined(CONFIG_ORBI_UI_LANGUAGE_EN)

#define TEXT_ROOM_LIVING "Living room"
#define TEXT_ROOM_DINING "Dining room"
#define TEXT_ROOM_BEDROOM "Bedroom"
#define TEXT_ROOM_GARDEN "Garden"

#define TEXT_MOOD_HAPPY "happy"
#define TEXT_MOOD_HUNGRY "hungry"
#define TEXT_MOOD_TIRED "tired"
#define TEXT_MOOD_SAD "sad"
#define TEXT_SLEEPING "asleep"

/** Pet age, takes hours and minutes. */
#define TEXT_AGE_FMT "Age: %dh %02dm"

#else

#define TEXT_ROOM_LIVING "Wohnzimmer"
#define TEXT_ROOM_DINING "Esszimmer"
#define TEXT_ROOM_BEDROOM "Schlafzimmer"
#define TEXT_ROOM_GARDEN "Garten"

#define TEXT_MOOD_HAPPY "froehlich"
#define TEXT_MOOD_HUNGRY "hungrig"
#define TEXT_MOOD_TIRED "muede"
#define TEXT_MOOD_SAD "traurig"
#define TEXT_SLEEPING "schlaeft"

/** Pet age, takes hours and minutes. */
#define TEXT_AGE_FMT "Alter: %dh %02dm"

#endif
