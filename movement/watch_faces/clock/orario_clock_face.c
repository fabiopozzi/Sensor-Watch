/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include "orario_clock_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_private_display.h"

void orario_clock_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr) {
    (void) settings;
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(orario_clock_state_t));
        orario_clock_state_t *state = (orario_clock_state_t *)*context_ptr;
        state->signal_enabled = false;
        state->watch_face_index = watch_face_index;
    }
}

void orario_clock_face_activate(movement_settings_t *settings, void *context) {
    (void) settings;
    orario_clock_state_t *state = (orario_clock_state_t *)context;

    if (watch_tick_animation_is_running()) watch_stop_tick_animation();

    // this ensures that none of the timestamp fields will match, so we can re-render them all.
    state->previous_date_time = 0xFFFFFFFF;
}

static int orario_clock_get_weekday(watch_date_time date_time) {
    return watch_utility_get_iso8601_weekday_number(date_time.unit.year + WATCH_RTC_REFERENCE_YEAR, date_time.unit.month, date_time.unit.day) - 1;
}

bool orario_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    (void) settings;
    orario_clock_state_t *state = (orario_clock_state_t *)context;
    char buf[11];
    uint8_t pos;
    // 1 = 3DIN   2 = 3GIN   3 = 4DIN  0 = NO LEZIONE
    char nome_classe[4][5] = {"NL", "3D", "3G", "4D"};

    int orario[6][5] = {
      { 3, 2, 0, 0, 0 },
      { 3, 2, 0, 0, 0 },
      { 2, 0, 1, 1, 0 },
      { 0, 3, 1, 1, 2 },
      { 0, 1, 0, 2, 3 },
      { 0, 1, 3, 2, 3 }};

    watch_date_time date_time;
    uint32_t previous_date_time;
    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            date_time = watch_rtc_get_date_time();
            previous_date_time = state->previous_date_time;
            state->previous_date_time = date_time.reg;
	    pos = 0;

            if ((date_time.reg >> 6) == (previous_date_time >> 6) && event.event_type != EVENT_LOW_ENERGY_UPDATE) {
                // everything before seconds is the same, don't waste cycles setting those segments.
		break;
            } else if ((date_time.reg >> 12) == (previous_date_time >> 12) && event.event_type != EVENT_LOW_ENERGY_UPDATE) {
		break;
            } else {
		    uint8_t ora_lezione = date_time.unit.hour - 8;
		    uint8_t giorno = orario_clock_get_weekday(date_time);
		    uint8_t classe = orario[ora_lezione][giorno];

		    if ((ora_lezione > 0) && (ora_lezione < 7)) {
			sprintf(buf, "%s  %s%02d", nome_classe[classe], nome_classe[classe], ora_lezione);
		    }
		    else {
			sprintf(buf, "%s  %s%02d", nome_classe[classe], nome_classe[classe], ora_lezione);
		    }
            }
            watch_display_string(buf, pos);
            break;
        case EVENT_MODE_BUTTON_UP:
            movement_move_to_next_face();
            return false;
        case EVENT_LIGHT_BUTTON_DOWN:
            movement_illuminate_led();
            break;
        case EVENT_ALARM_LONG_PRESS:
            break;
        case EVENT_BACKGROUND_TASK:
            // uncomment this line to snap back to the clock face when the hour signal sounds:
            // movement_move_to_face(state->watch_face_index);
            if (watch_is_buzzer_or_led_enabled()) {
                // if we are in the foreground, we can just beep.
                movement_play_signal();
            } else {
                // if we were in the background, we need to enable the buzzer peripheral first,
                watch_enable_buzzer();
                // beep quickly (this call blocks for 275 ms),
                movement_play_signal();
                // and then turn the buzzer peripheral off again.
                watch_disable_buzzer();
            }
            break;
        default:
            break;
    }

    return true;
}

void orario_clock_face_resign(movement_settings_t *settings, void *context) {
    (void) settings;
    (void) context;
}

bool orario_clock_face_wants_background_task(movement_settings_t *settings, void *context) {
    (void) settings;
    orario_clock_state_t *state = (orario_clock_state_t *)context;
    if (!state->signal_enabled) return false;

    watch_date_time date_time = watch_rtc_get_date_time();

    return date_time.unit.minute == 0;
}
