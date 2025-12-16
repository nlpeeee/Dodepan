#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "config.h"
#include "ssd1306.h"        // https://github.com/TuriSc/pico-ssd1306
#include "state.h"
#include "looper.h"
#include "display.h"
#include "i2c_mutex.h"

// Include assets
#include "display_fonts.h"
#include "display_strings.h"
#include "intro.h"
#include "icon_contrast_max.h"
#include "icon_contrast_med.h"
#include "icon_contrast_min.h"
#include "icon_imu_off.h"
#include "icon_imu_on_x.h"
#include "icon_imu_on_y.h"
#include "icon_imu_on_x_y.h"
#include "icon_close.h"
#include "icon_looper.h"
#include "icon_rec.h"
#include "icon_play.h"
#include "icon_ready.h"
#include "icon_pause.h"
// #include "icon_parameters.h" // Unused asset
#include "icon_low_batt.h"

#define ICON_CENTERED_MARGIN_X ((SSD1306_WIDTH / 2) - (32 / 2))

static alarm_id_t display_dim_alarm_id;
static volatile bool display_pending = false;  // Flag for deferred display updates

void display_init(ssd1306_t *p) {
    p->external_vcc=false;
    i2c1_mutex_enter();
    ssd1306_init(p, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_ADDRESS, SSD1306_I2C_PORT);
#if defined (SSD1306_ROTATE)
    ssd1306_rotate(p, 1);
#endif
    ssd1306_clear(p);
    ssd1306_show(p);
    i2c1_mutex_exit();
}

static inline void draw_info_screen(ssd1306_t *p) {
    uint8_t baseline = 0;
    char version_number[18];
    snprintf(version_number, sizeof(version_number), "%s%s", string_version, PROGRAM_VERSION);
    ssd1306_draw_string_with_font(p, 8, baseline, 1, spaced_font, version_number);
    baseline += 14;

    // Split the program URL into multiple lines
    uint8_t len = strlen(PROGRAM_URL);
    uint8_t max_chars = 17;
    uint8_t num_substr = (len + max_chars -1) / max_chars; // Calculate the number of substrings
    char **substrings = malloc(num_substr * sizeof(char *));
    uint8_t s = 0;

    for (uint8_t i = 0; i < len; i += max_chars) {
        uint8_t substr_len = (i + max_chars <= len) ? max_chars : len - i;
        substrings[s] = malloc((substr_len + 1) * sizeof(char));
        strncpy(substrings[s], PROGRAM_URL + i, substr_len);
        substrings[s][substr_len] = '\0'; // Terminate the substring
        s++;
    }

    // Print the lines to screen
    for (uint8_t i = 0; i < num_substr; i++) {
        ssd1306_draw_string_with_font(p, 8, baseline, 1, spaced_font, substrings[i]);
        baseline += 10;
        free(substrings[i]);
    }
    free(substrings);
}

static inline void draw_looper_screen(ssd1306_t *p) {
    // Draw only the looper icon if in page selection mode
    if(get_context() == CTX_SELECTION) {
        ssd1306_bmp_show_image_with_offset(p, icon_looper_data, icon_looper_size, ICON_CENTERED_MARGIN_X, 0);
        return;
    }

    // Row 1: state icon + label + progress bar
    uint8_t state_x = 0;
    uint8_t state_y = 0;
    const char *state_label = "";

    if(looper_is_recording()) {
        ssd1306_bmp_show_image_with_offset(p, icon_rec_data, icon_rec_size, state_x, state_y);
        state_label = "REC";
    } else if(looper_is_playing()) {
        ssd1306_bmp_show_image_with_offset(p, icon_play_data, icon_play_size, state_x, state_y);
        state_label = "PLAY";
    } else if(looper_has_recording()) {
        ssd1306_bmp_show_image_with_offset(p, icon_pause_data, icon_pause_size, state_x, state_y);
        state_label = "PAU";
    } else {
        ssd1306_bmp_show_image_with_offset(p, icon_ready_data, icon_ready_size, state_x, state_y);
        state_label = "RDY";
    }

    ssd1306_draw_string(p, 14, 0, 1, state_label);

    uint32_t length_ms = looper_get_loop_length_ms();
    uint32_t elapsed_ms = looper_get_elapsed_ms();
    bool is_rec = looper_is_recording();

    // For recording, show progress against max length
    if (is_rec && length_ms == 0) {
        length_ms = looper_get_max_length_ms();
    }

    if (looper_has_recording() || is_rec) {
        if (!is_rec && length_ms > 0) {
            elapsed_ms = elapsed_ms % length_ms;
        }
        uint8_t bar_x = 42;
        uint8_t bar_y = 0;
        uint8_t bar_w = 84;
        uint8_t bar_h = 8;
        ssd1306_draw_square(p, bar_x, bar_y, bar_w, bar_h);
        if (length_ms > 0) {
            uint32_t clamped = elapsed_ms;
            if (clamped > length_ms) clamped = length_ms;
            uint32_t width = clamped * (bar_w - 2) / length_ms;
            ssd1306_draw_square(p, bar_x + 1, bar_y + 1, width, bar_h - 2);
        }
    }

    // Row 2: time
    char time_buf[22];
    uint32_t length_tenths = (length_ms + 50) / 100; // ms to 0.1s rounding
    uint32_t elapsed_tenths = (elapsed_ms + 50) / 100;
    snprintf(time_buf, sizeof(time_buf), "%lu.%lu / %lu.%lu s",
        (unsigned long)(elapsed_tenths / 10), (unsigned long)(elapsed_tenths % 10),
        (unsigned long)(length_tenths / 10), (unsigned long)(length_tenths % 10));
    ssd1306_draw_string(p, 0, 12, 1, time_buf);

    // Row 3: hint or event count
    if(get_context() == CTX_LOOPER) {
        ssd1306_draw_string(p, 0, 24, 1, "Btn Rec/Play | Hold Clear | Enc Restart");
    } else {
        char evt_buf[16];
        snprintf(evt_buf, sizeof(evt_buf), "evts:%u", looper_get_event_count());
        ssd1306_draw_string(p, 0, 24, 1, evt_buf);
    }
}

static inline void draw_imu_axes_screen(ssd1306_t *p) {
    switch (get_imu_axes()) {
        case 0x0:
            ssd1306_bmp_show_image_with_offset(p, icon_imu_off_data, icon_imu_off_size, 48, 0);
        break;
        case 0x1:
            ssd1306_bmp_show_image_with_offset(p, icon_imu_on_x_data, icon_imu_on_x_size, 48, 0);
        break;
        case 0x2:
            ssd1306_bmp_show_image_with_offset(p, icon_imu_on_y_data, icon_imu_on_y_size, 48, 0);
        break;
        case 0x3:
            ssd1306_bmp_show_image_with_offset(p, icon_imu_on_x_y_data, icon_imu_on_x_y_size, 48, 0);
        break;
    }

    // Draw selection mark
    if(get_context() == CTX_IMU_CONFIG) {
        ssd1306_draw_square(p, 0, 0, 2, 32);
    }
}

static inline void draw_contrast_screen(ssd1306_t *p) {
    uint8_t contrast = get_contrast();
    switch (contrast) {
        case CONTRAST_MIN:
            ssd1306_bmp_show_image_with_offset(p, icon_contrast_min_data, icon_contrast_min_size, 48, 0);
        break;
        case CONTRAST_MED:
            ssd1306_bmp_show_image_with_offset(p, icon_contrast_med_data, icon_contrast_med_size, 48, 0);
        break;
        case CONTRAST_MAX:
            ssd1306_bmp_show_image_with_offset(p, icon_contrast_max_data, icon_contrast_max_size, 48, 0);
        break;
        case CONTRAST_AUTO:
            ssd1306_bmp_show_image_with_offset(p, icon_contrast_max_data, icon_contrast_max_size, 48, 0);
            ssd1306_clear_square(p, 48, 11, 32, 10);
            ssd1306_draw_string(p, 53, 13, 1, "AUTO");
        break;
    }

    // Draw selection mark
    ssd1306_draw_square(p, 0, 0, 2, 32);
}

static inline void draw_chord_mode_screen(ssd1306_t *p) {
    ssd1306_draw_string(p, 0, 0, 1, "Chord Mode:");
    
    uint8_t mode = get_chord_mode();
    
    // Draw mode name centered and large
    const char *mode_name = chord_mode_names[mode];
    uint8_t name_len = strlen(mode_name);
    uint8_t margin_x = (128 - (name_len * 12)) / 2;  // Center for scale 2 font
    ssd1306_draw_string(p, margin_x, 14, 2, mode_name);
    
    // Draw selection mark if in edit mode
    if(get_context() == CTX_CHORD) {
        ssd1306_draw_square(p, margin_x, 30, name_len * 12, 2);
    } else {
        // Selection mode - just show indicator
        ssd1306_draw_square(p, 0, 0, 2, 8);
    }
}

static inline void draw_arp_pattern_screen(ssd1306_t *p) {
    ssd1306_draw_string(p, 0, 0, 1, "Arp Pattern:");
    
    uint8_t pattern = get_arp_pattern();
    
    // Draw pattern name centered and large
    const char *pattern_name = arp_pattern_names[pattern];
    uint8_t name_len = strlen(pattern_name);
    uint8_t margin_x = (128 - (name_len * 12)) / 2;  // Center for scale 2 font
    ssd1306_draw_string(p, margin_x, 14, 2, pattern_name);
    
    // Draw selection mark if in edit mode
    if(get_context() == CTX_ARP_PATTERN) {
        ssd1306_draw_square(p, margin_x, 30, name_len * 12, 2);
    } else {
        // Selection mode - just show indicator
        ssd1306_draw_square(p, 0, 0, 2, 8);
    }
}

static inline void draw_arp_speed_screen(ssd1306_t *p) {
    ssd1306_draw_string(p, 0, 0, 1, "Arp Speed:");
    
    uint16_t speed_ms = get_arp_speed_ms();
    // Calculate BPM: 60000ms / interval = BPM
    uint16_t bpm = (speed_ms > 0) ? (60000 / speed_ms) : 0;
    
    // Format as BPM value
    char speed_str[16];
    snprintf(speed_str, sizeof(speed_str), "%d BPM", bpm);
    
    uint8_t name_len = strlen(speed_str);
    uint8_t margin_x = (128 - (name_len * 12)) / 2;  // Center for scale 2 font
    ssd1306_draw_string(p, margin_x, 14, 2, speed_str);
    
    // Draw selection mark if in edit mode
    if(get_context() == CTX_ARP_SPEED) {
        ssd1306_draw_square(p, margin_x, 30, name_len * 12, 2);
    } else {
        // Selection mode - just show indicator
        ssd1306_draw_square(p, 0, 0, 2, 8);
    }
}

static inline void draw_arp_octave_screen(ssd1306_t *p) {
    ssd1306_draw_string(p, 0, 0, 1, "Arp Octaves:");
    
    uint8_t octave = get_arp_octave();
    
    // Draw octave name centered and large
    const char *octave_name = arp_octave_names[octave];
    uint8_t name_len = strlen(octave_name);
    uint8_t margin_x = (128 - (name_len * 12)) / 2;  // Center for scale 2 font
    ssd1306_draw_string(p, margin_x, 14, 2, octave_name);
    
    // Draw selection mark if in edit mode
    if(get_context() == CTX_ARP_OCTAVE) {
        ssd1306_draw_square(p, margin_x, 30, name_len * 12, 2);
    } else {
        // Selection mode - just show indicator
        ssd1306_draw_square(p, 0, 0, 2, 8);
    }
}

static inline void draw_synth_edit_screen(ssd1306_t *p) {
    char str[3];
    sprintf(str, "%d", get_argument());
    ssd1306_draw_string(p, 4, 0, 1, parameter_names[get_parameter()]);

    // Center the string
    uint8_t margin_x = 38; // 3 digits
    if(get_argument() < 100) { // 2 digits
        margin_x = 50;
    } else if(get_argument() < 10) { // 1 digit
        margin_x = 62;
    }
    ssd1306_draw_string(p, margin_x, 18, 2, str);

    // Draw selection mark
    if(get_context() == CTX_SYNTH_EDIT_PARAM) {
        ssd1306_draw_square(p, 0, 0, 2, 7);
    } else {
        ssd1306_draw_square(p, 0, 18, 2, 14);
    }
}

// If you changed NUM_PRESET_SLOTS in config.h you will have to adjust this function accordingly
static inline void draw_synth_store_screen(ssd1306_t *p) {
    int8_t slot = get_preset_slot();

    uint8_t position = slot + 1;

    ssd1306_draw_string(p, 0, 0, 1, "Store preset in slot:");

    // Close icon
    ssd1306_bmp_show_image_with_offset(p, icon_close_data, icon_close_size, 8, 11);

    ssd1306_draw_string_with_font(p,  34, 12, 1, octave_font, "1");
    ssd1306_draw_string_with_font(p,  60, 12, 1, octave_font, "2");
    ssd1306_draw_string_with_font(p,  86, 12, 1, octave_font, "3");
    ssd1306_draw_string_with_font(p, 112, 12, 1, octave_font, "4");
    
    // Underline
    ssd1306_draw_square(p, 8 + 26 * position, 25, 12, 2);
}

static inline void draw_scale_edit_screen(ssd1306_t *p) {
    uint8_t spacing = 18;
    uint8_t line_height = 11;
    uint8_t margin = 2;
    ssd1306_draw_string(p, 0, 0, 1, "Edit scale");

    char str[2];
    for (uint8_t i = 0; i < 6; i++) {
        sprintf(str, "%d", get_degree(i) + 1);
        ssd1306_draw_string(p, margin + i * spacing, line_height, 1, str);
    }

    for (uint8_t i = 0; i < 6; i++) {
        sprintf(str, "%d", get_degree(i+6) + 1);
        ssd1306_draw_string(p, margin + i * spacing, line_height*2, 1, str);
    }

    uint8_t step = get_step();
    uint8_t newline = (step >= 6);
    uint8_t x = step * spacing;
    if(newline) { x -= 6 * spacing; }
    uint8_t y = 8 + line_height + line_height * newline;

    if(get_context() == CTX_SCALE_EDIT_STEP) {
        ssd1306_draw_square(p, margin + x, y, 11, 2);
    } else { // CTX_SCALE_EDIT_DEG
        ssd1306_draw_empty_square(p, x, y-line_height, 15, 11);
    }
}

// If you changed NUM_SCALE_SLOTS in config.h you will have to adjust this function accordingly
static inline void draw_scale_store_screen(ssd1306_t *p) {
    int8_t slot = get_scale_slot();

    uint8_t position = slot + 1;

    ssd1306_draw_string(p, 0, 0, 1, "Store scale in slot:");

    // Close icon
    ssd1306_bmp_show_image_with_offset(p, icon_close_data, icon_close_size, 8, 11);

    ssd1306_draw_string_with_font(p,  34, 12, 1, octave_font, "1");
    ssd1306_draw_string_with_font(p,  60, 12, 1, octave_font, "2");
    ssd1306_draw_string_with_font(p,  86, 12, 1, octave_font, "3");
    ssd1306_draw_string_with_font(p, 112, 12, 1, octave_font, "4");
    
    // Underline
    ssd1306_draw_square(p, 8 + 26 * position, 25, 12, 2);
}

void intro_animation(ssd1306_t *p, void (*callback)(void)) {
    for(uint8_t current_frame=0; current_frame < INTRO_FRAMES_NUM; current_frame++) {
        ssd1306_bmp_show_image_with_offset(p, intro_frames[current_frame], INTRO_FRAME_SIZE, ICON_CENTERED_MARGIN_X, 0);
        i2c1_mutex_enter();
        ssd1306_show(p);
        i2c1_mutex_exit();
        busy_wait_ms(42); // About 24fps
        ssd1306_clear(p);
    }
    callback();
}

// Helper to draw the currently playing note indicator
// Shows note name prominently in the center of the display
static inline void draw_note_indicator(ssd1306_t *p) {
    uint16_t active = get_active_pads();
    if (active == 0) return;  // No pads active
    
    uint8_t note = get_last_note();
    uint8_t note_tonic = note % 12;
    uint8_t note_octave = note / 12;
    
    // Map tonic to alteration (sharps)
    static const bool alteration_map[12] = {0,1,0,1,0,0,1,0,1,0,1,0};
    bool is_sharp = alteration_map[note_tonic];
    
    // Clear center area (preserve key on left and volume on right)
    ssd1306_clear_square(p, 28, 0, 90, 32);
    
    // Build note string: "C#4" or "G4" etc.
    // Draw large and centered
    uint8_t chord_mode = get_chord_mode();
    
    // Font widths at scale 1: key_font=24px, diesis_font=10px, octave_font=12px
    // At scale 1, these fit better. Use scale 1 for the note indicator.
    const uint8_t note_width = 24;   // key_font width
    const uint8_t sharp_width = 10;  // diesis_font width  
    const uint8_t octave_width = 12; // octave_font width
    const uint8_t chord_width = 12;  // chord symbol width (standard font at scale 1)
    
    // Calculate total width
    uint8_t total_width = note_width;  // Note letter
    if (is_sharp) total_width += sharp_width;
    total_width += octave_width;  // Octave
    if (chord_mode != CHORD_OFF) total_width += chord_width;
    
    uint8_t x_start = 28 + (90 - total_width) / 2;  // Center in the middle area
    uint8_t x = x_start;
    
    // Draw note name (scale 1)
    ssd1306_draw_string_with_font(p, x, 4, 1, key_font, note_names[note_tonic]);
    x += note_width;
    
    // Draw sharp if needed
    if (is_sharp) {
        ssd1306_draw_string_with_font(p, x, 0, 1, diesis_font, diesis);
        x += sharp_width;
    }
    
    // Draw octave number
    if (note_octave <= 10) {
        ssd1306_draw_string_with_font(p, x, 4, 1, octave_font, octave_names[note_octave]);
        x += octave_width;
    }
    
    // Show chord indicator if chord mode is active
    if (chord_mode != CHORD_OFF) {
        const char *chord_symbol;
        switch(chord_mode) {
            case CHORD_POWER: chord_symbol = "5"; break;
            case CHORD_TRIAD: chord_symbol = "+"; break;
            case CHORD_OCTAVE: chord_symbol = "8"; break;
            default: chord_symbol = ""; break;
        }
        ssd1306_draw_string(p, x + 2, 4, 1, chord_symbol);
    }
}

#define OFFSET_X    32
#define CHAR_W      7 // Includes spacing
static inline void draw_main_screen(ssd1306_t *p) {
    // Key
    ssd1306_draw_string_with_font(p, 0, 4, 1, key_font, note_names[get_tonic()]);
    if (get_alteration()) { ssd1306_draw_string_with_font(p, 15, 0, 1, diesis_font, diesis); }
    ssd1306_draw_string_with_font(p, 15, 16, 1, octave_font, octave_names[get_octave()]);

    // Scale
    if (get_scale_unsaved()) {
        ssd1306_draw_string_with_font(p, OFFSET_X, 4, 1, spaced_font, "Custom");
    } else {
        ssd1306_draw_string_with_font(p, OFFSET_X, 4, 1, spaced_font, scale_names[get_scale()]);
    }
    uint8_t scale_name_width = strlen(scale_names[get_scale()]);
    if (scale_name_width > 12) { scale_name_width = 12; }

    // Instrument
    ssd1306_draw_string_with_font(p, OFFSET_X, 21, 1, spaced_font, instrument_names[get_instrument()]);
    uint8_t instrument_name_width = strlen(instrument_names[get_instrument()]);
    if (instrument_name_width > 12) { instrument_name_width = 12; }

    // Volume
    uint8_t increments = 9; // 0 to 8
    uint8_t bar_height = 3;

    for (uint8_t i = 1; i < increments; i++) {
        uint8_t y = 32 - (bar_height * i + i);
        if(get_volume() >= i) {
            ssd1306_draw_square(p, 120, y, 7, bar_height);
        } 
    }
    if(get_volume() == 0) {
        ssd1306_draw_string(p, 122, 13, 1, "X");
    }
    
    // Draw selection mark and underline
    switch(get_context()) {
        case CTX_SELECTION: {
            switch(get_selection()) {
                case SELECTION_KEY:
                    ssd1306_draw_square(p, 0, 30, 6, 2);
                break;
                case SELECTION_SCALE:
                    ssd1306_draw_square(p, OFFSET_X, 13, 6, 2);
                break;
                case SELECTION_INSTRUMENT:
                    ssd1306_draw_square(p, OFFSET_X, 30, 6, 2);
                break;
                case SELECTION_VOLUME:
                    ssd1306_draw_square(p, 117, 0, 2, 6);
                break;
            }
        }
        break;
        case CTX_KEY:
            ssd1306_draw_square(p, 0, 30, 25, 2);
        break;
        case CTX_SCALE:
            ssd1306_draw_square(p, OFFSET_X, 13, CHAR_W * scale_name_width, 2);
        break;
        case CTX_INSTRUMENT:
            ssd1306_draw_square(p, OFFSET_X, 30, CHAR_W * instrument_name_width, 2);
        break;
        case CTX_VOLUME:
            ssd1306_draw_square(p, 117, 0, 2, 32);
        break;
    }

    // Low battery icon
    if(get_low_batt()) {
        // ssd1306_bmp_show_image_with_offset(p, icon_low_batt_data, icon_low_batt_size, 120, 21);
        // Clear an outline around the icon to avoid overlaps
        ssd1306_clear_square(p, 119, 17, 9, 15);
        ssd1306_bmp_show_image_with_offset(p, icon_low_batt_data, icon_low_batt_size, 121, 18);
    }
    
    // Visual note indicator - show currently playing note
    draw_note_indicator(p);
}

void display_dim(ssd1306_t *p) {
    // Use try_enter since this may be called from alarm callback (interrupt context)
    if (i2c1_mutex_try_enter()) {
        ssd1306_contrast(p, 0);
        i2c1_mutex_exit();
    }
    // If mutex busy, skip - collision is rare and display staying bright briefly is acceptable
}

int64_t display_dim_callback(alarm_id_t id, void * p) {
    display_dim((ssd1306_t *)p);
    return 0;  // Don't reschedule
}

void display_wake(ssd1306_t *p) {
    i2c1_mutex_enter();
    ssd1306_contrast(p, 255);
    i2c1_mutex_exit();
    if (display_dim_alarm_id) cancel_alarm(display_dim_alarm_id);
    display_dim_alarm_id = add_alarm_in_ms(DISPLAY_DIM_DELAY * 1000, display_dim_callback, p, true);
}

void display_refresh(ssd1306_t *p) {
    i2c1_mutex_enter();
    ssd1306_reset(p);
    ssd1306_show(p);
    i2c1_mutex_exit();
}

void display_request_refresh(void) {
    display_pending = true;
}

void display_update_contrast(ssd1306_t *p) {
    if (display_dim_alarm_id) cancel_alarm(display_dim_alarm_id);
    uint8_t contrast = get_contrast();
    i2c1_mutex_enter();
    switch (contrast) {
        case CONTRAST_MIN:
            ssd1306_contrast(p, 0);
        break;
        case CONTRAST_MED:
            ssd1306_contrast(p, 127);
        break;
        case CONTRAST_MAX:
            ssd1306_contrast(p, 255);
        break;
        case CONTRAST_AUTO:
            i2c1_mutex_exit();  // Release before calling display_wake which acquires mutex
            display_wake(p);
            return;  // display_wake already released mutex
        break;
    }
    i2c1_mutex_exit();
}

void display_draw(ssd1306_t *p) {
    // Try to acquire I2C lock - if busy, mark as pending and return immediately
    // This prevents blocking note playback while waiting for IMU reads to complete
    if (!i2c1_mutex_try_enter()) {
        display_pending = true;
        return;
    }
    
    display_pending = false;
    selection_t selection = get_selection();
    context_t context = get_context();

    ssd1306_clear(p);

    switch(context) {
        case CTX_SELECTION: {
            switch (selection) {
                case SELECTION_KEY:
                case SELECTION_SCALE:
                case SELECTION_INSTRUMENT:
                case SELECTION_VOLUME:
                    draw_main_screen(p);
                break;
                case SELECTION_CHORD:
                    draw_chord_mode_screen(p);
                break;
                case SELECTION_ARPEGGIO:
                    draw_arp_pattern_screen(p);
                break;
                case SELECTION_LOOPER:
                    draw_looper_screen(p);
                break;
                case SELECTION_IMU_CONFIG:
                    draw_imu_axes_screen(p);
                break;
            }
            break;
        }
        case CTX_KEY:
        case CTX_SCALE:
        case CTX_INSTRUMENT:
        case CTX_VOLUME:
            draw_main_screen(p);
        break;
        case CTX_CHORD:
            draw_chord_mode_screen(p);
        break;
        case CTX_ARP_PATTERN:
            draw_arp_pattern_screen(p);
        break;
        case CTX_ARP_SPEED:
            draw_arp_speed_screen(p);
        break;
        case CTX_ARP_OCTAVE:
            draw_arp_octave_screen(p);
        break;
        case CTX_CONTRAST:
            draw_contrast_screen(p);
        break;
        case CTX_LOOPER:
            draw_looper_screen(p);
        break;
        case CTX_IMU_CONFIG:
            draw_imu_axes_screen(p);
        break;
        case CTX_SYNTH_EDIT_PARAM:
        case CTX_SYNTH_EDIT_ARG:
            draw_synth_edit_screen(p);
        break;
        case CTX_SYNTH_EDIT_STORE:
            draw_synth_store_screen(p);
        break;
        case CTX_SCALE_EDIT_STEP:
        case CTX_SCALE_EDIT_DEG:
            draw_scale_edit_screen(p);
        break;
        case CTX_SCALE_EDIT_STORE:
            draw_scale_store_screen(p);
        break;
        case CTX_INFO:
            draw_info_screen(p);
        break;
    }

    ssd1306_show(p);
    i2c1_mutex_exit();
}

bool display_is_pending(void) {
    return display_pending;
}
